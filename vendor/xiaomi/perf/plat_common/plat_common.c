// SPDX-License-Identifier: GPL-2.0
/*
 * plat_common: "OS-CPU Platform Hook".
 *
 * Rebuilt for 6.12 from static analysis (disassembly, kCFI type hashes and
 * the stock 6.6 kernel BTF) of the HyperOS OS4.0.5 module
 * vendor_boot/lib/modules/plat_common.ko. Not yet run on a device.
 *
 * It holds a table of callbacks that a game/scheduler module (stock:
 * mi_game.ko) fills with register_plat_func(); the exported plat_*
 * wrappers call the callback in their slot or return a default. The stock
 * sched-walt.ko calls these wrappers from WALT; the KP 5.0 WALT on 6.12
 * does not, so on 6.12 only the module callers (rtg, ...) reach them.
 *
 * It also sets a default timer slack on fork and identifies the top-app,
 * foreground, background and audio cpu cgroups.
 */
#include <linux/cgroup.h>
#include <linux/cpumask.h>
#include <linux/kernfs.h>
#include <linux/module.h>
#include <linux/plist.h>
#include <linux/rwsem.h>
#include <linux/sched.h>
#include <trace/hooks/sched.h>
#include <plat_common/plat_common.h>

#define TAG "plat_hook: "

struct plat_hook_entry {
	unsigned int cmd;
	void *func;
};

static DECLARE_RWSEM(g_table_rwsem);
static struct plat_hook_entry g_plat_hook_table[PLAT_FUNC_MAX];

/* Module parameters; defaults from the stock .data/.bss, mode 0644. */
static unsigned int plat_sched_relaxed;
module_param_named(sched_relaxed, plat_sched_relaxed, uint, 0644);
static unsigned int plat_critical_rt_prio = 96;
module_param_named(critical_rt_prio, plat_critical_rt_prio, uint, 0644);

static int plat_cgroup_ta_id = -1;
static int plat_cgroup_fg_id = -1;
static int plat_cgroup_bg_id = -1;
static int plat_cgroup_au_id = -1;

static struct cpumask empty_mask;

int register_plat_func(unsigned int cmd, void *func)
{
	unsigned int nr = cmd & 0xff;

	if (nr >= PLAT_FUNC_MAX || !func)
		return -EINVAL;

	down_write(&g_table_rwsem);
	/* Stock behaviour: log the conflict, then overwrite. */
	if (g_plat_hook_table[nr].func)
		pr_err(TAG "perf_actuator: register %u conflict\n", cmd);
	g_plat_hook_table[nr].cmd = cmd;
	WRITE_ONCE(g_plat_hook_table[nr].func, func);
	up_write(&g_table_rwsem);
	return 0;
}
EXPORT_SYMBOL(register_plat_func);

int unregister_plat_func(unsigned int cmd)
{
	unsigned int nr = cmd & 0xff;

	if (nr >= PLAT_FUNC_MAX)
		return -EINVAL;

	down_write(&g_table_rwsem);
	/* Stock behaviour: log a mismatch, then clear anyway. */
	if (!g_plat_hook_table[nr].func || g_plat_hook_table[nr].cmd != cmd)
		pr_err(TAG "perf_actuator: unregister %u conflict\n", cmd);
	g_plat_hook_table[nr].cmd = 0;
	WRITE_ONCE(g_plat_hook_table[nr].func, NULL);
	up_write(&g_table_rwsem);
	return 0;
}
EXPORT_SYMBOL(unregister_plat_func);

/* The stock wrappers read the slot without taking the rwsem. */
#define SLOT(n, type) ((type)READ_ONCE(g_plat_hook_table[n].func))

#define PLAT_RET(name, slot, ret, def, proto, args)			\
ret name proto								\
{									\
	ret (*fn) proto = SLOT(slot, ret (*) proto);			\
	return fn ? fn args : (def);					\
}									\
EXPORT_SYMBOL(name)

#define PLAT_VOID(name, slot, proto, args)				\
void name proto								\
{									\
	void (*fn) proto = SLOT(slot, void (*) proto);			\
	if (fn)								\
		fn args;						\
}									\
EXPORT_SYMBOL(name)

PLAT_RET(plat_sched_set_group_id, PLAT_SCHED_SET_GROUP_ID, int, 0,
	 (struct task_struct *p, unsigned int id), (p, id));
PLAT_RET(plat_sched_get_group_id, PLAT_SCHED_GET_GROUP_ID, unsigned int, 0,
	 (struct task_struct *p), (p));
PLAT_RET(plat_task_util, PLAT_TASK_UTIL, unsigned long, 0,
	 (struct task_struct *p), (p));
