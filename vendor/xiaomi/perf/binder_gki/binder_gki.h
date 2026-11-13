/* SPDX-License-Identifier: GPL-2.0 */
/*
 * binder_gki <-> millet_binder interface, rebuilt for 6.12 from the stock
 * 6.6 binder_gki.ko / millet_binder.ko (piano, HyperOS OS4.0.5.0).
 *
 * The callback groups below match the kCFI type hashes of the stock
 * indirect calls: overflow/reply/wait4 share one type (0x4b047e29), trans
 * adds the target binder_proc (0x322ecad7), query is its own (0x91f15997).
 * Both modules must be built from this header.
 */
#ifndef _MILLET_BINDER_GKI_H
#define _MILLET_BINDER_GKI_H

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/freezer.h>
#include <linux/cgroup.h>
#include <linux/rcupdate.h>

struct binder_proc;
struct task_struct;

typedef void (*oem_binder_task_hook)(struct task_struct *dst,
				     struct task_struct *src, int pid,
				     bool oneway, unsigned int code);
typedef void (*oem_binder_trans_hook)(struct task_struct *dst,
				      struct task_struct *src, int pid,
				      bool oneway, unsigned int code,
				      struct binder_proc *proc);
typedef void (*oem_binder_query_hook)(int uid, struct task_struct *tsk,
				      int unused, int pid, int status);

/* 0x38 bytes, copied whole by oem_register_binder_hook(). */
struct oem_binder_hook {
	/*
	 * Async overflow report when free_async_space is below
	 * async_space_mult * (size + sizeof(struct binder_buffer)) or below
	 * async_space_min. millet_binder sets 3 and 0x20000.
	 */
	unsigned long async_space_mult;		/* 0x00 */
	unsigned long async_space_min;		/* 0x08 */
	oem_binder_task_hook overflow;		/* 0x10 alloc_new_buf_locked */
	oem_binder_task_hook reply;		/* 0x18 binder_reply */
	oem_binder_trans_hook trans;		/* 0x20 binder_trans */
	oem_binder_task_hook wait4;		/* 0x28 wait_for_work */
	oem_binder_query_hook query;		/* 0x30 query_binder_app_stat */
};

void oem_register_binder_hook(struct oem_binder_hook *set);
int reset_isfrozen(int uid, int pid, int clear, struct binder_proc *proc);
void query_binder_app_stat(int uid);

/*
 * Inline sequence the stock modules use to decide a task is (being)
 * frozen: freezing(), then the 6.6 cgroup_task_freeze() (gone in 6.12,
 * open-coded), then cgroup_task_frozen().
 */
static inline bool mi_task_freeze_state(struct task_struct *t)
{
	bool freeze = false;

	if (freezing(t))
		return true;
	if (!(t->flags & PF_KTHREAD)) {
		rcu_read_lock();
		freeze = test_bit(CGRP_FREEZE, &task_dfl_cgroup(t)->flags);
		rcu_read_unlock();
		if (freeze)
			return true;
	}
	return cgroup_task_frozen(t);
}

static inline uid_t mi_task_uid(struct task_struct *t)
{
	return __kuid_val(task_uid(t));
}

#endif
