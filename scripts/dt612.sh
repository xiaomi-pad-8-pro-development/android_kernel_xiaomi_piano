#!/bin/bash
# Compile piano 6.6 overlays against the Qualcomm 6.12 sun devicetree.
set -u
# Any failed step sets FAIL; the script exits 1 at the end so build612.sh
# stops instead of packing a stale or partial devicetree.
FAIL=0
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
# 6.12 SPS: "qcom,msm-sps-4k" now means the 0xC000 pipe layout; sun BAMs use 0x13000 (v1_7-4k), as on 6.6.
sed -i "s/compatible = \"qcom,msm-sps-4k\";/compatible = \"qcom,msm-sps-v1_7-4k\";/" $OUT/qcom/sun.dtsi
for f in piano-sm8750-overlay.dts piano-sm8750.dtsi piano-pinctrl.dtsi piano-charger-common.dtsi piano-xiaomi-touch-pinctrl.dtsi sm8750-xiaomi-common.dtsi sm8750-ir-sensor-common.dtsi; do
  cp $SRC66/arch/arm64/boot/dts/vendor/qcom/$f $OUT/qcom/ || { echo "MISSING $f"; FAIL=1; }
done
cp -r --update=none $SRC66/arch/arm64/boot/dts/vendor/qcom/. $OUT/qcom/
cp -r --update=none $SRC66/arch/arm64/boot/dts/techpack/display-devicetree/display/. $OUT/display/
cp $SRC66/arch/arm64/boot/dts/techpack/display-devicetree/display/{piano-*,dsi-panel-p8*,dsi-panel-nt37801-dsc-wqhd-plus-video.dtsi} $OUT/display/ 2>/dev/null
# The P81/P82 LCDs are Novatek NT36532: Xiaomi's 6.6 driver (and the
# bootloader) use its DSC rate control, which the 6.12 driver keys off this.
sed -i 's/^qcom,compression-mode = "dsc";$/&\nmi,mdss-dsc-nt36532-config;/' \
  $OUT/display/dsi-panel-p81-p82-dualdsi-dsc-lcd-video-common.dtsi
# Bring-up: ONLY120=1 keeps just the 120 Hz timing (no timing switch at all).
if [ "${ONLY120:-0}" = 1 ]; then
  for f in $OUT/display/dsi-panel-p8[12]-*-dualdsi-dsc-lcd-video.dtsi; do
    python3 - "$f" <<'EOF'
import re, sys
p = sys.argv[1]; s = open(p).read()
a = s.index("timing@144{"); a = s.rindex("\n", 0, a) + 1
b = s.index("\n        };", a) + 1
s = s[:a] + s[b:]
s = s.replace("qcom,dsi-supported-dfps-list = <120 144 90 60 50 48 30>;", "qcom,dsi-supported-dfps-list = <120>;")
s = s.replace("mi,mdss-dsi-supported-fps-list = <120 60>;", "mi,mdss-dsi-supported-fps-list = <120>;")
open(p, "w").write(s)
EOF
  done
  grep -c "timing@" $OUT/display/dsi-panel-p81-35-02-0b-dualdsi-dsc-lcd-video.dtsi
fi
# The pmk8550 ADC has 2 SDAMs, so at most 15 thermal monitor channels;
# piano's 10 plus Qualcomm's MTP ones make 19 and the ADC never probes
# (no board, skin or battery thermal zones). Drop monitoring from the MTP
# channels piano has no thermistor on; they stay readable over IIO.
python3 - $OUT/qcom/sun-pmic-overlay.dtsi <<'EOF2'
import re, sys
p = sys.argv[1]; s = open(p).read()
for n in ("pm8550_cam_flash_therm", "pm8550_wlan_therm", "pm8550_pa_therm1",
          "pm8550_pa_therm2", "pm8550_rear_tof_therm", "pmih010x_wls_therm"):
    a = s.index("\t" + n + " {"); b = s.index("\t};", a)
    s = s[:a] + re.sub(r"\n\t*qcom,adc-tm-type = <1>;", "", s[a:b]) + s[b:]
open(p, "w").write(s)
EOF2
# Qualcomm's MTP sys-therm zones read the same ADC channels as piano's
# own zones (sys-therm-2 is cpu_therm, -3 flash, -4 wifi, -7 backlight,
# -8 charger1, -11 conn); a channel binds only its first zone, so piano's
# never registered and mi_thermald had no inputs for its board sensor.
# -5 and -6 are MTP PA thermistors piano does not have.
python3 - $OUT/qcom/sun-pmic-overlay.dtsi <<'EOF3'
import sys
p = sys.argv[1]; s = open(p).read()
for n in (2, 3, 4, 5, 6, 7, 8, 11):
    a = s.index("\n\tsys-therm-%d {" % n); b = s.index("\n\t};", a) + 4
    s = s[:a] + s[b:]
