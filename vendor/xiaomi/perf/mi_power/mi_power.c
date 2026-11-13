// SPDX-License-Identifier: GPL-2.0
/*
 * mi_power - 6.12 port, reconstructed from the stock HyperOS 4 (6.6.57)
 * mi_power.ko (vendor_boot, OS4.0.0.44.XPYCNXM). No Xiaomi source exists.
 *
 * Provides:
 *  /sys/class/power_debug/{wakeup_device_list,debug_suspend,power_mode,
 *      temp_aware,subsystem_sleep_state,cxvt_state,ddrvt_state,llcvt_state}
 *  /sys/class/power_debug/mi_power_device/qcom_sleep_stats/{apss,modem,...}
 *  suspend/resume debug logging (suspend_resume tracepoint, freeze todo
 *  vendor hook, PM notifier, syscore resume), 7.5 s battery current log,
 *  and the exported registration API in mi_power.h that Xiaomi's modified
 *  qcom_stats / qcom-spmi-pmic / sys_pm_vx call on stock.
 *
 * See NOTES.md for what is stubbed on 6.12.
 */
#define pr_fmt(fmt) fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/timekeeping.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>
#include <linux/freezer.h>
#include <linux/sched/signal.h>
#include <linux/sched/debug.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/regmap.h>
#include <linux/soc/qcom/smem.h>
#include <linux/firmware/qcom/qcom_scm.h>
#include <clocksource/arm_arch_timer.h>
#include <trace/events/power.h>
#include <trace/hooks/power.h>

#include "mi_power.h"

/* From soc-repo qcom_scm.h; the perf package builds against the GKI copy. */
enum qcom_download_mode {
	QCOM_DOWNLOAD_NODUMP    = 0x00,
	QCOM_DOWNLOAD_EDL       = 0x01,
	QCOM_DOWNLOAD_FULLDUMP  = 0x10,
	QCOM_DOWNLOAD_MINIDUMP  = 0x20,
};
extern void qcom_scm_set_download_mode(enum qcom_download_mode mode);

#define TAG "[xiaomi_power]"

/* soc-repo pinctrl-msm (exported) */
extern void debug_pintctrl_msm_enable(void);
extern void debug_pintctrl_msm_disable(void);

static LIST_HEAD(wakeup_devices);
static DEFINE_SPINLOCK(wakeup_lock);
static LIST_HEAD(system_event_recorders);
static DEFINE_SPINLOCK(records_lock);
static LIST_HEAD(system_dbg_info_list);
static DEFINE_SPINLOCK(dbg_info_lock);

static struct regmap *spmi_regmap;
static int (*mi_cx_stats_get_ss_vote_info)(int n, u8 *votes);
static int (*mi_llc_stats_get_ss_vote_info)(void *buf);
static int (*mi_ddr_stats_get_ss_vote_info)(void *buf);
static void *ss_data;		/* subsystem_sleep_stats_dbg_register() */
static void *soc_data;		/* soc_sleep_stats_dbg_register() */

static u8 gpower_mode;
static u8 temp_aware_mode = 10;
static u64 power_debug_enable = 3;	/* bit0 pinctrl, bit1 regulators, bit24 PMIC dump */

static ktime_t last_monotime, last_stime, curr_monotime;

struct mi_power_dev {
	struct class *class;
	struct device *dev;
	int major;
};
static struct mi_power_dev *mi_power;
static struct delayed_work batt_current_print;

/* ------------------------------------------------------- exported API */

int pm_register_wakeup_device(struct pm_wakeup_device *wd)
{
	struct pm_wakeup_device *it;

	if (!wd)
		return 22;	/* stock returns positive EINVAL */
	list_for_each_entry(it, &wakeup_devices, node)
		if (it->name == wd->name)
			return 17;	/* positive EEXIST, as stock */
	spin_lock(&wakeup_lock);
	list_add_tail(&wd->node, &wakeup_devices);
	spin_unlock(&wakeup_lock);
	return 0;
}
EXPORT_SYMBOL(pm_register_wakeup_device);

