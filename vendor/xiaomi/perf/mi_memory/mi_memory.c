// SPDX-License-Identifier: GPL-2.0
/*
 * mi_memory - 6.12 port, reconstructed from the stock HyperOS 4 (6.6.57)
 * mi_memory.ko (vendor_boot, OS4.0.0.44.XPYCNXM; original source path
 * drivers/mi_ufs/mi_memory/mem_interface.c, not released by Xiaomi).
 *
 * Exposes /sys/class/mi_memory/mi_memory_device/{ufshcd0,ddr}/..., /proc/mv
 * and the debugfs link ufshcd0. Userspace users on stock: odm
 * device_info_qr_config.yml (storage_id_path .../ufshcd0/dump_string_desc_serial)
 * and vendor_sepolicy genfscon /devices/virtual/mi_memory/mi_memory_device/ufshcd.
 *
 * Differences from stock (see NOTES.md):
 *  - Stock called Xiaomi-only exports of its modified ufshcd/ufs-qcom
 *    (mi_ufshcd_read_desc_param, mi_ufshcd_query_descriptor_retry,
 *    get_ufs_data). This port uses the GKI 6.12 exports
 *    ufshcd_read_desc_param() and ufshcd_query_descriptor_retry().
 *  - "hr" (vendor health report through vendor-specific SCSI commands with
 *    per-vendor passwords) is not reconstructed: it prints "NOT SUPPORTED".
 *  - err_state / err_reason came from a Xiaomi ufs-qcom private struct;
 *    they report 0 / empty here.
 *  - show_hba drops the Xiaomi ufs-qcom private pa_/dl_ error counters.
 */
#define pr_fmt(fmt) "mi_memory: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/nls.h>
#include <linux/pm_runtime.h>
#include <linux/unaligned.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <ufs/ufshcd.h>
#include <ufs/ufs.h>

/* drivers/ufs/core/ufshcd-priv.h (exported, header private) */
int ufshcd_query_descriptor_retry(struct ufs_hba *hba, enum query_opcode opcode,
				  enum desc_idn idn, u8 index, u8 selector,
				  u8 *desc_buf, int *buf_len);

struct ddr_info {
	u8 size;	/* ddr_size, GB */
	u8 id;		/* ddr_manufacturer_id (JEDEC) */
	u32 type;	/* ddr_device_type */
};

struct mv_ufs {
	u16 id;		/* wManufacturerID */
	u64 size;	/* GB, rounded to a power of two */
	char name[18];
	char rev[6];
};

static struct scsi_device *sdev;
static struct ddr_info *ddr_info;
static struct mv_ufs *mv_ufs;
static int mem_major;
static struct class *mem_class;
static struct device *mem_dev;
static struct dentry *mem_dbg_link;

static const struct file_operations mem_ops = { .owner = THIS_MODULE };

/* ------------------------------------------------------------------ UFS */

static struct ufs_hba *get_ufs_hba_data(void)
{
	struct Scsi_Host *shost;

	if (!sdev) {
		shost = scsi_host_lookup(0);
		if (!shost)
			return NULL;
		sdev = scsi_device_lookup(shost, 0, 0, 0);
		if (sdev)
			pr_info("[mi_ufs_info] scsi device proc name is %s\n",
				sdev->host->hostt->proc_name);
		/* stock drops both references but keeps the pointer */
		if (sdev)
			scsi_device_put(sdev);
		scsi_host_put(shost);
		if (!sdev)
			return NULL;
	}
	return shost_priv(sdev->host);
}

/* stock ufs_read_desc_param(): read and convert big-endian params */
static int ufs_read_desc_param(struct ufs_hba *hba, enum desc_idn idn, int index,
			       u8 offset, void *out, u8 size)
{
	u8 raw[8] = {0};
	int ret;

	if (!hba || size > 8)
		return -EINVAL;

	pm_runtime_get_sync(hba->dev);
	ret = ufshcd_read_desc_param(hba, idn, index, offset, raw, size);
	pm_runtime_put(hba->dev);
	if (ret)
		return -EINVAL;

	switch (size) {
	case 2: *(u16 *)out = get_unaligned_be16(raw); break;
	case 4: *(u32 *)out = get_unaligned_be32(raw); break;
	case 8: *(u64 *)out = get_unaligned_be64(raw); break;
	default: *(u8 *)out = raw[0]; break;
	}
	return 0;
}

