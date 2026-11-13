// SPDX-License-Identifier: GPL-2.0
/*
 * millet_binder: binder reporter for Xiaomi millet (app freezer).
 *
 * Source port for Linux 6.12 of the stock 6.6 millet_binder.ko (piano,
 * HyperOS OS4.0.5.0) reconstructed from its disassembly.
 *
 * - Registers callbacks with binder_gki; they report binder events into
 *   frozen apps to userspace (BINDER_TYPE): reply, transaction, wait for
 *   work, async buffer overflow.
 * - A synchronous transaction into a frozen app from an allowed caller
 *   (millet_pkg find_and_clear_piduid) clears binder is_frozen through
 *   binder_gki reset_isfrozen.
 * - BINDER_ST_TYPE: userspace asks for an uid's pending binder work,
 *   answered through binder_gki query_binder_app_stat.
 */
#define pr_fmt(fmt) "millet_millet-binder: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/refcount.h>
#include <millet/millet.h>

#include "binder_gki.h"

/* millet_pkg.ko */
int find_and_clear_piduid(uid_t srcuid, struct task_struct *task,
			  uid_t dstuid);

/* async overflow: free_async_space < 3 * (size + buffer) or < 128 KiB */
#define MB_ASYNC_SPACE_MULT	3
#define MB_ASYNC_SPACE_MIN	0x20000
#define MB_NOT_FOUND		111	/* added to reason when not allowed */

enum mb_reason {
	MB_OVERFLOW	= 0,
	MB_REPLY	= 1,
	MB_TRANS	= 2,
	MB_WAIT4	= 3,
	MB_TRANS_THAWED	= 4,	/* is_frozen cleared, or proc not frozen */
};

/* layout of millet_data.mod for BINDER_TYPE */
struct binder_info {
	u64 reason;			/* millet_data + 0x38 */
	u8 pad[0x28];
	struct task_struct *src;	/* millet_data + 0x68 */
	struct task_struct *dst;	/* millet_data + 0x70 */
	u32 srcuid;			/* millet_data + 0x78 */
	u32 srcpid;			/* millet_data + 0x7c */
	u32 pid;			/* millet_data + 0x80 binder thread */
	u32 dstpid;			/* millet_data + 0x84 */
	u8 oneway;			/* millet_data + 0x88 */
	u32 code;			/* millet_data + 0x8c */
};
static_assert(offsetof(struct millet_data, mod) +
	      offsetof(struct binder_info, src) == 0x68);
static_assert(offsetof(struct millet_data, mod) +
	      offsetof(struct binder_info, code) == 0x8c);
static_assert(sizeof(struct binder_info) <= sizeof_field(struct millet_data, mod));

/* layout of millet_data.mod for BINDER_ST_TYPE */
struct binder_st_info {
	u8 pad[0x30];
	struct task_struct *tsk;	/* millet_data + 0x68 */
	u32 pid;			/* millet_data + 0x70 */
	u32 unused;			/* millet_data + 0x74 */
	u32 status;			/* millet_data + 0x78 */
};
static_assert(offsetof(struct millet_data, mod) +
	      offsetof(struct binder_st_info, status) == 0x78);

/* layout of millet_userconf.data for BINDER_ST_TYPE */
struct binder_st_userconf {
	u8 pad[0x30];
	u32 uid;			/* userconf + 0x48 */
};
static_assert(offsetof(struct millet_userconf, data) +
	      offsetof(struct binder_st_userconf, uid) == 0x48);

static inline struct binder_info *binder_info(struct millet_data *d)
{
	return (struct binder_info *)d->mod.data;
}

static inline struct binder_st_info *binder_st_info(struct millet_data *d)
{
	return (struct binder_st_info *)d->mod.data;
}