int pm_register_system_event_recorder(struct pm_system_event_recorder *r)
{
	struct pm_system_event_recorder *it;

	if (!r)
		return -EINVAL;
	if (r->nr_records) {
		r->records = kmalloc_array(r->nr_records, 120, GFP_KERNEL);
		if (!r->records) {
			pr_err("%s %s:  failed to alloc mem for recorder buffer\n",
			       TAG, __func__);
			return 0;
		}
	}
	list_for_each_entry(it, &system_event_recorders, node)
		if (it->type == r->type)
			return -EEXIST;
	spin_lock(&records_lock);
	list_add_tail(&r->node, &system_event_recorders);
	spin_unlock(&records_lock);
	return 0;
}

int pm_register_system_dbg_info(struct pm_system_dbg_info *info)
{
	struct pm_system_dbg_info *it;

	if (!info)
		return 0;
	if (info->type > 4) {
		pr_err("%s %s:  error type.", TAG, __func__);
		return 0;
	}
	list_for_each_entry(it, &system_dbg_info_list, node)
		if (it->type == info->type)
			return 17;
	spin_lock(&dbg_info_lock);
	list_add_tail(&info->node, &system_dbg_info_list);
	spin_unlock(&dbg_info_lock);
	return 0;
}
EXPORT_SYMBOL(pm_register_system_dbg_info);

void pm_system_dbg_info_print(int type)
{
	struct pm_system_dbg_info *it;

	if ((unsigned int)type >= 5)
		pr_err("%s %s:  error type.", TAG, __func__);	/* stock continues */
	spin_lock(&dbg_info_lock);
	list_for_each_entry(it, &system_dbg_info_list, node)
		if (it->type == type)
			it->print(it->data);
	spin_unlock(&dbg_info_lock);
}
EXPORT_SYMBOL(pm_system_dbg_info_print);

void pm_trigger_system_event_record(int type, void *data)
{
	struct pm_system_event_recorder *it;

	if ((unsigned int)type > 1 || !data)
		return;
	spin_lock(&records_lock);
	list_for_each_entry(it, &system_event_recorders, node)
		if (it->type == type)
			it->record(it, data);
	spin_unlock(&records_lock);
}

void mi_power_regmap_register(struct regmap *map)
{
	spmi_regmap = map;
}
EXPORT_SYMBOL(mi_power_regmap_register);

/* power_debug bit 24: arm full ramdump and program PMIC (SPMI 0-00) regs */
static void spmi_vol_enter_dump_sec(bool enable)
{
	if (!spmi_regmap) {
		pr_info("regmap 0-00 can not register\n");
		return;
	}
	if (!enable)
		return;
	qcom_scm_set_download_mode(QCOM_DOWNLOAD_FULLDUMP);
	regmap_write(spmi_regmap, 0x1316, 0x42);
	regmap_write(spmi_regmap, 0x811, 0x12);
	regmap_write(spmi_regmap, 0x813, 0x12);
	regmap_write(spmi_regmap, 0x815, 0x02);
	regmap_write(spmi_regmap, 0x844, 0x00);
	regmap_write(spmi_regmap, 0x845, 0x00);
	regmap_write(spmi_regmap, 0x846, 0x01);
	regmap_write(spmi_regmap, 0x847, 0x80);
}

void register_mi_cx_stats_get_ss_vote_info(int (*fn)(int n, u8 *votes))
{
	mi_cx_stats_get_ss_vote_info = fn;
}
EXPORT_SYMBOL(register_mi_cx_stats_get_ss_vote_info);

void register_mi_llc_stats_get_ss_vote_info(int (*fn)(void *buf))
{
	mi_llc_stats_get_ss_vote_info = fn;
}
EXPORT_SYMBOL(register_mi_llc_stats_get_ss_vote_info);

void register_mi_ddr_stats_get_ss_vote_info(int (*fn)(void *buf))
{
	mi_ddr_stats_get_ss_vote_info = fn;
}
EXPORT_SYMBOL(register_mi_ddr_stats_get_ss_vote_info);

u8 mi_power_mode(void)
{
	return gpower_mode < 5 ? gpower_mode : 0;
}
EXPORT_SYMBOL(mi_power_mode);

