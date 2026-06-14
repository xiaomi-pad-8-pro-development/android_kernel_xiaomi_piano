/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Minimal stand-in for the proprietary Xiaomi MCA sysfs header.
 *
 * The original lived in a vendor proprietary tree that is not available
 * in this source tree. Consumers build an attribute table with
 * mca_sysfs_attr_ro(), look up entries with mca_sysfs_lookup_attr() and
 * register them with mca_sysfs_create_files(). The table and the driver
 * show callbacks are kept fully intact so the driver behaves as if MCA
 * were present; only the actual node creation (which the absent MCA core
 * owned) is reduced to a no-op.
 */
#ifndef _LINUX_MCA_COMMON_MCA_SYSFS_H
#define _LINUX_MCA_COMMON_MCA_SYSFS_H

#include <linux/device.h>
#include <linux/string.h>
#include <linux/sysfs.h>

/* Logical MCA sysfs device slots maintained by the (absent) MCA core. */
enum mca_sysfs_dev {
	SYSFS_DEV_1,
	SYSFS_DEV_2,
	SYSFS_DEV_3,
	SYSFS_DEV_MAX,
};

struct mca_sysfs_attr_info {
	struct device_attribute	attr;
	int			sysfs_attr_name;
};

/*
 * Read-only attribute table entry.
 *   _prefix: used to derive the show callback name (<_prefix>_show)
 *   _mode:   permission bits
 *   _name:   driver enum identifying the attribute
 *   _file:   sysfs file name
 */
#define mca_sysfs_attr_ro(_prefix, _mode, _name, _file)		\
{								\
	.attr = __ATTR(_file, _mode, _prefix##_show, NULL),	\
	.sysfs_attr_name = (_name),				\
}

static inline struct mca_sysfs_attr_info *
mca_sysfs_lookup_attr(const char *name,
		      struct mca_sysfs_attr_info *tbl, int size)
{
	int i;

	if (!name || !tbl)
		return NULL;

	for (i = 0; i < size; i++) {
		if (!strcmp(name, tbl[i].attr.attr.name))
			return &tbl[i];
	}

	return NULL;
}

static inline int
mca_sysfs_create_files(int dev_id,
		       struct mca_sysfs_attr_info *tbl, int size)
{
	/*
	 * The MCA core owned the device that these files were attached to.
	 * It is not available here, so node creation is a no-op. The
	 * attribute table and show callbacks remain valid.
	 */
	(void)dev_id;
	(void)tbl;
	(void)size;

	return 0;
}

#endif /* _LINUX_MCA_COMMON_MCA_SYSFS_H */
