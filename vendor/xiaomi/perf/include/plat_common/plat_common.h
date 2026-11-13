/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Interface of plat_common ("OS-CPU Platform Hook"), rebuilt for 6.12 from
 * the stock HyperOS OS4.0.5 module vendor_boot plat_common.ko.
 *
 * Slot numbers: the g_plat_hook_table offset each exported wrapper loads,
 * divided by the 16-byte entry size. Prototypes: recovered from the kCFI
 * type hash the wrapper checks before its indirect call (xxhash64 of the
 * Itanium-mangled function type), except the two marked UNRESOLVED, whose
 * argument count and widths come from the sched-walt.ko call sites only.
 */
#ifndef _PLAT_COMMON_H
#define _PLAT_COMMON_H

#include <linux/types.h>

struct task_struct;
struct cpumask;
struct rq;
struct walt_related_thread_group;
struct cgroup_subsys_state;

enum plat_func_slot {
	PLAT_SCHED_SET_GROUP_ID = 1,
	PLAT_SCHED_GET_GROUP_ID = 2,
	PLAT_TASK_UTIL = 3,
	PLAT_CPU_HALTED = 4,
	PLAT_CPU_HALT_MASK = 5,
	PLAT_CAPACITY_ORIG_OF = 6,
	PLAT_IS_MAX_POSSIBLE_CLUSTER_CPU = 7,
	PLAT_FIND_IDLE_CPU_IN_FREQ_DOMAIN = 8,
	PLAT_GET_GROUP_PREFER_CPUS = 9,
	PLAT_GET_GROUP_PREFER_UTIL = 10,
	PLAT_CORE_CTL_UPDATE = 11,
	PLAT_LOOKUP_RELATED_THREAD_GROUP = 12,
	PLAT_MI_GAME_FIND_ENERGY_HOOK = 13,
	PLAT_UPDATE_PKG_LOAD = 14,
	PLAT_MI_GAME_WALT_GET_INDICIES = 15,
	PLAT_GAME_GET_RAVG_WINDOW = 16,
	PLAT_MI_SET_NEXT_FREQ = 17,
	PLAT_MI_GAME_TASK_FORK = 18,
	PLAT_MI_GAME_SCHEDULER_TICK = 19,
	PLAT_GAME_CHOOSE_CPU_FASTPATH = 20,
	PLAT_FREE_MI_TASK_STRUCT = 21,
	PLAT_GAME_CHOOSE_EXCLUSIVE_CPUS = 22,
	PLAT_GAME_CHOOSE_CPU = 23,
	PLAT_GAME_SKIP_CPUS = 24,
	PLAT_GAME_SKIP_CPUS_RT = 25,
	PLAT_GAME_SHOULD_SKIP_CPU_RT = 26,
	PLAT_GAME_SHOULD_SKIP_MIGRATE = 27,
	PLAT_GAME_CHECK_RT = 28,
	PLAT_GAME_VIP_CHECK = 29,
	PLAT_MIGT_ENABLE = 30,
	PLAT_UPDATE_FRAME_LOAD = 35,
	PLAT_FLT_ENABLE = 36,
	PLAT_GET_WAKEUP_INFO = 37,
	PLAT_MI_FORCE_UPDATE_FREQ = 38,
	PLAT_ENABLE_GPSYNC = 39,
	PLAT_MI_GAME_SCHEDULE = 40,
	PLAT_FUNC_MAX = 41,	/* register_plat_func rejects nr > 0x28 */
};

int register_plat_func(unsigned int cmd, void *func);
int unregister_plat_func(unsigned int cmd);

int plat_sched_set_group_id(struct task_struct *p, unsigned int id);
unsigned int plat_sched_get_group_id(struct task_struct *p);
unsigned long plat_task_util(struct task_struct *p);
bool plat_cpu_halted(int cpu);
struct cpumask *plat_cpu_halt_mask(void);
unsigned long plat_capacity_orig_of(int cpu);
bool plat_is_max_possible_cluster_cpu(int cpu);
int plat_find_idle_cpu_in_freq_domain(int cpu);
void plat_get_group_prefer_cpus(struct task_struct *p, struct cpumask *mask);
unsigned long plat_get_group_prefer_util(struct task_struct *p);
void plat_core_ctl_update(void);
struct walt_related_thread_group *plat_lookup_related_thread_group(unsigned int id);
void plat_mi_game_find_energy_hook(struct task_struct *p, bool *out);
void plat_update_pkg_load(struct task_struct *p, int a, int b, u64 c, u64 d);
void plat_mi_game_walt_get_indicies(struct task_struct *p, int *order_index,
				    int *end_index, int nr_clusters,
				    bool *a, bool *b);
void plat_game_get_ravg_window(unsigned int window);
/* UNRESOLVED prototype: 7 register and 2 stack arguments at the call site. */
void plat_mi_set_next_freq(void *a0, void *a1, unsigned int a2, u64 a3,
			   u64 *a4, void *a5, void *a6, unsigned int a7,
			   unsigned int a8);
void plat_mi_game_task_fork(struct task_struct *p);
void plat_mi_game_scheduler_tick(struct rq *rq);
/* UNRESOLVED prototype: returns a CPU (negative for none) at the call site. */
int plat_game_choose_cpu_fastpath(struct task_struct *p, struct cpumask *mask,
				  bool *out);
void plat_free_mi_task_struct(struct task_struct *p);
int plat_game_choose_exclusive_cpus(struct task_struct *p);
int plat_game_choose_cpu(struct task_struct *p);
bool plat_game_skip_cpus(struct cpumask *mask);
bool plat_game_skip_cpus_rt(struct task_struct *p, struct cpumask *mask);
bool plat_game_should_skip_cpu_rt(struct task_struct *p, int cpu);
bool plat_game_should_skip_migrate(struct task_struct *p, int cpu);
bool plat_game_check_rt(struct task_struct *p);
bool plat_game_vip_check(struct task_struct *p);
int plat_migt_enable(void);
void plat_update_frame_load(struct task_struct *p, struct rq *rq,
			    unsigned int event, u64 wallclock, u64 delta,
			    int flag);
int plat_flt_enable(void);
void plat_get_wakeup_info(struct task_struct *p);
void plat_mi_force_update_freq(unsigned int arg);
bool plat_enable_gpsync(void);
void plat_mi_game_schedule(struct task_struct *prev, struct task_struct *next,
			   struct rq *rq);

void plat_init_sched_cgroup_id(struct cgroup_subsys_state *css);
bool plat_is_relaxed_rt(struct task_struct *p);

#endif
