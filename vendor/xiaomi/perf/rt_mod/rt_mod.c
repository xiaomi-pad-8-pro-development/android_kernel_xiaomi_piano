// SPDX-License-Identifier: GPL-2.0
/*
 * rt_mod: per-CPU RT/DL task load for userspace.
 *
 * Rebuilt for 6.12 from the disassembly of the stock HyperOS OS4.0.5
 * module vendor_boot/lib/modules/rt_mod.ko (author string "Samuel
 * <huangshuai1@xiaomi.com>"; every instruction accounted for, task
 * offsets named from the stock 6.6 BTF). Not yet run on a device.
 *
 * /dev/rt_load ioctl 0x4c01 copies out one {min prio, count} pair per
 * possible CPU for the SCHED_FIFO, SCHED_RR and SCHED_DEADLINE tasks
 * whose task_cpu() is that CPU. The stock module also plants a kprobe on
 * __sched_setscheduler whose handlers do nothing; it is kept so that the
 * module behaves the same (it fails to load if the probe cannot be set).
 */
#include <linux/cpumask.h>
#include <linux/fs.h>
#include <linux/kprobes.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define RTLOAD_GET	0x4c01

struct rt_load_stat {
	int min_prio;	/* starts at 0, so only a negative (DL) prio lowers it */
	int count;
};

/* The stock audit table; nothing in the stock module fills it. */
struct whitelist_entry {
	char comm[TASK_COMM_LEN];
	int limit;
	int count;
};
static struct whitelist_entry whitelist[1024];

int sandbox_whitelist_audit(struct task_struct *p)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(whitelist); i++) {
		if (strncmp(p->comm, whitelist[i].comm, TASK_COMM_LEN))
			continue;
		if (++whitelist[i].count > whitelist[i].limit)
			return -1;
	}
	return 0;
}

static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	return 0;
}

static void handler_post(struct kprobe *p, struct pt_regs *regs,
			 unsigned long flags)
{
}

static struct kprobe kp = {
	.symbol_name = "__sched_setscheduler",
	.pre_handler = handler_pre,
	.post_handler = handler_post,
};

static long rtload_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct rt_load_stat *stat;
	struct task_struct *p;
	size_t size;
	long ret;

	if (!file) {
		pr_err("invalid file!\n");
		return -EFAULT;
	}
	if (cmd != RTLOAD_GET)
		return 0;

	/* The stock module sizes it from the first word of the possible mask. */
	size = hweight64(cpumask_bits(cpu_possible_mask)[0]) * sizeof(*stat);
	stat = kzalloc(size, GFP_ATOMIC);
	if (!stat) {
		pr_err("rtmod load stat malloc failure.\n");
		return -EFAULT;
	}

	rcu_read_lock();
	for_each_process(p) {
		unsigned int cpu;

		if (p->policy != SCHED_FIFO && p->policy != SCHED_RR &&
		    p->policy != SCHED_DEADLINE)
			continue;
		cpu = task_cpu(p);
		if ((cpu + 1) * sizeof(*stat) > size)
			continue;
		stat[cpu].count++;
		stat[cpu].min_prio = min(p->prio, stat[cpu].min_prio);
	}
	rcu_read_unlock();

	ret = copy_to_user((void __user *)arg, stat, size);
	if (ret)
		pr_err("%s : copy_to_user failed!\n", "rtload_get");
	kfree(stat);
	return ret ? -EFAULT : 0;
}

static int rtload_open(struct inode *inode, struct file *file)
{
	__module_get(THIS_MODULE);
	return 0;
}

static int rtload_release(struct inode *inode, struct file *file)
{
	module_put(THIS_MODULE);
	return 0;
}

static const struct file_operations rtload_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = rtload_ioctl,
	.open = rtload_open,
	.release = rtload_release,
};

static struct miscdevice rtload_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "rt_load",
	.fops = &rtload_fops,
};

static int __init rt_mod_init(void)
{
	int ret = register_kprobe(&kp);

	if (ret < 0) {
		pr_info("register_kprobe failed, returned %d\n", ret);
		return ret;
	}
	pr_info("rt_mod planted kprobe at %p = %s\n", kp.addr, kp.symbol_name);

	if (misc_register(&rtload_miscdev))
		pr_err("%s : register error.\n", "rtload_miscdev_init");
	else
		pr_info("%s : register complete.\n", "rtload_miscdev_init");
	return 0;
}

static void __exit rt_mod_exit(void)
{
	unregister_kprobe(&kp);
	pr_info("kprobe at %p unregistered\n", kp.addr);
	misc_deregister(&rtload_miscdev);
	pr_info("%s : exit complete.\n", "rtload_miscdev_exit");
}

module_init(rt_mod_init);
module_exit(rt_mod_exit);
MODULE_AUTHOR("Samuel <huangshuai1@xiaomi.com>");
MODULE_LICENSE("GPL");
