#!/usr/bin/env bash
# Build 6.12 for piano: the KP 5.0 kernel, every sun_perf techpack and Xiaomi
# module, then the devicetree, dlkm and boot images. Logs to
# /tmp/piano-kernel-build-612.log.
L=/tmp/piano-kernel-build-612.log
exec > $L 2>&1
set -x
KP=/home/alif/kernel/kp50/kernel_platform
cd $KP || exit 1
./build_with_bazel.py -t sun perf -s abl || { echo STEP=full FAIL; exit 1; }
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
