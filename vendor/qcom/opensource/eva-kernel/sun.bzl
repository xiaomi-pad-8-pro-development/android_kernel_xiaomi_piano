load(":eva_modules.bzl", "eva_modules")
load(":eva_module_build.bzl", "define_target_variant_modules")

def define_sun():
    define_target_variant_modules(
        target = "sun",
        variant = "perf",
        registry = eva_modules,
        modules = [
            "msm-eva",
        ],
        config_options = [
            #"CONFIG_TARGET_SYNX_ENABLE",
            "TARGET_SYNX_ENABLE",
            "TARGET_DSP_ENABLE",
            "TARGET_MMRM_ENABLE",
            "CONFIG_EVA_SUN",
            #"TARGET_PRESIL_ENABLE",
            "CONFIG_MSM_MMRM",
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
