// SPDX-License-Identifier: GPL-2.0
/*
 * mi_wq_dynamic_priority - 6.12 port, reconstructed from the stock
 * HyperOS 4 (6.6.57) mi_wq_dynamic_priority.ko of the Xiaomi Pad 8 Pro
 * (vendor_boot ramdisk, OS4.0.0.44.XPYCNXM). No Xiaomi source exists.
 *
 * What it does (as observed in the stock binary):
 *  - kprobes alloc_workqueue() and forces the flags of the "kverityd" and
 *    "loop%d" workqueues to carry MI_WQ_DYNPRIO (bit 14);
 *  - allocates its own unbound high-priority "mikblockd" workqueue;
 *  - for MI_WQ_DYNPRIO workqueues, android_rvh_alloc_and_link_pwqs installs
 *    unbound attrs with the magic nice value -1000, and
 *    android_rvh_create_worker turns workers of such pools into SCHED_FIFO
 *    (low) with nice -20;
 *  - android_vh_blk_mq_delay_run_hw_queue / _kick_requeue_list: an
 *    immediate (delay == 0) blk-mq run or requeue kick issued by a
 *    high-QoS or RT task, outside interrupt context, is queued on
 *    mikblockd instead of kblockd;
 *  - /proc/mikblockd_enable (0664) toggles the blk-mq redirection.
 */
#define pr_fmt(fmt) "mi_wq_dynprio: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/workqueue.h>
#include <linux/cpu.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/sched/prio.h>
#include <linux/blk-mq.h>
#include <linux/blkdev.h>
#include <trace/hooks/blk.h>
#include <trace/hooks/wqlockup.h>

/* Xiaomi private workqueue flag (unused bit in 6.6 and 6.12 enum wq_flags). */
#define MI_WQ_DYNPRIO		(1U << 14)
#define MI_WQ_DYNPRIO_NICE	(-1000)

#define KVERITYD_FLAGS	(MI_WQ_DYNPRIO | WQ_HIGHPRI | WQ_MEM_RECLAIM | WQ_UNBOUND)	/* 0x401a */
#define LOOP_FLAGS	(MI_WQ_DYNPRIO | WQ_FREEZABLE | WQ_UNBOUND)			/* 0x4006 */

#if IS_ENABLED(CONFIG_MI_WQ_DYNPRIO_XR_QI)
/* Exported by Xiaomi's xr_qi.ko (stock only); 1 and 2 are the high QoS levels. */
extern int get_task_qos_lvl(struct task_struct *p);
#else
static inline int get_task_qos_lvl(struct task_struct *p) { return 0; }
#endif

/*
 * struct workqueue_struct is private to kernel/workqueue.c. The stock 6.6
 * module read wq->flags at a hard-coded offset (0x100). This mirror copies
 * the 6.12.81 GKI layout (common/kernel/workqueue.c) up to @flags using only
 * public types. Re-check it whenever the GKI workqueue_struct changes.
 */
struct mi_wq_mirror {
	struct list_head	pwqs;
	struct list_head	list;
	struct mutex		mutex;
	int			work_color;
	int			flush_color;
	atomic_t		nr_pwqs_to_flush;
	void			*first_flusher;
	struct list_head	flusher_queue;
	struct list_head	flusher_overflow;
	struct list_head	maydays;
	void			*rescuer;
	int			nr_drainers;
	int			max_active;
	int			min_active;
	int			saved_max_active;
	int			saved_min_active;
	struct workqueue_attrs	*unbound_attrs;
	void __rcu		*dfl_pwq;
#ifdef CONFIG_SYSFS
	void			*wq_dev;
#endif
#ifdef CONFIG_LOCKDEP
	char			*lock_name;
	struct lock_class_key	key;
	struct lockdep_map	__lockdep_map;
	struct lockdep_map	*lockdep_map;
#endif
	char			name[32];	/* WQ_NAME_LEN (private enum) */
	struct rcu_head		rcu;
	unsigned int		flags ____cacheline_aligned;
};

static inline unsigned int wq_flags(struct workqueue_struct *wq)
{
	return ((struct mi_wq_mirror *)wq)->flags;
}

static bool mikblockd_enabled = true;
static struct workqueue_struct *mi_kblockd_workqueue;

