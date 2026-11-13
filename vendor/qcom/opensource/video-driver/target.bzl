load(":video_modules.bzl", "video_driver_modules")
load(":video_driver_build.bzl", "define_target_variant_modules")

def define_target_modules():
    # KP 5.0: piano builds sun perf only.
    for (target, variant) in [("sun", "perf")]:
        define_target_variant_modules(
            target = target,
            variant = variant,
            registry = video_driver_modules,
            modules = [
                "msm_video",
                "video",
            ],
            config_options = [
                "CONFIG_MSM_MMRM",
                "CONFIG_MSM_VIDC_SUN",
                # The soc-repo modules these call are not in the base kernel config, so
                # without these their headers compile in stubs (no firmware load, LLCC
                # or mem_buf).
                "CONFIG_QCOM_MDT_LOADER_MODULE",
                "CONFIG_QCOM_LLCC_MODULE",
                "CONFIG_QCOM_MEM_BUF_MODULE",
                "CONFIG_QCOM_MEM_BUF_DEV_MODULE",
                "CONFIG_QCOM_MEM_BUF_GH_MODULE",
            ],
        )
