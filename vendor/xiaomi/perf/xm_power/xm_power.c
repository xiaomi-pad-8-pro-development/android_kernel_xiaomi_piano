// SPDX-License-Identifier: GPL-2.0
/*
 * xm_power: Xiaomi power detector (6.12 source port of stock 6.6 xm_power.ko).
 *
 * Periodic delayed work that logs active wakeup sources (or, when none is
 * active, the two most recently active ones) every wakelock_print_period_s
 * seconds, and every task_print_period_s seconds the top 5 CPU-using tasks
 * over the last interval. Tunables under /sys/xm_power/.
 */
#define pr_fmt(fmt) "PowerDet:" fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/sched/cputime.h>
#include <linux/hashtable.h>
#include <linux/slab.h>
#include <linux/rtmutex.h>
#include <linux/spinlock.h>
#include <linux/math64.h>
#include <linux/string.h>

#define TOP_N		5
#define HASH_BITS_XM	10	/* 1024 buckets = stock 8192-byte hash_table */

int wakelock_print_period_s = 10;
int task_print_period_s = 20;
static ktime_t PrevKtime;
static ktime_t last_task_dump;	/* stock: dump_cpu_info.last */
static bool reset_write_flag;
static struct kobject *xm_power_kobj;
static DEFINE_RT_MUTEX(pid_lock);
static DEFINE_SPINLOCK(hash_lock);
static DEFINE_HASHTABLE(hash_table, HASH_BITS_XM);
static struct delayed_work power_info_dump;

struct pid_entry {
	struct hlist_node node;
	pid_t pid;
	u64 utime;
	u64 stime;
	u64 seen;		/* generation, used to prune exited tasks */
};

struct top_task {
	u64 delta, dutime, dstime;
	pid_t pid, ppid;
	char comm[TASK_COMM_LEN];
	char pcomm[TASK_COMM_LEN];
};

static u64 generation;

static void print_android_time(const char *tag)
{
	struct timespec64 ts;
	struct rtc_time tm;

	ktime_get_real_ts64(&ts);
	rtc_time64_to_tm(ts.tv_sec - sys_tz.tz_minuteswest * 60, &tm);
	pr_info("%s:%02d-%02d-%02d %02d:%02d:%02d.%03d(android time)\n", tag,
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
		tm.tm_min, tm.tm_sec, (int)(ts.tv_nsec / NSEC_PER_MSEC));
}

static void dump_wakeup_sources(void)
{
	struct wakeup_source *ws, *last = NULL, *second = NULL;
	bool any_active = false;
	int idx;

	idx = wakeup_sources_read_lock();
	for_each_wakeup_source(ws) {
		if (ws->active) {
			pr_info("active wake lock: %s, active_since: %lld ms , active_count: %lu\n",
				ws->name,
				ktime_to_ms(ktime_sub(ktime_get(), ws->last_time)),
				ws->active_count);
			any_active = true;
		} else if (!any_active) {
			if (!last || ktime_after(ws->last_time, last->last_time)) {
				second = last;
				last = ws;
			}
		}
	}
	if (!any_active && last)
		pr_info("last active wakeup source: %s, last_time:%lld ms, active_count: %lu\n",
			last->name, ktime_to_ms(last->last_time), last->active_count);
	if (!any_active && second)
		pr_info("last sec active wakeup source: %s, last_time:%lld ms, active_count: %lu\n",
			second->name, ktime_to_ms(second->last_time), second->active_count);
	wakeup_sources_read_unlock(idx);
}

static struct pid_entry *pid_entry_get(pid_t pid)
{
	struct pid_entry *e;

	hash_for_each_possible(hash_table, e, node, pid)
		if (e->pid == pid)
			return e;
	e = kzalloc(sizeof(*e), GFP_ATOMIC);
	if (!e) {
		pr_err("%s: failed to find the pid_entry for pid %d\n", __func__, pid);
		return NULL;
	}
	e->pid = pid;
	hash_add(hash_table, &e->node, pid);
	return e;
}

static void top_insert(struct top_task *top, struct task_struct *p,
		       u64 du, u64 ds)
{
	u64 d = du + ds;
	int i, j;

	for (i = 0; i < TOP_N; i++)
		if (d > top[i].delta)
			break;
	if (i == TOP_N)
		return;
	for (j = TOP_N - 1; j > i; j--)
		top[j] = top[j - 1];
	top[i].delta = d;
	top[i].dutime = du;
	top[i].dstime = ds;
	top[i].pid = p->pid;
	strscpy(top[i].comm, p->comm, TASK_COMM_LEN);
	top[i].ppid = p->real_parent->pid;
	strscpy(top[i].pcomm, p->real_parent->comm, TASK_COMM_LEN);
}

