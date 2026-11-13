#!/usr/bin/env bash
# Repack the 6.12 vendor_dlkm so only the first N modules of modules.load load.
# usage: dlkm612-first.sh <N>
set -euo pipefail
ROM=${ROM:-/home/alif/android/evolution-x}; OUTP=$ROM/out/target/product/piano; HOST=$ROM/out/host/linux-x86/bin
W=/home/alif/kernel/dlkm612; N=$1
rm -rf $W/first-$N && cp -a $W/vendor_dlkm $W/first-$N
M=$W/first-$N/lib/modules
head -n $N $M/modules.load | grep -v -E "^(qcedev-mod_dlkm|qcrypto-msm_dlkm)\.ko$" > $M/l.tmp && mv $M/l.tmp $M/modules.load
FC=$(ls $OUTP/obj/ETC/file_contexts.bin_intermediates/file_contexts.bin)
$HOST/mkfs.erofs -zlz4hc --mount-point=/vendor_dlkm --file-contexts=$FC -T 0 $W/out/vendor_dlkm-first$N.img $W/first-$N >/dev/null
echo "first $N: $(tail -1 $M/modules.load)"; ls -l $W/out/vendor_dlkm-first$N.img
