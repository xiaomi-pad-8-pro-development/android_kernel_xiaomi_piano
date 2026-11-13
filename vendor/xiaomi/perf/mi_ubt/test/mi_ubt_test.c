// SPDX-License-Identifier: GPL-2.0+
/*
 * mi_ubt_test will show user that how to use mi_ubt.
 *
 * Copyright (C) 2024 Xiaomi Ltd.
 *
 * 6.12: unchanged from the alxp 6.6 tree (drivers/xiaomi/mi_ubt/test);
 * every API it uses (proc_create_single, remove_proc_entry, mi_ubt's
 * dump_user_backtrace) exists on GKI 6.12.81.
 */

#define pr_fmt(fmt) "mi_ubt_test: "fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/seq_file.h>

#if IS_ENABLED(CONFIG_MI_UBT)
#include <mi_ubt/mi_ubt.h>
#endif

/*
 * cat /proc/mi_ubt_test
 *
 * [63327.625007] mi_ubt: [cat, 18471, R, 63327597484034] User backtrace:
 * [63327.632253] mi_ubt: #0 <0x7f44bbad78> in /apex/com.android.runtime/lib64/bionic/libc.so[7f44b40000-7f44bd0000]
 * [63327.644429] mi_ubt: #1 <0x61652211d4> in /system/bin/toybox[6165210000-616525f000]
 */
static int ubt_show(struct seq_file *m, void *v)
{
#if IS_ENABLED(CONFIG_MI_UBT)
	dump_user_backtrace(NULL);
#endif
	return 0;
}

static int __init mi_ubt_test_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create_single("mi_ubt_test", S_IRUSR, NULL, ubt_show);
	if (!entry) {
		remove_proc_entry("mi_ubt_test", NULL);
		return -ENOMEM;
	}

	pr_err("%s\n", __func__);

	return 0;
}

static void __exit mi_ubt_test_exit(void)
{
	remove_proc_entry("mi_ubt_test", NULL);
	pr_err("%s\n", __func__);
}

module_init(mi_ubt_test_init);
module_exit(mi_ubt_test_exit);
MODULE_DESCRIPTION("Register Mi Ubt Test driver");
MODULE_LICENSE("GPL v2");