static void dump_top5_task(void)
{
	struct top_task top[TOP_N];
	struct task_struct *g, *p;
	struct pid_entry *e;
	struct hlist_node *tmp;
	unsigned long flags;
	ktime_t now = ktime_get();
	u64 elapsed;
	int i, bkt;

	memset(top, 0, sizeof(top));
	rt_mutex_lock(&pid_lock);
	if (!PrevKtime) {
		PrevKtime = now;
		rt_mutex_unlock(&pid_lock);
		return;
	}
	elapsed = ktime_to_ns(ktime_sub(now, PrevKtime));
	generation++;

	rcu_read_lock();
	spin_lock_irqsave(&hash_lock, flags);
	for_each_process_thread(g, p) {
		u64 ut, st, du, ds;

		task_cputime_adjusted(p, &ut, &st);
		e = pid_entry_get(p->pid);
		if (!e)
			continue;
		if (e->seen && ut >= e->utime && st >= e->stime) {
			du = ut - e->utime;
			ds = st - e->stime;
		} else {
			du = ds = 0;	/* new task or pid reuse */
		}
		e->utime = ut;
		e->stime = st;
		e->seen = generation;
		top_insert(top, p, du, ds);
	}
	/* Port addition: stock never frees entries of exited tasks. */
	hash_for_each_safe(hash_table, bkt, tmp, e, node)
		if (e->seen != generation) {
			hash_del(&e->node);
			kfree(e);
		}
	spin_unlock_irqrestore(&hash_lock, flags);
	rcu_read_unlock();

	PrevKtime = ktime_get();
	rt_mutex_unlock(&pid_lock);

	if (!elapsed)
		return;
	print_android_time(__func__);
	for (i = 0; i < TOP_N; i++) {
		if (!top[i].pid)
			break;
		pr_info("Cpu usage:%d%% user + %d%% kernel, comm:%s, pid:%d, pcomm:%s, ppid:%d\n",
			(int)mul_u64_u64_div_u64(top[i].dutime, 100, elapsed),
			(int)mul_u64_u64_div_u64(top[i].dstime, 100, elapsed),
			top[i].comm, top[i].pid, top[i].pcomm, top[i].ppid);
	}
}

static void power_info_dump_func(struct work_struct *work)
{
	ktime_t now;

	pr_info("start \n");
	dump_wakeup_sources();

	now = ktime_get();
	if (ktime_to_ms(ktime_sub(now, last_task_dump)) >=
	    (s64)task_print_period_s * 1000) {
		last_task_dump = now;
		dump_top5_task();
	}

	queue_delayed_work(system_wq, &power_info_dump,
		round_jiffies_relative(msecs_to_jiffies(wakelock_print_period_s * 1000)));
}

static ssize_t reset_show(struct kobject *k, struct kobj_attribute *a, char *buf)
{
	return sprintf(buf, "%d\n", reset_write_flag);
}

static ssize_t reset_store(struct kobject *k, struct kobj_attribute *a,
			   const char *buf, size_t count)
{
	const char *nl = memchr(buf, '\n', count);
	size_t len = nl ? nl - buf : count;
	bool ok = len == 5 && !strncmp(buf, "reset", 5);

	if (ok) {
		wakelock_print_period_s = 10;
		task_print_period_s = 20;
	}
	reset_write_flag = ok;
	return count;
}

#define PERIOD_ATTR(name)						\
static ssize_t name##_show(struct kobject *k, struct kobj_attribute *a,	\
			   char *buf)					\
{									\
	return sprintf(buf, "%d\n", name##_s);				\
}									\
static ssize_t name##_store(struct kobject *k, struct kobj_attribute *a,\
			    const char *buf, size_t count)		\
{									\
	unsigned long long v;						\
	if (kstrtoull(buf, 10, &v))					\
		return -EINVAL;						\
	name##_s = (int)v;						\
	return count;							\
}									\
static struct kobj_attribute name##_attr = __ATTR_RW_MODE(name, 0644);

PERIOD_ATTR(task_print_period)
PERIOD_ATTR(wakelock_print_period)
static struct kobj_attribute reset_attr = __ATTR_RW_MODE(reset, 0644);

static struct attribute *g[] = {
	&reset_attr.attr,
	&task_print_period_attr.attr,
	&wakelock_print_period_attr.attr,
	NULL,
};
static const struct attribute_group attr_group = { .attrs = g };
static const struct attribute_group *attr_groups[] = { &attr_group, NULL };

static int __init xm_power_init(void)
{
	int ret;

	pr_info("start \n");
	INIT_DELAYED_WORK(&power_info_dump, power_info_dump_func);
	queue_delayed_work(system_wq, &power_info_dump,
		round_jiffies_relative(msecs_to_jiffies(wakelock_print_period_s * 1000)));

	xm_power_kobj = kobject_create_and_add("xm_power", NULL);
	if (!xm_power_kobj)
		return 0;	/* stock keeps the work running without sysfs */
	ret = sysfs_create_groups(xm_power_kobj, attr_groups);
	if (ret)
		pr_err("sysfs_create_groups failed %d\n", ret);
	return 0;
}

static void __exit xm_power_exit(void)
{
	struct pid_entry *e;
	struct hlist_node *tmp;
	int bkt;

	cancel_delayed_work_sync(&power_info_dump);
	if (xm_power_kobj) {
		sysfs_remove_groups(xm_power_kobj, attr_groups);
		kobject_put(xm_power_kobj);
	}
	hash_for_each_safe(hash_table, bkt, tmp, e, node) {
		hash_del(&e->node);
		kfree(e);
	}
	pr_info("powerdet_exit\n");
}

module_init(xm_power_init);
module_exit(xm_power_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("XiaoMI inc.");
MODULE_DESCRIPTION("The Power detector");