/* power modes 2..4 remap the reported battery capacity */
long mi_power_save_battery_cave(long cap)
{
	long v;

	if (gpower_mode < 2 || gpower_mode > 4)
		return cap;
	if (cap >= 21)
		v = (cap * 100 - 1894) / 107 + 25;
	else if (cap >= 0)
		v = (u16)(cap * 100 + 79) / 80;
	else
		v = cap;
	return clamp(v, 0L, 100L);
}
EXPORT_SYMBOL(mi_power_save_battery_cave);

void log_irq_wakeup_reason(int irq)
{
	pr_err("%s: null irqs list\n", __func__);	/* stock body is just this */
}
EXPORT_SYMBOL(log_irq_wakeup_reason);

void soc_sleep_stats_dbg_register(void *data)
{
	if (data)
		soc_data = data;
}
EXPORT_SYMBOL(soc_sleep_stats_dbg_register);

void subsystem_sleep_stats_dbg_register(void *data)
{
	if (!data)
		return;
	pr_info("%s %s:  entry.", TAG, __func__);
	ss_data = data;
}
EXPORT_SYMBOL(subsystem_sleep_stats_dbg_register);

void subsystem_sleep_stats_dbg_unregister(void)
{
	pr_info("%s %s:  entry.", TAG, __func__);
	ss_data = NULL;
}
EXPORT_SYMBOL(subsystem_sleep_stats_dbg_unregister);

/* ------------------------------------------------ SMEM sleep stats */

struct sleep_stats {
	u32 stat_type;
	u32 count;
	u64 last_entered_at;
	u64 last_exited_at;
	u64 accumulated;
};

struct subsystem_data {
	const char *name;
	u32 smem_item;
	int pid;	/* -2: rpmh master (soc_data), stub on 6.12 */
};

static const struct subsystem_data subsystems[] = {
	{ "apss", 631, QCOM_SMEM_HOST_ANY }, { "modem", 605, 1 },
	{ "wpss", 605, 1 }, { "adsp", 606, 2 }, { "adsp_island", 613, 2 },
	{ "cdsp", 607, 5 }, { "slpi", 608, 3 }, { "gpu", 609, 0 },
	{ "display", 610, 0 }, { "slpi_island", 613, 3 },
	{ "aosd", 0, -2 }, { "cxsd", 1, -2 }, { "ddr", 2, -2 },
};

static int sleep_stats_print(char *buf, const char *name,
			     struct sleep_stats __iomem *s)
{
	struct sleep_stats st;
	char type[5] = {0};
	int n = 0;

	memcpy_fromio(&st, s, sizeof(st));
	memcpy(type, &st.stat_type, 4);
	if (st.last_entered_at > st.last_exited_at)
		st.accumulated += arch_timer_read_counter() - st.last_entered_at;

	n += sprintf(buf + n, "%s:", name);
	n += sprintf(buf + n, "%s\n", type);
	n += sprintf(buf + n, "Count = %u\n", st.count);
	n += sprintf(buf + n, "Last Entered At = %llu\n", st.last_entered_at);
	n += sprintf(buf + n, "Last Exited At = %llu\n", st.last_exited_at);
	n += sprintf(buf + n, "Accumulated Duration = %llu\n", st.accumulated);
	return n;
}

static ssize_t smem_stat_show(const char *name, char *buf)
{
	const struct subsystem_data *s = NULL;
	void *p;
	int i;

	for (i = 0; i < ARRAY_SIZE(subsystems); i++)
		if (!strcmp(subsystems[i].name, name))
			s = &subsystems[i];
	if (!s)
		return 0;

	if (s->pid == -2) {
		/* stock read Xiaomi qcom_stats' rpmh master table (soc_data) */
		pr_err("%s %s:  subsystem %s stats %s.\n", TAG, name, name, "is NULL");
		return 0;
	}

	p = qcom_smem_get(s->pid, s->smem_item, NULL);
	if (IS_ERR_OR_NULL(p)) {
		pr_err("%s %s:  subsystem %s stats %s.\n", TAG, name, name,
		       IS_ERR(p) ? "is not support" : "is null");
		return 0;
	}
	return sleep_stats_print(buf, name, p);
}

