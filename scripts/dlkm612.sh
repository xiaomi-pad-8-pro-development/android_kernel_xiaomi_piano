#!/usr/bin/env bash
# Pack 6.12 vendor_dlkm and system_dlkm images for a piano slot.
# Modules: KP 5.0 dist (vendor_dlkm list) plus the techpack modules built
# with bazel. Order: the 6.6 vendor_dlkm modules.load from the ROM build,
# then anything new. Labels: the ROM's vendor_dlkm file contexts.
set -euo pipefail
KP=/home/alif/kernel/kp50/kernel_platform
DIST=$KP/out/msm-kernel-sun-perf/dist
BIN=$KP/bazel-bin/vendor/qcom/opensource
ROM=/home/alif/android/evolution-12.3
OUTP=$ROM/out/target/product/piano
HOST=$ROM/out/host/linux-x86/bin
W=/home/alif/kernel/dlkm612
VER=6.12.81-android16-6-maybe-dirty-4k
STRIP=$(ls -d $KP/prebuilts/clang/host/linux-x86/clang-r*/bin/llvm-strip | tail -1)

rm -rf $W; mkdir -p $W/vendor_dlkm/lib/modules/$VER $W/out
M=$W/vendor_dlkm/lib/modules/$VER

# Vendor modules from dist.
xargs -n1 basename < $DIST/vendor_dlkm.modules.load | while read -r k; do
	cp $DIST/$k $M/
done
# Techpack modules (stripped staging copies), skipping ones already in
# vendor_boot.
find $BIN -path '*sun_perf*' -path '*/staging/*' -name '*.ko' | while read -r k; do
	b=$(basename $k)
	grep -qx "$b" $W/../pack612/dlkm/lib/modules/modules.load 2>/dev/null && continue
	cp --update=none $k $M/
done
$STRIP --strip-debug $M/*.ko

# Load order: 6.6 order first, then the rest.
{
	xargs -n1 basename < $OUTP/vendor_dlkm/lib/modules/modules.load | while read -r k; do
		if [ -f $M/$k ]; then echo $k; fi
	done
	ls $M | grep '\.ko$'
} | awk '!seen[$0]++' > $M/modules.load

depmod -b $W/vendor_dlkm $VER
# Flatten to /lib/modules like the stock image.
mv $M/* $W/vendor_dlkm/lib/modules/ && rmdir $M
sed -i -E "s#(^|[ :])([^ :]+\.ko)#\1/vendor_dlkm/lib/modules/\2#g" $W/vendor_dlkm/lib/modules/modules.dep
cp -a $OUTP/vendor_dlkm/etc $W/vendor_dlkm/ 2>/dev/null || true

FC=$(ls $OUTP/obj/ETC/file_contexts.bin_intermediates/file_contexts.bin)
$HOST/mkfs.erofs -zlz4hc --mount-point=/vendor_dlkm --file-contexts=$FC \
	-T 0 $W/out/vendor_dlkm.img $W/vendor_dlkm
cp $DIST/system_dlkm.flatten.erofs.img $W/out/system_dlkm.img

echo "vendor modules: $(grep -c . $W/vendor_dlkm/lib/modules/modules.load)"
ls -l $W/out
