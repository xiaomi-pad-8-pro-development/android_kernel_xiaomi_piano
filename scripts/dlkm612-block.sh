#!/usr/bin/env bash
# Repack the 6.12 vendor_dlkm with some modules kept from loading.
# usage: dlkm612-block.sh <tag> <module.ko>...
set -euo pipefail
ROM=${ROM:-/home/alif/android/evolution-x}; OUTP=$ROM/out/target/product/piano; HOST=$ROM/out/host/linux-x86/bin
W=/home/alif/kernel/dlkm612; T=$1; shift
rm -rf $W/blk-$T && cp -a $W/vendor_dlkm $W/blk-$T
M=$W/blk-$T/lib/modules
for k in "$@"; do grep -vxF "$k" $M/modules.load > $M/l.tmp && mv $M/l.tmp $M/modules.load; echo "blocklist ${k%.ko}" >> $M/modules.blocklist; done
FC=$(ls $OUTP/obj/ETC/file_contexts.bin_intermediates/file_contexts.bin)
$HOST/mkfs.erofs -zlz4hc --mount-point=/vendor_dlkm --file-contexts=$FC -T 0 $W/out/vendor_dlkm-$T.img $W/blk-$T >/dev/null
cat $M/modules.blocklist; wc -l < $M/modules.load; ls -l $W/out/vendor_dlkm-$T.img