static void mb_report(u64 reason, struct task_struct *dst,
		      struct task_struct *src, int pid, bool oneway,
		      unsigned int code)
{
	struct millet_data data = {};
	struct binder_info *info = binder_info(&data);

	info->reason = reason;
	info->src = src;
	info->dst = dst;
	info->pid = pid;
	info->oneway = oneway;
	info->code = code;
	millet_sendmsg(BINDER_TYPE, dst, &data);
}

/* BINDER_TYPE send_to: only report into frozen tasks. */
static int binder_sendmsg(struct task_struct *tsk, struct millet_data *data,
			  struct millet_sock *sk)
{
	struct binder_info *info;
	int found;

	if (!tsk || !data || !sk) {
		pr_err("%s input invalid\n", __func__);
		return -1;
	}

	info = binder_info(data);
	data->msg_type = MSG_TO_USER;
	data->owner = BINDER_TYPE;
	info->srcpid = info->src->pid;
	info->srcuid = mi_task_uid(info->src);
	info->dstpid = info->dst->pid;
	data->uid = mi_task_uid(info->dst);

	if (!mi_task_freeze_state(tsk))
		return 0;

	if (freezebinder_enable &&
	    info->reason != MB_OVERFLOW && info->reason != MB_TRANS_THAWED) {
		found = find_and_clear_piduid(info->srcuid, info->src,
					      data->uid);
		if (!found)
			info->reason += MB_NOT_FOUND;
		if (millet_debug)
			pr_err("bindersend found=%d srcuid=%d srcpid=%d uid=%d oneway=%d\n",
			       found, mi_task_uid(info->src), info->src->pid,
			       data->uid, info->oneway);
		/* not allowed: drop oneway calls, still report sync ones */
		if (!found && info->oneway)
			return 0;
	}
	return millet_sendto_user(tsk, data, sk);
}

static void binder_init_millet(struct millet_sock *sk)
{
	if (!sk)
		return;
	sk->mod[BINDER_TYPE].monitor = BINDER_TYPE;
	sk->mod[BINDER_TYPE].enable = 1;
}

/* BINDER_ST_TYPE recv_from: userspace asks for an uid's binder state. */
static void binder_recv_hook(void *buf, unsigned int len)
{
	struct millet_userconf *uc = buf;

	if (!uc || len < sizeof(*uc))
		return;
	query_binder_app_stat(((struct binder_st_userconf *)uc->data)->uid);
}

static int binder_st_sendmsg(struct task_struct *tsk,
			     struct millet_data *data, struct millet_sock *sk)
{
	if (!tsk || !data || !sk) {
		pr_err("%s input invalid\n", __func__);
		return -1;
	}
	data->msg_type = MSG_TO_USER;
	data->owner = BINDER_ST_TYPE;
	return millet_sendto_user(tsk, data, sk);
}

static void binder_st_init_millet(struct millet_sock *sk)
{
	if (!sk)
		return;
	/* as stock: status replies are routed to the BINDER monitor */
	sk->mod[BINDER_ST_TYPE].monitor = BINDER_TYPE;
	sk->mod[BINDER_ST_TYPE].enable = 1;
}

/* binder_gki reply hook (no binder lock held). */
static void mi_binder_reply_hook(struct task_struct *dst,
				 struct task_struct *src, int pid,
				 bool oneway, unsigned int code)
{
	if (!dst)
		return;
	get_task_struct(dst);
	if (mi_task_uid(dst) <= frozen_uid_min) {
		get_task_struct(src);
		mb_report(MB_REPLY, dst, src, pid, oneway, code);
		put_task_struct(src);
	}
	put_task_struct(dst);
}

/*
 * binder_gki trans hook: no binder lock held and binder_gki dropped the
 * tracepoint's preempt_disable, so reset_isfrozen() may take its mutex.
 */
static void mi_binder_trans_hook(struct task_struct *dst,
				 struct task_struct *src, int pid,
				 bool oneway, unsigned int code,
				 struct binder_proc *proc)
{
	int found = 0, ret = 0;
	bool thawed;

