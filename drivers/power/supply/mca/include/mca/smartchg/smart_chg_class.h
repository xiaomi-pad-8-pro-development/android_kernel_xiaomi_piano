#ifndef _MCA_SMARTCHG_SMART_CHG_CLASS_H_
#define _MCA_SMARTCHG_SMART_CHG_CLASS_H_

#include <linux/types.h>

/*
 * BASP (battery aging safety/spec) blob layout, parsed by mca_smart_charge.c
 * from mmap'd memory via sizeof()/offsetof().
 *
 * WARNING: the open-source drop ships NO definition for these structs. The
 * layout below is INFERRED from field usage only and is NOT verified against
 * the real binary format. It lets the module build; the BAA/spec-override
 * runtime path is unreliable and should be treated as unsupported until the
 * true layout is obtained.
 */
struct smart_batt_spec_curve {
	int temp;
	int curr;
};

struct smart_batt_jeita_term_para {
	int temp;
	int term_curr;
};

struct smart_batt_spec {
	u32 step_size;
	struct smart_batt_spec_curve steps[];
};

struct smart_basp_header {
	u32 type;
	u32 total_len;
	u32 checksum;
	u32 jeita_ffc_term_size;
	u32 jeita_normal_term_size;
	u32 wired_ffc_size;
	u32 wired_normal_size;
	u32 wls_ffc_size;
	u32 wls_normal_size;
};

enum mca_smartchg_if_chg_type {
	MCA_SMARTCHG_IF_CHG_TYPE_BUCK = 0,
	MCA_SMARTCHG_IF_CHG_TYPE_QC,
	MCA_SMARTCHG_IF_CHG_TYPE_JEITA,
	MCA_SMARTCHG_IF_CHG_TYPE_THERMAL,
	MCA_SMARTCHG_IF_CHG_TYPE_WL_BUCK,
	MCA_SMARTCHG_IF_CHG_TYPE_WL_QC,
	MCA_SMARTCHG_IF_CHG_TYPE_END,
};

struct mca_smartchg_if_ops {
	int type;
	void *data;
	int (*set_delta_fv)(void *data, int val);
	int (*set_delta_ichg)(void *data, int val);
	int (*set_pwr_boost_sts)(void *data, bool en);
	int (*set_soc_limit_sts)(void *data, bool en);
	int (*set_wls_quiet_sts)(void *data, bool en);
	int (*set_wls_super_sts)(void *data, bool en);
	int (*update_baa_para)(void *data, void *buf, int size1, int size2);
};

int mca_smartchg_if_ops_register(struct mca_smartchg_if_ops *ops);
void mca_smartchg_set_scene(int scene);
int mca_smartchg_get_scene(void);
void mca_smartchg_set_board_temp(int board_temp);
int mca_smartchg_get_board_temp(void);
int mca_smartchg_is_extreme_cold_enabled(void);
int mca_smartchg_get_limit_soc(void);

#endif /* _MCA_SMARTCHG_SMART_CHG_CLASS_H_ */
