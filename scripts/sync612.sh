#!/usr/bin/env bash
# Copy this source tree into the KP 5.0 build tree, so build612.sh builds
# exactly what the 6.12 branch holds. Each part replaces the matching
# folder in kernel_platform; the build outputs and the Qualcomm build
# system there are left alone.
set -euo pipefail
SRC=$(cd "$(dirname "$0")/.." && pwd)
KP=${KP:-/home/alif/kernel/kp50}
sync() { mkdir -p "$KP/$2"; rsync -a --delete --exclude=.git "$SRC/$1/" "$KP/$2/"; echo "synced $1"; }
sync kernel_platform/common kernel_platform/common
sync kernel_platform/soc-repo kernel_platform/soc-repo
sync kernel_platform/qcom/opensource/devicetree kernel_platform/qcom/opensource/devicetree
for d in "$SRC"/vendor/qcom/opensource/*/; do n=$(basename "$d"); sync vendor/qcom/opensource/$n vendor/qcom/opensource/$n; done
for d in "$SRC"/vendor/xiaomi/*/; do n=$(basename "$d"); sync vendor/xiaomi/$n vendor/xiaomi/$n; done
echo SYNC612_DONE
