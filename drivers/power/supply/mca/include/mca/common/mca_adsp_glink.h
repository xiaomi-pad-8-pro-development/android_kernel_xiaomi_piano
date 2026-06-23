#ifndef _MCA_COMMON_MCA_ADSP_GLINK_H_
#define _MCA_COMMON_MCA_ADSP_GLINK_H_

#include <linux/types.h>

enum mca_adsp_prop_id {
	ADSP_PROP_ID_DC_INPUT_CURR_LIMIT = 0,
};

int mca_adsp_glink_write_prop(int prop_id, void *value, size_t size);

#endif /* _MCA_COMMON_MCA_ADSP_GLINK_H_ */