/*
 * stock ufs_get_string_desc(hba, buf, size, devdesc_offset, ascii):
 * index = device descriptor byte @devdesc_offset; read that string
 * descriptor; ascii -> UTF-16 to printable ASCII, else for iSerialNumber
 * (0x16) produce "serial:" + "%02x%02x" per UTF-16 unit + "\n".
 */
static int ufs_get_string_desc(struct ufs_hba *hba, char *buf, int size,
			       u8 devdesc_offset, bool ascii)
{
	u8 *desc, *str;
	int len = QUERY_DESC_MAX_SIZE, ret, i, pos;
	u8 index;

	if (!hba)
		return -ENODEV;

	desc = kzalloc(QUERY_DESC_MAX_SIZE, GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	pm_runtime_get_sync(hba->dev);
	ret = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
					    QUERY_DESC_IDN_DEVICE, 0, 0, desc, &len);
	pm_runtime_put(hba->dev);
	if (ret || devdesc_offset >= len) {
		kfree(desc);
		return ret ? ret : -EINVAL;
	}
	index = desc[devdesc_offset];

	memset(desc, 0, QUERY_DESC_MAX_SIZE);
	pm_runtime_get_sync(hba->dev);
	ret = ufshcd_read_desc_param(hba, QUERY_DESC_IDN_STRING, index, 0,
				     desc, QUERY_DESC_MAX_SIZE);
	pm_runtime_put(hba->dev);
	if (ret < 0) {
		dev_err(hba->dev, "Reading String Desc failed after %d retries. err = %d\n",
			3, ret);
		kfree(desc);
		return ret;
	}

	len = desc[0];	/* bLength */
	if (len < 3) {
		kfree(desc);
		return 0;
	}

	if (ascii) {
		int ulen = (len - 2) / 2 + 1;

		str = kzalloc(ulen, GFP_KERNEL);
		if (!str) {
			kfree(desc);
			return -ENOMEM;
		}
		ret = utf16s_to_utf8s((const wchar_t *)&desc[2], (len - 2) / 2,
				      UTF16_BIG_ENDIAN, str, ulen - 1);
		for (i = 0; i < ret; i++)
			if (str[i] < 0x20 || str[i] >= 0x7f)
				str[i] = ' ';
		str[max(ret, 0)] = '\0';
		strscpy(buf, str, size);
		kfree(str);
		ret = 0;
	} else if (devdesc_offset == 0x16) {
		pos = scnprintf(buf, size, "serial:");
		for (i = 2; i + 1 < len; i += 2)
			pos += scnprintf(buf + pos, size - pos, "%02x%02x",
					 desc[i], desc[i + 1]);
		scnprintf(buf + pos, size - pos, "\n");
		ret = 0;
	} else {
		memcpy(buf, desc, min(size, len));
		ret = 0;
	}

	kfree(desc);
	return ret;
}

struct desc_field { const char *name; u8 offset; u8 size; };

