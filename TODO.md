# 6.12 to-do for the Xiaomi Pad 8 Pro (piano)

What 6.12 still needs before it can replace 6.6, in priority order.
Updated 2026-11-13.

## Blocks daily use

1. Boot 6.12.96. The kernel has not started on the tablet yet; install
   the full test ROM to slot b (never flash the slot-a safe pack there).
2. Refresh-rate switching. The white static on every switch is fixed in
   display-drivers.patch (timing command at post-on only); test it.
3. ThinLTO and BBR. Both are in common.patch but not built yet. If
   ThinLTO does not boot, drop it and keep BBR.

## Missing compared with 6.6

4. Xiaomi performance modules (perfmgr, mi_sched, mist, vip_sched,
   mi_game) have no source. Either reverse-engineer them or tune the
   Qualcomm WALT scheduler that 6.12 already has.
5. Charging. Works from a computer port on 6.12.81; Xiaomi adapter fast
   charging is untested.
6. CIFS hangs boot when loaded early, so it is left out. Load it after
   boot or find what it waits for.
7. Build and test the Xiaomi modules added from dada-closed-modules
   (mi_mempool, minet, mi_wmark, scene_swappiness, mist and its hooks).
   They are in the patches but have not been compiled on 6.12 yet.

## Release cleanup

8. Build the release with RELEASE=1, which turns off the memcon RAM
   console, the block log and the reset checkpoints. The ROM build still
   adds piano_skip_old_modules, dwc3_msm.piano_force_peripheral and the
   panic options to the command line; drop them once USB role switching
   and module loading are tested without them.
9. Done: the Coresight, STM, test and LT9611 HDMI bridge modules are left
   out of vendor_dlkm by dlkm612.sh.
10. Full test on 6.12.96: Wi-Fi speed, charging, audio, fingerprint,
    camera, pen, reading mode, overnight sleep and battery drain.

## Not needed on piano

Satellite modem (tiantong, rsmc, wl2868c), other speaker and haptic
amps (sia91xx, cs40l26), the lt9611uxc HDMI bridge, mtdoops crash
storage, diag. USB tethering uses NCM, which 6.12 builds in; only RNDIS
is missing.