open(p, "w").write(s)
EOF3
# sun-thermal-overlay adds trips to sys-therm-2; without its sensor that
# zone stops thermal_of from finding any zone at all, so drop it too.
python3 - $OUT/qcom/sun-thermal-overlay.dtsi <<'EOF3'
import sys
p = sys.argv[1]; s = open(p).read()
a = s.index("\n\tsys-therm-2 {"); b = s.index("\n\t};", a) + 4
open(p, "w").write(s[:a] + s[b:])
EOF3
# 6.12 takes the bus and SID as arguments.
sed -i -E "s/\b(PMIH010X_ADC5_GEN4_[A-Z0-9_]+)\b([^(]|\$)/\1(0, PMIH010X_SID)\2/g" $OUT/qcom/piano-*.dtsi $OUT/qcom/sm8750-*.dtsi
INC="-I /home/alif/kernel/dt612-extra -I $K/include -I $OUT/qcom -I $OUT/display -I $QDT/bindings -I $K/include -I $K/include/dt-bindings -I $K/arch/arm64/boot/dts -I $QDT/qcom -I $KP/kernel_platform/common/scripts/dtc/include-prefixes -I $KP/kernel_platform/common/include"
build() { # src out [extra cpp args]
  rm -f $2 $2.pp
  if ! $CC -E ${3:-} -nostdinc -undef -D__DTS__ -x assembler-with-cpp $INC -o $2.pp $1 > $2.log 2>&1; then
    head -20 $2.log; echo "FAIL cpp $1"; FAIL=1; return 1
  fi
  if ! $DTC -@ -I dts -O dtb -o $2 -i $(dirname $1) $2.pp > $2.log 2>&1; then
    grep -E "rror" $2.log | head -20; echo "FAIL dtc $1"; FAIL=1; rm -f $2; return 1
  fi
  ls -la $2
}
cp $SRC66/arch/arm64/boot/dts/techpack/graphics-devicetree/gpu/sun-*gpu*.dtsi $OUT/qcom/
cp /home/alif/kernel/piano-sun-additions.dtsi /home/alif/kernel/sun-synx.dtsi /home/alif/kernel/piano-fastrpc.dtsi $OUT/qcom/
# ABL picks the base by SoC and revision, so build every sun/sunp variant.
BASES="sun-tp-v2 sun-tp sun-v2 sun sunp-tp-v2 sunp-tp sunp-v2 sunp"
for v in $BASES; do
  # The 6.12 base only has an empty GPU node; Qualcomm fills it from graphics-devicetree.
  case $v in *v2*) gpu=sun-v2-gpu ;; *) gpu=sun-gpu ;; esac
  printf '#include "%s.dts"\n#include "piano-sun-additions.dtsi"\n#include "sun-synx.dtsi"\n#include "piano-fastrpc.dtsi"\n#include "%s.dtsi"\n#include "%s-pwrlevels.dtsi"\n' $v $gpu $gpu > $OUT/qcom/piano-$v.dts
  echo "== base $v"; build $OUT/qcom/piano-$v.dts $OUT/$v.dtb "${QCEDEV_NO_SCB:+-DQCEDEV_NO_SCB}"
