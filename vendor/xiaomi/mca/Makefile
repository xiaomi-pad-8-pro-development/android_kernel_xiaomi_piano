# kleaf restores the Module.symvers of the soc-repo dependencies under
# COMMON_OUT_DIR; hand them to modpost.
KBUILD_EXTRA_SYMBOLS += $(wildcard $(COMMON_OUT_DIR)/soc-repo/sun_perf/drivers/*/*/*_Module.symvers \
	$(COMMON_OUT_DIR)/soc-repo/sun_perf/drivers/*/*/*/*_Module.symvers)

modules modules_install clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $(KBUILD_OPTIONS) KBUILD_EXTRA_SYMBOLS="$(KBUILD_EXTRA_SYMBOLS)" $@
