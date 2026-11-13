// SPDX-License-Identifier: GPL-2.0
/*
 * millet_core: "Millet smart frozen control subsystem".
 *
 * Rebuilt for 6.12 from static analysis (disassembly, kCFI type hashes and
 * the stock 6.6 kernel BTF) of the HyperOS OS4.0.5 module
 * vendor_boot/lib/modules/millet_core.ko. Not yet run on a device.
 *
 * A NETLINK_MILLET (31) socket links the HyperOS millet daemon with the
 * millet modules (sig, binder, pkg, hs, ...). The daemon handshakes per
 * module type; afterwards the modules report events with millet_sendmsg()
 * and get the daemon's commands through their recv_from hook. Two uid
 * hash tables and /proc/millet/{millet_stat,version} complete it.
 */
#include <linux/hash.h>
#include <linux/hashtable.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <net/net_namespace.h>
#include <net/sock.h>
#include <millet/millet.h>

#undef pr_fmt
#define pr_fmt(fmt) "millet_millet-core: " fmt

struct millet_sock millet_sk;

unsigned int millet_debug;
module_param(millet_debug, uint, 0644);
EXPORT_SYMBOL(millet_debug);
unsigned int frozen_uid_min = 10000;
module_param(frozen_uid_min, uint, 0644);
EXPORT_SYMBOL(frozen_uid_min);
unsigned long binder_warn_ahead_space = 0x20000;
module_param(binder_warn_ahead_space, ulong, 0644);
EXPORT_SYMBOL(binder_warn_ahead_space);
static int millet_freeze_switch;
module_param(millet_freeze_switch, int, 0660);
bool millet_binder_switch = true;
module_param(millet_binder_switch, bool, 0644);
EXPORT_SYMBOL(millet_binder_switch);
int millet_v = 1;
/* Exported like the stock module; nothing in it writes the value. */
int freezebinder_enable;
EXPORT_SYMBOL(freezebinder_enable);

struct proc_dir_entry *millet_rootdir;
atomic_t core_black_pid_rec[10];
EXPORT_SYMBOL(core_black_pid_rec);
atomic_t core_black_uid_rec[10];
EXPORT_SYMBOL(core_black_uid_rec);

static const char *const millet_type_name[] = {
	"NULL", "SIG", "BINDER", "BINDER_STAT", "MEM", "PKG", "HANDSHK", "invalid",
};

static inline bool type_valid(int type)
{
	return type >= SIG_TYPE && type < MILLET_TYPES_NUM;
}

bool judge_millet_freeze_switch(void)
{
	return millet_freeze_switch == 1;
}

/* ---- uid hash tables (64 buckets, hash_32) ---- */

struct uid_node {
	uid_t uid;
	struct hlist_node node;
};

