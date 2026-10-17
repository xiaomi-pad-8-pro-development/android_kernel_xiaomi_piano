/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Interface of the MCA onewire battery authentication core, which keeps the
 * battery serial number, dates, cycle count and UI state of health in the
 * authentication chip of the battery pack.
 *
 * Written by sidharthify (Sidharth Sharma) <wednisegit@gmail.com>
 */
#ifndef _MCA_HARDWARE_ONEWIRE_AUTH_H_
#define _MCA_HARDWARE_ONEWIRE_AUTH_H_

#include <linux/errno.h>
#include <linux/types.h>

#define ONEWIRE_UI_SOH_LEN	11

#if IS_ENABLED(CONFIG_MCA_ONEWIRE_AUTH)
void onewire_check_chip_ok(bool *ok);
void onewire_check_auth_pass(bool *pass);
int onewire_get_batt_sn(u8 *buf);
int onewire_get_manufacturing_date(u8 *buf);
int onewire_get_first_usage_date(u8 *buf);
int onewire_set_first_usage_date(u8 *buf);
int onewire_get_batt_cycle_count(int *count);
/* the chip counts the cycles: each call adds one */
int onewire_set_batt_cycle_count(void);
int onewire_reset_batt_cycle_count(void);
int onewire_get_batt_ui_soh(u8 *buf);
int onewire_set_batt_ui_soh(u8 *buf);
int onewire_get_shutdown_uisoc(int *uisoc);
int onewire_set_shutdown_uisoc(int uisoc);
#else
static inline void onewire_check_chip_ok(bool *ok) { *ok = false; }
static inline void onewire_check_auth_pass(bool *pass) { *pass = false; }
static inline int onewire_get_batt_sn(u8 *buf) { return -EOPNOTSUPP; }
static inline int onewire_get_manufacturing_date(u8 *buf) { return -EOPNOTSUPP; }
static inline int onewire_get_first_usage_date(u8 *buf) { return -EOPNOTSUPP; }
static inline int onewire_set_first_usage_date(u8 *buf) { return -EOPNOTSUPP; }
static inline int onewire_get_batt_cycle_count(int *count) { return -EOPNOTSUPP; }
static inline int onewire_set_batt_cycle_count(void) { return -EOPNOTSUPP; }
static inline int onewire_reset_batt_cycle_count(void) { return -EOPNOTSUPP; }
static inline int onewire_get_batt_ui_soh(u8 *buf) { return -EOPNOTSUPP; }
static inline int onewire_set_batt_ui_soh(u8 *buf) { return -EOPNOTSUPP; }
static inline int onewire_get_shutdown_uisoc(int *uisoc) { return -EOPNOTSUPP; }
static inline int onewire_set_shutdown_uisoc(int uisoc) { return -EOPNOTSUPP; }
#endif

#endif /* _MCA_HARDWARE_ONEWIRE_AUTH_H_ */
