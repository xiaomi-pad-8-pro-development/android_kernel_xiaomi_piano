# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/misc/hwid

obj-$(CONFIG_XM_POWER_SUPPLY)	+= mca_basic_wireless.o
obj-$(CONFIG_XM_POWER_SUPPLY)	+= mca_quick_wireless.o
obj-$(CONFIG_XM_POWER_SUPPLY)	+= mca_wireless_revchg.o

CFLAGS_mca_basic_wireless.o += -Wno-error=format
CFLAGS_mca_quick_wireless.o += -Wno-error=format
