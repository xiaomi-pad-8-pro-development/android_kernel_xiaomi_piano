#ifndef _MCA_COMMON_MCA_EVENT_H_
#define _MCA_COMMON_MCA_EVENT_H_

#include <linux/notifier.h>

#define MCA_EVENT_NOTIFY_SIZE 64
#define MCA_EVENT_WR_BUF_SIZE 64

enum mca_event_type {
	MCA_EVENT_TYPE_BATTERY_INFO = 0,
	MCA_EVENT_TYPE_CHARGER_CONNECT,
	MCA_EVENT_TYPE_CHARGE_TYPE,
	MCA_EVENT_TYPE_CP_INFO,
	MCA_EVENT_TYPE_HW_INFO,
	MCA_EVENT_TYPE_PANEL,
	MCA_EVENT_TYPE_SUBPMIC_INFO,
	MCA_EVENT_TYPE_THERMAL_TEMP,
	MCA_EVENT_TYPE_TYPEC_PORT_STATUS,
	MCA_EVENT_TYPE_END,
};

enum mca_event_battery_code {
	MCA_EVENT_BATTERY_STS_CHANGE = 0,
	MCA_EVENT_BATTERY_HEALTH_CHANGE,
	MCA_EVENT_BATTERY_FAKE_POWER,
	MCA_EVENT_BATTERY_DTPT,
};

struct mca_event_notify_data {
	const char *event;
	int event_len;
};

int mca_event_block_notify_register(unsigned int type,
				    struct notifier_block *nb);
int mca_event_block_notify_unregister(unsigned int type,
				      struct notifier_block *nb);
void mca_event_block_notify(unsigned int type, unsigned long event, void *data);
void mca_event_report_uevent(const struct mca_event_notify_data *n_data);

#endif /* _MCA_COMMON_MCA_EVENT_H_ */
