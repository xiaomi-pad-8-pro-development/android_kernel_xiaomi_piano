#!/usr/bin/env bash
# Pack 6.12 vendor_dlkm and system_dlkm images for a piano slot.
# Modules: KP 5.0 dist (vendor_dlkm list) plus the techpack modules built
# with bazel. Order: the 6.6 vendor_dlkm modules.load from the ROM build,
# then anything new. Labels: the ROM's vendor_dlkm file contexts.
set -euo pipefail
KP=/home/alif/kernel/kp50/kernel_platform
DIST=$KP/out/msm-kernel-sun-perf/dist
BIN=$KP/bazel-bin/vendor/qcom/opensource
ROM=/home/alif/android/evolution-x
OUTP=$ROM/out/target/product/piano
HOST=$ROM/out/host/linux-x86/bin
W=/home/alif/kernel/dlkm612
VER=$(basename $(ls -d /home/alif/kernel/kp50/kernel_platform/bazel-bin/vendor/qcom/opensource/mm-drivers/msm_ext_display/sun_perf_msm_ext_display/staging/lib/modules/* -td | head -1))
STRIP=$(ls -d $KP/prebuilts/clang/host/linux-x86/clang-r*/bin/llvm-strip | tail -1)

rm -rf $W; mkdir -p $W/vendor_dlkm/lib/modules/$VER $W/out
M=$W/vendor_dlkm/lib/modules/$VER

# Vendor modules from dist.
xargs -n1 basename < $DIST/vendor_dlkm.modules.load | while read -r k; do
	cp $DIST/$k $M/
done
# Techpack modules (stripped staging copies), skipping the first-stage
# (vendor_boot) ones. Use the dist list for that, not pack612's: with
# ALL_IN_RAMDISK that holds every module and vendor_dlkm shrinks each round.
# bazel-bin keeps outputs of targets a build did not ask for, and those
# fail to load against a newer kernel. build612.sh lists what it built in
# $BUILT; take only those (all of bazel-bin without the list).
BUILT=$KP/out/piano-modules.list
find $BIN $KP/bazel-bin/vendor/xiaomi -path '*sun_perf*' -path '*/staging/*' -name '*.ko' | while read -r k; do
	b=$(basename $k)
	grep -q "/$b\$" $DIST/vendor_boot.modules.load && continue
	if [ -f $BUILT ] && ! grep -q "/$b\$" $BUILT; then echo "not built this round, left out: $b"; continue; fi
	cp --update=none $k $M/
done
# Xiaomi perf modules: take them from the package output directly, the
# staging search above misses the new ones.
ls $KP/bazel-bin/vendor/xiaomi/perf/sun_perf_perf/*.ko | while read -r k; do
	b=$(basename $k)
	grep -q "/$b\$" $DIST/vendor_boot.modules.load && continue
	cp --update=none $k $M/
done
$STRIP --strip-debug $M/*.ko

# Load order: 6.6 order first, then the rest. The crypto modules load since
# the v1.7 4K SPS layout fix (r79); QCE_EXCLUDE="qcedev-mod_dlkm|qcrypto-msm_dlkm|"
# leaves them out again.
# Xiaomi's mca stack drives the battery over pmic_glink, so Qualcomm's
# qti_battery_charger stays out too, as on 6.6. Remove the file as well:
# mca_qcom_subpmic_proxy exports the same qti_battery_charger_get_prop
# that the haptics and flash LED modules use, and depmod must pick it.
rm -f $M/qti_battery_charger.ko
# video.ko is the alternative video driver for the same nodes; 6.6 loads
# msm_video only.
rm -f $M/video.ko
# The bootmonitor, dump_display and swinfo ports stop a full ROM install
# from booting (it falls back to recovery early, before pstore survives).
# Keep them out until each is tested on its own.
rm -f $M/bootmonitor.ko $M/dump_display.ko $M/swinfo.ko
# Debug only or not piano hardware: the coresight/STM tracing modules and
# the WCD937x and simple-amp codecs (piano uses WCD9378 and the FS19xx
# amps), and the LT9611 HDMI bridges (piano has no HDMI output).
# coresight.ko itself stays: msm_kgsl depends on it. Stop if a kept
# module depends on a dropped one.
# The WLAN host driver builds once per chip; piano has peach v2 (WCN7880),
# and the others would overflow the 31.8 MB partition. The test drivers
# are Qualcomm bring-up tools.
DROP=$(cd $M && ls coresight[-_]*.ko stm[-_]*.ko wcd937x*.ko simple[-_]amp*.ko lt9611*.ko \
	qca_cld3_*.ko *_test.ko seccam_test_driver.ko 2>/dev/null | grep -vx qca_cld3_peach_v2.ko || true)
for k in $DROP; do rm -f $M/$k; done
for k in $M/*.ko; do
	for d in $(modinfo -F depends $k | tr , ' '); do
		if ! ls $M/$d.ko $M/${d//_/-}.ko >/dev/null 2>&1 && echo "$DROP" | grep -qE "^(${d}|${d//_/-})\.ko$"; then
			echo "$(basename $k) needs dropped $d"; exit 1
		fi
	done
done
# system_dlkm_a is 7 MiB: strip the GKI modules and compress harder.
$HOST/fsck.erofs --extract=$W/system_dlkm $DIST/system_dlkm.flatten.erofs.img >/dev/null
find $W/system_dlkm -name '*.ko' -exec $STRIP --strip-debug {} +
# Stripping drops the appended signature, and GKI refuses unsigned
# system_dlkm modules (EACCES), so sign them again with the build's key.
# The prebuilt sign-file only does SHA-1, so use openssl and append the
# same trailer: struct module_signature (PKCS#7, big-endian length), magic.
CERTS=$KP/bazel-bin/common/kernel_aarch64/certs
for k in $W/system_dlkm/lib/modules/*.ko; do
	openssl cms -sign -binary -noattr -nocerts -nosmimecap -md sha256 -outform DER \
		-signer $CERTS/signing_key.x509 -inkey $CERTS/signing_key.pem \
		-in $k -out $k.p7s
	python3 -c 'import struct,sys; k,s=sys.argv[1:]; sig=open(s,"rb").read()
open(k,"ab").write(sig + struct.pack(">BBBBB3xI", 0, 0, 2, 0, 0, len(sig)) + b"~Module signature appended~\n")' $k $k.p7s
	rm $k.p7s
done
# The CIFS set is GKI, but with it system_dlkm overflows its 7389184-byte
# partition, so it ships in vendor_dlkm, as on 6.6. The files keep the GKI
# signature added above. Their dependency netfs stays in system_dlkm.
# Loading the set at boot hangs 6.12 (r75, r78), so it stays out of
# modules.load; load it when needed with `modprobe -d /vendor_dlkm/lib/modules cifs`.
CIFS="dns_resolver cifs_arc4 cifs_md4 nls_ucs2_utils cifs"
for k in $CIFS; do mv $W/system_dlkm/lib/modules/$k.ko $M/; done
{
	xargs -n1 basename < $OUTP/vendor_dlkm/lib/modules/modules.load | while read -r k; do
		if [ -f $M/$k ]; then echo $k; fi
	done
	ls $M | grep '\.ko$'
} | grep -v -x -E "($(echo $CIFS | tr ' ' '|'))\.ko" | grep -v -E "^(${QCE_EXCLUDE-}qti_battery_charger|video)\.ko$" | awk '!seen[$0]++' > $M/modules.load

depmod -b $W/vendor_dlkm $VER
# Flatten to /lib/modules like the stock image.
mv $M/* $W/vendor_dlkm/lib/modules/ && rmdir $M
sed -i -E "s#(^|[ :])([^ :]+\.ko)#\1/vendor_dlkm/lib/modules/\2#g" $W/vendor_dlkm/lib/modules/modules.dep
cp -a $OUTP/vendor_dlkm/etc $W/vendor_dlkm/ 2>/dev/null || true

FC=$(ls $OUTP/obj/ETC/file_contexts.bin_intermediates/file_contexts.bin)
$HOST/mkfs.erofs -zlz4hc --mount-point=/vendor_dlkm --file-contexts=$FC \
	-T 0 $W/out/vendor_dlkm.img $W/vendor_dlkm
# Drop GKI modules piano has no use for, so the image fits. Nothing
# depends on them. $CIFS moved to vendor_dlkm above; this only drops
# its system_dlkm modules.load, modules.dep and modules.alias entries.
SM=$W/system_dlkm/lib/modules
for k in $CIFS aes-neon-blk efivarfs exynos-hvm gnss pstore_blk ptp_kvm rust_binder \
		snd-hda-codec-hdmi tipc_diag xhci-pci-renesas; do
	rm -f $SM/$k.ko
	sed -i "/^$k\.ko\$/d" $SM/modules.load
	sed -i "\#/$k\.ko:#d" $SM/modules.dep
	sed -i "/ ${k//-/_}\$/d" $SM/modules.alias
done
$HOST/mkfs.erofs -zlz4hc,12 -C65536 --mount-point=/system_dlkm --file-contexts=$FC \
	-T 0 $W/out/system_dlkm.img $W/system_dlkm

echo "vendor modules: $(grep -c . $W/vendor_dlkm/lib/modules/modules.load)"
ls -l $W/out
