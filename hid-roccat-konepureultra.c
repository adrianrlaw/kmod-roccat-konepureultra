// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Roccat KonePureUltra driver for Linux
 *
 * Copyright (c) 2020 Adrian Law <adrianrlaw12@gmail.com>
 */

/*
 * Roccat KonePureUltra is a new version of the KonePure with a newer sensor and
 * is lighter than its predecessor.
 *
 * This code is taken from Stefan Achatz's code for the KonePure in the upstream
 * kernel release.
 */

#include <linux/types.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/hid-roccat.h>
#include "hid-ids.h"
#include "hid-roccat-common.h"

enum {
	KONEPUREULTRA_MOUSE_REPORT_NUMBER_BUTTON = 3,
};

struct konepureultra_mouse_report_button {
	uint8_t report_number; /* always KONEPUREULTRA_MOUSE_REPORT_NUMBER_BUTTON */
	uint8_t zero;
	uint8_t type;
	uint8_t data1;
	uint8_t data2;
	uint8_t zero2;
	uint8_t unknown[2];
} __packed;

static struct class *konepureultra_class;

ROCCAT_COMMON2_BIN_ATTRIBUTE_W(control, 0x04, 0x03);
ROCCAT_COMMON2_BIN_ATTRIBUTE_RW(actual_profile, 0x05, 0x03);
ROCCAT_COMMON2_BIN_ATTRIBUTE_RW(profile_settings, 0x06, 0x1f);
ROCCAT_COMMON2_BIN_ATTRIBUTE_RW(profile_buttons, 0x07, 0x3b);
ROCCAT_COMMON2_BIN_ATTRIBUTE_W(macro, 0x08, 0x0822);
ROCCAT_COMMON2_BIN_ATTRIBUTE_RW(info, 0x09, 0x06);
ROCCAT_COMMON2_BIN_ATTRIBUTE_RW(tcu, 0x0c, 0x04);
ROCCAT_COMMON2_BIN_ATTRIBUTE_R(tcu_image, 0x0c, 0x0404);
ROCCAT_COMMON2_BIN_ATTRIBUTE_RW(sensor, 0x0f, 0x06);
ROCCAT_COMMON2_BIN_ATTRIBUTE_W(talk, 0x10, 0x10);

static struct bin_attribute *konepureultra_bin_attrs[] = {
	&bin_attr_actual_profile,
	&bin_attr_control,
	&bin_attr_info,
	&bin_attr_talk,
	&bin_attr_macro,
	&bin_attr_sensor,
	&bin_attr_tcu,
	&bin_attr_tcu_image,
	&bin_attr_profile_settings,
	&bin_attr_profile_buttons,
	NULL,
};

static const struct attribute_group konepureultra_group = {
	.bin_attrs = konepureultra_bin_attrs,
};

static const struct attribute_group *konepureultra_groups[] = {
	&konepureultra_group,
	NULL,
};

static int konepureultra_init_specials(struct hid_device *hdev)
{
	struct usb_interface *intf = to_usb_interface(hdev->dev.parent);
	struct usb_device *usb_dev = interface_to_usbdev(intf);
	struct roccat_common2_device *konepureultra;
	int retval;

	if (intf->cur_altsetting->desc.bInterfaceProtocol
			!= USB_INTERFACE_PROTOCOL_MOUSE) {
		hid_set_drvdata(hdev, NULL);
		return 0;
	}

	konepureultra = kzalloc(sizeof(*konepureultra), GFP_KERNEL);
	if (!konepureultra) {
		hid_err(hdev, "can't alloc device descriptor\n");
		return -ENOMEM;
	}
	hid_set_drvdata(hdev, konepureultra);

	retval = roccat_common2_device_init_struct(usb_dev, konepureultra);
	if (retval) {
		hid_err(hdev, "couldn't init KonePureUltra device\n");
		goto exit_free;
	}

	retval = roccat_connect(konepureultra_class, hdev,
			sizeof(struct konepureultra_mouse_report_button));
	if (retval < 0) {
		hid_err(hdev, "couldn't init char dev\n");
	} else {
		konepureultra->chrdev_minor = retval;
		konepureultra->roccat_claimed = 1;
	}

	return 0;
exit_free:
	kfree(konepureultra);
	return retval;
}

static void konepureultra_remove_specials(struct hid_device *hdev)
{
	struct usb_interface *intf = to_usb_interface(hdev->dev.parent);
	struct roccat_common2_device *konepureultra;

	if (intf->cur_altsetting->desc.bInterfaceProtocol
			!= USB_INTERFACE_PROTOCOL_MOUSE)
		return;

	konepureultra = hid_get_drvdata(hdev);
	if (konepureultra->roccat_claimed)
		roccat_disconnect(konepureultra->chrdev_minor);
	kfree(konepureultra);
}

static int konepureultra_probe(struct hid_device *hdev,
		const struct hid_device_id *id)
{
	int retval;

	retval = hid_parse(hdev);
	if (retval) {
		hid_err(hdev, "parse failed\n");
		goto exit;
	}

	retval = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (retval) {
		hid_err(hdev, "hw start failed\n");
		goto exit;
	}

	retval = konepureultra_init_specials(hdev);
	if (retval) {
		hid_err(hdev, "couldn't install mouse\n");
		hid_hw_stop(hdev);
	}

exit:
	return retval;
}

static void konepureultra_remove(struct hid_device *hdev)
{
	konepureultra_remove_specials(hdev);
	hid_hw_stop(hdev);
}

static int konepureultra_raw_event(struct hid_device *hdev,
		struct hid_report *report, u8 *data, int size)
{
	struct usb_interface *intf = to_usb_interface(hdev->dev.parent);
	struct roccat_common2_device *konepureultra = hid_get_drvdata(hdev);

	if (intf->cur_altsetting->desc.bInterfaceProtocol
			!= USB_INTERFACE_PROTOCOL_MOUSE)
		return 0;

	if (data[0] != KONEPUREULTRA_MOUSE_REPORT_NUMBER_BUTTON)
		return 0;

	if (konepureultra != NULL && konepureultra->roccat_claimed)
		roccat_report_event(konepureultra->chrdev_minor, data);

	return 0;
}

static const struct hid_device_id konepureultra_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_ROCCAT, USB_DEVICE_ID_ROCCAT_KONEPUREULTRA) },
	{ }
};

MODULE_DEVICE_TABLE(hid, konepureultra_devices);

static struct hid_driver konepureultra_driver = {
		.name = "konepureultra",
		.id_table = konepureultra_devices,
		.probe = konepureultra_probe,
		.remove = konepureultra_remove,
		.raw_event = konepureultra_raw_event
};

static int __init konepureultra_init(void)
{
	int retval;

	konepureultra_class = class_create(THIS_MODULE, "konepureultra");
	if (IS_ERR(konepureultra_class))
		return PTR_ERR(konepureultra_class);
	konepureultra_class->dev_groups = konepureultra_groups;

	retval = hid_register_driver(&konepureultra_driver);
	if (retval)
		class_destroy(konepureultra_class);
	return retval;
}

static void __exit konepureultra_exit(void)
{
	hid_unregister_driver(&konepureultra_driver);
	class_destroy(konepureultra_class);
}

module_init(konepureultra_init);
module_exit(konepureultra_exit);

MODULE_AUTHOR("Adrian Law");
MODULE_DESCRIPTION("USB Roccat KonePureUltra driver");
MODULE_LICENSE("GPL v2");
