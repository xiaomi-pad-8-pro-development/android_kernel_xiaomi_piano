# Kernel 6.12 bring-up for the Xiaomi Pad 8 Pro (piano)

Research branch, not used by any ROM build. The daily kernel stays on
`seventeen` (android15-6.6).

The 6.12 kernel is Qualcomm KERNEL.PLATFORM.5.0 r48-00400 (ACK
android16-6.12.81 plus the sun SoC drivers). Xiaomi released no 6.12
source, so piano's own drivers are ported from 6.6.

## Layout

- `patches/` holds one diff per KP 5.0 project, against the commit in
  `patches/BASE`. Apply them in a KP 5.0 checkout:
  `kernel_platform/common`, `vendor/qcom/opensource/<project>`.
  `touch-driver.patch` is against the 6.6 `techpack/touch-driver` of
  this repository (the commit in `BASE`); copy that folder to
  `vendor/xiaomi/touch-driver` and apply it there.
  `mca.patch` is against the commit in its `BASE` line: copy
  `drivers/power/supply/mca` to `vendor/xiaomi/mca`, with
  `drivers/misc/hwid` as `hwid/`, `drivers/staging/miev` as `miev/` and
  `include/miev` as `include/miev`, and apply it there. It builds the
  Xiaomi charging stack out of tree, with Qualcomm's 6.12 fsa4480 USB-C
  switch driver, which the sun build leaves out.
  `camera-kernel.patch` and `eva-kernel.patch` are against the same
  commit: copy `techpack/camera-kernel` and `techpack/eva-kernel` to
  `vendor/qcom/opensource/` and apply them there. Xiaomi's camera driver
  is kept rather than Qualcomm's 6.12 one, since the HyperOS camera HAL
  is built against its interface.
- `scripts/` builds and tests it:
  - build: `./build_with_bazel.py -t sun perf -s abl` in `kernel_platform`
  - `dt612.sh` merges the piano devicetree, `pack612.sh` packs boot,
    vendor_boot and dtbo
  - `dlkm612.sh` builds vendor_dlkm; `dlkm612-first.sh <N>` keeps only
    the first N modules of modules.load, for bisecting
  - `r612.sh <N>` (Mac) flashes a round on slot a, waits for fastboot,
    restores the 6.6 slot and saves the boot log

## Debugging

`common.patch` adds two boot logs, since pstore does not survive a
reboot on piano: `piano_memcon` (early RAM console) and `piano_blklog`,
which writes the kernel log to the `rawdump` partition. The header is
`PIANOLOG`, then u32 seq and u32 length, with the text at offset 4096.

## State (2026-11-04)

- Android boots to the lock screen on the 6.12 kernel: display, GPU,
  audio (sound card and the four FS19xx speaker amps), Wi-Fi, Bluetooth,
  USB, touch and pen.
- `qcedev-mod_dlkm` hangs the SoC at about 1.3 s when it loads. Its
  devicetree node matches stock 6.6, so the fault is in the driver. It
  and `qcrypto-msm_dlkm` are left out of modules.load for now.
- Battery and charging work through the Xiaomi mca stack;
  `qti_battery_charger` stays out, as on 6.6.
- Camera, video and EVA work: the camera HAL lists all four cameras.
  The 6.12 base devicetree has no camera, video, EVA or MMRM nodes, so
  `dt612.sh` adds the 6.6 techpack overlays.
- Missing: the hall sensor and 144 Hz.