static const struct desc_field device_desc_fields[] = {
	{ "bLength", 0x00, 1 }, { "bDescriptorType", 0x01, 1 },
	{ "bDevice", 0x02, 1 }, { "bDeviceClass", 0x03, 1 },
	{ "bDeviceSubClass", 0x04, 1 }, { "bProtocol", 0x05, 1 },
	{ "bNumberLU", 0x06, 1 }, { "bNumberWLU", 0x07, 1 },
	{ "bBootEnable", 0x08, 1 }, { "bDescrAccessEn", 0x09, 1 },
	{ "bInitPowerMode", 0x0a, 1 }, { "bHighPriorityLUN", 0x0b, 1 },
	{ "bSecureRemovalType", 0x0c, 1 }, { "bSecurityLU", 0x0d, 1 },
	{ "Reserved", 0x0e, 1 }, { "bInitActiveICCLevel", 0x0f, 1 },
	{ "wSpecVersion", 0x10, 2 }, { "wManufactureDate", 0x12, 2 },
	{ "iManufactureName", 0x14, 1 }, { "iProductName", 0x15, 1 },
	{ "iSerialNumber", 0x16, 1 }, { "iOemID", 0x17, 1 },
	{ "wManufactureID", 0x18, 2 }, { "bUD0BaseOffset", 0x1a, 1 },
	{ "bUDConfigPLength", 0x1b, 1 }, { "bDeviceRTTCap", 0x1c, 1 },
	{ "wPeriodicRTCUpdate", 0x1d, 2 }, { "bUFSFeaturesSupport", 0x1f, 1 },
	{ "bFFUTimeout", 0x20, 1 }, { "bQueueDepth", 0x21, 1 },
	{ "wDeviceVersion", 0x22, 2 }, { "bNumSecureWpArea", 0x24, 1 },
	{ "dPSAMaxDataSize", 0x25, 4 }, { "bPSAStateTimeout", 0x29, 1 },
	{ "iProductRevisionLevel", 0x2a, 1 },
};

static const struct desc_field health_desc_fields[] = {
	{ "bLength", 0, 1 }, { "bDescriptorType", 1, 1 }, { "bPreEOLInfo", 2, 1 },
	{ "bDeviceLifeTimeEstA", 3, 1 }, { "bDeviceLifeTimeEstB", 4, 1 },
};

static ssize_t dump_desc(char *buf, enum desc_idn idn, const char *what,
			 const struct desc_field *f, int nf, const char *fail)
{
	struct ufs_hba *hba = get_ufs_hba_data();
	u8 len = 0;
	u8 *desc;
	int ret, i, n = 0;
	u32 v;

	if (!hba)
		return -ENODEV;
	ufs_read_desc_param(hba, idn, 0, 0, &len, 1);	/* bLength */
	desc = kzalloc(len ? len : 1, GFP_KERNEL);
	if (!desc)
		return sysfs_emit(buf, "%s", fail);

	pm_runtime_get_sync(hba->dev);
	ret = ufshcd_read_desc_param(hba, idn, 0, 0, desc, len);
	pm_runtime_put(hba->dev);
	if (ret) {
		dev_err(hba->dev, "%s: Failed reading  %s, ret %d\n", __func__, what, ret);
		kfree(desc);
		return 0;
	}

	for (i = 0; i < nf; i++) {
		if (f[i].offset + f[i].size > len)
			break;
		switch (f[i].size) {
		case 2: v = get_unaligned_be16(desc + f[i].offset); break;
		case 4: v = get_unaligned_be32(desc + f[i].offset); break;
		default: v = desc[f[i].offset]; break;
		}
		n += scnprintf(buf + n, PAGE_SIZE - n,
			       /* stock uses this string for health too */
			       "Device Descriptor[Byte offset 0x%x]: %s = 0x%x\n",
			       f[i].offset, f[i].name, v);
	}
	kfree(desc);
	return n;
}

static ssize_t dump_health_desc_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return dump_desc(buf, QUERY_DESC_IDN_HEALTH, "dump_health_desc_show",
			 health_desc_fields, ARRAY_SIZE(health_desc_fields),
			 "get health info fail\n");
}

static ssize_t dump_device_desc_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return dump_desc(buf, QUERY_DESC_IDN_DEVICE, "dump_device_desc_show",
			 device_desc_fields, ARRAY_SIZE(device_desc_fields),
			 "get desc info fail\n");
}

static ssize_t dump_string_desc_serial_show(struct device *dev,
					    struct device_attribute *attr, char *buf)
{
	char serial[256] = {0};

	ufs_get_string_desc(get_ufs_hba_data(), serial, sizeof(serial), 0x16, false);
	return sysfs_emit(buf, "%s", serial);
}