PLAT_RET(plat_cpu_halted, PLAT_CPU_HALTED, bool, false, (int cpu), (cpu));
PLAT_RET(plat_cpu_halt_mask, PLAT_CPU_HALT_MASK, struct cpumask *,
	 &empty_mask, (void), ());
PLAT_RET(plat_capacity_orig_of, PLAT_CAPACITY_ORIG_OF, unsigned long, 0,
	 (int cpu), (cpu));
PLAT_RET(plat_is_max_possible_cluster_cpu, PLAT_IS_MAX_POSSIBLE_CLUSTER_CPU,
	 bool, false, (int cpu), (cpu));
PLAT_RET(plat_find_idle_cpu_in_freq_domain, PLAT_FIND_IDLE_CPU_IN_FREQ_DOMAIN,
	 int, -1, (int cpu), (cpu));

void plat_get_group_prefer_cpus(struct task_struct *p, struct cpumask *mask)
{
	void (*fn)(struct task_struct *, struct cpumask *) =
		SLOT(PLAT_GET_GROUP_PREFER_CPUS,
		     void (*)(struct task_struct *, struct cpumask *));

	if (fn)
		fn(p, mask);
	else
		/* Stock default: all of the first nr_cpu_ids bits. */
		cpumask_setall(mask);
}
EXPORT_SYMBOL(plat_get_group_prefer_cpus);

PLAT_RET(plat_get_group_prefer_util, PLAT_GET_GROUP_PREFER_UTIL,
	 unsigned long, 0, (struct task_struct *p), (p));
PLAT_VOID(plat_core_ctl_update, PLAT_CORE_CTL_UPDATE, (void), ());
PLAT_RET(plat_lookup_related_thread_group, PLAT_LOOKUP_RELATED_THREAD_GROUP,
	 struct walt_related_thread_group *, NULL, (unsigned int id), (id));
PLAT_VOID(plat_mi_game_find_energy_hook, PLAT_MI_GAME_FIND_ENERGY_HOOK,
	  (struct task_struct *p, bool *out), (p, out));
PLAT_VOID(plat_update_pkg_load, PLAT_UPDATE_PKG_LOAD,
	  (struct task_struct *p, int a, int b, u64 c, u64 d), (p, a, b, c, d));
PLAT_VOID(plat_mi_game_walt_get_indicies, PLAT_MI_GAME_WALT_GET_INDICIES,
	  (struct task_struct *p, int *order_index, int *end_index,
	   int nr_clusters, bool *a, bool *b),
	  (p, order_index, end_index, nr_clusters, a, b));
PLAT_VOID(plat_game_get_ravg_window, PLAT_GAME_GET_RAVG_WINDOW,
	  (unsigned int window), (window));
PLAT_VOID(plat_mi_set_next_freq, PLAT_MI_SET_NEXT_FREQ,
	  (void *a0, void *a1, unsigned int a2, u64 a3, u64 *a4, void *a5,
	   void *a6, unsigned int a7, unsigned int a8),
	  (a0, a1, a2, a3, a4, a5, a6, a7, a8));
PLAT_VOID(plat_mi_game_task_fork, PLAT_MI_GAME_TASK_FORK,
	  (struct task_struct *p), (p));
PLAT_VOID(plat_mi_game_scheduler_tick, PLAT_MI_GAME_SCHEDULER_TICK,
	  (struct rq *rq), (rq));
PLAT_RET(plat_game_choose_cpu_fastpath, PLAT_GAME_CHOOSE_CPU_FASTPATH, int, 0,
	 (struct task_struct *p, struct cpumask *mask, bool *out),
	 (p, mask, out));
PLAT_VOID(plat_free_mi_task_struct, PLAT_FREE_MI_TASK_STRUCT,
	  (struct task_struct *p), (p));
PLAT_RET(plat_game_choose_exclusive_cpus, PLAT_GAME_CHOOSE_EXCLUSIVE_CPUS,
	 int, -1, (struct task_struct *p), (p));
PLAT_RET(plat_game_choose_cpu, PLAT_GAME_CHOOSE_CPU, int, -1,
	 (struct task_struct *p), (p));
PLAT_RET(plat_game_skip_cpus, PLAT_GAME_SKIP_CPUS, bool, false,
	 (struct cpumask *mask), (mask));
PLAT_RET(plat_game_skip_cpus_rt, PLAT_GAME_SKIP_CPUS_RT, bool, false,
	 (struct task_struct *p, struct cpumask *mask), (p, mask));
PLAT_RET(plat_game_should_skip_cpu_rt, PLAT_GAME_SHOULD_SKIP_CPU_RT, bool,
	 false, (struct task_struct *p, int cpu), (p, cpu));
PLAT_RET(plat_game_should_skip_migrate, PLAT_GAME_SHOULD_SKIP_MIGRATE, bool,
	 false, (struct task_struct *p, int cpu), (p, cpu));
