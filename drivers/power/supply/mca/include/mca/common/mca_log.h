#ifndef _MCA_COMMON_MCA_LOG_H_
#define _MCA_COMMON_MCA_LOG_H_

#include <linux/compiler.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/limits.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>

enum mca_charge_log_id_ele {
	MCA_CHARGE_LOG_ID_BATTERY_INFO = 0,
	MCA_CHARGE_LOG_ID_BUSINESS_CHG,
	MCA_CHARGE_LOG_ID_CP_MASTER_IC,
	MCA_CHARGE_LOG_ID_FG_MASTER_IC,
	MCA_CHARGE_LOG_ID_FG_SLAVE_IC,
	MCA_CHARGE_LOG_ID_THERMAL,
	MCA_CHARGE_LOG_ID_USCP,
	MCA_CHARGE_LOG_ID_MAX,
};

struct mca_log_charge_log_ops {
	int (*dump_log_head)(void *data, char *buf, int size);
	int (*dump_log_context)(void *data, char *buf, int size);
};

__printf(1, 2) void __mca_log_err(const char *format, ...);
__printf(1, 2) void __mca_log_info(const char *format, ...);
__printf(1, 2) void __mca_log_debug(const char *format, ...);

void mca_log_charge_log_register(enum mca_charge_log_id_ele type,
				 struct mca_log_charge_log_ops *ops,
				 void *data);
int mca_log_get_charge_boot_mode(void);

#define mca_log_err(fmt, ...) \
	__mca_log_err("[%s]" fmt, MCA_LOG_TAG, ##__VA_ARGS__)
#define mca_log_info(fmt, ...) \
	__mca_log_info("[%s]" fmt, MCA_LOG_TAG, ##__VA_ARGS__)
#define mca_log_debug(fmt, ...) \
	__mca_log_debug("[%s]" fmt, MCA_LOG_TAG, ##__VA_ARGS__)

#endif /* _MCA_COMMON_MCA_LOG_H_ */
