// SPDX-License-Identifier: GPL-2.0
/*
 * millet_oem_cgroup: permission check for the cgroup v1 freezer.
 *
 * Rebuilt for 6.12 from the disassembly of the stock HyperOS OS4.0.5
 * module vendor_boot/lib/modules/millet_oem_cgroup.ko (every instruction
 * is accounted for; cred offsets named from the stock 6.6 BTF). Not yet
 * run on a device.
 *
 * It points freezer_cgrp_subsys.can_attach/cancel_attach at wrappers
 * that call a local hook table, and puts millet_can_attach in it: a task
 * that is neither euid 1000 (system) nor CAP_SYS_ADMIN may only move
 * itself into a freezer cgroup.
 */
#include <linux/capability.h>
#include <linux/cgroup.h>
#include <linux/cred.h>
#include <linux/module.h>
#include <linux/sched.h>

#undef pr_fmt
#define pr_fmt(fmt) "millet-oem_cgroup: " fmt

static struct cgroup_subsys oem_cgroup_hook;

static int millet_can_attach(struct cgroup_taskset *tset)
{
	const struct cred *cred = current_cred();
	struct cgroup_subsys_state *css;
	struct task_struct *task;

	cgroup_taskset_for_each(task, css, tset) {
		if (task == current)
			continue;
		if (__kuid_val(cred->euid) == 1000)
			continue;
		if (capable(CAP_SYS_ADMIN))
			continue;
		pr_err("Permission problem\n");
		/* The stock module returns 1, not -EPERM; cgroup core treats it as failure. */
		return 1;
	}
	return 0;
}

static int freezer_can_attach(struct cgroup_taskset *tset)
{
	if (oem_cgroup_hook.can_attach)
		return oem_cgroup_hook.can_attach(tset);
	return 0;
}

static void freezer_cancel_attach(struct cgroup_taskset *tset)
{
	if (oem_cgroup_hook.cancel_attach)
		oem_cgroup_hook.cancel_attach(tset);
}

static int __init oem_cgrp_init(void)
{
	pr_err("enter oem_cgrp_init func!\n");
	oem_cgroup_hook.can_attach = millet_can_attach;
	freezer_cgrp_subsys.can_attach = freezer_can_attach;
	freezer_cgrp_subsys.cancel_attach = freezer_cancel_attach;
	return 0;
}

/* No exit: the freezer subsystem keeps pointers into this module. */
module_init(oem_cgrp_init);
MODULE_LICENSE("GPL");
