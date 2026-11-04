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
  `drivers/staging/mi_rmap_efficiency`, `drivers/xiaomi/lz4asm`,
  `drivers/thermal/xiaomi` (as
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
  `PIANODBG qce_open <step>` so one boot shows where it stops. Static
  comparison: for a CE 5.x engine, which sun has (6.6 accepts no other),
  the KP 5.0 qce50/qcedev and the SPS BAM driver behave as on 6.6, and
  the devicetree and CE0 interconnect BCM match. What is left is the
  IOMMU attach of the context banks inside `of_platform_populate()`
  (the secure one, VMID 9, goes through TZ). `PIANODBG qcedev cb <label>`
  and `populate_done` mark that step, and `QCEDEV_NO_SCB=1 dt612.sh`
  drops the secure bank to test it.
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
- LLCC heuristics and SLC MPAM (`mpam_msc`, `mpam_msc_slc`, `slc_mpam`)
  are enabled for sun as on canoe, with the 6.6 devicetree nodes that
  `piano-sun-additions.dtsi` now carries; they load and bind (r46).
  `qcom_cpuss_sleep_stats_v4` stays off: with it, the SoC hard-resets
  about 1.4 s into boot while vendor_dlkm loads (r45 bisect).
- Not ported, with the reason:
  - `c1dcvs_scmi_v2`, `dynpf_scmi`, `cpufreq_stats_scmi_v2/v3`,
    `qfprom_sys`, `phy_msm_snps_eusb2`, `leds_qcom_flash`: no sun or piano
    devicetree node on 6.6 either, so they loaded and never probed.
  - `spcom`, `spss_utils`: in `spu-kernel`, which supports sun at
    LA.VENDOR.16.2.6.r1-10900-kaanapali.0 (13be6e68a838); the 6.12
    sun.dtsi has their nodes. Check it out in `vendor/qcom/opensource`.
  - Xiaomi modules with no source (not in the 6.6 tree nor in
    MiCode piano-w-oss): 31 prebuilts in the stock OS4.0.5.0.XPYCNXM
    vendor_boot (mem_object in vendor_dlkm; sched_penalty and mem_hooks
    are not in the ROM). A 6.6 .ko cannot load on 6.12 (vermagic,
    modversions CRCs, 6.6 struct layouts), so each needs a rewrite.
    Static check of their imports against the 6.12 build:
    - every vendor hook they use exists in 6.12; only
      android_vh_filemap_map_pages (io_monitor) changed prototype
      (adds orig_start_pgoff);
    - API changes a rewrite must handle: register_shrinker (node_cache;
      shrinker_alloc since 6.7), check_preempt_curr (vip_sched;
      wakeup_preempt), kfree_skb_reason (millet_core), log_read_mmio
      (mi_power; MMIO tracing is off), plus the kmalloc_trace,
      __kmalloc, cpu_number and fortify_panic renames that a rebuild
      picks up by itself;
    - millet_binder/hs/oem_cgroup/pkg, perf_actuator, rtg and
      plat_common import nothing missing on 6.12.
    Rewrites, rebuilt from disassembly of the stock .ko (static only,
    untested on a device), in `vendor/xiaomi/perf`:
    - perf_actuator: /dev/xr_perf_actuator and /dev/xr_fbt ioctl
      dispatch tables (43 and 12 entries, ioctl type 'x');
      register_perf_actuator/register_xr_fbt and the unregister calls
      are exported GPL as in the stock module.
    - plat_common: the "OS-CPU Platform Hook" callback table (41 slots)
      and its 36 exported plat_* wrappers, the cpu cgroup id lookup,
      the RT relaxed check and the fork timer-slack hook. Field offsets
      were named from the stock 6.6 kernel BTF in boot.img. Prototypes
      come from the kCFI hashes; 34 of 36 match the stock binary.
      plat_mi_set_next_freq and plat_game_choose_cpu_fastpath are not
      recovered (argument count only). The stock sched-walt.ko calls
      these wrappers; the KP 5.0 WALT does not, so on 6.12 only module
      callers reach them.
    - rtg: the five perf_actuator ioctls ('x' 0x02, 0x03, 0x04, 0x07,
      0x25) that move tasks into WALT related thread groups and set a
      group's preferred CPUs and util, and the exported os_cpu_* helpers.
      soc-repo.patch appends mi_prefer_cpus and mi_prefer_util to
      struct walt_related_thread_group (the stock module wrote them
      right after start_ktime_ts). The stock sched-walt.ko used the
      os_cpu_* helpers in task placement; the KP 5.0 WALT does not, so
      the values are stored but have no effect until WALT is patched.
    - millet_core: the NETLINK_MILLET (31) link between the HyperOS
      millet daemon and the millet modules: per-type handshake, routing
      and counters, the core_uid/uid_rec hash tables, the black-list
      counters and /proc/millet/{millet_stat,version}. Message layouts
      (kernel to user 0x90 bytes, user to kernel 0x50 bytes) and the
      module table follow the stock offsets; the three hook prototypes
      match the stock kCFI hashes. Only uids up to 1000 may send.
    - millet_hs: the HANDSHK type; echoes a daemon message back as an
      empty MSG_TO_USER message, routed through the SIG monitor.
    - millet_sig: android_vh_do_send_sig_info; reports SIGQUIT, SIGABRT,
      SIGKILL and SIGTERM sent to a frozen or freezing task (once per
      process in a row) so the daemon can thaw it.
    - millet_oem_cgroup: a can_attach check on the v1 freezer cgroup;
      only euid 1000 or CAP_SYS_ADMIN may move other tasks into it.
    - rt_mod: /dev/rt_load, ioctl 0x4c01 returns a {min prio, count}
      pair per possible CPU for the FIFO/RR/DEADLINE tasks; also the
      stock no-op kprobe on __sched_setscheduler. The task walk now
      holds rcu_read_lock (the stock code did not).
  - `snd_soc_hdmi_codec`: 6.6 loads it, but its only users are HDMI
    bridge chips piano lacks (adv7511, lt9611, anx7625, ...); DP audio
    goes through `msm_ext_display` and audio-kernel `msm_hdmi_codec_rx`.
  - `mi_async_reclaim` and `mem_reclaim_ctl`: not piano modules; on 6.6
    only bixi's module list carries mi_async_reclaim.
  - CIFS: builds, but does not fit in system_dlkm (7.84 MB against
    7389184 bytes), so `dlkm612.sh` leaves it out.
- Recovery: `pack612.sh` now puts the vendor_dlkm modules and the
  signed GKI modules they need in vendor_boot and writes
  `modules.load.recovery`, as the 6.6 vendor_boot does, so recovery on a
  6.12 slot does not need the dlkm partitions. Run `dlkm612.sh` before
  `pack612.sh`. Untested; `round612.sh` still restores the 6.6 images
  for recovery until it is.
