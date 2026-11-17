#!/usr/bin/env bash
# Rebuild the EvoX test ROM with clean ROM patches and the 6.12 kernel.
set -uo pipefail
export PATH=$HOME/bin:$HOME/.local/bin:$PATH
L=/tmp/piano-build.log; exec > >(tee -a $L) 2>&1
say() { echo "=== $(date +%H:%M) $*"; }
R=/home/alif/android/evolution-x; cd $R
P="build/blueprint frameworks/base frameworks/native hardware/qcom-caf/sm8750/audio/pal hardware/qcom-caf/sm8750/audio/primary-hal hardware/qcom-caf/sm8750/display/core hardware/qcom-caf/sm8750/display/hal packages/apps/FaceUnlock packages/apps/Launcher3 packages/apps/LineageParts packages/apps/Settings packages/modules/Bluetooth vendor/pixel-style"
say "reset patched projects"
for p in $P; do [ -d $p ] && git -C $p reset -q --hard && git -C $p clean -fdq; done
.repo/repo/repo sync -l -d $P || { say RESET_FAIL; exit 1; }
git -C device/xiaomi/piano-kernel checkout -q . && git -C device/xiaomi/piano-kernel clean -fdq
say stage; bash ~/kernel/stage612.sh || { say STAGE_FAIL; exit 1; }
export PIANO_AVB_KEY_PATH=piano-local-keys/lineage-23.2-avb.pem
source build/envsetup.sh > /tmp/envsetup.log 2>&1; grep -iE "fail|error" /tmp/envsetup.log && { say PATCH_FAIL; exit 1; }
lunch lineage_piano-cp2a-userdebug
say "rom build"; m evolution; E=$?; say "BUILD EXIT $E"
Z=$(ls -t out/target/product/piano/EvolutionX-*.zip | head -1); ls -l $Z
strings out/target/product/piano/kernel | grep -m1 "Linux version" | cut -c1-40
[ $E = 0 ] && say REL612_FINAL_DONE
