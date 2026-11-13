// SPDX-License-Identifier: GPL-2.0
/*
 * rtg: Xiaomi related-thread-group control.
 *
 * Rebuilt for 6.12 from static analysis (disassembly and the stock 6.6
 * kernel BTF) of the HyperOS OS4.0.5 module vendor_boot/lib/modules/rtg.ko.
 * Not yet run on a device.
 *
 * It registers five perf_actuator ioctls that move tasks into WALT
 * related thread groups (through plat_sched_set_group_id) and set a
 * group's preferred CPUs and util, and exports the os_cpu_* helpers that
 * Xiaomi's 6.6 sched-walt.ko called during task placement. The KP 5.0
 * WALT on 6.12 does not call those helpers, so the preferred CPUs and
 * util are stored but not acted on until WALT is patched to use them.
 */
#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/sched/walt.h>
#include <linux/uaccess.h>
#include <perf_actuator/perf_actuator.h>
#include <plat_common/plat_common.h>

#define TAG "rtg: "

/* Group ids accepted by the ioctls: not 1 (DEFAULT_CGROUP_COLOC_ID), at most 0x13. */
#define RTG_MAX_ID		0x13
#define RTG_INIT_GRP		13
#define RTG_MAX_UTIL		1024
#define RTG_ARRAY_MAX		15
#define RTG_CLEAR_MAX_LOOP	200

struct rtg_task_arg {
	int pid;
	int grp_id;
};

struct rtg_task_array_arg {
	int grp_id;
	int count;
	int pids[RTG_ARRAY_MAX];
};

struct rtg_value_arg {
	int grp_id;
	unsigned int value;
};

#define CMD_SET_TASK_RTG	_IOW(PERF_ACTUATOR_MAGIC, 0x02, struct rtg_task_arg)
#define CMD_SET_RTG_CPUS	_IOW(PERF_ACTUATOR_MAGIC, 0x03, struct rtg_value_arg)
#define CMD_SET_RTG_UTIL	_IOW(PERF_ACTUATOR_MAGIC, 0x04, struct rtg_value_arg)
#define CMD_CLEAR_RTG		_IOW(PERF_ACTUATOR_MAGIC, 0x07, int)
#define CMD_SET_TASK_ARRAY_RTG	_IOW(PERF_ACTUATOR_MAGIC, 0x25, struct rtg_task_array_arg)

static_assert(CMD_SET_TASK_RTG == 0x40087802);
static_assert(CMD_SET_RTG_CPUS == 0x40087803);
static_assert(CMD_SET_RTG_UTIL == 0x40087804);
static_assert(CMD_CLEAR_RTG == 0x40047807);
static_assert(CMD_SET_TASK_ARRAY_RTG == 0x40447825);

static inline struct walt_related_thread_group *task_rtg(struct task_struct *p)
{
	struct walt_task_struct *wts =
		(struct walt_task_struct *)android_task_vendor_data(p);

	/* The stock helpers read wts->grp without rcu_dereference. */
	return rcu_dereference_raw(wts->grp);
}

static inline bool rtg_id_valid(int id)
{
	return id != 1 && (unsigned int)id <= RTG_MAX_ID;
}

struct walt_related_thread_group *get_related_thread_group(unsigned int id)
{
	if (!rtg_id_valid(id))
		return NULL;
	return plat_lookup_related_thread_group(id);
}
EXPORT_SYMBOL(get_related_thread_group);

void os_cpu_get_group_prefer_cpus(struct task_struct *p, struct cpumask *mask)
{
	struct walt_related_thread_group *grp = task_rtg(p);

	if (grp && grp->mi_prefer_cpus)
		cpumask_bits(mask)[0] = grp->mi_prefer_cpus;
	else
		cpumask_setall(mask);
}
EXPORT_SYMBOL(os_cpu_get_group_prefer_cpus);

unsigned long os_cpu_get_group_prefer_util(struct task_struct *p)
{
	struct walt_related_thread_group *grp = task_rtg(p);

	return grp ? grp->mi_prefer_util : 0;
}
EXPORT_SYMBOL(os_cpu_get_group_prefer_util);

bool os_cpu_is_task_prefer_cpu(struct task_struct *p, int cpu)
{
	struct walt_related_thread_group *grp = task_rtg(p);

	if (!grp || !grp->mi_prefer_cpus)
		return true;
	return test_bit(cpu, (unsigned long *)&grp->mi_prefer_cpus);
}
EXPORT_SYMBOL(os_cpu_is_task_prefer_cpu);

bool os_cpu_can_migrate_task(struct task_struct *p, int src_cpu, int dst_cpu)
{
	struct walt_related_thread_group *grp = task_rtg(p);

	if (!grp || !grp->mi_prefer_cpus)
		return true;
	/* Only a task on a preferred CPU is held to the preferred set. */
	if (!test_bit(src_cpu, (unsigned long *)&grp->mi_prefer_cpus))
		return true;
	return test_bit(dst_cpu, (unsigned long *)&grp->mi_prefer_cpus);
}
EXPORT_SYMBOL(os_cpu_can_migrate_task);

void os_cpu_init_rtg_grp_prefer_cpus(unsigned int cpus)
{
	struct walt_related_thread_group *grp =
		plat_lookup_related_thread_group(RTG_INIT_GRP);

	if (!grp) {
		pr_warn(TAG "init grp %u prefer_cpus 0x%x failed: grp not found\n",
			RTG_INIT_GRP, cpus & 0xff);
		return;
	}
	grp->mi_prefer_cpus = cpus & 0xff;
	pr_info(TAG "init grp %u prefer_cpus 0x%x\n", RTG_INIT_GRP, cpus & 0xff);
}
EXPORT_SYMBOL(os_cpu_init_rtg_grp_prefer_cpus);

