// SPDX-License-Identifier: GPL-2.0
/*
 * millet_hs: the millet HANDSHK module type.
 *
 * Rebuilt for 6.12 from the disassembly of the stock HyperOS OS4.0.5
 * module vendor_boot/lib/modules/millet_hs.ko (every instruction is
 * accounted for). Not yet run on a device.
 *
 * A MSG_TO_KERN message for this type is echoed back to the daemon as an
 * empty MSG_TO_USER message, which lets the daemon check the link.
 */
#include <linux/module.h>
#include <millet/millet.h>

#undef pr_fmt
#define pr_fmt(fmt) "millet-millet_hs: " fmt

static void hs_recv_hook(void *data, unsigned int len)
{
	struct millet_data msg = {};

	/*
	 * The stock code passes the len register through as the task
	 * argument; hs_sendmsg never reads it, so pass NULL.
	 */
	millet_sendmsg(HANDSHK_TYPE, NULL, &msg);
}

static int hs_sendmsg(struct task_struct *tsk, struct millet_data *data,
		      struct millet_sock *sk)
{
	if (!data || !sk) {
		pr_err("%s input invalid\n", __func__);
		return -1;
	}
	data->msg_type = MSG_TO_USER;
	data->owner = HANDSHK_TYPE;
	return millet_sendto_user(tsk, data, sk);
}

static void hs_init_millet(struct millet_sock *sk)
{
	if (!sk)
		return;
	sk->mod[HANDSHK_TYPE].monitor = SIG_TYPE;
	sk->mod[HANDSHK_TYPE].enable = 1;
}

static int __init millet_hs_init(void)
{
	pr_err("enter millet_hs_init func!\n");
	register_millet_hook(HANDSHK_TYPE, hs_recv_hook, hs_sendmsg,
			     hs_init_millet);
	init_millet_subsystem(HANDSHK_TYPE);
	return 0;
}

static void __exit millet_hs_exit(void)
{
	unregister_millet_hook(HANDSHK_TYPE);
}

module_init(millet_hs_init);
module_exit(millet_hs_exit);
MODULE_LICENSE("GPL");