static ssize_t show_hba_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct ufs_hba *hba = get_ufs_hba_data();
	struct ufs_stats *st;
	int n = 0;

	if (!hba)
		return -ENODEV;
	st = &hba->ufs_stats;

#define P(fmt, ...) (n += scnprintf(buf + n, PAGE_SIZE - n, fmt, ##__VA_ARGS__))
	P("hba->outstanding_tasks = 0x%x\n", (u32)hba->outstanding_tasks);
	P("hba->outstanding_reqs = 0x%x\n", (u32)hba->outstanding_reqs);
	P("hba->capabilities = 0x%x\n", hba->capabilities);
	P("hba->nutrs = %d\n", hba->nutrs);
	P("hba->nutmrs = %d\n", hba->nutmrs);
	P("hba->ufs_version = 0x%x\n", hba->ufs_version);
	P("hba->irq = 0x%x\n", hba->irq);
	P("hba->auto_bkops_enabled = %d\n", hba->auto_bkops_enabled);
	P("hba->ufshcd_state = 0x%x\n", hba->ufshcd_state);
	P("hba->clk_gating.state = 0x%x\n", hba->clk_gating.state);
	P("hba->eh_flags = 0x%x\n", hba->eh_flags);
	P("hba->intr_mask = 0x%x\n", hba->intr_mask);
	P("hba->ee_ctrl_mask = 0x%x\n", hba->ee_ctrl_mask);
	P("hba->errors = 0x%x\n", hba->errors);
	P("hba->uic_error = 0x%x\n", hba->uic_error);
	P("hba->saved_err = 0x%x\n", hba->saved_err);
	P("hba->saved_uic_err = 0x%x\n", hba->saved_uic_err);
	P("hibern8_exit_cnt = %d\n", st->hibern8_exit_cnt);
	P("ufs_event_pa_error_cnt = %lld\n", st->event[UFS_EVT_PA_ERR].cnt);
	P("ufs_event_dl_error_cnt = %lld\n", st->event[UFS_EVT_DL_ERR].cnt);
	P("ufs_event_nl_error_cnt = %lld\n", st->event[UFS_EVT_NL_ERR].cnt);
	P("ufs_event_tl_error_cnt = %lld\n", st->event[UFS_EVT_TL_ERR].cnt);
	P("ufs_event_dme_error_cnt = %lld\n", st->event[UFS_EVT_DME_ERR].cnt);
	P("ufs_event_auto_hibern8_cnt = %lld\n", st->event[UFS_EVT_AUTO_HIBERN8_ERR].cnt);
	P("ufs_event_fatal_error_cnt= %lld\n", st->event[UFS_EVT_FATAL_ERR].cnt);
	P("ufs_event_link_startup_fail_cnt = %lld\n", st->event[UFS_EVT_LINK_STARTUP_FAIL].cnt);
	P("ufs_event_resume_error_cnt = %lld\n", st->event[UFS_EVT_RESUME_ERR].cnt);
	P("ufs_event_suspend_error_cnt = %lld\n", st->event[UFS_EVT_SUSPEND_ERR].cnt);
	P("ufs_event_device_reset_cnt = %lld\n", st->event[UFS_EVT_DEV_RESET].cnt);
	P("ufs_event_host_reset_cnt = %lld\n", st->event[UFS_EVT_HOST_RESET].cnt);
	P("ufs_event_abort_cnt = %lld\n", st->event[UFS_EVT_ABORT].cnt);
	/* stock then printed ~20 Xiaomi ufs-qcom private pa_/dl_ counters */
#undef P
	return n;
}

static ssize_t hr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* vendor health report (Samsung/SKhynix/Micron/Kioxia/WDC/XBSTOR) not ported */
	return sysfs_emit(buf, "NOT SUPPORTED %s\n", "hr");
}

static ssize_t err_state_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return sysfs_emit(buf, "%lld\n", 0LL);	/* stock: get_ufs_data()->err_state */
}

static ssize_t err_reason_show(struct device *dev, struct device_attribute *attr,
			       char *buf)
{
	return sysfs_emit(buf, "\n");	/* stock: 10 x 32-byte reason strings */
}

