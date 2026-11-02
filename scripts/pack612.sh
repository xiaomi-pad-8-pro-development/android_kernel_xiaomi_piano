#!/usr/bin/env bash
# Pack a Qualcomm 6.12 test boot/vendor_boot/dtbo for piano slot b.
# Kernel and modules from KP 5.0, display modules from the vendor packs,
# devicetree from dt612.sh, vendor ramdisk fstab from the EvoX build.
set -euo pipefail
KP=/home/alif/kernel/kp50/kernel_platform
DIST=$KP/out/msm-kernel-sun-perf/dist
BIN=$KP/bazel-bin/vendor/qcom/opensource
HOST=/home/alif/android/evolution-12.3/out/host/linux-x86/bin
W=/home/alif/kernel/pack612
DT=/home/alif/kernel/dt612-out
VER=6.12.81-android16-6-maybe-dirty-4k
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
# Load the display stack in first stage, after the board modules.
{ # UFS_FIRST=1: storage first, so piano_blklog can log the rest of first stage.
  [ "${UFS_FIRST:-0}" = 1 ] && printf "%s\n" qcom_ice.ko phy-qcom-ufs.ko phy-qcom-ufs-qmp-v4-sun.ko ufshcd-crypto-qti.ko ufs-qcom.ko
  xargs -n1 basename < $DIST/vendor_boot.modules.load
  # Display and USB suppliers, so recovery (vendor_boot only) gets both.
  printf "%s\n" nvmem_qfprom.ko smp2p.ko phy-generic.ko phy-msm-ssusb-qmp.ko phy-msm-m31-eusb2.ko repeater.ko repeater-qti-pmic-eusb2.ko dwc3-msm.ko
  printf "%s\n" msm-mmrm.ko sync_fence.ko msm_ext_display.ko msm_hw_fence.ko ipclite.ko synx-driver.ko
} | awk "!seen[\$0]++" > $M/modules.load
# The vendor_boot partition is 96 MiB; drop debug info.
STRIP=$(ls -d $KP/prebuilts/clang/host/linux-x86/clang-r*/bin/llvm-strip | tail -1)
$STRIP --strip-debug $M/*.ko
depmod -b $W/dlkm $VER
for m in $(cat $M/modules.load); do test -f $M/$m || echo "MISSING $m"; done
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
$HOST/mkbootimg --header_version 4 --kernel $DIST/Image --os_version 17.0.0 --os_patch_level 2026-10 \
  --cmdline "piano_reset_at=${RESET_AT:-0} msm_drm.pianodbg_reset=${PIANODBG:-0} printk.devkmsg=on piano_oneshot=612-qcom panic=${PANIC:-3} softlockup_panic=1 hung_task_panic=1 hung_task_timeout_secs=10 qcom_dload_mode.download_mode=0 ignore_loglevel clk_ignore_unused pd_ignore_unused" \
  -o $W/out/boot.img
cp $DT/dtbo.img $W/out/dtbo.img
ls -la $W/out/*.img
test $(stat -c %s $W/out/vendor_boot.img) -le 100663296
sha256sum $W/out/*.img
echo PACK612_DONE
