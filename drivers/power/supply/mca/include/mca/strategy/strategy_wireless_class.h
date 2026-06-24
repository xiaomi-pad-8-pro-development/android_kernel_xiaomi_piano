#ifndef _MCA_STRATEGY_STRATEGY_WIRELESS_CLASS_H_
#define _MCA_STRATEGY_STRATEGY_WIRELESS_CLASS_H_

#include <linux/types.h>

struct wls_adapter_power_cap {
	int max_fcc;
	int max_power;
};

int strategy_class_wireless_ops_get_adapter_power(
	struct wls_adapter_power_cap *adapter_power);
int strategy_class_wireless_ops_set_parallel_charge(bool parallel_charge_flag);
int strategy_class_wireless_ops_get_wls_type(int *wls_type);
int strategy_class_wireless_ops_get_adapter_charger_mode(int *cp_charger_mode);
void strategy_class_wireless_op_get_rx_iout_limit(int *rx_iout_limit_ma);

int mca_wireless_rev_set_wired_chg_ok(bool ok);
int mca_wireless_rev_get_reverse_chg_state(int *state);
int mca_wireless_rev_set_user_reverse_chg(bool user_reverse_chg);
int mca_wireless_rev_update_fw_version(int cmd);

#endif /* _MCA_STRATEGY_STRATEGY_WIRELESS_CLASS_H_ */