static int set_task_rtg(int pid, int grp_id)
{
	struct task_struct *p;
	int ret;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (!p) {
		pr_info(TAG "task == NULL %d", pid);
		rcu_read_unlock();
		return -ENOENT;
	}
	ret = plat_sched_set_group_id(p, grp_id);
	rcu_read_unlock();
	return ret;
}

static int perf_actuator_set_task_rtg(void __user *uarg)
{
	struct rtg_task_arg arg = {};
	int ret;

	if (!uarg)
		return -EINVAL;
	if (copy_from_user(&arg, uarg, sizeof(arg)))
		return -EFAULT;

	ret = set_task_rtg(arg.pid, arg.grp_id);
	if (ret)
		pr_info(TAG "set task %d to rtg %d failed", arg.pid, arg.grp_id);
	return ret;
}

static int perf_actuator_set_task_array_rtg(void __user *uarg)
{
	struct rtg_task_array_arg arg = {};
	struct task_struct *p;
	int i, ret = 0;

	if (!uarg)
		return -EINVAL;
	if (copy_from_user(&arg, uarg, sizeof(arg)) ||
	    (unsigned int)arg.count > RTG_ARRAY_MAX)
		return -EFAULT;

	for (i = 0; i < arg.count; i++) {
		int pid = arg.pids[i];

		if (!pid)
			continue;
		rcu_read_lock();
		p = find_task_by_vpid(pid);
		if (!p) {
			rcu_read_unlock();
			pr_err(TAG " task %d is not exist\n", pid);
			continue;
		}
		ret = plat_sched_set_group_id(p, arg.grp_id);
		rcu_read_unlock();
		if (ret)
			pr_err(TAG "set task %d to rtg %d failed", pid, arg.grp_id);
	}
	/* As in the stock module, the result of the last move is returned. */
	return ret;
}

static int perf_actuator_set_rtg_cpus(void __user *uarg)
{
	struct rtg_value_arg arg = {};
	struct walt_related_thread_group *grp = NULL;

	if (!uarg)
		return -EINVAL;
	if (copy_from_user(&arg, uarg, sizeof(arg)))
		return -EFAULT;

	if (rtg_id_valid(arg.grp_id))
		grp = plat_lookup_related_thread_group(arg.grp_id);
	if (!grp) {
		pr_err(TAG "set preferred cpus for group %d fail\n", arg.grp_id);
		return -ENODEV;
	}
	grp->mi_prefer_cpus = arg.value;
	plat_core_ctl_update();
	return 0;
}

static int perf_actuator_set_rtg_util(void __user *uarg)
{
	struct rtg_value_arg arg = {};
	struct walt_related_thread_group *grp = NULL;

	if (!uarg)
		return -EINVAL;
	if (copy_from_user(&arg, uarg, sizeof(arg)))
		return -EFAULT;

	if (rtg_id_valid(arg.grp_id))
		grp = plat_lookup_related_thread_group(arg.grp_id);
	if (!grp) {
		pr_err(TAG "set prefer util for group %d fail\n", arg.grp_id);
		return -ENODEV;
	}
	grp->mi_prefer_util = min_t(unsigned int, arg.value, RTG_MAX_UTIL);
	return 0;
}

/*
 * Move every task out of the group, one at a time under the group lock,
 * giving up after 200 tasks (-EAGAIN if the group is still not empty).
 */
static int perf_actuator_clear_rtg(void __user *uarg)
{
	struct walt_related_thread_group *grp;
	struct walt_task_struct *wts;
	struct task_struct *p;
	unsigned long flags;
	bool left = false;
	int grp_id = 0, i;

	if (!uarg)
		return -EINVAL;
	if (copy_from_user(&grp_id, uarg, sizeof(grp_id)))
		return -EFAULT;
	if (!rtg_id_valid(grp_id))
		return -ENOENT;

	grp = plat_lookup_related_thread_group(grp_id);
	if (!grp)
		return -ENOENT;

	local_irq_save(flags);
	preempt_disable();
	for (i = 0; i < RTG_CLEAR_MAX_LOOP; i++) {
		raw_spin_lock(&grp->lock);
		wts = list_first_entry_or_null(&grp->tasks,
					       struct walt_task_struct, grp_list);
		left = wts != NULL;
		if (!wts) {
			raw_spin_unlock(&grp->lock);
			break;
		}
		p = wts_to_ts(wts);
		get_task_struct(p);
		raw_spin_unlock(&grp->lock);

		plat_sched_set_group_id(p, 0);
		put_task_struct(p);
	}
	local_irq_restore(flags);
	preempt_enable();

	return left ? -EAGAIN : 0;
}

static const struct {
	unsigned int cmd;
	perf_actuator_func func;
} rtg_cmds[] = {
	{ CMD_SET_TASK_RTG, perf_actuator_set_task_rtg },
	{ CMD_SET_TASK_ARRAY_RTG, perf_actuator_set_task_array_rtg },
	{ CMD_SET_RTG_CPUS, perf_actuator_set_rtg_cpus },
	{ CMD_SET_RTG_UTIL, perf_actuator_set_rtg_util },
	{ CMD_CLEAR_RTG, perf_actuator_clear_rtg },
};

static int __init rtg_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rtg_cmds); i++)
		register_perf_actuator(rtg_cmds[i].cmd, rtg_cmds[i].func);
	return 0;
}

/* No exit: the stock module cannot be unloaded either. */
module_init(rtg_init);
MODULE_LICENSE("GPL v2");
