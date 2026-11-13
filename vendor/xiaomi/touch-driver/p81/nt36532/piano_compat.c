// SPDX-License-Identifier: GPL-2.0-only
/*
 * Helpers the NT36532 driver took from Xiaomi's 6.6 display (mi_disp) and
 * charger (mca) drivers, which the 6.12 kernel does not have.
 */
#include <linux/err.h>
#include <linux/notifier.h>
#include <linux/soc/qcom/smem.h>
#include <linux/string.h>
#include <linux/types.h>

/* Lockdown info the bootloader leaves in SMEM (as mi_disp reads it) */
#define SMEM_SW_DISPLAY_LOCKDOWN_TABLE	499

ssize_t mi_dsi_panel_lockdown_info_read(unsigned char *plockdowninfo)
{
	size_t item_size;
	void *lockdown;

	if (!plockdowninfo)
		return -EINVAL;

	lockdown = qcom_smem_get(QCOM_SMEM_HOST_ANY,
			SMEM_SW_DISPLAY_LOCKDOWN_TABLE, &item_size);
	if (IS_ERR(lockdown) || !item_size)
		return -EINVAL;

	memcpy(plockdowninfo, lockdown, item_size);
	return item_size;
}

/* The 6.12 panel driver has no TDDI gesture mode to switch. */
void dsi_panel_gesture_enable(bool on)
{
}

/* Pen charging events come from the Xiaomi charger, not ported yet. */
static BLOCKING_NOTIFIER_HEAD(pen_charge_state_chain);

int pen_charge_state_notifier_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&pen_charge_state_chain, nb);
}

int pen_charge_state_notifier_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&pen_charge_state_chain, nb);
}
