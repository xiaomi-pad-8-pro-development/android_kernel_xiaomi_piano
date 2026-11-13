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
7. perf.patch is behind the builder tree by about 30 Xiaomi module
   files; some may belong in xiaomi-ports.patch.

## Release cleanup

8. Remove the bring-up options: memcon logging and reset, forced USB
   device mode, skip_old_modules, the Kconfig.debug default.
9. Drop the Coresight and STM debug modules (about 20).
10. Full test on 6.12.96: Wi-Fi speed, charging, audio, fingerprint,
    camera, pen, reading mode, overnight sleep and battery drain.

## Not needed on piano

Satellite modem (tiantong, rsmc, wl2868c), other speaker and haptic
amps (sia91xx, cs40l26), the lt9611uxc HDMI bridge, mtdoops crash
storage, diag. USB tethering uses NCM, which 6.12 builds in; only RNDIS
is missing.
