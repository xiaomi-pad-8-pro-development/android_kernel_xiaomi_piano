# SPDX-License-Identifier: GPL-2.0
#
# Makefile for sensor chip drivers.
#

#xiaomi thermal control interface
ifdef CONFIG_XM_POWER_SUPPLY
ccflags-y += -I$(srctree)/drivers/power/supply/mca/include
endif
obj-$(CONFIG_MI_THERMAL_INTERFACE)	+= mi_thermal_interface.o
