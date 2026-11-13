// SPDX-License-Identifier: GPL-2.0
/*
 * binder_gki: binder vendor hooks for Xiaomi millet (app freezer).
 *
 * Source port for Linux 6.12 of the stock 6.6 binder_gki.ko (piano,
 * HyperOS OS4.0.5.0) reconstructed from its disassembly. Struct access
 * uses the 6.12 drivers/android/binder_internal.h field names.
 *
 * - Forwards binder events (async buffer pressure, reply, transaction,
 *   wait for work) to the callbacks millet_binder registers with
 *   oem_register_binder_hook().
 * - Keeps binder_procs and binder_procs_lock from the preset hook so
 *   millet can clear is_frozen (reset_isfrozen) and ask whether an app
 *   still has binder work pending (query_binder_app_stat).
 */
#define pr_fmt(fmt) "millet-binder_gki: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/preempt.h>
#include <linux/hardirq.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/uaccess.h>
#include <linux/minmax.h>
#include <uapi/linux/android/binder.h>
#include <trace/hooks/binder.h>
#include <millet/millet.h>

#include "binder_internal.h"
#include "binder_gki.h"

#define MI_APP_UID_MIN		10000
#define MI_TR_PEEK_LEN		140	/* bytes of the parcel copied for debug */
#define MI_TR_DESC_OFF		16	/* String16 interface token in the parcel */

static struct oem_binder_hook oem_binder_hook_set;

static struct mutex *get_binder_lock;		/* &binder_procs_lock */
static struct hlist_head *get_binder_hhead;	/* &binder_procs */
static bool bgot;

/* Stock helper, no callers in the module. */
static struct task_struct *binder_buff_owner(size_t *free_async_space)
{
	if (!free_async_space)
		return NULL;
	return container_of(free_async_space, struct binder_proc,
			    alloc.free_async_space)->tsk;
}

/*
 * Clear @proc->is_frozen so the transaction from @uid goes through.
 * Returns 0 nothing done (or not allowed in this context), 1 cleared,
 * 2 @proc was not frozen. Takes binder_procs_lock, so it must be called
 * from a context that can sleep; it refuses (and logs) otherwise.
 */
