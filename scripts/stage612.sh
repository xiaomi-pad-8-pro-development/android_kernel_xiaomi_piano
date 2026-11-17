#!/usr/bin/env bash
# Put the 6.12 kernel into the EvoX tree's device/xiaomi/piano-kernel
# (working tree only, never committed) for a full-install test build.
# Undo: git -C device/xiaomi/piano-kernel checkout . && git clean -fdq
set -euo pipefail
R=/home/alif/android/evolution-x; PK=$R/device/xiaomi/piano-kernel
KP=/home/alif/kernel/kp50/kernel_platform
DT=/home/alif/kernel/dt612-out; D=/home/alif/kernel/dlkm612; P=/home/alif/kernel/pack612
test -z "$(git -C $PK status --porcelain)" || { echo "piano-kernel not clean"; exit 1; }
# The dist Image from the same build612.sh run: dlkm612.sh signs the
# system_dlkm modules with its key, and any other Image refuses them.
cp ${PIANO_612_IMAGE:-/home/alif/kernel/kp50/kernel_platform/out/msm-kernel-sun-perf/dist/Image} $PK/Image
rm -f $PK/dtb/*.dtb; cp $DT/dtb $PK/dtb/sun612.dtb
cp $DT/dtbo.img $PK/dtbo.img
# Keep the fragment LZ4 compressed like the 6.6 prebuilt: raw it is 93 MB
# and ABL never starts the kernel.
cp $P/out/dlkm.lz4 $PK/modules/vendor_boot/dlkm.cpio
for p in vendor_dlkm system_dlkm; do
  O=$PK/modules/$p; B=$(ls $O/modules.blocklist 2>/dev/null && cp $O/modules.blocklist /tmp/bl.$p || true)
  rm -rf $O; mkdir -p $O
  cp $D/$p/lib/modules/*.ko $O/
  sed 's#.*/##' $D/$p/lib/modules/modules.load > $O/modules.load
  [ -f /tmp/bl.$p ] && cp /tmp/bl.$p $O/modules.blocklist || true
done
touch $PK/modules/vendor_dlkm/modules.blocklist
git -C $PK status --short | wc -l
echo STAGE612_DONE
