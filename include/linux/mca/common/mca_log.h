/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Minimal stand-in for the proprietary Xiaomi MCA logging header.
 *
 * The original lived in a vendor proprietary tree that is not available
 * in this source tree. Consumers only rely on the mca_log_* helpers, so
 * map them onto the standard kernel printk helpers. Drivers continue to
 * build and behave as if the MCA logging core were present.
 */
#ifndef _LINUX_MCA_COMMON_MCA_LOG_H
#define _LINUX_MCA_COMMON_MCA_LOG_H

#include <linux/printk.h>

/*
 * Each consumer is expected to define MCA_LOG_TAG before use. We do NOT
 * provide a default here so that a driver's own (guarded) definition wins.
 */

#define mca_log_info(fmt, ...) \
	pr_info("[%s] " fmt, MCA_LOG_TAG, ##__VA_ARGS__)
#define mca_log_err(fmt, ...) \
	pr_err("[%s] " fmt, MCA_LOG_TAG, ##__VA_ARGS__)
#define mca_log_warn(fmt, ...) \
	pr_warn("[%s] " fmt, MCA_LOG_TAG, ##__VA_ARGS__)
#define mca_log_dbg(fmt, ...) \
	pr_debug("[%s] " fmt, MCA_LOG_TAG, ##__VA_ARGS__)

#endif /* _LINUX_MCA_COMMON_MCA_LOG_H */
