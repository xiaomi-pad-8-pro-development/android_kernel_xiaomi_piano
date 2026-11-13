/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Millet ("smart frozen control subsystem") core interface, rebuilt for
 * 6.12 from static analysis of the stock HyperOS OS4.0.5 millet_core.ko
 * and its users. Offsets in the comments are the stock 6.6 ones; the
 * hook prototypes match the kCFI type hashes in the stock binaries.
 */
#ifndef _MILLET_H
#define _MILLET_H

#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/time64.h>
#include <linux/types.h>

/* Module types, from the stock name table at .rodata+0x9f8. */
enum MILLET_TYPE {
	O_TYPE = 0,	/* "NULL" */
	SIG_TYPE,	/* "SIG" */
	BINDER_TYPE,	/* "BINDER" */
	BINDER_ST_TYPE,	/* "BINDER_STAT" */
	MEM_TYPE,	/* "MEM" */
	PKG_TYPE,	/* "PKG" */
	HANDSHK_TYPE,	/* "HANDSHK" */
	MILLET_TYPES_NUM,
};

/* msg_type values: the core accepts 1..3 when sending. */
enum MILLET_MSG_TYPE {
	MSG_NONE = 0,
	LOOPBACK_MSG = 1,	/* handshake reply */
	MSG_TO_USER = 2,
	MSG_TO_KERN = 3,
};

#define NETLINK_MILLET		31
#define MILLET_KERNEL_PORT	0x12341234UL
#define MILLET_USER_PORT	0xabcddcbaUL
#define MILLET_PRIV_UID_MAX	1000	/* recv_handler rejects uid > 1000 */

/* Kernel -> user message, 0x90 bytes. */
struct millet_data {
	u32 owner;			/* 0x00 sending module type */
	u32 monitor;			/* 0x04 module type it was routed to */
	u32 msg_type;			/* 0x08 */
	u32 reserved;			/* 0x0c */
	u64 src_port;			/* 0x10 MILLET_KERNEL_PORT */
	u64 dst_port;			/* 0x18 MILLET_USER_PORT */
	u32 uid;			/* 0x20 */
	u32 reserved2;			/* 0x24 */
	struct timespec64 tm;		/* 0x28 set by the core */
	union {				/* 0x38 module specific */
		u64 sig;
		struct {		/* SIG_TYPE, from millet_sig.ko */
			u64 sig;		/* 0x38 */
			u64 reserved[5];
			u64 sig_mask;		/* 0x68 1 << sig */
			struct task_struct *task;	/* 0x70 */
			u32 pid;		/* 0x78 tgid */
		} sig_info;
		u8 data[0x58];
	} mod;
};
static_assert(sizeof(struct millet_data) == 0x90);
static_assert(offsetof(struct millet_data, mod.sig_info.pid) == 0x78);

/* User -> kernel message, 0x50 bytes. */
struct millet_userconf {
	u32 owner;			/* 0x00 module type */
	u32 msg_type;			/* 0x04 LOOPBACK_MSG or MSG_TO_KERN */
	u64 src_port;			/* 0x08 MILLET_USER_PORT */
	u64 dst_port;			/* 0x10 MILLET_KERNEL_PORT */
	u8 data[0x38];			/* 0x18 module specific */
};
static_assert(sizeof(struct millet_userconf) == 0x50);

struct millet_sock;
typedef void (*recv_from_user)(void *data, unsigned int len);
typedef int (*send_to_user)(struct task_struct *tsk, struct millet_data *data,
			    struct millet_sock *sk);
typedef void (*init_millet)(struct millet_sock *sk);

/* One per module type, 0x78 bytes on 6.6. */
struct millet_mod {
	u32 id;				/* 0x00 */
	u32 monitor;			/* 0x04 route messages to this type */
	u32 port;			/* 0x08 netlink portid after handshake */
	u32 handshake;			/* 0x0c */
	recv_from_user recv_from;	/* 0x10 */
	send_to_user send_to;		/* 0x18 */
	init_millet init;		/* 0x20 */
	spinlock_t lock;		/* 0x28 protects the counters */
	u64 send_suc;			/* 0x30 */
	u64 send_fail;			/* 0x38 */
	u64 runtime;			/* 0x40 us */
	char name[32];			/* 0x48 */
	void *priv;			/* 0x68 module data (millet_sig) */
	u32 enable;			/* 0x70 */
};

static_assert(sizeof(struct millet_mod) == 0x78);

struct millet_sock {
	struct sock *sock;
	struct millet_mod mod[MILLET_TYPES_NUM];
};

extern unsigned int millet_debug;
extern unsigned int frozen_uid_min;
extern unsigned long binder_warn_ahead_space;
extern bool millet_binder_switch;
extern int freezebinder_enable;
extern atomic_t core_black_pid_rec[10];
extern atomic_t core_black_uid_rec[10];

int register_millet_hook(int type, recv_from_user recv_from,
			 send_to_user send_to, init_millet init);
int unregister_millet_hook(int type);
int init_millet_subsystem(int type);
int millet_sendmsg(int type, struct task_struct *tsk, struct millet_data *data);
int millet_sendto_user(struct task_struct *tsk, struct millet_data *data,
		       struct millet_sock *sk);

int core_uid_hash_add(uid_t uid);
int core_uid_hash_del(uid_t uid);
bool core_uid_hash_find(uid_t uid);
void core_uid_hash_clear(void);
void core_uid_hash_show(struct seq_file *m, const char *sep);
int uid_rec_hash_add(uid_t uid);
int uid_rec_hash_del(uid_t uid);
bool uid_rec_hash_find(uid_t uid);
bool uid_rec_hash_find_and_clear(uid_t uid);
void uid_rec_hash_clear(void);
void uid_rec_hash_show(struct seq_file *m, const char *sep);

#endif