	if (!dst || !dst->stack || !refcount_read(&dst->usage))
		return;
	get_task_struct(dst);
	if (mi_task_uid(dst) <= frozen_uid_min || dst->pid == src->pid)
		goto out_dst;
	get_task_struct(src);

	if (millet_debug && !mi_task_freeze_state(dst) &&
	    (READ_ONCE(dst->__state) & TASK_FROZEN))
		pr_err("binder_trans_hook dst=%d srcpid=%d\n",
		       mi_task_uid(dst), src->pid);

	if (freezebinder_enable == 1 && !oneway && mi_task_freeze_state(dst)) {
		found = find_and_clear_piduid(mi_task_uid(src), src,
					      mi_task_uid(dst));
		ret = reset_isfrozen(mi_task_uid(src), dst->pid, found, proc);
		if (found && millet_debug && ret == 1)
			pr_err("binder_trans_hook dst=%d srcpid=%d no is_frozen\n",
			       mi_task_uid(dst), src->pid);
	}

	thawed = (ret == 1 && found) || ret == 2;
	mb_report(thawed ? MB_TRANS_THAWED : MB_TRANS, dst, src, pid, oneway,
		  code);
	put_task_struct(src);
out_dst:
	put_task_struct(dst);
}

/* binder_gki wait4 hook: runs under proc->inner_lock, must not sleep. */
static void mi_binder_wait4_hook(struct task_struct *dst,
				 struct task_struct *src, int pid,
				 bool oneway, unsigned int code)
{
	if (!dst)
		return;
	get_task_struct(dst);
	if (mi_task_uid(dst) <= frozen_uid_min && dst->pid != src->pid) {
		get_task_struct(src);
		mb_report(MB_WAIT4, dst, src, pid, oneway, code);
		put_task_struct(src);
	}
	put_task_struct(dst);
}

/* binder_gki overflow hook: runs under alloc->lock, must not sleep. */
static void mi_binder_overflow_hook(struct task_struct *dst,
				    struct task_struct *src, int pid,
				    bool oneway, unsigned int code)
{
	if (!dst)
		return;
	get_task_struct(dst);
	get_task_struct(src);
	mb_report(MB_OVERFLOW, dst, src, pid, oneway, code);
	put_task_struct(src);
	put_task_struct(dst);
}

/* binder_gki query hook: answer for query_binder_app_stat(). */
static void mi_query_binder_st_hook(int uid, struct task_struct *tsk,
				    int unused, int pid, int status)
{
	struct millet_data data = {};
	struct binder_st_info *info = binder_st_info(&data);

	data.uid = uid;
	info->tsk = tsk;
	info->pid = pid;
	info->unused = unused;
	info->status = status;
	millet_sendmsg(BINDER_ST_TYPE, tsk, &data);
}

static int __init init_millet_binder_drv(void)
{
	struct oem_binder_hook set = {
		.async_space_mult	= MB_ASYNC_SPACE_MULT,
		.async_space_min	= MB_ASYNC_SPACE_MIN,
		.overflow		= mi_binder_overflow_hook,
		.reply			= mi_binder_reply_hook,
		.trans			= mi_binder_trans_hook,
		.wait4			= mi_binder_wait4_hook,
		.query			= mi_query_binder_st_hook,
	};

	pr_err("enter init_millet_binder_drv func!\n");
	oem_register_binder_hook(&set);

	/* Stock has no exit: the hooks stay in binder_gki, not unloadable. */
	register_millet_hook(BINDER_TYPE, NULL, binder_sendmsg,
			     binder_init_millet);
	init_millet_subsystem(BINDER_TYPE);
	register_millet_hook(BINDER_ST_TYPE, binder_recv_hook,
			     binder_st_sendmsg, binder_st_init_millet);
	init_millet_subsystem(BINDER_ST_TYPE);
	return 0;
}

module_init(init_millet_binder_drv);
MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: millet_core millet_pkg binder_gki");