#define SS_ATTR(_n)							\
static ssize_t _n##_show(struct device *d, struct device_attribute *a,	\
			 char *buf)					\
{ return smem_stat_show(#_n, buf); }					\
static DEVICE_ATTR_RO(_n)

SS_ATTR(apss); SS_ATTR(modem); SS_ATTR(adsp); SS_ATTR(adsp_island);
SS_ATTR(cdsp); SS_ATTR(slpi); SS_ATTR(slpi_island); SS_ATTR(aosd);
SS_ATTR(cxsd); SS_ATTR(ddr);

static struct attribute *qcom_sleep_stats_attrs[] = {
	&dev_attr_apss.attr, &dev_attr_modem.attr, &dev_attr_adsp.attr,
	&dev_attr_adsp_island.attr, &dev_attr_cdsp.attr, &dev_attr_slpi.attr,
	&dev_attr_slpi_island.attr, &dev_attr_aosd.attr, &dev_attr_cxsd.attr,
	&dev_attr_ddr.attr, NULL,
};

static const struct attribute_group qcom_sleep_stats_sysfs_group = {
	.name = "qcom_sleep_stats",
	.attrs = qcom_sleep_stats_attrs,
};

/* Logs every SMEM subsystem's sleep counter change since the last call. */
static struct { u32 count; u64 entered; } sleep_info_pre[ARRAY_SIZE(subsystems)];

static void subsystem_sleep_stats_print(void *unused)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(subsystems); i++) {
		struct sleep_stats __iomem *p;
		struct sleep_stats st;

		if (subsystems[i].pid == -2)
			continue;
		p = qcom_smem_get(subsystems[i].pid, subsystems[i].smem_item, NULL);
		if (IS_ERR_OR_NULL(p))
			continue;
		memcpy_fromio(&st, p, sizeof(st));
		if (st.last_entered_at > st.last_exited_at)
			st.accumulated += arch_timer_read_counter() - st.last_entered_at;
		pr_info("%s %s:  %s:%u:%llu sleep increased:%u:%llu\n", TAG,
			"soc_rpmh_master_print_one", subsystems[i].name,
			st.count, st.accumulated,
			st.count - sleep_info_pre[i].count,
			st.accumulated - sleep_info_pre[i].entered);
		sleep_info_pre[i].count = st.count;
		sleep_info_pre[i].entered = st.accumulated;
		pr_info("%s %s:  Last Entered/Exit:%llu/%llu\n", TAG,
			"soc_rpmh_master_print_one", st.last_entered_at,
			st.last_exited_at);
	}
}

static struct pm_system_dbg_info subsystem_sleep_dbg_info = {
	.type = 3,	/* printed at machine_suspend */
	.print = subsystem_sleep_stats_print,
};

static int subsystem_sleep_stats_sysfs_print(char *buf)
{
	int i, n = 0;

	for (i = 0; i < ARRAY_SIZE(subsystems); i++)
		if (subsystems[i].pid != -2)
			n += smem_stat_show(subsystems[i].name, buf + n);
	return n;
}

/* ------------------------------------------------ class attributes */

static ssize_t wakeup_device_list_show(const struct class *c,
				       const struct class_attribute *a, char *buf)
{
	struct pm_wakeup_device *it;
	size_t n = 0;

	spin_lock(&wakeup_lock);
	list_for_each_entry(it, &wakeup_devices, node)
		if (it->name)
			n += sprintf(buf + n, "%s ", it->name);
	spin_unlock(&wakeup_lock);
	buf[n] = '\n';
	buf[n + 1] = '\0';
	return n + 1;
}

static ssize_t wakeup_device_list_store(const struct class *c,
					const struct class_attribute *a,
					const char *buf, size_t count)
{
	return count;	/* stock: no-op */
}
static CLASS_ATTR_RW(wakeup_device_list);

static ssize_t debug_suspend_show(const struct class *c,
				  const struct class_attribute *a, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "0x%lx\n", (unsigned long)power_debug_enable);
}

