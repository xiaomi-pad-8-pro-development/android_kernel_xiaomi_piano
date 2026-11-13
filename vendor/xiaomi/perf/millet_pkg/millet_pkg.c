// SPDX-License-Identifier: GPL-2.0
/*
 * millet_pkg: network-activity reporter for Xiaomi millet (app freezer).
 *
 * Source port for Linux 6.12 of the stock 6.6 millet_pkg.ko (piano,
 * HyperOS OS4.0.5.0) reconstructed from its disassembly.
 *
 * - LOCAL_IN IPv4/IPv6 TCP: an app uid (>= 10000) that userspace put in
 *   the uid_rec hash is reported once to millet (PKG_TYPE) and cleared.
 * - Commands from userspace: uid_rec add/del/clear, core uid add/del,
 *   black pid/uid list add/del; core uid 111 toggles freezebinder_enable.
 * - find_and_clear_piduid() is exported for millet_binder.
 */
#define pr_fmt(fmt) "millet-millet_pkg: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/in.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>
#include <linux/uidgid.h>
#include <net/sock.h>
#include <net/request_sock.h>
#include <net/tcp_states.h>
#include <net/ipv6.h>
#include <net/net_namespace.h>
#include <millet/millet.h>

#define PKG_PRIORITY		226
#define PER_USER_RANGE		100000	/* Android AID_USER_OFFSET */
#define PKG_APP_UID_MIN		10000
#define PKG_FREEZEBINDER_UID	111
#define PKG_BLACK_NUM		ARRAY_SIZE(core_black_pid_rec)

enum pkg_cmd {
	PKG_REC_UID_ADD		= 1,
	PKG_REC_UID_DEL		= 2,
	PKG_REC_UID_CLEAR	= 3,
	PKG_CORE_UID_ADD	= 100,
	PKG_CORE_UID_DEL	= 101,
	PKG_BLACK_PID_ADD	= 102,
	PKG_BLACK_PID_DEL	= 103,
	PKG_BLACK_UID_ADD	= 104,
	PKG_BLACK_UID_DEL	= 105,
};

/* layout of millet_userconf.data (offset 0x18) for PKG_TYPE */
struct pkg_userconf {
	u8 pad[0x30];
	u32 cmd;	/* userconf + 0x48 */
	u32 uid;	/* userconf + 0x4c (pid for black pid cmds) */
};
static_assert(offsetof(struct millet_userconf, data) +
	      offsetof(struct pkg_userconf, cmd) == 0x48);

/* layout of millet_data.mod for PKG_TYPE */
struct pkg_info {
	u8 pad[0x30];
	u32 uid;	/* millet_data + 0x68 */
	u32 reserved;	/* millet_data + 0x6c */
};
static_assert(offsetof(struct millet_data, mod) +
	      offsetof(struct pkg_info, uid) == 0x68);

static inline struct pkg_info *pkg_info(struct millet_data *d)
{
	return (struct pkg_info *)d->mod.data;
}

static bool black_rec_has(atomic_t *rec, u32 val)
{
	int i;

	for (i = 0; i < PKG_BLACK_NUM; i++)
		if (atomic_read_acquire(&rec[i]) == val)
			return true;
	return false;
}

/*
 * Return false when the binder transaction from @srcuid/@task to @dstuid
 * must not thaw/report (blacklisted), true otherwise.
 */
int find_and_clear_piduid(uid_t srcuid, struct task_struct *task,
			  uid_t dstuid)
{
	u32 appid;

	if (!srcuid || !freezebinder_enable)
		return 1;

	if (core_uid_hash_find(dstuid)) {
		if (millet_debug)
			pr_err("find_and_clear_piduid dstuid srcuid=%d srcpid=%d, dstuid=%d\n",
			       srcuid, task->pid, dstuid);
		return 1;
	}

	if (black_rec_has(core_black_pid_rec, task->pid))
		return 0;

	appid = srcuid % PER_USER_RANGE;
	if (appid < frozen_uid_min) {
		if (black_rec_has(core_black_uid_rec, appid))
			return 0;
		if (millet_debug)
			pr_err("find_and_clear_piduid srcuid srcuid=%d srcpid=%d, dstuid=%d\n",
			       srcuid, task->pid, dstuid);
		return 1;
	}

	if (!core_uid_hash_find(srcuid))
		return 0;
	if (millet_debug)
		pr_err("find_and_clear_piduid realuid srcuid=%d srcpid=%d, dstuid=%d\n",
		       srcuid, task->pid, dstuid);
	return 1;
}
EXPORT_SYMBOL_GPL(find_and_clear_piduid);

static void pkg_add_uid(u32 val, atomic_t *rec)
{
	int i, slot = -1;

	for (i = 0; i < PKG_BLACK_NUM; i++) {
		u32 cur = atomic_read_acquire(&rec[i]);

		if (cur == val)
			return;
		if (!cur && slot < 0)
			slot = i;
	}
	if (slot < 0) {
		pr_err("%s : add uid:%d failed (full)!\n", __func__, val);
		return;
	}
	atomic_set(&rec[slot], val);
}

static void pkg_del_uid(u32 val, atomic_t *rec)
{
	int i;

	for (i = 0; i < PKG_BLACK_NUM; i++) {
		if (atomic_read_acquire(&rec[i]) == val) {
			atomic_set(&rec[i], 0);
			return;
		}
	}
}

static void pkg_report_uid(uid_t uid)
{
	struct millet_data data = {};

	pkg_info(&data)->uid = uid;
	if (millet_sendmsg(PKG_TYPE, current, &data) < 0)
		pr_err("%s : up report failed!\n", __func__);
}