PLAT_RET(plat_game_check_rt, PLAT_GAME_CHECK_RT, bool, false,
	 (struct task_struct *p), (p));
PLAT_RET(plat_game_vip_check, PLAT_GAME_VIP_CHECK, bool, false,
	 (struct task_struct *p), (p));
PLAT_RET(plat_migt_enable, PLAT_MIGT_ENABLE, int, 0, (void), ());
PLAT_VOID(plat_update_frame_load, PLAT_UPDATE_FRAME_LOAD,
	  (struct task_struct *p, struct rq *rq, unsigned int event,
	   u64 wallclock, u64 delta, int flag),
	  (p, rq, event, wallclock, delta, flag));
PLAT_RET(plat_flt_enable, PLAT_FLT_ENABLE, int, 0, (void), ());
PLAT_VOID(plat_get_wakeup_info, PLAT_GET_WAKEUP_INFO,
	  (struct task_struct *p), (p));
PLAT_VOID(plat_mi_force_update_freq, PLAT_MI_FORCE_UPDATE_FREQ,
	  (unsigned int arg), (arg));
PLAT_RET(plat_enable_gpsync, PLAT_ENABLE_GPSYNC, bool, false, (void), ());
PLAT_VOID(plat_mi_game_schedule, PLAT_MI_GAME_SCHEDULE,
	  (struct task_struct *prev, struct task_struct *next, struct rq *rq),
	  (prev, next, rq));

/*
 * Called by the cgroup side for each new cpu cgroup css. Only first-level
 * groups are looked at (css->cgroup->level == 1); the id is css->id.
 */
void plat_init_sched_cgroup_id(struct cgroup_subsys_state *css)
{
	const char *name;

	if (css->cgroup->level != 1)
		return;

	rcu_read_lock();
	name = css->cgroup->kn->name;
	if (!strcmp(name, "top-app"))
		plat_cgroup_ta_id = css->id;
	else if (!strcmp(name, "foreground"))
		plat_cgroup_fg_id = css->id;
	else if (!strcmp(name, "background"))
		plat_cgroup_bg_id = css->id;
	else if (!strcmp(name, "audio"))
		plat_cgroup_au_id = css->id;
	rcu_read_unlock();

	pr_info(TAG "Identified sched cgroup: level=%d, name=%s, id=%d(ta=%d fg=%d bg=%d au=%d)\n",
		css->cgroup->level, css->cgroup->kn->name, css->id,
		plat_cgroup_ta_id, plat_cgroup_fg_id, plat_cgroup_bg_id,
		plat_cgroup_au_id);
}
EXPORT_SYMBOL(plat_init_sched_cgroup_id);

/*
 * True for a queued (pushable) RT task at or below critical_rt_prio in
 * the audio cpu cgroup, when sched_relaxed is set.
 */
bool plat_is_relaxed_rt(struct task_struct *p)
{
	struct cgroup_subsys_state *css;
	int id = -1;

	if (!plat_sched_relaxed)
		return false;
	if (p->prio >= MAX_RT_PRIO || (unsigned int)p->prio > plat_critical_rt_prio)
		return false;
	if (plist_node_empty(&p->pushable_tasks))
		return false;

	rcu_read_lock();
	css = task_css(p, cpu_cgrp_id);
	if (css)
		id = css->id;
	rcu_read_unlock();

	return id != -1 && id == plat_cgroup_au_id;
}
EXPORT_SYMBOL(plat_is_relaxed_rt);

/*
 * android_rvh_finish_prio_fork: give a task a 50 us default slack if it
 * has none, and copy it to timer_slack_ns for non-RT/DL tasks that have
 * no slack.
 */
static void plat_fix_timer_slack_fork(void *unused, struct task_struct *p)
{
	if (!p->default_timer_slack_ns)
		p->default_timer_slack_ns = 50000;
	if (p->timer_slack_ns)
		return;
	if (p->policy == SCHED_FIFO || p->policy == SCHED_RR ||
	    p->policy == SCHED_DEADLINE)
		return;
	p->timer_slack_ns = p->default_timer_slack_ns;
}

static int __init plat_common_init(void)
{
	pr_info(TAG "plat_task_struct size: %zu/%d bytes\n", (size_t)120, 1024);
	return register_trace_android_rvh_finish_prio_fork(plat_fix_timer_slack_fork,
							   NULL);
}

static void __exit plat_common_exit(void)
{
	/* Restricted hooks cannot be unregistered; the stock exit is empty. */
}

module_init(plat_common_init);
module_exit(plat_common_exit);
MODULE_DESCRIPTION("OS-CPU Platform Hook");
MODULE_LICENSE("GPL");
