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

## State (2026-11-02)

- Kernel, display, UFS and first and second stage init work.
- `qcedev-mod_dlkm` hangs the SoC at about 1.3 s when it loads. Its
  devicetree node matches stock 6.6, so the fault is in the driver. It
  and `qcrypto-msm_dlkm` are left out of modules.load for now.
- A second hang is in modules.load entries 233 to 238.
- With 232 modules Android reaches keystore and qseecom, but KeyMint
  returns -38 when vold unwraps the metadata key, /data stays locked,
  apexd activates nothing and init reboots for `netbpfload-missing`.
- No charging driver yet: never leave the tablet idle on 6.12.