int reset_isfrozen(int uid, int pid, int clear, struct binder_proc *proc)
{
	int ret = 0;

	if (!get_binder_lock || !get_binder_hhead)
		return 0;

	if (in_interrupt()) {
		pr_err("millet reset_isfrozen in_interrupt pid=%d\n", pid);
		return 0;
	}
	if (hardirq_count() || softirq_count()) {
		pr_err("millet reset_isfrozen irq=%lu soft=%lu pid=%d\n",
		       (unsigned long)hardirq_count(),
		       (unsigned long)softirq_count(), pid);
		return 0;
	}
	if (preempt_count() >= 1) {
		pr_err("millet reset_isfrozen preempt_count=%d pid=%d\n",
		       preempt_count(), pid);
		return 0;
	}

	mutex_lock(get_binder_lock);
	spin_lock(&proc->inner_lock);
	if (proc->tsk) {
		if (!proc->is_frozen) {
			ret = 2;
		} else if (clear) {
			proc->is_frozen = false;
			pr_err("millet reset_isfrozen sruid=%d pid=%d:\n",
			       uid, proc->pid);
			ret = 1;
		}
	}
	spin_unlock(&proc->inner_lock);
	mutex_unlock(get_binder_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(reset_isfrozen);

/* Caller holds @proc->inner_lock. Returns the stock status code. */
static int mi_proc_binder_stat(struct binder_proc *proc, int *sync)
{
	struct binder_transaction *t;
	struct binder_thread *thread;
	struct binder_work *w;
	struct rb_node *n;

	if (proc->tsk && !list_empty(&proc->todo)) {
		if (!millet_binder_switch)
			return 3;
		list_for_each_entry(w, &proc->todo, entry) {
			if (w->type != BINDER_WORK_TRANSACTION)
				continue;
			t = container_of(w, struct binder_transaction, work);
			spin_lock(&t->lock);
			if (!t->to_thread)
				*sync = t->need_reply;
			spin_unlock(&t->lock);
			if (*sync == 1)
				return 3;
		}
	}

	for (n = rb_first(&proc->threads); n; n = rb_next(n)) {
		thread = rb_entry(n, struct binder_thread, rb_node);
		if (!thread->task)
			continue;

		if (!list_empty(&thread->todo)) {
			if (!millet_binder_switch)
				return 2;
			list_for_each_entry(w, &thread->todo, entry) {
				if (w->type != BINDER_WORK_TRANSACTION)
					continue;
				t = container_of(w, struct binder_transaction,
						 work);
				spin_lock(&t->lock);
				if (t->to_thread == thread)
					*sync = t->need_reply;
				spin_unlock(&t->lock);
				if (*sync == 1)
					return 2;
			}
		}

		t = thread->transaction_stack;
		if (!t)
			continue;
		spin_lock(&t->lock);
		if (t->to_thread == thread) {
			if (!millet_binder_switch || t->need_reply) {
				spin_unlock(&t->lock);
				return 4;
			}
			*sync = 0;
		}
		spin_unlock(&t->lock);
	}
	return 0;
}

/*
 * Report to millet whether any binder_proc of @uid still has binder work:
 * status 1 if some proc has pending work (3 proc todo, 2 thread todo,
 * 4 thread is the target of an unanswered transaction), 0 otherwise.
 */
void query_binder_app_stat(int uid)
{
	struct binder_proc *proc;
	bool idle = true;
	int ret;

	if (!oem_binder_hook_set.query || !get_binder_lock ||
	    !get_binder_hhead)
		return;

	mutex_lock(get_binder_lock);
	hlist_for_each_entry(proc, get_binder_hhead, proc_node) {
		int sync = -1;

		if (!proc->tsk)
			continue;
		if (mi_task_uid(proc->tsk) != uid)
			continue;

		ret = 0;
		if (oem_binder_hook_set.query && proc->tsk) {
			spin_lock(&proc->inner_lock);
			ret = mi_proc_binder_stat(proc, &sync);
			spin_unlock(&proc->inner_lock);
		}
		idle &= !ret;
	}
	oem_binder_hook_set.query(uid, current, 0, current->pid, !idle);
	mutex_unlock(get_binder_lock);
}
EXPORT_SYMBOL_GPL(query_binder_app_stat);

/*
 * android_vh_binder_alloc_new_buf_locked: runs under alloc->lock (a
 * spinlock on 6.12), so nothing here may sleep. millet_sendmsg() is
 * GFP_ATOMIC + spinlock only.
 */
static void mi_binder_alloc_new_buf_locked(void *data, size_t size,
					   size_t *free_async_space,
					   int is_async, bool *should_fail)
{
	struct oem_binder_hook *set = &oem_binder_hook_set;
	struct task_struct *tsk;
	size_t need;

	if (!is_async || !set->overflow || !free_async_space)
		return;

	need = set->async_space_mult * (size + sizeof(struct binder_buffer));
	if (*free_async_space >= need &&
	    *free_async_space >= set->async_space_min)
		return;

	tsk = binder_buff_owner(free_async_space);
	if (!tsk)
		return;
	set->overflow(tsk, current, current->pid, false, 0);
}

/* android_vh_binder_reply: no binder lock held. */
static void mi_binder_replay(void *data, struct binder_proc *target_proc,
			     struct binder_proc *proc,
			     struct binder_thread *thread,
			     struct binder_transaction_data *tr)
{
	if (!oem_binder_hook_set.reply || !target_proc->tsk)
		return;
	oem_binder_hook_set.reply(target_proc->tsk, proc->tsk, thread->pid,
				  tr->flags & TF_ONE_WAY, tr->code);
}

/* Debug: log the interface of a oneway call into a frozen app. */
static void mi_binder_trans_debug(struct binder_proc *target_proc,
				  struct binder_proc *proc,
				  struct binder_transaction_data *tr)
{
	char buf[MI_TR_PEEK_LEN] = {};
	char desc[MI_TR_PEEK_LEN] = {};
	struct task_struct *tsk = target_proc->tsk;
	size_t len, i, j;

	if (!(tr->flags & TF_ONE_WAY))
		return;
	if (mi_task_uid(tsk) <= MI_APP_UID_MIN)
		return;
	if (!mi_task_freeze_state(tsk))
		return;

	/*
	 * Stock used copy_from_user() here with preemption disabled; use the
	 * non-faulting copy so a non-resident parcel just skips the log.
	 */
	len = min_t(size_t, tr->data_size, sizeof(buf));
	if (copy_from_user_nofault(buf,
				   u64_to_user_ptr((u64)tr->data.ptr.buffer),
				   len))
		return;

	/* UTF-16 interface token -> ASCII, stop at the first NUL */
	for (i = MI_TR_DESC_OFF, j = 0; i + 1 < len && j < sizeof(desc) - 1;
	     i += 2, j++) {
		if (!buf[i])
			break;
		desc[j] = buf[i];
	}

	printk(KERN_ERR "millet_kernel calleruid:%d callerPid:%d targetUid:%d targetPid:%d  code:%d  desc:%s",
	       mi_task_uid(proc->tsk), proc->tsk->tgid,
	       mi_task_uid(tsk), tsk->tgid, tr->code, desc);
}

/* android_vh_binder_trans: no binder lock held. */
static void mi_binder_transaction(void *data, struct binder_proc *target_proc,
				  struct binder_proc *proc,
				  struct binder_thread *thread,
				  struct binder_transaction_data *tr)
{
	int pc = preempt_count();

	/*
	 * As stock: the tracepoint only holds preempt_disable_notrace(). Drop
	 * it around the millet callback so reset_isfrozen() can take
	 * binder_procs_lock. Anything else on the count (a real lock) leaves
	 * preemption off and reset_isfrozen() then declines.
	 */
	if (pc == 1)
		preempt_enable();

	if (target_proc && oem_binder_hook_set.trans && target_proc->tsk)
		oem_binder_hook_set.trans(target_proc->tsk, proc->tsk,
					  thread->pid, tr->flags & TF_ONE_WAY,
					  tr->code, target_proc);

	if (pc == 1)
		preempt_disable();

	if (millet_debug && target_proc && target_proc->tsk)
		mi_binder_trans_debug(target_proc, proc, tr);
}

/*
 * android_vh_binder_wait_for_work: runs under proc->inner_lock, nothing
 * may sleep. Reports the target of the transaction this thread waits on.
 */
static void mi_binder_wait_for_work(void *data, bool do_proc_work,
				    struct binder_thread *thread,
				    struct binder_proc *proc)
{
	/* transaction_stack is stable: we hold proc->inner_lock */
	struct binder_transaction *t = thread->transaction_stack;
	struct task_struct *tsk = NULL;
	unsigned int code = 0;
	bool oneway = false;

	if (!t)
		return;

	spin_lock(&t->lock);
	if (oem_binder_hook_set.wait4 && !thread->is_dead &&
	    t->to_proc && t->to_proc->tsk) {
		tsk = t->to_proc->tsk;
		get_task_struct(tsk);
		code = t->code;
		oneway = t->flags & TF_ONE_WAY;
	}
	spin_unlock(&t->lock);

	if (!tsk)
		return;
	oem_binder_hook_set.wait4(tsk, proc->tsk, thread->pid, oneway, code);
	put_task_struct(tsk);
}

/* android_vh_binder_preset: called from binder_open(), keep the first. */
static void mi_get_hhead_and_lock(void *data, struct hlist_head *hhead,
				  struct mutex *lock, struct binder_proc *proc)
{
	if (bgot)
		return;
	if (hhead)
		get_binder_hhead = hhead;
	if (lock)
		get_binder_lock = lock;
	bgot = true;
}

void oem_register_binder_hook(struct oem_binder_hook *set)
{
	if (!set)
		return;
	oem_binder_hook_set = *set;
}
EXPORT_SYMBOL_GPL(oem_register_binder_hook);

static int __init init_binder_gki(void)
{
	pr_err("enter init_binder_gki func!\n");

	/* Stock ignores the return values and has no exit: not unloadable. */
	register_trace_android_vh_binder_alloc_new_buf_locked(
		mi_binder_alloc_new_buf_locked, NULL);
	register_trace_android_vh_binder_reply(mi_binder_replay, NULL);
	register_trace_android_vh_binder_trans(mi_binder_transaction, NULL);
	register_trace_android_vh_binder_wait_for_work(mi_binder_wait_for_work,
						       NULL);
	register_trace_android_vh_binder_preset(mi_get_hhead_and_lock, NULL);
	return 0;
}

module_init(init_binder_gki);
MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: millet_core");
