#!/usr/bin/env bash
# Sign a 6.12 round for slot a, so it boots with the ROM's vbmeta flags 0.
# boot gets the slot's own boot rollback index and props; vendor_boot, dtbo
# and the dlkm images get footers, and vbmeta_a is the slot's own vbmeta_a
# with those four descriptors replaced. A vbmeta from another ROM build
# fails verification of the slot's other partitions.
# usage: sign612.sh <slot a backup dir with boot_a.img and vbmeta_a.img>
set -euo pipefail
R=/home/alif/android/evolution-12.3
AVB=$R/external/avb
KEY=$R/piano-local-keys/lineage-23.2-avb.pem
SLOT=$(realpath $1)
W=/home/alif/kernel/sign612
VD_SIZE=31830016 # vendor_dlkm_a on piano
SD_SIZE=7389184  # system_dlkm_a
avb() { python3 $AVB/avbtool.py "$@"; }

rm -rf $W; mkdir -p $W; cd $W
cp /home/alif/kernel/pack612/out/{boot,vendor_boot,dtbo}.img .
cp /home/alif/kernel/dlkm612/out/vendor_dlkm.img vendor_dlkm.img
cp /home/alif/kernel/dlkm612/out/system_dlkm.img system_dlkm.img

# boot: same rollback index and props as the slot's boot.
INFO=$(avb info_image --image $SLOT/boot_a.img)
RB=$(sed -n "s/^Rollback Index: *//p" <<< "$INFO")
PROPS=()
while IFS= read -r p; do PROPS+=(--prop "$p"); done < <(
	sed -n -E "s/^ *Prop: ([^ ]+) -> '(.*)'$/\1:\2/p" <<< "$INFO")
avb add_hash_footer --image boot.img --partition_name boot --partition_size 100663296 \
	--key $KEY --algorithm SHA256_RSA4096 --rollback_index $RB "${PROPS[@]}"

avb add_hash_footer --image vendor_boot.img --partition_name vendor_boot --partition_size 100663296
avb add_hash_footer --image dtbo.img --partition_name dtbo --partition_size 25165824
avb add_hashtree_footer --image vendor_dlkm.img --partition_name vendor_dlkm --partition_size $VD_SIZE \
	--hash_algorithm sha256 --do_not_generate_fec
avb add_hashtree_footer --image system_dlkm.img --partition_name system_dlkm --partition_size $SD_SIZE \
	--hash_algorithm sha256 --do_not_generate_fec

python3 - $AVB $KEY $SLOT/vbmeta_a.img <<'EOF'
import sys
sys.path.insert(0, sys.argv[1])
import avbtool
a = avbtool.Avb()
def descs(p):
    _, _, d, _ = a._parse_image(avbtool.ImageHandler(p, read_only=True))
    return d
NEW = ("vendor_boot.img", "dtbo.img", "system_dlkm.img", "vendor_dlkm.img")
new = sum((descs(p) for p in NEW), [])
names = {d.partition_name for d in new}
keep = [d for d in descs(sys.argv[3]) if getattr(d, "partition_name", None) not in names]
blob = a._generate_vbmeta_blob("SHA256_RSA4096", sys.argv[2], None, keep + new,
    None, None, 0, 0, 0, None, None, None, False, None, None, None, None,
    "avbtool 1.4.0", None, 0)
open("vbmeta_a-612.img", "wb").write(blob + b"\0" * (131072 - len(blob)))
EOF
# verify_image wants every partition of the slot, so list vbmeta instead.
avb info_image --image vbmeta_a-612.img | grep -E "^Flags|Partition Name" | tr -s " " | tr "\n" " "; echo
avb verify_image --image boot.img --key $KEY | grep -E "^boot: Successfully"
sha256sum *.img
echo SIGN612_DONE
