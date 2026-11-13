# alxp kernel for the Xiaomi Pad 8 Pro (piano)

| | |
| --- | --- |
| Linux | 6.6 (android15-6.6 GKI) |
| Base | Xiaomi Pad 8 history, replayed on piano |
| Build | [auto](https://github.com/xiaomi-pad-8-pro-development/auto) |
| Output | [prebuilts](https://github.com/xiaomi-pad-8-pro-development/android_device_xiaomi_piano-kernel) |

| Config | |
| --- | --- |
| `piano_defconfig` | Plain |
| `piano_ksu.config` | + KernelSU-Next, SUSFS |

| Branch | What |
| --- | --- |
| `6.6` | Daily kernel, used by the ROM |
| `6.12` | 6.12 bring-up, as patches on Qualcomm KP 5.0 |
| `6.12-source` | The same 6.12 work as one full source tree |

Android Common Kernel contribution rules: [README.ACK.md](README.ACK.md)
