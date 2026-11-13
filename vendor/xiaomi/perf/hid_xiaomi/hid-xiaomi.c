// SPDX-License-Identifier: GPL-2.0
/*
 * Xiaomi Focus Pen filter for the Xiaomi Pad 8 Pro (piano), 6.12
 *
 * The android16-6.12 GKI does not build hid-xiaomi, so this is the 6.6
 * piano driver built as a vendor module. The Mi Silent Mouse fixup is
 * left to the GKI kernel's generic handling.
 *
 * The Focus Pen and Focus Pen Pro send their gestures, tip pressure and
 * motion in reports 2, 5 and 6 for the touch controller host, which reads
 * them over hidraw. The descriptor leaves the pressure and motion
 * collections on the keyboard usage page, so the input layer would turn
 * every sample into a Keypad Plus press and every gesture into keypad
 * navigation. Keep them off the input devices.
 */

#include <linux/hid.h>
#include <linux/module.h>

#define BT_VENDOR_ID_XIAOMI_PEN			0x0022
#define BT_DEVICE_ID_XIAOMI_FOCUS_PEN		0x4d80
#define BT_DEVICE_ID_XIAOMI_FOCUS_PEN_PRO	0x5081

static int xiaomi_pen_input_mapping(struct hid_device *hdev,
				    struct hid_input *hi,
				    struct hid_field *field,
				    struct hid_usage *usage,
				    unsigned long **bit, int *max)
{
	/*
	 * Stock reads the squeeze, double tap and slide gestures as buttons
	 * 7 to 10 of the HID button page. Only the keyboard page is noise.
	 */
	if ((usage->hid & HID_USAGE_PAGE) == HID_UP_BUTTON)
		return 0;

	switch (field->report->id) {
	case 2:
	case 5:
	case 6:
		return -1;
	}

	return 0;
}

static const struct hid_device_id xiaomi_pen_devices[] = {
	{ HID_BLUETOOTH_DEVICE(BT_VENDOR_ID_XIAOMI_PEN, BT_DEVICE_ID_XIAOMI_FOCUS_PEN) },
	{ HID_BLUETOOTH_DEVICE(BT_VENDOR_ID_XIAOMI_PEN, BT_DEVICE_ID_XIAOMI_FOCUS_PEN_PRO) },
	{ }
};
MODULE_DEVICE_TABLE(hid, xiaomi_pen_devices);

static struct hid_driver xiaomi_pen_driver = {
	.name = "xiaomi-pen",
	.id_table = xiaomi_pen_devices,
	.input_mapping = xiaomi_pen_input_mapping,
};
module_hid_driver(xiaomi_pen_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Keep Xiaomi Focus Pen host reports off the input devices");