static ssize_t debug_suspend_store(const struct class *c,
				   const struct class_attribute *a,
				   const char *buf, size_t count)
{
	unsigned long long val;

	if (kstrtoull(buf, 0, &val))
		return -EINVAL;

	power_debug_enable = val ? (power_debug_enable | (u32)val) : 0;
	if (power_debug_enable & BIT(0))
		debug_pintctrl_msm_enable();
	else
		debug_pintctrl_msm_disable();
	if (power_debug_enable & BIT(24))
		spmi_vol_enter_dump_sec(true);
	return count;
}
static CLASS_ATTR_RW(debug_suspend);

static ssize_t power_mode_show(const struct class *c,
			       const struct class_attribute *a, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", gpower_mode);
}

static ssize_t power_mode_store(const struct class *c,
				const struct class_attribute *a,
				const char *buf, size_t count)
{
	unsigned long long val;

	if (kstrtoull(buf, 10, &val))
		return -EINVAL;
	gpower_mode = val;
	return count;
}
static CLASS_ATTR_RW(power_mode);

static ssize_t temp_aware_show(const struct class *c,
			       const struct class_attribute *a, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", temp_aware_mode);
}

static ssize_t temp_aware_store(const struct class *c,
				const struct class_attribute *a,
				const char *buf, size_t count)
{
	unsigned long long val;

	if (kstrtoull(buf, 10, &val))
		return -EINVAL;
	if (val <= 9)
		temp_aware_mode = val;
	return count;
}
static CLASS_ATTR_RW(temp_aware);

static ssize_t subsystem_sleep_state_show(const struct class *c,
					  const struct class_attribute *a, char *buf)
{
	return subsystem_sleep_stats_sysfs_print(buf);
}
static CLASS_ATTR_RO(subsystem_sleep_state);

static ssize_t cxvt_state_show(const struct class *c,
			       const struct class_attribute *a, char *buf)
{
	u8 votes[29] = {0};
	int i, n = 0;

	if (!mi_cx_stats_get_ss_vote_info) {
		pr_err("%s %s:  mi_cx_stats_get_ss_vote_info is NULL!", TAG,
		       "cxvt_stats_sysfs_print");
		return 0;
	}
	mi_cx_stats_get_ss_vote_info(29, votes);
	for (i = 0; i < 29; i++)
		n += sprintf(buf + n, "owen DRV: %d, vote: %d \n", i, votes[i]);
	return n;
}
static CLASS_ATTR_RO(cxvt_state);

static ssize_t ddrvt_state_show(const struct class *c,
				const struct class_attribute *a, char *buf)
{
	if (!mi_ddr_stats_get_ss_vote_info) {
		pr_err("%s %s:  mi_ddr_stats_get_ss_vote_info is NULL!", TAG,
		       "ddrvt_state_show");
		return 0;
	}
	/* stock formats "owner DRV: %d avg: %d peak: %d" from the callback
	 * buffer; layout not reconstructed (no 6.12 provider exists). */
	return 0;
}
static CLASS_ATTR_RO(ddrvt_state);

static ssize_t llcvt_state_show(const struct class *c,
				const struct class_attribute *a, char *buf)
{
	struct { u64 hdr; u64 vote[64]; u64 status; } *v;
	int i, n = 0;

	if (!mi_llc_stats_get_ss_vote_info) {
		pr_err("%s %s:  mi_llc_stats_get_ss_vote_info is NULL!", TAG,
		       "llcvt_stats_sysfs_print");
		return 0;
	}
	v = kzalloc(0x228, GFP_KERNEL);
	if (!v)
		return -ENOMEM;
	if (!mi_llc_stats_get_ss_vote_info(v)) {
		for (i = 0; i < 64; i++)
			if (v->vote[i])
				n += sprintf(buf + n, "LLC CLIENT: %d, vote: 0x%llx | ",
					     i, v->vote[i]);
		n += sprintf(buf + n, "status: 0x%llx \n", v->status);
	}
	kfree(v);
	return n;
}
static CLASS_ATTR_RO(llcvt_state);

static struct class_attribute *power_debug_attrs[] = {
	&class_attr_wakeup_device_list, &class_attr_debug_suspend,
	&class_attr_power_mode, &class_attr_temp_aware,
	&class_attr_subsystem_sleep_state, &class_attr_cxvt_state,
	&class_attr_ddrvt_state, &class_attr_llcvt_state, NULL,
};