#define DEFINE_UID_HASH(prefix)						\
static DEFINE_SPINLOCK(prefix##_hash_lock);				\
static DEFINE_HASHTABLE(prefix##_hashtable, 6);			\
									\
static struct uid_node *prefix##_lookup(uid_t uid)			\
{									\
	struct uid_node *n;						\
									\
	hash_for_each_possible(prefix##_hashtable, n, node, uid)	\
		if (n->uid == uid)					\
			return n;					\
	return NULL;							\
}									\
									\
int prefix##_hash_add(uid_t uid)					\
{									\
	struct uid_node *n;						\
	unsigned long flags;						\
									\
	if (!uid)							\
		return -EINVAL;						\
	spin_lock_irqsave(&prefix##_hash_lock, flags);			\
	n = prefix##_lookup(uid);					\
	spin_unlock_irqrestore(&prefix##_hash_lock, flags);		\
	if (n)								\
		return 0;						\
	n = kmalloc(sizeof(*n), GFP_KERNEL);				\
	if (!n)								\
		return -ENOMEM;						\
	n->uid = uid;							\
	spin_lock_irqsave(&prefix##_hash_lock, flags);			\
	hash_add(prefix##_hashtable, &n->node, uid);			\
	spin_unlock_irqrestore(&prefix##_hash_lock, flags);		\
	return 0;							\
}									\
EXPORT_SYMBOL(prefix##_hash_add);					\
									\
int prefix##_hash_del(uid_t uid)					\
{									\
	struct uid_node *n;						\
	unsigned long flags;						\
									\
	spin_lock_irqsave(&prefix##_hash_lock, flags);			\
	n = prefix##_lookup(uid);					\
	if (n)								\
		hash_del(&n->node);					\
	spin_unlock_irqrestore(&prefix##_hash_lock, flags);		\
	if (!n)								\
		return -ENOENT;						\
	kfree(n);							\
	return 0;							\
}									\
EXPORT_SYMBOL(prefix##_hash_del);					\
									\
bool prefix##_hash_find(uid_t uid)					\
{									\
	unsigned long flags;						\
	bool found;							\
									\
	if (!uid)							\
		return false;						\
	spin_lock_irqsave(&prefix##_hash_lock, flags);			\
	found = prefix##_lookup(uid) != NULL;				\
	spin_unlock_irqrestore(&prefix##_hash_lock, flags);		\
	return found;							\
}									\
EXPORT_SYMBOL(prefix##_hash_find);					\
									\
bool prefix##_hash_find_and_clear(uid_t uid)				\
{									\
	struct uid_node *n;						\
	unsigned long flags;						\
									\
	if (!uid)							\
		return false;						\
	spin_lock_irqsave(&prefix##_hash_lock, flags);			\
	n = prefix##_lookup(uid);					\
	if (n)								\
		hash_del(&n->node);					\
	spin_unlock_irqrestore(&prefix##_hash_lock, flags);		\
	kfree(n);							\
	return n != NULL;						\
}									\
									\
void prefix##_hash_clear(void)						\
{									\
	struct uid_node *n;						\
	struct hlist_node *tmp;						\
	unsigned long flags;						\
	int bkt;							\
									\
	spin_lock_irqsave(&prefix##_hash_lock, flags);			\
	hash_for_each_safe(prefix##_hashtable, bkt, tmp, n, node) {	\
		hash_del(&n->node);					\
		kfree(n);						\
	}								\
	spin_unlock_irqrestore(&prefix##_hash_lock, flags);		\
}									\
EXPORT_SYMBOL(prefix##_hash_clear);					\
									\
/* Up to 128 uids are copied out under the lock, then printed. */	\
void prefix##_hash_show(struct seq_file *m, const char *sep)		\
{									\
	uid_t uids[128];						\
	struct uid_node *n;						\
	unsigned long flags;						\
	int bkt, cnt = 0, i;						\
									\
	spin_lock_irqsave(&prefix##_hash_lock, flags);			\
	hash_for_each(prefix##_hashtable, bkt, n, node)			\
		if (cnt < ARRAY_SIZE(uids))				\
			uids[cnt++] = n->uid;				\
	spin_unlock_irqrestore(&prefix##_hash_lock, flags);		\
	for (i = 0; i < cnt; i++)					\
		seq_printf(m, "%d%s", uids[i], sep);			\
}									\
EXPORT_SYMBOL(prefix##_hash_show)

DEFINE_UID_HASH(core_uid);
DEFINE_UID_HASH(uid_rec);
EXPORT_SYMBOL(uid_rec_hash_find_and_clear);

/* ---- hooks ---- */

int register_millet_hook(int type, recv_from_user recv_from,
			 send_to_user send_to, init_millet init)
{
	struct millet_mod *mod;

	if (!type_valid(type)) {
		pr_err("%s: type is invalid! %d\n", __func__, type);
		return -1;
	}
	mod = &millet_sk.mod[type];
	/* Same order as the stock module: init, recv_from, send_to. */
	if (mod->init) {
		pr_err("%s: init has been registered %d\n", __func__, type);
		return -1;
	}
	mod->init = init;
	if (mod->recv_from) {
		pr_err("%s: recv_from has been registered %d\n", __func__, type);
		return -1;
	}
	mod->recv_from = recv_from;
	if (mod->send_to) {
		pr_err("%s: send_to has been registered %d\n", __func__, type);
		return -1;
	}
	mod->send_to = send_to;
	if (millet_debug) {
		pr_info("type %d register hook\n", type);
		dump_stack();
	}
	return 0;
}
EXPORT_SYMBOL(register_millet_hook);

int unregister_millet_hook(int type)
{
	if (!type_valid(type)) {
		pr_err("%s: type is invalid! %d\n", __func__, type);
		return -1;
	}
	millet_sk.mod[type].recv_from = NULL;
	millet_sk.mod[type].send_to = NULL;
	millet_sk.mod[type].init = NULL;
	return 0;
}
EXPORT_SYMBOL(unregister_millet_hook);

int init_millet_subsystem(int type)
{
	struct millet_mod *mod;

	if (!type_valid(type)) {
		pr_err("%s: type is invalid! %d\n", __func__, type);
		return -1;
	}
	mod = &millet_sk.mod[type];
	mod->port = 0;
	mod->handshake = 0;
	spin_lock_init(&mod->lock);
	if (mod->init)
		mod->init(&millet_sk);
	strscpy(mod->name, millet_type_name[type], sizeof(mod->name));
	return 0;
}
EXPORT_SYMBOL(init_millet_subsystem);

/* ---- netlink ---- */

static void dump_up_msg(const struct millet_data *msg)
{
	pr_info("-----up-report msg-head dump-----\n");
	pr_info("up msg: %d\n", msg->msg_type);
	pr_info("up type: %d\n", msg->owner);
	pr_info("up src_port: 0x%lu\n", (unsigned long)msg->src_port);
	pr_info("up dest_port: 0x%lu\n", (unsigned long)msg->dst_port);
	pr_info("up uid: %d\n", msg->uid);
	if (msg->owner == SIG_TYPE)
		pr_info("up sig: 0x%lu\n", (unsigned long)msg->mod.sig);
}

int millet_sendto_user(struct task_struct *tsk, struct millet_data *msg,
		       struct millet_sock *sk)
{
	struct millet_data *payload;
	struct nlmsghdr *nlh;
	struct sk_buff *skb;
	struct millet_mod *mod;
	struct timespec64 ts;
	unsigned int dest;
	int ret;

	if (!msg || msg->msg_type < LOOPBACK_MSG || msg->msg_type > MSG_TO_KERN) {
		pr_err("%s:msg or  msg type is invalid! %d\n", __func__,
		       msg ? msg->msg_type : 0);
		return -1;
	}

	skb = alloc_skb(NLMSG_SPACE(sizeof(*msg)), GFP_ATOMIC);
	if (!skb) {
		pr_err("%s alloc_skb failed! %d\n", __func__, msg->owner);
		return -1;
	}
	nlh = nlmsg_put(skb, 0, 0, 0, sizeof(*msg), 0);
	if (!nlh)
		goto free;

	payload = nlmsg_data(nlh);
	memcpy(payload, msg, sizeof(*msg));
	payload->src_port = MILLET_KERNEL_PORT;
	payload->dst_port = MILLET_USER_PORT;

	if (payload->owner >= MILLET_TYPES_NUM)
		goto free;
	dest = sk->mod[payload->owner].monitor;
	if (!type_valid(dest))
		dest = payload->owner;
	mod = &sk->mod[dest];

	if (!READ_ONCE(mod->enable))
		goto free;
	if (!READ_ONCE(mod->handshake)) {
		if (millet_debug)
			pr_err("monitor %d no hand shake!!", dest);
		goto free;
	}

	payload->monitor = dest;
	ktime_get_ts64(&ts);
	payload->tm = ts;
	if (millet_debug)
		dump_up_msg(payload);

	ret = netlink_unicast(sk->sock, skb, READ_ONCE(mod->port), MSG_DONTWAIT);
	if (ret < 0) {
		if (millet_debug)
			pr_err("nlmsg_unicast failed! %s errno %d\n", __func__, ret);
		return -1;
	}
	if (millet_debug)
		pr_info("nlmsg_unicast snd msg success to %d\n", mod->port);
	return 0;
free:
	kfree_skb(skb);
	return -1;
}
EXPORT_SYMBOL(millet_sendto_user);

int millet_sendmsg(int type, struct task_struct *tsk, struct millet_data *data)
{
	struct millet_mod *mod;
	unsigned long flags;
	s64 start;
	int ret;

	if (!type_valid(type)) {
		pr_err("wrong type valid %d\n", type);
		return -1;
	}
	mod = &millet_sk.mod[type];
	if (!mod->send_to) {
		pr_err("mod %d send_to interface is NULL\n", type);
		return -1;
	}

	start = ktime_get();
	ret = mod->send_to(tsk, data, &millet_sk);
	if (READ_ONCE(mod->enable)) {
		spin_lock_irqsave(&mod->lock, flags);
		/* As in the stock module: >0 counts as sent, <0 as failed. */
		if (ret < 0)
			mod->send_fail++;
		else if (ret > 0)
			mod->send_suc++;
		mod->runtime += ktime_to_us(ktime_get()) - ktime_to_us(start);
		spin_unlock_irqrestore(&mod->lock, flags);
	}
	return ret;
}
EXPORT_SYMBOL(millet_sendmsg);

static void dump_recv_msg(const struct millet_userconf *uc)
{
	if (!millet_debug)
		return;
	pr_info("-----recv msg from user-----\n");
	pr_info("msg: %d\n", uc->msg_type);
	pr_info("type: %d\n", uc->owner);
	pr_info("src_port: 0x%lu\n", (unsigned long)uc->src_port);
	pr_info("dest_port: 0x%lu\n", (unsigned long)uc->dst_port);
}

static void recv_handler(struct sk_buff *skb)
{
	struct millet_userconf *uc;
	struct millet_data ack = {};
	struct nlmsghdr *nlh;
	struct millet_mod *mod;
	uid_t uid;

	if (!skb) {
		pr_err("recv_handler %s: skb is\tNULL!\n", __func__);
		return;
	}
	uid = from_kuid(&init_user_ns, NETLINK_CB(skb).creds.uid);
	if (uid > MILLET_PRIV_UID_MAX) {
		pr_err("uid: %d, permission denied\n", uid);
		return;
	}
	if (millet_debug)
		pr_info("kernel millet receive msg now\n");
	if (skb->len < NLMSG_HDRLEN) {
		pr_err("msg len is invalid\n");
		return;
	}
	nlh = nlmsg_hdr(skb);
	if (nlh->nlmsg_len < NLMSG_SPACE(sizeof(*uc))) {
		pr_err("msg len err %d need %d\n", nlh->nlmsg_len,
		       (int)NLMSG_SPACE(sizeof(*uc)));
		return;
	}

	uc = nlmsg_data(nlh);
	if (uc->src_port != MILLET_USER_PORT) {
		if (millet_debug) {
			pr_err("src_port %lu invalid! from %d need len %d len %d\n",
			       (unsigned long)uc->src_port, nlh->nlmsg_pid,
			       nlh->nlmsg_len, (int)NLMSG_SPACE(sizeof(*uc)));
			pr_err("-------invalid msg dump------");
			pr_err("**current pid %d****\n", current->pid);
		}
		return;
	}
	if (uc->dst_port != MILLET_KERNEL_PORT) {
		pr_err("dst_port is %lu not kernel!\n", (unsigned long)uc->dst_port);
		return;
	}
	if (!type_valid(uc->owner)) {
		pr_err("mod %d is not valid!\n", uc->owner);
		return;
	}
	mod = &millet_sk.mod[uc->owner];

	switch (uc->msg_type) {
	case LOOPBACK_MSG:
		WRITE_ONCE(mod->port, nlh->nlmsg_pid);
		WRITE_ONCE(mod->handshake, 1);
		dump_recv_msg(uc);
		ack.owner = uc->owner;
		ack.msg_type = LOOPBACK_MSG;
		millet_sendto_user(NULL, &ack, &millet_sk);
		break;
	case MSG_TO_KERN:
		if (mod->recv_from)
			mod->recv_from(uc, sizeof(*uc));
		dump_recv_msg(uc);
		break;
	default:
		pr_err("msg type is valid %d\n", uc->msg_type);
		break;
	}
}

/* ---- /proc/millet ---- */

static int millet_sock_show(struct seq_file *m, void *v)
{
	u64 suc[MILLET_TYPES_NUM], fail[MILLET_TYPES_NUM], rt[MILLET_TYPES_NUM];
	u64 tsuc = 0, tfail = 0, trt = 0;
	unsigned long flags;
	int i;

	for (i = SIG_TYPE; i < MILLET_TYPES_NUM; i++) {
		struct millet_mod *mod = &millet_sk.mod[i];

		spin_lock_irqsave(&mod->lock, flags);
		suc[i] = mod->send_suc;
		fail[i] = mod->send_fail;
		rt[i] = mod->runtime;
		spin_unlock_irqrestore(&mod->lock, flags);
	}

	seq_puts(m, "-----------------------------\n\n");
	for (i = SIG_TYPE; i < MILLET_TYPES_NUM; i++) {
		if (!suc[i] && !fail[i])
			continue;
		seq_printf(m, "name: %s mod id %d:\n", millet_sk.mod[i].name, i);
		seq_printf(m, "send suc: %llu\n", suc[i]);
		seq_printf(m, "send fail: %llu\n", fail[i]);
		seq_printf(m, "runtime: %llu us\n\n", rt[i]);
		tsuc += suc[i];
		tfail += fail[i];
		trt += rt[i];
	}
	seq_puts(m, "---------total info--------------------\n");
	seq_printf(m, "send_suc: %llu\n", tsuc);
	seq_printf(m, "send_fail: %llu\n", tfail);
	seq_printf(m, "runtime: %llu us\n", trt);

	seq_puts(m, "core_uid_rec:\n");
	core_uid_hash_show(m, ",");
	seq_puts(m, "\nuid_rec:\n");
	uid_rec_hash_show(m, "\t");
	seq_puts(m, "\ncore_black_pid_rec:\n");
	for (i = 0; i < ARRAY_SIZE(core_black_pid_rec); i++)
		if (atomic_read(&core_black_pid_rec[i]))
			seq_printf(m, "%d,", atomic_read(&core_black_pid_rec[i]));
	seq_puts(m, "\ncore_black_uid_rec:\n");
	for (i = 0; i < ARRAY_SIZE(core_black_uid_rec); i++)
		if (atomic_read(&core_black_uid_rec[i]))
			seq_printf(m, "%d,", atomic_read(&core_black_uid_rec[i]));
	seq_puts(m, "\n");
	return 0;
}

static int millet_stat_open(struct inode *inode, struct file *file)
{
	return single_open(file, millet_sock_show, NULL);
}

/* Writing "1" resets the per-module counters. */
static ssize_t millet_stat_write(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	char c[2] = {};
	unsigned long flags;
	long val;
	int i;

	if (get_user(c[0], buf))
		return count;
	val = simple_strtol(c, NULL, 10);
	pr_info("input value number: %d\n", (int)val);
	if (val == 1) {
		for (i = SIG_TYPE; i < MILLET_TYPES_NUM; i++) {
			struct millet_mod *mod = &millet_sk.mod[i];

			spin_lock_irqsave(&mod->lock, flags);
			mod->send_suc = 0;
			mod->send_fail = 0;
			mod->runtime = 0;
			spin_unlock_irqrestore(&mod->lock, flags);
		}
		pr_warn("stat reset now\n");
	}
	return count;
}

static int millet_version_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", millet_v);
	return 0;
}

static int millet_version_open(struct inode *inode, struct file *file)
{
	return single_open(file, millet_version_show, NULL);
}

static const struct proc_ops millet_stat_fops = {
	.proc_open = millet_stat_open,
	.proc_read = seq_read,
	.proc_write = millet_stat_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops millet_version_fops = {
	.proc_open = millet_version_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int __init millet_init(void)
{
	struct netlink_kernel_cfg cfg = {
		.input = recv_handler,
	};

	pr_err("enter millet_init func!\n");
	millet_sk.sock = netlink_kernel_create(&init_net, NETLINK_MILLET, &cfg);
	if (!millet_sk.sock) {
		pr_err("%s: create socket error!\n", __func__);
		return -1;
	}

	/* The stock init logs proc failures and still succeeds. */
	millet_rootdir = proc_mkdir("millet", NULL);
	if (!millet_rootdir) {
		pr_err("create /proc/millet failed\n");
		return 0;
	}
	if (!proc_create("millet_stat", 0644, millet_rootdir, &millet_stat_fops))
		pr_err("create millet stat failed\n");
	if (!proc_create("version", 0644, millet_rootdir, &millet_version_fops))
		pr_err("create millet version failed\n");
	return 0;
}

static void __exit millet_exit(void)
{
	if (millet_rootdir) {
		remove_proc_entry("millet_stat", millet_rootdir);
		remove_proc_entry("version", millet_rootdir);
		remove_proc_entry("millet", NULL);
	}
	if (millet_sk.sock) {
		netlink_kernel_release(millet_sk.sock);
		millet_sk.sock = NULL;
	}
	uid_rec_hash_clear();
	core_uid_hash_clear();
}

module_init(millet_init);
module_exit(millet_exit);
MODULE_DESCRIPTION("Millet smart frozen control subsystem");
MODULE_LICENSE("GPL");
