/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mi_power exported API, reconstructed from the stock 6.6 mi_power.ko.
 * Field names are invented; offsets match the stock binary (arm64).
 */
#ifndef _MI_POWER_H
#define _MI_POWER_H

#include <linux/list.h>
#include <linux/types.h>

struct regmap;
struct device;

/* pm_register_wakeup_device(): resume() called from syscore resume */
struct pm_wakeup_device {
	struct list_head node;		/* 0x00 */
	const char *name;		/* 0x10, shown in wakeup_device_list */
	void *data;			/* 0x18 */
	void *reserved;			/* 0x20 */
	void (*resume)(void *data);	/* 0x28 */
};

/* pm_register_system_dbg_info(): type 0..4; 3/4 run at machine_suspend */
struct pm_system_dbg_info {
	struct list_head node;		/* 0x00 */
	int type;			/* 0x10 */
	void (*print)(void *data);	/* 0x18 */
	void *data;			/* 0x20 */
};

/* pm_register_system_event_recorder(): type 0..1 */
struct pm_system_event_recorder {
	struct list_head node;		/* 0x00 */
	void *reserved;			/* 0x10 */
	int type;			/* 0x18 */
	void *priv;			/* 0x20 */
	void (*record)(struct pm_system_event_recorder *r, void *data); /* 0x28 */
	void *reserved2;		/* 0x30 */
	u32 nr_records;			/* 0x38 */
	void *records;			/* 0x40, nr_records * 120 bytes */
};

int pm_register_wakeup_device(struct pm_wakeup_device *wd);
int pm_register_system_dbg_info(struct pm_system_dbg_info *info);
int pm_register_system_event_recorder(struct pm_system_event_recorder *r);
void pm_system_dbg_info_print(int type);
void pm_trigger_system_event_record(int type, void *data);
void mi_power_regmap_register(struct regmap *map);
void register_mi_cx_stats_get_ss_vote_info(int (*fn)(int n, u8 *votes));
void register_mi_llc_stats_get_ss_vote_info(int (*fn)(void *buf));
void register_mi_ddr_stats_get_ss_vote_info(int (*fn)(void *buf));
void soc_sleep_stats_dbg_register(void *soc_data);
void subsystem_sleep_stats_dbg_register(void *ss_data);
void subsystem_sleep_stats_dbg_unregister(void);
u8 mi_power_mode(void);
long mi_power_save_battery_cave(long capacity);
void log_irq_wakeup_reason(int irq);

#endif
