# WALT is a soc-repo module the base kernel config does not know. The
# CPU count is the kernel/sched/walt/Kconfig default (sun_perf.bzl does
# not set it); it changes struct walt_task_struct, so it must match.
ccflags-y += -DCONFIG_SCHED_WALT_MODULE=1 -DCONFIG_SCHED_WALT_NR_CPUS=8
obj-$(CONFIG_MI_RTG) += rtg.o
