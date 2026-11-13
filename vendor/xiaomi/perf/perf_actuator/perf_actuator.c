// SPDX-License-Identifier: GPL-2.0
/*
 * perf_actuator: ioctl dispatch devices for Xiaomi performance helpers.
 *
 * Rebuilt for 6.12 from static analysis (disassembly) of the stock HyperOS
 * OS4.0.5 module vendor_boot/lib/modules/perf_actuator.ko. The behaviour
 * matches that binary function by function; it has not been run on a
 * device yet.
 *
 * Two misc devices, /dev/xr_perf_actuator and /dev/xr_fbt. Other modules
 * register a handler per ioctl number; an ioctl with type 'x' and a
 * registered number is passed to the handler with the user pointer.
 */
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/rwsem.h>
#include <perf_actuator/perf_actuator.h>

#define PA_TAG "perf_actuator: "
#define FBT_TAG "xr_fbt: "

struct actuator_entry {
	unsigned int cmd;
	perf_actuator_func func;
};

static DECLARE_RWSEM(g_table_rwsem);
static struct actuator_entry g_perf_actuator_table[PERF_ACTUATOR_MAX_CMD];
static DECLARE_RWSEM(g_fbt_table_rwsem);
static struct actuator_entry g_xr_fbt_table[XR_FBT_MAX_CMD];

static int table_register(struct rw_semaphore *sem, struct actuator_entry *tbl,
			  unsigned int max, unsigned int cmd,
			  perf_actuator_func func, const char *tag)
{
	unsigned int nr = _IOC_NR(cmd);

	if (nr >= max || !func) {
		if (tbl == g_perf_actuator_table)
			pr_err("%sinvalid cmd:%d or func:%p\n", tag, cmd, func);
		return -EINVAL;
	}

	down_write(sem);
	/* The stock module logs a conflict and overwrites the old entry. */
	if (tbl[nr].func)
		pr_err("%sregister %u conflict\n", tag, cmd);
	tbl[nr].cmd = cmd;
	tbl[nr].func = func;
	up_write(sem);
	return 0;
}

static int table_unregister(struct rw_semaphore *sem, struct actuator_entry *tbl,
			    unsigned int max, unsigned int cmd, const char *tag)
{
	unsigned int nr = _IOC_NR(cmd);

	if (nr >= max)
		return -EINVAL;

	down_write(sem);
	/* Logged but still cleared, as in the stock module. */
	if (!tbl[nr].func || tbl[nr].cmd != cmd)
		pr_err("%sunregister %u conflict\n", tag, cmd);
	tbl[nr].cmd = 0;
	tbl[nr].func = NULL;
	up_write(sem);
	return 0;
}

static long table_dispatch(struct rw_semaphore *sem, struct actuator_entry *tbl,
			   unsigned int max, unsigned int cmd, unsigned long arg,
			   const char *tag)
{
	unsigned int nr = _IOC_NR(cmd);
	void __user *uarg = (void __user *)arg;
	long ret;

	if (!uarg) {
		pr_err("%sinvalid user uarg\n", tag);
		return -EINVAL;
	}
	if (_IOC_TYPE(cmd) != PERF_ACTUATOR_MAGIC) {
		pr_err("%sinvalid magic %u\n", tag, _IOC_TYPE(cmd));
		return -EINVAL;
	}
	if (nr >= max) {
		pr_err("%sinvalid cmd, %u > %d\n", tag, nr, max);
		return -ERANGE;
	}

	down_read(sem);
	if (tbl[nr].func && tbl[nr].cmd == cmd) {
		ret = tbl[nr].func(uarg);
	} else {
		if (tbl == g_perf_actuator_table)
			pr_err("%sidx %d, cmd %d not match or not registered!\n",
			       tag, nr, cmd);
		else
			pr_err("%scmd not match or not registered!\n", tag);
		ret = -ENOENT;
	}
	up_read(sem);
	return ret;
}

int register_perf_actuator(unsigned int cmd, perf_actuator_func func)
{
	return table_register(&g_table_rwsem, g_perf_actuator_table,
			      PERF_ACTUATOR_MAX_CMD, cmd, func, PA_TAG);
}
EXPORT_SYMBOL_GPL(register_perf_actuator);

int unregister_perf_actuator(unsigned int cmd)
{
	return table_unregister(&g_table_rwsem, g_perf_actuator_table,
				PERF_ACTUATOR_MAX_CMD, cmd, PA_TAG);
}
EXPORT_SYMBOL_GPL(unregister_perf_actuator);

int register_xr_fbt(unsigned int cmd, perf_actuator_func func)
{
	return table_register(&g_fbt_table_rwsem, g_xr_fbt_table,
			      XR_FBT_MAX_CMD, cmd, func, FBT_TAG);
}
EXPORT_SYMBOL_GPL(register_xr_fbt);

int unregister_xr_fbt(unsigned int cmd)
{
	return table_unregister(&g_fbt_table_rwsem, g_xr_fbt_table,
				XR_FBT_MAX_CMD, cmd, FBT_TAG);
}
EXPORT_SYMBOL_GPL(unregister_xr_fbt);

static long perf_actuator_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	pr_info(PA_TAG "perf_actuator_ioctl idx %d\n", _IOC_NR(cmd));
	return table_dispatch(&g_table_rwsem, g_perf_actuator_table,
			      PERF_ACTUATOR_MAX_CMD, cmd, arg, PA_TAG);
}

static long xr_fbt_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return table_dispatch(&g_fbt_table_rwsem, g_xr_fbt_table,
			      XR_FBT_MAX_CMD, cmd, arg, FBT_TAG);
}

static int actuator_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int actuator_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations perf_actuator_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = perf_actuator_ioctl,
	.open = actuator_open,
	.release = actuator_release,
};

static const struct file_operations xr_fbt_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = xr_fbt_ioctl,
	.open = actuator_open,
	.release = actuator_release,
};

static struct miscdevice perf_actuator_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "xr_perf_actuator",
	.fops = &perf_actuator_fops,
};

static struct miscdevice xr_fbt_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "xr_fbt",
	.fops = &xr_fbt_fops,
};

static int __init perf_actuator_init(void)
{
	int ret = misc_register(&perf_actuator_device);

	if (ret) {
		pr_info(PA_TAG "init failed!\n");
		return ret;
	}
	pr_info(PA_TAG "init success!\n");
	/* As in the stock init, an xr_fbt failure leaves the first device registered. */
	return misc_register(&xr_fbt_device);
}

static void __exit perf_actuator_exit(void)
{
	misc_deregister(&perf_actuator_device);
	misc_deregister(&xr_fbt_device);
}

module_init(perf_actuator_init);
module_exit(perf_actuator_exit);
MODULE_LICENSE("GPL");
