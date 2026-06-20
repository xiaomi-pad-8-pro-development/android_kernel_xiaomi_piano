#ifndef _MCA_STRATEGY_STRATEGY_CLASS_H_
#define _MCA_STRATEGY_STRATEGY_CLASS_H_

#include <linux/types.h>

enum mca_strategy_func_type {
	STRATEGY_FUNC_TYPE_BUCK_CHARGE = 0,
	STRATEGY_FUNC_TYPE_QUICK_CHARGE,
	STRATEGY_FUNC_TYPE_JEITA,
	STRATEGY_FUNC_TYPE_THERMAL,
	STRATEGY_FUNC_TYPE_FG,
	STRATEGY_FUNC_TYPE_SMARTCHG,
	STRATEGY_FUNC_TYPE_BMD,
	STRATEGY_FUNC_TYPE_CONNECTOR_ANTIBURN,
	STRATEGY_FUNC_TYPE_BASIC_WIRELESS,
	STRATEGY_FUNC_TYPE_QUICK_WIRELESS,
	STRATEGY_FUNC_TYPE_REV_WIRELESS,
	STRATEGY_FUNC_TYPE_MAX,
};

enum strategy_status_type {
	STRATEGY_STATUS_TYPE_ONLINE = 0,
	STRATEGY_STATUS_TYPE_CHARGING,
	STRATEGY_STATUS_TYPE_ENABLE,
	STRATEGY_STATUS_TYPE_IBUS,
	STRATEGY_STATUS_TYPE_VBUS,
	STRATEGY_STATUS_TYPE_MODE,
	STRATEGY_STATUS_TYPE_POWER_MAX,
	STRATEGY_STATUS_TYPE_QC_ENABLE,
	STRATEGY_STATUS_TYPE_QC_IBAT_MAX,
	STRATEGY_STATUS_TYPE_QC_START_FLAG,
	STRATEGY_STATUS_TYPE_QC_TYPE,
	STRATEGY_STATUS_TYPE_JEITA_FFC_ITERM,
	STRATEGY_STATUS_TYPE_JEITA_FFC_SIZE,
	STRATEGY_STATUS_TYPE_JEITA_NORMAL_VTERM,
	STRATEGY_STATUS_TYPE_REV_TEST,
	STRATEGY_STATUS_TYPE_WLS_MAGNET_LIMIT,
};

typedef int (*mca_strategy_func)(int event, int value, void *data);
typedef int (*mca_strategy_get_status)(int status, void *value, void *data);
typedef int (*mca_strategy_set_config)(int config, int value, void *data);

int mca_strategy_func_get_status(int type, int status, void *value);
int mca_strategy_func_process(unsigned int type, int event, int value);
int mca_strategy_func_set_config(int type, int config, int value);
int mca_strategy_ops_register(unsigned int type, mca_strategy_func func,
			      mca_strategy_get_status get_func,
			      mca_strategy_set_config set_config, void *data);

#endif /* _MCA_STRATEGY_STRATEGY_CLASS_H_ */