static bool mi_should_redirect(void)
{
	int qos;

	/* preempt_count() & (NMI|HARDIRQ|SOFTIRQ_OFFSET) == 0xff0100 */
	if (in_nmi() || in_hardirq() || in_serving_softirq())
		return false;

	qos = get_task_qos_lvl(current);
	if (qos == 1 || qos == 2)
		return true;

	return current->prio < MAX_RT_PRIO;	/* stock: prio <= 99 */
}

static void sfi_delay_run_hw_queue(void *unused, int cpu,
				   struct blk_mq_hw_ctx *hctx,
				   unsigned long delay, bool *skip)
{
	bool queued = false;

	if (!delay && READ_ONCE(mikblockd_enabled) && mi_kblockd_workqueue &&
	    mi_should_redirect())
		queued = queue_work(mi_kblockd_workqueue, &hctx->run_work.work);

	*skip = queued;
}

static void sfi_kick_requeue_list(void *unused, struct request_queue *q,
				  unsigned long delay, bool *skip)
{
	bool queued = false;

	if (!delay && READ_ONCE(mikblockd_enabled) && mi_kblockd_workqueue &&
	    mi_should_redirect())
		queued = queue_work(mi_kblockd_workqueue, &q->requeue_work.work);

	*skip = queued;
}

static void sfi_create_worker(void *unused, struct task_struct *p,
			      struct workqueue_attrs *attrs)
{
	if (attrs->nice != MI_WQ_DYNPRIO_NICE)
		return;

	sched_set_fifo_low(p);
	set_user_nice(p, MIN_NICE);
}

/*
 * 6.12 calls this hook from alloc_and_link_pwqs() with wq_pool_mutex and
 * cpus_read_lock() already held (apply_wqattrs_lock()). The stock 6.6 probe
 * called apply_workqueue_attrs() + cpus_read_lock(); on 6.12 that deadlocks,
 * so use apply_workqueue_attrs_locked() and take no locks here.
 */
static void sfi_alloc_and_link_pwqs(void *unused, struct workqueue_struct *wq,
				    int *ret, bool *skip)
{
	struct workqueue_attrs *attrs;
	unsigned int flags;

	*skip = false;
	if (!wq)
		return;

	flags = wq_flags(wq);
	if (!(flags & MI_WQ_DYNPRIO) || !(flags & WQ_UNBOUND) ||
	    (flags & __WQ_ORDERED))
		return;

	attrs = alloc_workqueue_attrs();
	if (!attrs) {
		pr_err("sfi_v3: alloc_workqueue_attrs failed.\n");
		return;
	}

	attrs->nice = MI_WQ_DYNPRIO_NICE;
	*ret = apply_workqueue_attrs_locked(wq, attrs);
	*skip = true;
	free_workqueue_attrs(attrs);
}

static int handler_alloc_workqueue_pre(struct kprobe *p, struct pt_regs *regs)
{
	const char *fmt = (const char *)regs->regs[0];
	unsigned int flags = (unsigned int)regs->regs[1];

	if (!fmt)
		return 0;

	if (!strncmp(fmt, "kverityd", 8)) {
		if (flags != KVERITYD_FLAGS)
			regs->regs[1] = KVERITYD_FLAGS;
	} else if (!strncmp(fmt, "loop", 4)) {
		if (flags != LOOP_FLAGS)
			regs->regs[1] = LOOP_FLAGS;
	}

	return 0;
}

static struct kprobe mi_alloc_workqueue_kp = {
	.symbol_name	= "alloc_workqueue",
	.pre_handler	= handler_alloc_workqueue_pre,
};

static int mikblockd_enable_show(struct seq_file *m, void *v)
{
	if (!m) {
		pr_err("seq_file is Null.\n");
		return -EFAULT;
	}
	seq_printf(m, "%d\n", READ_ONCE(mikblockd_enabled));
	return 0;
}

static int mikblockd_enable_open(struct inode *inode, struct file *file)
{
	return single_open(file, mikblockd_enable_show, pde_data(inode));
}

