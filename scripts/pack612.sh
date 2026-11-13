#!/usr/bin/env bash
# Pack a Qualcomm 6.12 test boot/vendor_boot/dtbo for piano slot b.
# Kernel and modules from KP 5.0, display modules from the vendor packs,
# devicetree from dt612.sh, vendor ramdisk fstab from the EvoX build.
set -euo pipefail
KP=/home/alif/kernel/kp50/kernel_platform
DIST=$KP/out/msm-kernel-sun-perf/dist
BIN=$KP/bazel-bin/vendor/qcom/opensource
ROM=${ROM:-/home/alif/android/evolution-x}
HOST=$ROM/out/host/linux-x86/bin
W=/home/alif/kernel/pack612
DT=/home/alif/kernel/dt612-out
VER=$(basename $(ls -d /home/alif/kernel/kp50/kernel_platform/bazel-bin/vendor/qcom/opensource/mm-drivers/msm_ext_display/sun_perf_msm_ext_display/staging/lib/modules/* -td | head -1))
rm -rf $W/dlkm $W/out; mkdir -p $W/dlkm/lib/modules/$VER $W/out
M=$W/dlkm/lib/modules/$VER
cp $DIST/*.ko $M/
for k in mm-drivers/msm_ext_display/sun_perf_msm_ext_display/staging/lib/modules/$VER/extra/vendor/qcom/opensource/mm-drivers/msm_ext_display/msm_ext_display.ko \
         synx-kernel/sun_perf_ipclite_synx/staging/lib/modules/$VER/extra/vendor/qcom/opensource/synx-kernel/ipclite.ko \
         synx-kernel/sun_perf_synx-driver_synx/staging/lib/modules/$VER/extra/vendor/qcom/opensource/synx-kernel/synx-driver.ko \
         mm-drivers/sync_fence/sun_perf_sync_fence/staging/lib/modules/$VER/extra/vendor/qcom/opensource/mm-drivers/sync_fence/sync_fence.ko \
         mm-drivers/hw_fence/sun_perf_msm_hw_fence/staging/lib/modules/$VER/extra/vendor/qcom/opensource/mm-drivers/hw_fence/msm_hw_fence.ko \
         mmrm-driver/sun_perf_mmrm_driver/staging/lib/modules/$VER/extra/vendor/qcom/opensource/mmrm-driver/msm-mmrm.ko \
         display-drivers/sun_perf_msm_drm/msm_drm.ko; do
  cp $BIN/$k $M/
done
# First stage loads the board modules, then the USB suppliers.
{ # UFS_FIRST=1: storage first, so piano_blklog can log the rest of first stage.
  [ "${UFS_FIRST:-0}" = 1 ] && printf "%s\n" qcom_ice.ko phy-qcom-ufs.ko phy-qcom-ufs-qmp-v4-sun.ko ufshcd-crypto-qti.ko ufs-qcom.ko
  xargs -n1 basename < $DIST/vendor_boot.modules.load
  # USB suppliers, so recovery (vendor_boot only) gets USB.
  printf "%s\n" nvmem_qfprom.ko smp2p.ko phy-generic.ko phy-msm-ssusb-qmp.ko phy-msm-m31-eusb2.ko repeater.ko repeater-qti-pmic-eusb2.ko dwc3-msm.ko
  # FS_DISPLAY=1: the display stack too. msm_hw_fence and synx-driver need
  # qcom_q6v5_pas, which then boots the DSPs before /vendor and /odm hold
  # their firmware; they never retry, leaving no ADSP, USB role or charging.
  if [ "${FS_DISPLAY:-0}" = 1 ]; then
    printf "%s\n" msm-mmrm.ko sync_fence.ko msm_ext_display.ko msm_hw_fence.ko ipclite.ko synx-driver.ko
  fi
} | awk "!seen[\$0]++" > $M/modules.load
# Recovery boots from vendor_boot alone and the 6.12 kernel cannot use
# the 6.6 vendor_dlkm, so carry the vendor_dlkm modules in the ramdisk and
# load them all in recovery, as the 6.6 vendor_boot does
# (vendor_boot.modules.load.recovery). Needs a dlkm612.sh run first;
# RECOVERY_DLKM=0 leaves recovery with the first-stage set only.
VD=/home/alif/kernel/dlkm612/vendor_dlkm/lib/modules
cp $M/modules.load $M/modules.load.recovery
if [ "${RECOVERY_DLKM:-1}" = 1 ]; then
  test -f $VD/modules.load || { echo "run dlkm612.sh first, or RECOVERY_DLKM=0"; exit 1; }
  xargs -n1 basename < $VD/modules.load | while read -r k; do
    cp --update=none $VD/$k $M/
    echo $k
  done >> $M/modules.load.recovery
  awk '!seen[$0]++' $M/modules.load.recovery > $M/l.tmp && mv $M/l.tmp $M/modules.load.recovery
  # First-stage init stops loading at the first module that fails, which
  # leaves recovery with no display or adb. icnss2 is for the other WLAN
  # chip type and fails with ENODEV on piano (the 6.6 recovery list leaves
  # it out too).
  sed -i '/^icnss2\.ko$/d' $M/modules.load.recovery
fi
# The KP dist ships qti_battery_charger, but mca_qcom_subpmic_proxy exports
# the same qti_battery_charger_get_prop (dlkm612.sh removes it from
# vendor_dlkm for that reason). Left here, depmod makes the haptics and
# flash LED modules depend on it, and the proxy then fails with a
# duplicate symbol, again ending first-stage module loading.
rm -f $M/qti_battery_charger.ko
sed -i '/^qti_battery_charger\.ko$/d' $M/modules.load $M/modules.load.recovery
# The vendor_boot partition is 96 MiB; drop debug info.
STRIP=$(ls -d $KP/prebuilts/clang/host/linux-x86/clang-r*/bin/llvm-strip | tail -1)
$STRIP --strip-debug $M/*.ko
# The GKI modules vendor_dlkm needs, from the signed system_dlkm tree
# dlkm612.sh builds. Copied after the strip: stripping drops the signature
# and GKI refuses unsigned system_dlkm modules.
SD=/home/alif/kernel/dlkm612/system_dlkm/lib/modules
if [ "${RECOVERY_DLKM:-1}" = 1 ]; then
  { xargs -n1 basename < $SD/modules.load; cat $M/modules.load.recovery; } |
    awk '!seen[$0]++' > $M/l.tmp
  xargs -n1 basename < $SD/modules.load | while read -r k; do
    cp $SD/$k $M/
  done
  mv $M/l.tmp $M/modules.load.recovery
fi
depmod -b $W/dlkm $VER
for m in $(cat $M/modules.load $M/modules.load.recovery); do test -f $M/$m || echo "MISSING $m"; done
# Flatten to /lib/modules like the stock vendor ramdisk.
mv $M/* $W/dlkm/lib/modules/ && rmdir $M
sed -i -E "s#(^|[ :])([^ :]+\.ko)#\1/lib/modules/\2#g" $W/dlkm/lib/modules/modules.dep
(cd $W/dlkm && find . | sort | cpio -o -H newc -R 0:0 2>/dev/null | lz4 -l -12 --favor-decSpeed > $W/out/dlkm.lz4)
cp $W/vb/vendor_ramdisk00 $W/out/ramdisk00
# Land in fastboot (warm reset, pstore kept) if first-stage init fails.
sed "s/init_fatal_reboot_target=recovery/init_fatal_reboot_target=bootloader/" $W/vb/bootconfig > $W/out/bootconfig
$HOST/mkbootimg --header_version 4 --pagesize 0x1000 --base 0 --kernel_offset 0x8000 \
  --ramdisk_offset 0x01000000 --tags_offset 0x100 --dtb_offset 0x1f00000 \
  --vendor_cmdline "bootconfig buildvariant=userdebug" --dtb $DT/dtb --vendor_bootconfig $W/out/bootconfig \
  --ramdisk_type 1 --ramdisk_name "" --vendor_ramdisk_fragment $W/out/ramdisk00 \
  --ramdisk_type 3 --ramdisk_name dlkm --vendor_ramdisk_fragment $W/out/dlkm.lz4 \
  --vendor_boot $W/out/vendor_boot.img
# Bring-up only: IGNORE_UNUSED=1 keeps every clock and power domain the
# bootloader left on. 6.6 boots without these; with them CX and the CPU
# subsystem cannot collapse, so the SoC never reaches its deep idle states.
UNUSED=; [ "${IGNORE_UNUSED:-0}" = 1 ] && UNUSED=" clk_ignore_unused pd_ignore_unused"
# The kernel from the last bazel build, not dist: dlkm612.sh signs the
# system_dlkm modules with that build's key, and dist only changes on a
# full build_with_bazel.py run.
# RELEASE=1 drops the bring-up switches: reset checkpoints, the one-shot
# boot marker, the verbose log and the panics on soft lockups and hung tasks.
CMDLINE="piano_reset_at=${RESET_AT:-0} printk.devkmsg=on piano_oneshot=612-qcom panic=${PANIC:-3} softlockup_panic=1 hung_task_panic=1 hung_task_timeout_secs=10 qcom_dload_mode.download_mode=0 ignore_loglevel firmware_class.path=/odm/firmware/o8$UNUSED"
[ "${RELEASE:-0}" = 1 ] && CMDLINE="qcom_dload_mode.download_mode=0 firmware_class.path=/odm/firmware/o8"
$HOST/mkbootimg --header_version 4 --kernel $KP/bazel-bin/common/kernel_aarch64/Image --os_version 17.0.0 --os_patch_level 2026-10 \
  --cmdline "$CMDLINE" \
  -o $W/out/boot.img
cp $DT/dtbo.img $W/out/dtbo.img
ls -la $W/out/*.img
test $(stat -c %s $W/out/vendor_boot.img) -le 100663296
sha256sum $W/out/*.img
echo PACK612_DONE