/* ------------------------------------------------ suspend debugging */

static void power_debug_suspend_trace_probe(void *unused, const char *action,
					    int val, bool start)
{
	struct pm_system_dbg_info *it;
	struct timespec64 ts;
	struct tm tm;

	if (val <= 0 || strcmp(action, "machine_suspend"))
		return;

	ktime_get_real_ts64(&ts);
	time64_to_tm(ts.tv_sec, 8 * 3600, &tm);	/* stock: China time */
	pr_warn("%s [%ld-%02d-%02d %02d:%02d:%02d.%06lu]\n", TAG,
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
		tm.tm_min, tm.tm_sec, ts.tv_nsec / 1000);

	spin_lock(&dbg_info_lock);
	list_for_each_entry(it, &system_dbg_info_list, node)
		if (it->type == 3)
			it->print(it->data);
	list_for_each_entry(it, &system_dbg_info_list, node)
		if (it->type == 4)
			it->print(it->data);
	spin_unlock(&dbg_info_lock);

	if (!start)
		return;
	/*
	 * Stock: clock_debug_print_enabled_clocks(NULL) and, with bit1,
	 * regulator_debug_print_enable_regulators(). Both are Xiaomi additions
	 * to the qcom clk/regulator debug drivers, absent from the 6.12
	 * soc-repo. On 6.12 use the soc-repo debugfs knobs instead
	 * (clk debug_suspend, regulator debug_suspend).
	 */
}

static void power_debug_freeze_todo_trace_probe(void *unused, unsigned int todo,
						unsigned int elapsed_msecs,
						bool wq_busy)
{
	struct task_struct *g, *p;

	if (!todo)
		return;
	read_lock(&tasklist_lock);
	for_each_process_thread(g, p) {
		if (p == current)
			continue;
		if (freezing(p) && !(READ_ONCE(p->__state) & TASK_FROZEN))
			sched_show_task(p);
	}
	read_unlock(&tasklist_lock);
}

static void pm_debug_resume(void)
{
	struct pm_wakeup_device *it;

	spin_lock(&wakeup_lock);
	list_for_each_entry(it, &wakeup_devices, node)
		if (it->resume)
			it->resume(it->data);
	spin_unlock(&wakeup_lock);
}

static struct syscore_ops pm_debug_ops = {
	.resume = pm_debug_resume,
};

static int pi_pm_notify(struct notifier_block *nb, unsigned long mode, void *p)
{
	switch (mode) {
	case PM_SUSPEND_PREPARE:
		last_monotime = ktime_get();
		last_stime = ktime_get_boottime();
		pr_info("%s %s:  [xiaomi_power]system suspend prepare\n", TAG, __func__);
		break;
	case PM_POST_SUSPEND: {
		ktime_t now = ktime_get_boottime();

		curr_monotime = ktime_get();
		pr_info("%s %s:  [xiaomi_power]suspend exit at boottime %lld ms, AP total resume time %lld ms, suspend for %lld ms\n",
			TAG, __func__, ktime_to_ms(now),
			ktime_to_ms(ktime_sub(curr_monotime, last_monotime)),
			ktime_to_ms(ktime_sub(now, last_stime)) -
			ktime_to_ms(ktime_sub(curr_monotime, last_monotime)));
		break;
	}
	}
	return 0;
}

static struct notifier_block pi_pm_nb = { .notifier_call = pi_pm_notify };

static void batt_current_print_func(struct work_struct *work)
{
	union power_supply_propval val = {0};
	struct power_supply *psy;
	int ret;

	psy = power_supply_get_by_name("battery");
	if (!psy) {
		pr_err("%s %s:  get battery power supply failed\n", TAG,
		       "pm_get_batt_current_now");
	} else {
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
		if (ret < 0)
			pr_err("%s %s:  get battery current failed: %d\n", TAG,
			       "pm_get_batt_current_now", ret);
		else
			pr_info("%s %s:  battery current: %dmA \n", TAG,
				"pm_get_batt_current_now", val.intval / 1000);
		power_supply_put(psy);	/* stock leaked this reference */
	}
	queue_delayed_work(system_wq, &batt_current_print,
			   round_jiffies_relative(msecs_to_jiffies(7500)));
}