static void pkg_recv_hook(void *buf, unsigned int len)
{
	struct millet_userconf *uc = buf;
	struct pkg_userconf *pc;
	int ret;

	if (!uc || len < sizeof(*uc))
		return;
	pc = (struct pkg_userconf *)uc->data;

	switch (pc->cmd) {
	case PKG_REC_UID_ADD:
		uid_rec_hash_add(pc->uid);
		break;
	case PKG_REC_UID_DEL:
		uid_rec_hash_del(pc->uid);
		break;
	case PKG_REC_UID_CLEAR:
		uid_rec_hash_clear();
		break;
	case PKG_CORE_UID_ADD:
		if (pc->uid == PKG_FREEZEBINDER_UID) {
			freezebinder_enable = 1;
			pkg_report_uid(PKG_FREEZEBINDER_UID);
			pr_err("freezebinder_enable=%d : set!\n",
			       freezebinder_enable);
			break;
		}
		ret = core_uid_hash_add(pc->uid);
		if (ret < 0)
			pr_err("%s: core_uid_hash_add failed for uid %d, ret=%d\n",
			       __func__, pc->uid, ret);
		break;
	case PKG_CORE_UID_DEL:
		if (pc->uid == PKG_FREEZEBINDER_UID) {
			freezebinder_enable = 0;
			pr_err("freezebinder_enable=%d : set!\n", 0);
			break;
		}
		core_uid_hash_del(pc->uid);
		break;
	case PKG_BLACK_PID_ADD:
		pkg_add_uid(pc->uid, core_black_pid_rec);
		break;
	case PKG_BLACK_PID_DEL:
		pkg_del_uid(pc->uid, core_black_pid_rec);
		break;
	case PKG_BLACK_UID_ADD:
		pkg_add_uid(pc->uid, core_black_uid_rec);
		break;
	case PKG_BLACK_UID_DEL:
		pkg_del_uid(pc->uid, core_black_uid_rec);
		break;
	default:
		break;
	}
}

static int pkg_sendmsg(struct task_struct *tsk, struct millet_data *data,
		       struct millet_sock *sk)
{
	if (!tsk || !data || !sk) {
		pr_err("%s input invalid\n", __func__);
		return -1;
	}
	data->msg_type = MSG_TO_USER;
	data->owner = PKG_TYPE;
	data->uid = pkg_info(data)->uid;
	return millet_sendto_user(tsk, data, sk);
}

static void pkg_init_millet(struct millet_sock *sk)
{
	if (!sk)
		return;
	sk->mod[PKG_TYPE].monitor = PKG_TYPE;
	sk->mod[PKG_TYPE].enable = 1;
}

static void pkg_check_sk(struct sk_buff *skb)
{
	struct sock *sk = skb->sk;
	uid_t uid;

	if (!sk)
		return;
	if (sk->sk_state == TCP_NEW_SYN_RECV) {
		sk = inet_reqsk(sk)->rsk_listener;
		if (!sk)
			return;
	}
	if (!sk_fullsock(sk))
		return;

	uid = from_kuid(&init_user_ns, sock_i_uid(sk));
	if (uid < PKG_APP_UID_MIN)
		return;
	if (uid_rec_hash_find_and_clear(uid))
		pkg_report_uid(uid);
}

static unsigned int pkg_ip4_in(void *priv, struct sk_buff *skb,
			       const struct nf_hook_state *state)
{
	if (ip_hdr(skb)->protocol == IPPROTO_TCP)
		pkg_check_sk(skb);
	return NF_ACCEPT;
}

static unsigned int pkg_ip6_in(void *priv, struct sk_buff *skb,
			       const struct nf_hook_state *state)
{
	unsigned int thoff = 0;
	unsigned short frag_off = 0;

	if (ipv6_find_hdr(skb, &thoff, -1, &frag_off, NULL) == IPPROTO_TCP)
		pkg_check_sk(skb);
	return NF_ACCEPT;
}

static unsigned int pkg_ip_out(void *priv, struct sk_buff *skb,
			       const struct nf_hook_state *state)
{
	return NF_ACCEPT;
}

static const struct nf_hook_ops pkg_nf_ops[] = {
	{
		.hook		= pkg_ip4_in,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_LOCAL_IN,
		.priority	= PKG_PRIORITY,
	}, {
		.hook		= pkg_ip6_in,
		.pf		= NFPROTO_IPV6,
		.hooknum	= NF_INET_LOCAL_IN,
		.priority	= PKG_PRIORITY,
	}, {
		.hook		= pkg_ip_out,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_LOCAL_OUT,
		.priority	= PKG_PRIORITY,
	}, {
		.hook		= pkg_ip_out,
		.pf		= NFPROTO_IPV6,
		.hooknum	= NF_INET_LOCAL_OUT,
		.priority	= PKG_PRIORITY,
	},
};

static int __init millet_pkg_init(void)
{
	freezebinder_enable = 0;
	pr_err("enter millet_pkg_init func!\n");

	if (nf_register_net_hooks(&init_net, pkg_nf_ops,
				  ARRAY_SIZE(pkg_nf_ops)) < 0) {
		pr_err("nf_register_hooks(millet hooks) error\n");
		return -1;
	}
	pr_err("nf_register_hooks(millet hooks) success\n");

	register_millet_hook(PKG_TYPE, pkg_recv_hook, pkg_sendmsg,
			     pkg_init_millet);
	init_millet_subsystem(PKG_TYPE);
	return 0;
}

static void __exit millet_pkg_exit(void)
{
	unregister_millet_hook(PKG_TYPE);
	nf_unregister_net_hooks(&init_net, pkg_nf_ops, ARRAY_SIZE(pkg_nf_ops));
}

module_init(millet_pkg_init);
module_exit(millet_pkg_exit);
MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: millet_core");