static ssize_t mikblockd_enable_write(struct file *file, const char __user *ubuf,
				      size_t count, loff_t *ppos)
{
	char buf[64] = {0};
	size_t len = min_t(size_t, count, sizeof(buf) - 1);
	unsigned long long val;
	int ret;

	if (copy_from_user(buf, ubuf, len))
		return -EFAULT;
	buf[len] = '\0';

	ret = kstrtoull(buf, 10, &val);
	if (ret)
		return ret;

	if (val == 1) {
		WRITE_ONCE(mikblockd_enabled, true);
		pr_err("mikblockd_enabled is  true.\n");
	} else {
		WRITE_ONCE(mikblockd_enabled, false);
		pr_err("mikblockd_enabled is false.\n");
	}

	return count;
}

static const struct proc_ops mikblockd_enable_fops = {
	.proc_open	= mikblockd_enable_open,
	.proc_read	= seq_read,
	.proc_write	= mikblockd_enable_write,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

static void mi_unregister_blk_hooks(void)
{
	unregister_trace_android_vh_blk_mq_delay_run_hw_queue(sfi_delay_run_hw_queue, NULL);
	unregister_trace_android_vh_blk_mq_kick_requeue_list(sfi_kick_requeue_list, NULL);
}

static int __init mi_wq_dynamic_priority_init(void)
{
	int ret;

	ret = register_trace_android_vh_blk_mq_delay_run_hw_queue(sfi_delay_run_hw_queue, NULL);
	if (ret) {
		pr_err("register_trace_android_vh_blk_mq_delay_run_hw_queue failed! ret=%d\n", ret);
		goto fail_hooks;
	}
	ret = register_trace_android_vh_blk_mq_kick_requeue_list(sfi_kick_requeue_list, NULL);
	if (ret) {
		pr_err("register_trace_android_vh_blk_mq_kick_requeue_list failed! ret=%d\n", ret);
		unregister_trace_android_vh_blk_mq_delay_run_hw_queue(sfi_delay_run_hw_queue, NULL);
		goto fail_hooks;
	}
	/* restricted hooks cannot be unregistered: module pins itself below */
	ret = register_trace_android_rvh_create_worker(sfi_create_worker, NULL);
	if (ret) {
		pr_err("register_trace_android_rvh_create_worker failed! ret=%d\n", ret);
		goto fail_blk;
	}
	ret = register_trace_android_rvh_alloc_and_link_pwqs(sfi_alloc_and_link_pwqs, NULL);
	if (ret) {
		pr_err("register_trace_android_rvh_alloc_and_link_pwqs failed! ret=%d\n", ret);
		goto fail_blk;
	}

	if (!proc_create("mikblockd_enable", 0664, NULL, &mikblockd_enable_fops)) {
		mikblockd_enabled = false;
		pr_err("fail to create file node\n");
		goto fail_blk;
	}

	mi_kblockd_workqueue = alloc_workqueue("mikblockd", KVERITYD_FLAGS, 0);
	if (!mi_kblockd_workqueue) {
		mikblockd_enabled = false;
		pr_err("alloc  mi_kblockd_workqueue fail!\n");
		remove_proc_entry("mikblockd_enable", NULL);
		goto fail_blk;
	}

	ret = register_kprobe(&mi_alloc_workqueue_kp);
	if (ret < 0) {
		mikblockd_enabled = false;
		pr_err("register_kprobe alloc_workqueue failed, returned %d\n", ret);
		remove_proc_entry("mikblockd_enable", NULL);
		mi_unregister_blk_hooks();
		destroy_workqueue(mi_kblockd_workqueue);
		mi_kblockd_workqueue = NULL;
		return 0;	/* stock returns 0 here too */
	}

	/* rvh probes stay installed forever: never allow unload */
	__module_get(THIS_MODULE);
	return 0;

fail_blk:
	mi_unregister_blk_hooks();
fail_hooks:
	mikblockd_enabled = false;
	pr_err("register_mi_wq_dynprio_vendor_hooks failed.\n");
	return 0;	/* stock always returns 0 */
}

static void __exit mi_wq_dynamic_priority_exit(void)
{
	WRITE_ONCE(mikblockd_enabled, false);
	mi_unregister_blk_hooks();
	unregister_kprobe(&mi_alloc_workqueue_kp);
	synchronize_rcu();
	remove_proc_entry("mikblockd_enable", NULL);
	if (mi_kblockd_workqueue)
		destroy_workqueue(mi_kblockd_workqueue);
}

module_init(mi_wq_dynamic_priority_init);
module_exit(mi_wq_dynamic_priority_exit);
MODULE_LICENSE("GPL");
