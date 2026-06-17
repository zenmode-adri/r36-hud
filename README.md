# R36 HUD

[![GitHub release](https://img.shields.io/github/v/release/zenmode-adri/r36-hud?style=flat-square)](https://github.com/zenmode-adri/r36-hud/releases)
[![GitHub downloads](https://img.shields.io/github/downloads/zenmode-adri/r36-hud/total?style=flat-square)](https://github.com/zenmode-adri/r36-hud/releases)
[![GitHub stars](https://img.shields.io/github/stars/zenmode-adri/r36-hud?style=flat-square)](https://github.com/zenmode-adri/r36-hud/stargazers)
[![Ko-fi](https://img.shields.io/badge/Ko--fi-Support-FF5E5B?logo=ko-fi&style=flat-square)](https://ko-fi.com/zenmodeadri)

Real-time HUD overlay for R36S and compatible RK3326 devices running [dArkOSRE-R36](https://github.com/southoz/dArkOSRE-R36). Hooks into any SDL2/EGL game or emulator via `LD_PRELOAD` — no game modifications required.

---

## Features

- **CPU** — frequency and voltage (per-core from RK3326 OPP table)
- **GPU** — frequency and voltage (Mali-G31 MP2)
- **RAM** — usage (used / total MB)
- **FPS** — frames per second, measured at swap interval
- **Temp** — CPU temperature from sysfs thermal zone
- **Dual font** — 8×8 compact (default) or 8×16 VGA-style, selectable from UI
- **4 corner positions** — top-left, top-right, bottom-left, bottom-right
- **2× scale** — doubled pixel size for high-resolution displays
- **Per-metric toggle** — show only what you want
- **SDL2 settings UI** — graphical menu, no terminal needed. Runs from dArkOSRE Options menu.

---

## Requirements

- **Device:** R36S or compatible RK3326 / RK3326S clone
- **OS:** [dArkOSRE-R36](https://github.com/southoz/dArkOSRE-R36) by southoz
- **Install:** one file — `R36.HUD.sh` bundles everything

---

## Installation

Download **`R36.HUD.sh`** from the [latest release](https://github.com/zenmode-adri/r36-hud/releases/latest). Copy it to `/opt/system/` on the device. It will appear under **Options** in the dArkOSRE menu.

**Option A — SSH (recommended)**
```bash
scp "R36.HUD.sh" ark@<device-ip>:/opt/system/
```

**Option B — SD card + file manager (no network needed)**

Copy the file to the **FAT32 partition** of the SD card (visible on any PC). Insert the card, boot the device, open a file manager (e.g. **351Files**) and move it to `/opt/system/`.

**First launch:** extracts bundled binaries — takes a few seconds, one time only.

---

## Configuration

Settings are saved to `/etc/r36overlay.conf`. Edit from the in-game UI (launch **R36 HUD** from Options) or manually:

| Key | Values | Default |
|-----|--------|---------|
| `enabled` | 0 / 1 | 1 |
| `show_cpu` | 0 / 1 | 1 |
| `show_gpu` | 0 / 1 | 1 |
| `show_ram` | 0 / 1 | 1 |
| `show_fps` | 0 / 1 | 1 |
| `show_temp` | 0 / 1 | 1 |
| `pos` | 0 top-left / 1 top-right / 2 bottom-left / 3 bottom-right | 0 |
| `scale` | 1 / 2 | 1 |
| `font` | 0 (8×8) / 1 (8×16 VGA) | 0 |

---

## How it works

`R36.HUD.sh` installs `libr36overlay.so` and registers it in `/etc/ld.so.preload`. This causes the library to be injected into every process that loads, including emulators and games. The library hooks `eglSwapBuffers` and `SDL_GL_SwapWindow` — drawing the HUD immediately before each frame is presented to the screen.

Stats are read directly from sysfs at swap time, with no background threads.

---

## Credits

Built for [dArkOSRE-R36](https://github.com/southoz/dArkOSRE-R36) by [southoz](https://github.com/southoz).

## License

[MIT](LICENSE)