done
# 6.12 sun-mtp.dtsi no longer pulls in the USB PHY register names.
echo "== piano"; build $OUT/qcom/piano-sm8750-overlay.dts $OUT/piano-overlay.dtbo "-include dt-bindings/phy/qcom,usb3-3nm-qmp-combo.h"
echo "== display"; build $OUT/display/piano-sde-display-mtp-overlay.dts $OUT/piano-display-overlay.dtbo
# Audio: Qualcomm's generic sun overlay, then the stock-derived piano one.
AUD=$SRC66/arch/arm64/boot/dts/techpack/audio-devicetree
AINC="-I $AUD -I $KP/vendor/qcom/opensource/audio-kernel/include -I $SRC66/arch/arm64/boot/dts/vendor"
echo "== audio"
build $AUD/sun-audio.dts $OUT/sun-audio.dtbo "$AINC"
# 6.12 names the TDM1 data pins sd0/sd1 (6.6: din/dout).
sed -e 's/tdm1_din_/tdm1_sd0_/g' -e 's/tdm1_dout_/tdm1_sd1_/g' $AUD/piano-audio-mtp.dts > $OUT/piano-audio-mtp.dts
# sun-audio.dts leaves the ADSP loader disabled (6.6 enabled it in
# sun-audio-overlay.dtsi), so nothing ever boots the ADSP. Enable it here.
sed -i "s|^\txiaomi,miboard-id = <9 0>;$|&\n\tfragment@9000 {\n\t\ttarget = <\&adsp_loader>;\n\t\t__overlay__ { status = \"ok\"; };\n\t};|" $OUT/piano-audio-mtp.dts
build $OUT/piano-audio-mtp.dts $OUT/piano-audio.dtbo "$AINC"
# Bluetooth and WLAN (Peach): the piano board overlays from 6.6.
TP=$SRC66/arch/arm64/boot/dts/techpack
echo "== bt/wlan"
build $TP/bt-devicetree/piano-peach-bt-v8.dts $OUT/piano-bt.dtbo "-I $TP/bt-devicetree"
build $TP/wlan-devicetree/piano-peach-cnss-v8.dts $OUT/piano-wlan.dtbo "-I $TP/wlan-devicetree"
# Multimedia: on 6.6 merge_dtbs folds these techpack overlays into the base;
# the 6.12 base has no camera, video, EVA or MMRM nodes. piano is sun v2.
echo "== camera/video/eva/mmrm"
CAM="-I $TP/camera-devicetree -I $KP/vendor/qcom/opensource/camera-kernel"
build $TP/mmrm-devicetree/sun/sun-mmrm.dts $OUT/sun-mmrm.dtbo "-I $TP/mmrm-devicetree/sun"
# One overlay for the camera: piano-sm8750-camera-sensor overrides the cci0
# pinctrl states that sun-camera sets, and fdtoverlaymerge loses the local
# phandle fixups of such an override (cci0 then fails "invalid phandle").
printf '/dts-v1/;\n/plugin/;\n#include <dt-bindings/clock/qcom,gcc-sun.h>\n#include <dt-bindings/clock/qcom,camcc-sun.h>\n#include <dt-bindings/clock/qcom,cambistmclkcc-sun.h>\n#include <dt-bindings/interconnect/qcom,sun.h>\n#include <dt-bindings/interrupt-controller/arm-gic.h>\n#include <dt-bindings/regulator/qcom,rpmh-regulator-levels.h>\n#include "sun-camera.dtsi"\n#include "piano-sm8750-camera-sensor.dtsi"\n' > $OUT/piano-camera.dts
build $OUT/piano-camera.dts $OUT/piano-camera.dtbo "$CAM"
build $TP/video-devicetree/sun-vidc-v2.dts $OUT/sun-vidc.dtbo "-I $TP/video-devicetree"
build $TP/eva-devicetree/sun-eva-v2.dts $OUT/sun-eva.dtbo "-I $TP/eva-devicetree"
MM="$OUT/sun-mmrm.dtbo $OUT/piano-camera.dtbo $OUT/sun-vidc.dtbo $OUT/sun-eva.dtbo"
H=$KP/kernel_platform/out/msm-kernel-sun-perf/host/bin
echo "== check"; $H/fdtoverlay -i $OUT/sun-v2.dtb -o $OUT/merged.dtb $OUT/piano-overlay.dtbo $OUT/piano-display-overlay.dtbo $OUT/sun-audio.dtbo $OUT/piano-audio.dtbo $OUT/piano-bt.dtbo $OUT/piano-wlan.dtbo $MM && ls -la $OUT/merged.dtb || { echo "FAIL fdtoverlay"; FAIL=1; }
echo "== pack"
$H/fdtoverlaymerge -i $OUT/piano-overlay.dtbo -o $OUT/piano-all.dtbo $OUT/piano-display-overlay.dtbo $OUT/sun-audio.dtbo $OUT/piano-audio.dtbo $OUT/piano-bt.dtbo $OUT/piano-wlan.dtbo $MM || { echo "FAIL fdtoverlaymerge"; FAIL=1; }
(cd $OUT && cat $(printf '%s.dtb ' $BASES)) > $OUT/dtb || { echo "FAIL dtb"; FAIL=1; }
$KP/kernel_platform/build/kernel/build-tools/path/linux-x86/mkdtboimg create $OUT/dtbo.img --page_size=4096 $OUT/piano-all.dtbo || { echo "FAIL mkdtboimg"; FAIL=1; }
ls -la $OUT/dtb $OUT/dtbo.img
if [ $FAIL != 0 ]; then echo DT612_FAIL; exit 1; fi
echo DT612_DONE
