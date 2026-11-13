#!/bin/zsh
# usage: r612.sh N   -> flash 6.12 with first N modules on slot a, wait for fastboot, restore, decode log
N=$1; S=/private/tmp/claude-501/-Volumes-XiaomiPad8Pro-xiaomi-pad-8-pro/28244d58-1434-465a-a237-1f6c5905841c/scratchpad/t612; K=~/piano-builds/slota-backup-20261102; B=~/piano-builds/t612-b6
adb wait-for-device; adb reboot recovery; sleep 20; for i in $(seq 40); do adb get-state 2>/dev/null | grep -q recovery && break; sleep 3; done
adb push $S/system_dlkm.img /tmp/s.img >/dev/null && adb push $S/vendor_dlkm-first$N.img /tmp/v.img >/dev/null && adb shell 'dd if=/tmp/s.img of=/dev/block/mapper/system_dlkm_a bs=1M conv=fsync && dd if=/tmp/v.img of=/dev/block/mapper/vendor_dlkm_a bs=1M conv=fsync && sync && echo DD_OK' || exit 1
adb reboot bootloader; for i in $(seq 60); do fastboot devices | grep -q . && break; sleep 2; done
fastboot getvar current-slot 2>&1 | head -1 | grep -q ': a' || exit 2
fastboot flash boot_a $B/boot.img && fastboot flash vendor_boot_a $B/vendor_boot.img && fastboot flash dtbo_a $B/dtbo.img && fastboot flash vbmeta_a $S/vbmeta_a-noverify.img && fastboot reboot || exit 3
echo BOOTING_612; sleep 20; until fastboot devices | grep -q .; do sleep 2; done
fastboot flash boot_a $K/boot_a.img; fastboot flash vendor_boot_a $K/vendor_boot_a.img; fastboot flash dtbo_a $K/dtbo_a.img; fastboot flash vbmeta_a $K/vbmeta_a.img; fastboot set_active a; fastboot reboot recovery
for i in $(seq 60); do adb get-state 2>/dev/null | grep -q recovery && break; sleep 3; done
adb shell 'dd if=/dev/block/by-name/rawdump of=/tmp/rd.bin bs=1M count=8' && adb pull /tmp/rd.bin $S/rawdump-first$N.bin
adb push $K/system_dlkm.img /tmp/s.img && adb push $K/vendor_dlkm.img /tmp/v.img && adb shell 'dd if=/tmp/s.img of=/dev/block/mapper/system_dlkm_a bs=1M conv=fsync && dd if=/tmp/v.img of=/dev/block/mapper/vendor_dlkm_a bs=1M conv=fsync && sync && echo RESTORE_OK' && adb reboot
python3 -c "
import struct;d=open('$S/rawdump-first$N.bin','rb').read();s,l=struct.unpack('<II',d[8:16]);open('$S/blklog-first$N.txt','w').write(d[4096:4096+l].decode('utf8','replace'))"
tail -3 $S/blklog-first$N.txt | cut -c1-160