static DEVICE_ATTR_RO(dump_health_desc);
static DEVICE_ATTR_RO(dump_string_desc_serial);
static DEVICE_ATTR_RO(dump_device_desc);
static DEVICE_ATTR_RO(show_hba);
static DEVICE_ATTR_RO(hr);
static DEVICE_ATTR_RO(err_state);
static DEVICE_ATTR_RO(err_reason);

static struct attribute *ufshcd_sysfs[] = {
	&dev_attr_dump_health_desc.attr, &dev_attr_dump_string_desc_serial.attr,
	&dev_attr_dump_device_desc.attr, &dev_attr_show_hba.attr,
	&dev_attr_hr.attr, &dev_attr_err_state.attr, &dev_attr_err_reason.attr,
	NULL,
};

static const struct attribute_group ufshcd_sysfs_group = {
	.name = "ufshcd0",
	.attrs = ufshcd_sysfs,
};

/* ------------------------------------------------------------------ DDR */

static void init_ddr_info(void)
{
	struct device_node *np;
	u32 type = 0, size = 0, id = 0;

	ddr_info = kzalloc(sizeof(*ddr_info), GFP_KERNEL);
	if (!ddr_info)
		return;

	np = of_find_node_by_path("/memory");
	if (!np)
		return;
	of_property_read_u32(np, "ddr_device_type", &type);
	of_property_read_u32(np, "ddr_size", &size);
	of_property_read_u32(np, "ddr_manufacturer_id", &id);
	of_node_put(np);

	ddr_info->type = type;
	ddr_info->size = size;
	ddr_info->id = id;
}

static struct ddr_info *ddr(void)
{
	static const struct ddr_info zero;

	if (!ddr_info)
		init_ddr_info();
	return ddr_info ? ddr_info : (struct ddr_info *)&zero;
}

static u8 get_ddr_size(void) { return ddr()->size; }
static u8 get_ddr_id(void) { return ddr()->id; }

static ssize_t ddr_size_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%d\n", ddr()->size);
}

static ssize_t ddr_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "0x%02x\n", ddr()->id);
}

static ssize_t ddr_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "0x%0x\n", ddr()->type);
}

static ssize_t ddr_vendor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	const char *v;

	switch (ddr()->id) {	/* JEDEC LPDDR manufacturer IDs */
	case 0x01: v = "SAMSUNG"; break;
	case 0x03: v = "ELPIDA"; break;
	case 0x05: v = "NANYA"; break;
	case 0x06: v = "SKHYNIX"; break;
	case 0x0e: v = "INTEL"; break;
	case 0x13: v = "CXMT"; break;
	case 0xff: v = "MICRON"; break;
	default: v = "UNKNOWN"; break;
	}
	return sysfs_emit(buf, "%s\n", v);
}

static DEVICE_ATTR_RO(ddr_size);
static DEVICE_ATTR_RO(ddr_id);
static DEVICE_ATTR_RO(ddr_vendor);
static DEVICE_ATTR_RO(ddr_type);

static struct attribute *dram_sysfs[] = {
	&dev_attr_ddr_size.attr, &dev_attr_ddr_id.attr,
	&dev_attr_ddr_vendor.attr, &dev_attr_ddr_type.attr, NULL,
};

static const struct attribute_group dram_sysfs_group = {
	.name = "ddr",
	.attrs = dram_sysfs,
};

static const struct attribute_group *memory_sysfs_groups[] = {
	&ufshcd_sysfs_group, &dram_sysfs_group, NULL,
};

/* ------------------------------------------------------------- /proc/mv */

