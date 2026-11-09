#!/bin/zsh
# Chained 6.12 round on slot a, no restore: from recovery or a running 6.12, write the
# test images, boot 6.12, hold, capture, then end in recovery with the kernel log pulled.
# Restore slot a with restore612.sh when testing is over.
N=${1:?round}; S=${T612:-$HOME/piano-builds/t612-logs}
B=/Users/alif/piano-builds/t612-signed
mkdir -p $S/$N
K=/Users/alif/piano-builds/slota-backup-20261103
# Recovery runs on the boot_a kernel, and 6.12 cannot map the dlkm partitions:
# put the 6.6 boot images back (dlkm stays) before going to recovery.
to_recovery() {
  for i in $(seq 100); do
    st=$(adb get-state 2>/dev/null)
    if [ "$st" = recovery ] && adb shell uname -r 2>/dev/null | grep -q '^6\.6'; then return 0; fi
    [ -n "$st" ] && adb reboot bootloader
    if fastboot devices | grep -q .; then
      fastboot getvar current-slot 2>&1 | head -1 | grep -q ': a' || return 1
      fastboot flash boot_a $K/boot_a.img && fastboot flash vendor_boot_a $K/vendor_boot_a.img &&
        fastboot flash dtbo_a $K/dtbo_a.img && fastboot flash vbmeta_a $K/vbmeta_a.img &&
        fastboot reboot recovery
      for j in $(seq 40); do adb get-state 2>/dev/null | grep -q recovery && break; sleep 3; done
    fi
    sleep 3
  done
  return 1
}
to_recovery || { echo NO_RECOVERY; exit 1; }
for i in $(seq 40); do adb shell 'test -e /dev/block/mapper/system_dlkm_a && test -e /dev/block/mapper/vendor_dlkm_a' 2>/dev/null && break; sleep 3; done
adb push $B/sd.img /tmp/s.img >/dev/null && adb push $B/vd.img /tmp/v.img >/dev/null && adb shell 'dd if=/tmp/s.img of=/dev/block/mapper/system_dlkm_a bs=1M conv=fsync && dd if=/tmp/v.img of=/dev/block/mapper/vendor_dlkm_a bs=1M conv=fsync && sync && echo DD_OK' || exit 1
adb reboot bootloader; for i in $(seq 60); do fastboot devices | grep -q . && break; sleep 2; done
fastboot getvar current-slot 2>&1 | head -1 | grep -q ': a' || exit 2
fastboot flash boot_a $B/boot.img && fastboot flash vendor_boot_a $B/vb.img && fastboot flash dtbo_a $B/dtbo.img && fastboot flash vbmeta_a $B/vbmeta_a-612.img && fastboot reboot || exit 3
echo BOOTING_612 $(date +%T); sleep 15
GOT=none
for i in $(seq 200); do
  if fastboot devices | grep -q .; then GOT=fastboot; break; fi
  if adb get-state 2>/dev/null | grep -q device; then GOT=adb; break; fi
  sleep 3
done
echo "612_RESULT=$GOT $(date +%T)"
if [ $GOT = adb ]; then
  sleep ${HOLD:-60}
  adb shell 'uname -r; getprop sys.boot_completed; getprop init.svc.surfaceflinger; ls -l /dev/kgsl-3d0; cat /sys/class/kgsl/kgsl-3d0/gpu_model 2>&1' > $S/$N/state.txt 2>&1
  adb shell dumpsys SurfaceFlinger > $S/$N/sf.txt 2>&1
  adb logcat -d -b all > $S/$N/logcat.txt 2>&1
  adb shell screencap -p /data/local/tmp/sc.png 2>/dev/null && adb pull /data/local/tmp/sc.png $S/$N/screen.png >/dev/null 2>&1
fi
[ $GOT = none ] && echo "NO_DEVICE: press the combo for fastboot"
to_recovery || { echo NO_RECOVERY; exit 4; }
adb shell 'dd if=/dev/block/by-name/rawdump of=/tmp/rd.bin bs=1M count=8' >/dev/null 2>&1 && adb pull /tmp/rd.bin $S/rawdump-$N.bin >/dev/null
python3 -c "
import struct;d=open('$S/rawdump-$N.bin','rb').read();s,l=struct.unpack('<II',d[8:16]);open('$S/blklog-$N.txt','w').write(d[4096:4096+l].decode('utf8','replace'))"
echo ROUND_DONE_IN_RECOVERY
