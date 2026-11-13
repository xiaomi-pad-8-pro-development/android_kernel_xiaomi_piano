# Build one DDK module target and swap it into the 6.12 vendor_dlkm, then re-sign.
# usage: swapko.sh <bazel target> <module.ko> [<target> <module.ko> ...]
set -e
L=/tmp/piano-kernel-build.log
mv $L /tmp/piano-kernel-build-$(date +%Y%m%d-%H%M%S).log 2>/dev/null || true
exec >> $L 2>&1
KP=/home/alif/kernel/kp50/kernel_platform; ROM=${ROM:-/home/alif/android/evolution-x}; HOST=$ROM/out/host/linux-x86/bin; W=/home/alif/kernel/dlkm612
STRIP=$(ls -d $KP/prebuilts/clang/host/linux-x86/clang-r*/bin/llvm-strip | tail -1)
cd $KP
while [ $# -ge 2 ]; do
  T=$1; M=$2; shift 2
  echo "== build $T"
  tools/bazel build $T
  P=${T#//}; P=${P/://}
  K=$(find $KP/bazel-bin/$P -name $M | head -1); echo "ko $K"
  cp $K $W/vendor_dlkm/lib/modules/$M; $STRIP --strip-debug $W/vendor_dlkm/lib/modules/$M
done
FC=$(ls $ROM/out/target/product/piano/obj/ETC/file_contexts.bin_intermediates/file_contexts.bin)
$HOST/mkfs.erofs -zlz4hc --mount-point=/vendor_dlkm --file-contexts=$FC -T 0 $W/out/vendor_dlkm.img $W/vendor_dlkm >/dev/null
ls -la $W/out/vendor_dlkm.img
cd /home/alif/kernel && bash sign612.sh /home/alif/kernel/slota | tail -1
echo PIANO_KERNEL_EXIT=0
