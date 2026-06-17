# Changelog — R36 Overlay

All notable changes to this project will be documented here.

---

## [1.3] — 2026-06-17

### Added
- **Dual font system**: 8×8 (default, compact) and 8×16 VGA-style selectable via UI
- **Font selector screen** in overlay_ui: "Fuente" option in main menu
- `font=` key in `/etc/r36overlay.conf` persisted via `cfg_save()`

### Fixed
- **Font 8×16 mirror bug**: font data uses LSB=leftmost, bit test corrected from `(0x80>>col)` to `(1<<col)`
- **Voltage label spacing**: `"CPU%d G/R%dmV"` → `"CPU %d G/R %dmV"` (number was flush against label)
- **Console boot spam**: removed `write(2, "R36OVL:loaded\n", 14)` from constructor — LD_PRELOAD injects .so into every process including tty sessions, causing garbage on boot screen

### Changed
- `OH` (overlay height) raised from 80 → 120 to fit 6 lines at 8×16 scale 1
- Dynamic line height: `LH = charHeight * scale + (scale==2 ? 4 : 2)`
- Constructor is now fully silent (logs only to `/tmp/r36overlay.log`)

---

## [1.2] — 2026-05-28 (internal)

### Added
- Self-extracting `.sh` bundle: single file embeds both `libr36overlay.so` and `overlay_ui` as base64, extracts on first run via stamp file `/usr/local/lib/.r36overlay_ok_vX.X`
- VERSION stamp system prevents re-extraction on every launch
- `deploy_all.py`: compiles both binaries on-device over SSH, generates and installs the `.sh` bundle in one step

### Fixed
- Overlay UI detaches from EmulationStation process group via `setsid` so ES stop signal doesn't kill the settings menu

---

## [1.1] — 2026-05-27 (internal)

### Added
- `overlay_ui.c`: ncurses-free SDL2 settings menu running standalone (stops/restarts ES around it)
- Screens: Métricas (toggle CPU/GPU/RAM/FPS/temp), Posición (4 corners), Escala (1×/2×), Instalar/Desinstalar
- Config file `/etc/r36overlay.conf` with keys: `enabled`, `show_cpu`, `show_gpu`, `show_ram`, `show_fps`, `show_temp`, `pos`, `scale`, `font`
- `r36_ssh.py`: `R36SSH` helper class — connect, run, run_sudo, put; defaults HOST=192.168.1.87 USER=ark PASS=ark

---

## [1.0] — 2026-05-27 (initial)

### Added
- `r36_overlay.c`: LD_PRELOAD shared library hooking `SDL_GL_SwapWindow` and `eglSwapBuffers`
- Real-time overlay rendering: CPU freq/voltage, GPU freq/voltage, RAM usage, FPS counter, CPU temp
- 8×8 bitmap font (96 glyphs, bit0=LSB=leftmost)
- Stats read from sysfs: `/sys/devices/system/cpu/`, `/sys/class/devfreq/`, `/proc/meminfo`, `/sys/class/thermal/`
- Voltage read via RK3326 sysfs regulator nodes
- Config loaded from `/etc/r36overlay.conf` at startup
- Installed via `/etc/ld.so.preload` (global injection into all EGL/SDL2 processes)
- Target hardware: R36S (Rockchip RK3326, Mali-G31 MP2, dArkOSRE OS)
