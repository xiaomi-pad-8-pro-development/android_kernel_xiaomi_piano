/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Interface of perf_actuator, rebuilt for 6.12 from the stock HyperOS
 * OS4.0.5 module (vendor_boot perf_actuator.ko). The names and limits come
 * from that binary's symbols and code; the header itself is new.
 */
#ifndef _PERF_ACTUATOR_H
#define _PERF_ACTUATOR_H

#include <linux/types.h>

/* ioctl type byte checked by both devices (cmp w1, #0x78). */
#define PERF_ACTUATOR_MAGIC	'x'
/* Table sizes: register_perf_actuator rejects nr > 0x2a, register_xr_fbt nr > 0xb. */
#define PERF_ACTUATOR_MAX_CMD	43
#define XR_FBT_MAX_CMD		12

/* Handlers get the raw user pointer from the ioctl. */
typedef int (*perf_actuator_func)(void __user *uarg);

int register_perf_actuator(unsigned int cmd, perf_actuator_func func);
int unregister_perf_actuator(unsigned int cmd);
int register_xr_fbt(unsigned int cmd, perf_actuator_func func);
int unregister_xr_fbt(unsigned int cmd);

#endif
