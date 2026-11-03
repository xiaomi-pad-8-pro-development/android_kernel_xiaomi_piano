#!/bin/bash
# Compile piano 6.6 overlays against the Qualcomm 6.12 sun devicetree.
set -u
KP=/home/alif/kernel/kp50
SRC66=/home/alif/kernel/piano-alxp-full-lto-20261025
QDT=$KP/kernel_platform/qcom/opensource/devicetree
DDT=$KP/vendor/qcom/opensource/display-devicetree
K=$KP/kernel_platform/soc-repo
CC=$KP/kernel_platform/prebuilts/clang/host/linux-x86/clang-r*/bin/clang
CC=$(ls -d $CC | tail -1)
DTC=$(ls $KP/kernel_platform/prebuilts/kernel-build-tools/linux-x86/bin/dtc 2>/dev/null || which dtc)
OUT=/home/alif/kernel/dt612-out; rm -rf $OUT; mkdir -p $OUT/qcom $OUT/display
# piano sources on top of a copy of the 6.12 trees
cp -r $QDT/qcom/. $OUT/qcom/; cp -r $DDT/display/. $OUT/display/
for f in piano-sm8750-overlay.dts piano-sm8750.dtsi piano-pinctrl.dtsi piano-charger-common.dtsi piano-xiaomi-touch-pinctrl.dtsi sm8750-xiaomi-common.dtsi sm8750-ir-sensor-common.dtsi; do
  cp $SRC66/arch/arm64/boot/dts/vendor/qcom/$f $OUT/qcom/ || echo "MISSING $f"
done
cp -r --update=none $SRC66/arch/arm64/boot/dts/vendor/qcom/. $OUT/qcom/
cp -r --update=none $SRC66/arch/arm64/boot/dts/techpack/display-devicetree/display/. $OUT/display/
cp $SRC66/arch/arm64/boot/dts/techpack/display-devicetree/display/{piano-*,dsi-panel-p8*,dsi-panel-nt37801-dsc-wqhd-plus-video.dtsi} $OUT/display/ 2>/dev/null
# 6.12 takes the bus and SID as arguments.
sed -i -E "s/\b(PMIH010X_ADC5_GEN4_[A-Z0-9_]+)\b([^(]|\$)/\1(0, PMIH010X_SID)\2/g" $OUT/qcom/piano-*.dtsi $OUT/qcom/sm8750-*.dtsi
INC="-I /home/alif/kernel/dt612-extra -I $K/include -I $OUT/qcom -I $OUT/display -I $QDT/bindings -I $K/include -I $K/include/dt-bindings -I $K/arch/arm64/boot/dts -I $QDT/qcom -I $KP/kernel_platform/common/scripts/dtc/include-prefixes -I $KP/kernel_platform/common/include"
build() { # src out [extra cpp args]
  $CC -E ${3:-} -nostdinc -undef -D__DTS__ -x assembler-with-cpp $INC -o $2.pp $1 2>&1 | head -20 &&
  $DTC -@ -I dts -O dtb -o $2 -i $(dirname $1) $2.pp 2>&1 | grep -E "rror" | head -20
  ls -la $2 2>/dev/null
}
cp /home/alif/kernel/piano-sun-additions.dtsi /home/alif/kernel/sun-synx.dtsi /home/alif/kernel/piano-fastrpc.dtsi $OUT/qcom/
# ABL picks the base by SoC and revision, so build every sun/sunp variant.
BASES="sun-tp-v2 sun-tp sun-v2 sun sunp-tp-v2 sunp-tp sunp-v2 sunp"
for v in $BASES; do
  printf '#include "%s.dts"\n#include "piano-sun-additions.dtsi"\n#include "sun-synx.dtsi"\n#include "piano-fastrpc.dtsi"\n' $v > $OUT/qcom/piano-$v.dts
  echo "== base $v"; build $OUT/qcom/piano-$v.dts $OUT/$v.dtb
done
# 6.12 sun-mtp.dtsi no longer pulls in the USB PHY register names.
echo "== piano"; build $OUT/qcom/piano-sm8750-overlay.dts $OUT/piano-overlay.dtbo "-include dt-bindings/phy/qcom,usb3-3nm-qmp-combo.h"
echo "== display"; build $OUT/display/piano-sde-display-mtp-overlay.dts $OUT/piano-display-overlay.dtbo
H=$KP/kernel_platform/out/msm-kernel-sun-perf/host/bin
echo "== check"; $H/fdtoverlay -i $OUT/sun-v2.dtb -o $OUT/merged.dtb $OUT/piano-overlay.dtbo $OUT/piano-display-overlay.dtbo && ls -la $OUT/merged.dtb
echo "== pack"
$H/fdtoverlaymerge -i $OUT/piano-overlay.dtbo -o $OUT/piano-all.dtbo $OUT/piano-display-overlay.dtbo
(cd $OUT && cat $(printf '%s.dtb ' $BASES)) > $OUT/dtb
$KP/kernel_platform/build/kernel/build-tools/path/linux-x86/mkdtboimg create $OUT/dtbo.img --page_size=4096 $OUT/piano-all.dtbo
ls -la $OUT/dtb $OUT/dtbo.img
