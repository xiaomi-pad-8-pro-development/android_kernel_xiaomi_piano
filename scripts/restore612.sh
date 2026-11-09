#!/bin/zsh
# End of 6.12 testing: put slot a back to the daily 6.6 build and boot it.
K=/Users/alif/piano-builds/slota-backup-20261103
for i in $(seq 100); do
  st=$(adb get-state 2>/dev/null)
  [ "$st" = recovery ] && break
  [ "$st" = device ] && adb reboot recovery
  fastboot devices | grep -q . && fastboot reboot recovery
  sleep 3
done
adb push $K/system_dlkm.img /tmp/s.img >/dev/null && adb push $K/vendor_dlkm.img /tmp/v.img >/dev/null && adb shell 'dd if=/tmp/s.img of=/dev/block/mapper/system_dlkm_a bs=1M conv=fsync && dd if=/tmp/v.img of=/dev/block/mapper/vendor_dlkm_a bs=1M conv=fsync && sync && echo DLKM_OK' || exit 1
adb reboot bootloader; for i in $(seq 60); do fastboot devices | grep -q . && break; sleep 2; done
fastboot flash boot_a $K/boot_a.img && fastboot flash vendor_boot_a $K/vendor_boot_a.img && fastboot flash dtbo_a $K/dtbo_a.img && fastboot flash vbmeta_a $K/vbmeta_a.img && fastboot set_active a && fastboot reboot && echo RESTORE_OK
