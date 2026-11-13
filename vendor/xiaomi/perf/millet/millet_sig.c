// SPDX-License-Identifier: GPL-2.0
/*
 * millet_sig: the millet SIG module type.
 *
 * Rebuilt for 6.12 from the disassembly of the stock HyperOS OS4.0.5
 * module vendor_boot/lib/modules/millet_sig.ko (every instruction is
 * accounted for; task_struct offsets named from the stock 6.6 BTF).
 * Not yet run on a device.
 *
 * Reports SIGQUIT, SIGABRT, SIGKILL and SIGTERM sent to a frozen or
 * freezing task to the millet daemon, once per target process in a row,
 * so the daemon can thaw it.
 */
#include <linux/cgroup.h>
#include <linux/cred.h>
#include <linux/freezer.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <trace/hooks/signal.h>
#include <millet/millet.h>

#undef pr_fmt
#define pr_fmt(fmt) "millet_millet-sig: " fmt

#define MILLET_SIG_MASK (BIT(SIGQUIT) | BIT(SIGABRT) | BIT(SIGKILL) | BIT(SIGTERM))

static int last_report_task;

static void millet_sig(void *unused, int sig, struct task_struct *killer,
		       struct task_struct *dst)
{
	struct millet_data msg = {};

	if (sig < 0 || sig > 15 || !(BIT(sig) & MILLET_SIG_MASK))
		return;
	msg.mod.sig_info.sig = sig;
	msg.mod.sig_info.sig_mask = BIT(sig);
	msg.mod.sig_info.task = dst;
	millet_sendmsg(SIG_TYPE, dst, &msg);
}

static bool task_frozen_or_freezing(struct task_struct *p)
{
	bool frozen = false;

	if (freezing(p))
		return true;
	if (!(p->flags & PF_KTHREAD)) {
		rcu_read_lock();
		frozen = test_bit(CGRP_FREEZE, &task_dfl_cgroup(p)->flags);
		rcu_read_unlock();
		if (frozen)
			return true;
	}
	return p->frozen;
}

static int signals_sendmsg(struct task_struct *tsk, struct millet_data *data,
			   struct millet_sock *sk)
{
	int *last;

	if (!tsk || !data || !sk) {
		pr_err("%s input invalid\n", __func__);
		return -1;
	}

	data->mod.sig_info.pid = tsk->tgid;
	rcu_read_lock();
	data->uid = __kuid_val(rcu_dereference(tsk->real_cred)->uid);
	rcu_read_unlock();
	data->msg_type = MSG_TO_USER;
	data->owner = SIG_TYPE;

	if (!task_frozen_or_freezing(tsk))
		return 0;

	last = sk->mod[SIG_TYPE].priv;
	if (data->mod.sig_info.pid == *last)
		return 0;
	*last = data->mod.sig_info.pid;
	return millet_sendto_user(tsk, data, sk);
}

static void signas_init_millet(struct millet_sock *sk)
{
	if (!sk)
		return;
	sk->mod[SIG_TYPE].priv = &last_report_task;
	sk->mod[SIG_TYPE].monitor = SIG_TYPE;
	sk->mod[SIG_TYPE].enable = 1;
}

static int __init sig_mod_init(void)
{
	pr_err("enter sig_mod_init func!\n");
	register_millet_hook(SIG_TYPE, NULL, signals_sendmsg, signas_init_millet);
	init_millet_subsystem(SIG_TYPE);
	/* Vendor hooks cannot be safely removed, so like the stock module there is no exit. */
	return register_trace_android_vh_do_send_sig_info(millet_sig, NULL);
}

module_init(sig_mod_init);
MODULE_LICENSE("GPL");