static int mv_proc_show(struct seq_file *m, void *v)
{
	if (!mv_ufs) {
		struct ufs_hba *hba = get_ufs_hba_data();
		u64 raw = 0, gb;

		mv_ufs = kzalloc(sizeof(*mv_ufs), GFP_KERNEL);
		if (!mv_ufs)
			return -ENOMEM;
		ufs_get_string_desc(hba, mv_ufs->name, 17, 0x15, true);	/* iProductName */
		ufs_get_string_desc(hba, mv_ufs->rev, 5, 0x2a, true);	/* iProductRevisionLevel */
		ufs_read_desc_param(hba, QUERY_DESC_IDN_DEVICE, 0, 0x18, &mv_ufs->id, 2);
		ufs_read_desc_param(hba, QUERY_DESC_IDN_GEOMETRY, 0, 0x04, &raw, 8);

		gb = raw >> 21;	/* qTotalRawDeviceCapacity, 512 B units -> GiB */
		if (gb > 512 && gb <= 1024)	mv_ufs->size = 1024;
		else if (gb > 256)		mv_ufs->size = 512;
		else if (gb > 128)		mv_ufs->size = 256;
		else if (gb > 64)		mv_ufs->size = 128;
		else if (gb > 32)		mv_ufs->size = 64;
		else if (gb > 16)		mv_ufs->size = 32;
		else if (gb > 8)		mv_ufs->size = 8;	/* stock quirk */
		else {
			mv_ufs->size = 0;
			pr_info("mv unkonwn ufs size %lld\n", gb);
		}
	}

	seq_printf(m, "D: 0x%02x %d\n", get_ddr_id(), get_ddr_size());
	seq_printf(m, "U: 0x%04x %lld %s %s\n", mv_ufs->id, mv_ufs->size,
		   mv_ufs->name, mv_ufs->rev);
	return 0;
}

static int mv_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mv_proc_show, NULL);
}

static const struct proc_ops mv_proc_fops = {
	.proc_open	= mv_proc_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

/* ----------------------------------------------------------------- init */

static int __init mi_memory_init(void)
{
	int ret;

	mem_class = class_create("mi_memory");
	if (IS_ERR(mem_class)) {
		ret = PTR_ERR(mem_class);
		pr_err("%s: mi memory info class creation failed (err = %d)\n",
		       "mi_memory_sysfs_init", ret);
		return ret;
	}

	mem_major = register_chrdev(0, "mi_memory_module", &mem_ops);
	if (mem_major < 0) {
		ret = mem_major;
		pr_err("%s: mi memory info chrdev creation failed (err = %d)\n",
		       "mi_memory_sysfs_init", ret);
		goto err_class;
	}

	mem_dev = device_create(mem_class, NULL, MKDEV(mem_major, 1), NULL,
				"mi_memory_device");
	if (IS_ERR(mem_dev)) {
		ret = -ENODEV;
		pr_err("%s: mi memory info device creation failed (err = %d)\n",
		       "mi_memory_sysfs_init", ret);
		goto err_chrdev;
	}

	ret = sysfs_create_groups(&mem_dev->kobj, memory_sysfs_groups);
	if (ret) {
		pr_err("%s: sysfs groups creation failed (err = %d)\n",
		       "mi_memory_sysfs_init", ret);
		goto err_dev;
	}

	proc_create("mv", 0555, NULL, &mv_proc_fops);
	mem_dbg_link = debugfs_create_symlink("ufshcd0", NULL,
			"../../class/mi_memory/mi_memory_device/ufshcd0");
	return 0;

err_dev:
	device_destroy(mem_class, MKDEV(mem_major, 1));
err_chrdev:
	unregister_chrdev(mem_major, "mi_memory_module");
err_class:
	class_destroy(mem_class);
	return ret;
}

static void __exit mi_memory_exit(void)
{
	debugfs_remove(mem_dbg_link);
	remove_proc_entry("mv", NULL);
	sysfs_remove_groups(&mem_dev->kobj, memory_sysfs_groups);
	device_destroy(mem_class, MKDEV(mem_major, 1));
	unregister_chrdev(mem_major, "mi_memory_module");
	class_destroy(mem_class);
	kfree(ddr_info);
	kfree(mv_ufs);
}

module_init(mi_memory_init);
module_exit(mi_memory_exit);
MODULE_DESCRIPTION("Xiaomi memory (UFS/DDR) info");
MODULE_LICENSE("GPL");
