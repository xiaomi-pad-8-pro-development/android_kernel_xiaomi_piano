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
  `input.patch` is against the same commit: copy
  `drivers/input/keyboard/nanosic_driver` to `vendor/xiaomi/input/nanosic`,
  `drivers/input/misc/xiaomi_hall.c` to `hall/`, `drivers/ntag/fm19511`
  to `ntag/` and `drivers/input/fingerprint/mi_fp` to `mi_fp/`, and apply
  it there. It builds the keyboard cover, hall sensor, NFC tag and
  fingerprint drivers out of tree.
  `perf.patch` is against the same commit: copy `drivers/xiaomi/`
  `{boottime,kshrink_slabd,mi_stack,mi_trace,mi_ubt,printk_enhance}`,
  `drivers/staging/mi_rmap_efficiency`, `drivers/thermal/xiaomi` (as
  `mi_thermal/`), `include/mi_ubt` and the mca headers
  `mca/common/mca_event.h` and `mca/smartchg/smart_chg_class.h` (under
  `include/`) to `vendor/xiaomi/perf`, drop the Kconfig files and
  `mi_ubt/test`, and apply it there. Build with
  `tools/bazel build //vendor/xiaomi/perf:sun_perf_perf`.
  `soc-repo.patch` goes in `kernel_platform/soc-repo`: it enables the IR
  blaster (Xiaomi's 6.6 `ir-spi`), the hardware RNG and USB audio
  offload.
  `ksu.patch` is optional and goes in `kernel_platform/common` after
  `common.patch`: SUSFS v2.3.0 (susfs4ksu `gki-android16-6.12`, with
  the `show_smaps_rollup` hunk redone for the KP 5.0 loop) and the
  KernelSU hooks. Check out KernelSU-Next as on 6.6
  (`pershoot/KernelSU-Next` `dev-susfs`, commit d21e0a66c887) at
  `kernel_platform/common/KernelSU-Next`. KSU and KSU_SUSFS default to y
  in its Kconfig, so applying the patch makes the root build, as
  `piano_ksu.config` does on 6.6.
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

## State (2026-11-04, r44)

- Android boots to the lock screen on the 6.12 kernel: display, GPU,
  audio (sound card and the four FS19xx speaker amps), Wi-Fi, Bluetooth,
  USB, touch and pen.
- `qcedev-mod_dlkm` hangs the SoC at about 1.3 s when it loads. Its
  devicetree node matches stock 6.6, so the fault is in the driver. It
  and `qcrypto-msm_dlkm` are left out of modules.load for now.
  The KP 5.0 `qce_open` adds a core IRQ, a TCSR SoC revision read and
  a recovery request; `securemsm-kernel.patch` logs each step as
  `PIANODBG qce_open <step>` so one boot shows where it stops.
- `pack612.sh` no longer passes `clk_ignore_unused pd_ignore_unused`
  (6.6 boots without them; `IGNORE_UNUSED=1` brings them back) and adds
  the 6.6 `firmware_class.path=/odm/firmware/o8`. The CPU idled at
  about 72 C with them; the cpuidle devicetree matches 6.6.
- Battery and charging work through the Xiaomi mca stack;
  `qti_battery_charger` stays out, as on 6.6.
- Camera, video and EVA work: the camera HAL lists all four cameras.
  The 6.12 base devicetree has no camera, video, EVA or MMRM nodes, so
  `dt612.sh` adds the 6.6 techpack overlays.
- 144 Hz, the IR blaster (`/dev/ir_spi`, IR HAL up) and the hardware RNG
  work. The keyboard cover and hall sensor drivers load and register
  their input devices; typing on the cover is not tested yet.
- LLCC heuristics, SLC MPAM (`mpam_msc`, `mpam_msc_slc`, `slc_mpam`) and
  `qcom_cpuss_sleep_stats_v4` are enabled for sun as on canoe; the first
  two need the 6.6 devicetree nodes that `piano-sun-additions.dtsi` now
  carries. Untested.
- Not ported, with the reason:
  - `c1dcvs_scmi_v2`, `dynpf_scmi`, `cpufreq_stats_scmi_v2/v3`,
    `qfprom_sys`, `phy_msm_snps_eusb2`, `leds_qcom_flash`: no sun or piano
    devicetree node on 6.6 either, so they loaded and never probed.
  - `spcom`, `spss_utils`: in `spu-kernel`, which supports sun at
    LA.VENDOR.16.2.6.r1-10900-kaanapali.0 (13be6e68a838); the 6.12
    sun.dtsi has their nodes. Check it out in `vendor/qcom/opensource`.
  - Xiaomi modules with no source in the 6.6 tree (millet, mi_game,
    mi_power, mist, perf_helper, perf_actuator, rtg, vip_sched,
    sched_penalty, xm_power, ...): 6.6 prebuilts, cannot be rebuilt.
  - `mi_async_reclaim` (5 folio trylock hooks) and `lz4asm`
    (`lz4_decompress_bypass`): their vendor hooks are not in the 6.12 GKI.
  - `cpq`: the block elevator API changed; needs a real port.
  - `ntsync`: `depends on BROKEN` in this 6.12; 6.6 carries a backport.
  - CIFS: needs `fs/smb` plus netfs and dns_resolver as GKI modules.
  - bpf small-buffer change (seventeen 0c6cc1d9bcab): 12 of 14 hunks
    miss on 6.12's syscall.c.
- Recovery on boot_a still needs the 6.6 boot images (`round612.sh`).