static const struct file_operations power_ops = { .owner = THIS_MODULE };

static int __init mi_power_init(void)
{
	int ret, i;

	mi_power = kzalloc(sizeof(*mi_power), GFP_KERNEL);
	if (!mi_power)
		return -ENOMEM;

	mi_power->class = class_create("power_debug");
	if (IS_ERR(mi_power->class)) {
		ret = PTR_ERR(mi_power->class);
		goto err_free;
	}
	for (i = 0; power_debug_attrs[i]; i++) {
		ret = class_create_file(mi_power->class, power_debug_attrs[i]);
		if (ret) {
			pr_err("%s %s:  failed to create attribute file", TAG, "pm_debug_init");
			goto err_class;
		}
	}

	register_syscore_ops(&pm_debug_ops);
	pm_register_system_dbg_info(&subsystem_sleep_dbg_info);
	/* stock: clk_debug_suspend_atomic_enable(1) - Xiaomi clk-debug export, absent */
	register_trace_suspend_resume(power_debug_suspend_trace_probe, NULL);
	register_trace_android_vh_try_to_freeze_todo(power_debug_freeze_todo_trace_probe, NULL);
	register_pm_notifier(&pi_pm_nb);

	if (power_debug_enable & BIT(0))
		debug_pintctrl_msm_enable();
	else
		debug_pintctrl_msm_disable();

	INIT_DELAYED_WORK(&batt_current_print, batt_current_print_func);
	queue_delayed_work(system_wq, &batt_current_print,
			   round_jiffies_relative(msecs_to_jiffies(7500)));

	mi_power->major = register_chrdev(0, "mi_power_module", &power_ops);
	if (mi_power->major < 0)
		pr_err("%s %s:  mi power info chrdev creation failed (err = %d)\n",
		       TAG, "pm_debug_init", mi_power->major);

	mi_power->dev = device_create(mi_power->class, NULL,
				      MKDEV(mi_power->major, 1), NULL,
				      "mi_power_device");
	if (IS_ERR(mi_power->dev)) {
		pr_err("%s %s:  mi power info device creation failed (err = %d)\n",
		       TAG, "pm_debug_init", -ENODEV);
		mi_power->dev = NULL;
	} else if (sysfs_create_group(&mi_power->dev->kobj,
				      &qcom_sleep_stats_sysfs_group)) {
		pr_err("%s %s:  sysfs_create_group failed.\n", TAG, "pm_debug_init");
	}

	pr_info("%s %s:  register sysfs succecss.\n", TAG, "pm_debug_init");
	return 0;

err_class:
	while (--i >= 0)
		class_remove_file(mi_power->class, power_debug_attrs[i]);
	class_destroy(mi_power->class);
err_free:
	kfree(mi_power);
	mi_power = NULL;
	return ret;
}

static void __exit mi_power_exit(void)
{
	int i;

	cancel_delayed_work_sync(&batt_current_print);
	unregister_pm_notifier(&pi_pm_nb);
	unregister_trace_android_vh_try_to_freeze_todo(power_debug_freeze_todo_trace_probe, NULL);
	unregister_trace_suspend_resume(power_debug_suspend_trace_probe, NULL);
	tracepoint_synchronize_unregister();
	unregister_syscore_ops(&pm_debug_ops);
	if (mi_power->dev) {
		sysfs_remove_group(&mi_power->dev->kobj, &qcom_sleep_stats_sysfs_group);
		device_destroy(mi_power->class, MKDEV(mi_power->major, 1));
	}
	if (mi_power->major >= 0)
		unregister_chrdev(mi_power->major, "mi_power_module");
	for (i = 0; power_debug_attrs[i]; i++)
		class_remove_file(mi_power->class, power_debug_attrs[i]);
	class_destroy(mi_power->class);
	kfree(mi_power);
}

module_init(mi_power_init);
module_exit(mi_power_exit);
MODULE_LICENSE("GPL");
