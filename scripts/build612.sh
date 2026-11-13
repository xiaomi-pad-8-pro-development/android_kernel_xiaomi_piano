#!/usr/bin/env bash
# Build 6.12 for piano: the KP 5.0 kernel, every sun_perf techpack and Xiaomi
# module, then the devicetree, dlkm and boot images. Logs to
# /tmp/piano-kernel-build-612.log.
L=/tmp/piano-kernel-build-612.log
exec > $L 2>&1
set -x
KP=/home/alif/kernel/kp50/kernel_platform
cd $KP || exit 1
# PIANO_BRINGUP=1 (the default while 6.12 is in testing) builds the memcon
# RAM console, block log and reset checkpoints. RELEASE=1 turns it off.
[ "${RELEASE:-0}" = 1 ] && PIANO_BRINGUP=0
C=soc-repo/configs/sun_perf.bzl
sed -i '/"CONFIG_PIANO_BRINGUP"/d' $C
[ "${PIANO_BRINGUP:-1}" = 1 ] && sed -i 's/^    # keep sorted$/&\n    "CONFIG_PIANO_BRINGUP": "y",/' $C
# The GKI Image (common/kernel_aarch64) never sees sun_perf.bzl, so the
# switch also has to reach it: default the option on in Kconfig.debug.
K=common/arch/arm64/Kconfig.debug
sed -i '/^\tdefault y # piano bring-up$/d' $K
[ "${PIANO_BRINGUP:-1}" = 1 ] && sed -i 's/^\tbool "Xiaomi Pad 8 Pro (piano) bring-up debug tools"$/&\n\tdefault y # piano bring-up/' $K
# dist is never emptied by the build, so a module that is no longer built
# would stay there from an older kernel and end up in the images.
D=out/msm-kernel-sun-perf/dist
rm -rf $D
./build_with_bazel.py -t sun perf -s abl --config=stamp || { echo STEP=full FAIL; exit 1; }
# Every module must match the kernel it ships with.
V=$(strings $D/Image | sed -n 's/^Linux version \([^ ]*\) .*/\1/p' | head -1)
for k in $D/*.ko; do
	m=$(modinfo -F vermagic $k | cut -d' ' -f1)
	[ "${m%%-*}" = "${V%%-*}" ] || { echo "STEP=vermagic FAIL $(basename $k) $m, kernel $V"; exit 1; }
done
# Every techpack module for sun perf, so none is left over from an older
# build. Camera needs its project flag and builds on its own below.
T=$(tools/bazel query 'attr(name, "^sun_perf_", kind("ddk_module|kernel_module", //vendor/qcom/opensource/... + //vendor/xiaomi/...)) except //vendor/qcom/opensource/camera-kernel/...' 2>/dev/null)
[ -n "$T" ] || { echo STEP=query FAIL; exit 1; }
tools/bazel build --keep_going $T || echo STEP=modules PARTIAL
tools/bazel build --keep_going --//vendor/qcom/opensource/camera-kernel:project_name=sun \
	//vendor/qcom/opensource/camera-kernel:sun_perf_camera \
	//vendor/qcom/opensource/camera-kernel:sun_perf_cameralog \
	//vendor/qcom/opensource/camera-kernel:sun_perf_cameramsger || { echo STEP=camera FAIL; exit 1; }
# The module files those targets produce, for dlkm612.sh.
C="--//vendor/qcom/opensource/camera-kernel:project_name=sun"
# cquery takes one expression, so join the labels with set().
{ tools/bazel cquery --keep_going --output=files "set($(echo $T))" 2>/dev/null
  tools/bazel cquery $C --keep_going --output=files "set(//vendor/qcom/opensource/camera-kernel:sun_perf_camera \
	//vendor/qcom/opensource/camera-kernel:sun_perf_cameralog \
	//vendor/qcom/opensource/camera-kernel:sun_perf_cameramsger)" 2>/dev/null
} | grep '\.ko$' > out/piano-modules.list
[ -s out/piano-modules.list ] || { echo STEP=list FAIL; exit 1; }
cd /home/alif/kernel
bash dt612.sh || { echo STEP=dt FAIL; exit 1; }
bash dlkm612.sh || { echo STEP=dlkm FAIL; exit 1; }
RESET_AT=0 bash pack612.sh || { echo STEP=pack FAIL; exit 1; }
echo R45_BUILD_OK
