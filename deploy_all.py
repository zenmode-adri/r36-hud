#!/usr/bin/env python3
"""deploy_all.py — compila en la R36S y genera R36 Overlay.sh self-extracting.

Uso: python deploy_all.py
"""
import os, sys

sys.path.insert(0, os.path.dirname(__file__))
from r36_ssh import connect

ROOT    = os.path.dirname(os.path.abspath(__file__))
SO_SRC  = os.path.join(ROOT, "r36_overlay.c")
UI_SRC  = os.path.join(ROOT, "overlay_ui.c")

VERSION  = "1.3"
STAMP    = f"/usr/local/lib/.r36overlay_ok_v{VERSION}"
SO_DEST  = "/usr/local/lib/libr36overlay.so"
UI_DEST  = "/opt/system/overlay_ui"
SH_DEST  = "/opt/system/R36 Overlay.sh"

LAUNCHER_TEMPLATE = """\
#!/bin/bash
# R36 Overlay.sh — self-extracting launcher v{VERSION}

# Detach from ES process group so systemctl stop ES doesn't kill this script
if [ -z "$R36OVL_DETACHED" ]; then
    export R36OVL_DETACHED=1
    exec setsid "$0" "$@"
fi

SELF="$(readlink -f "$0")"
STAMP="{STAMP}"
SO_DEST="{SO_DEST}"
UI_DEST="{UI_DEST}"

if [ ! -f "$STAMP" ]; then
    echo "[r36overlay] Extrayendo binarios..."
    awk '/^# __SO_START__$/{{f=1;next}}/^# __SO_END__$/{{exit}}f{{print}}' "$SELF" \\
        | base64 -d > /tmp/.ovl_so_$$
    echo ark | sudo -S mv /tmp/.ovl_so_$$ "$SO_DEST"
    echo ark | sudo -S chmod 755 "$SO_DEST"

    awk '/^# __UI_START__$/{{f=1;next}}/^# __UI_END__$/{{exit}}f{{print}}' "$SELF" \\
        | base64 -d > /tmp/.ovl_ui_$$
    echo ark | sudo -S mv /tmp/.ovl_ui_$$ "$UI_DEST"
    echo ark | sudo -S chmod 755 "$UI_DEST"

    echo ark | sudo -S touch "$STAMP"
    echo "[r36overlay] Listo."
fi

export SDL_VIDEO_EGL_DRIVER=/lib/aarch64-linux-gnu/libmali-bifrost-g31-rxp0-gbm.so
export XDG_RUNTIME_DIR=/run/user/1000
export SDL_GAMECONTROLLERCONFIG_FILE=/opt/inttools/gamecontrollerdb.txt

if [ -z "$SDL_VIDEODRIVER" ]; then
    if [ -e /dev/dri/card0 ]; then
        export SDL_VIDEODRIVER=kmsdrm
        export SDL_VIDEO_KMSDRM_DRM_DEVICE=/dev/dri/card0
    elif [ -e /dev/dri/card1 ]; then
        export SDL_VIDEODRIVER=kmsdrm
        export SDL_VIDEO_KMSDRM_DRM_DEVICE=/dev/dri/card1
    elif [ -e /dev/fb0 ]; then
        export SDL_VIDEODRIVER=fbdev
        export SDL_FBDEV=/dev/fb0
    fi
fi

LOG=/tmp/r36overlay_launch.log
: > "$LOG"
exec >> "$LOG" 2>&1

cleanup() {{
    echo "[r36overlay] Reiniciando EmulationStation..."
    echo ark | sudo -S systemctl start emulationstation 2>/dev/null || true
}}
trap cleanup EXIT SIGINT SIGTERM

echo "[r36overlay] Deteniendo EmulationStation..."
echo ark | sudo -S systemctl stop emulationstation 2>/dev/null || true
echo ark | sudo -S pkill -9 -f emulationstation 2>/dev/null || true
for i in $(seq 1 30); do
    pgrep -f emulationstation > /dev/null 2>&1 || break
    sleep 0.1
done
sleep 0.3

TERM=linux clear > /dev/tty1 2>/dev/null || true
chvt 1 2>/dev/null || true

echo "[r36overlay] Lanzando overlay_ui..."
"$UI_DEST"
echo "[r36overlay] overlay_ui salió ($?)"
exit 0
# __SO_START__
{SO_B64}
# __SO_END__
# __UI_START__
{UI_B64}
# __UI_END__
"""

def main():
    print("Conectando a R36S...")
    with connect() as r36:

        # — Subir fuentes —
        print("Subiendo fuentes...")
        r36.put(SO_SRC, "/tmp/r36_overlay.c",  mode="644", sudo=False)
        r36.put(UI_SRC, "/tmp/overlay_ui.c",   mode="644", sudo=False)

        # — Compilar .so —
        print("Compilando libr36overlay.so...")
        out, err, code = r36.run(
            "gcc -shared -fPIC -O2 -o /tmp/libr36overlay_new.so "
            "/tmp/r36_overlay.c -ldl -lGLESv2 2>&1"
        )
        if code != 0:
            print("ERROR .so:\n", out or err); sys.exit(1)
        print("  OK")

        # — Compilar overlay_ui —
        print("Compilando overlay_ui...")
        out, err, code = r36.run(
            "gcc -O2 -o /tmp/overlay_ui /tmp/overlay_ui.c "
            "-lSDL2 -lSDL2_ttf 2>&1",
            timeout=90
        )
        if code != 0:
            print("ERROR ui:\n", out or err); sys.exit(1)
        print("  OK")

        # — Base64 de ambos binarios —
        print("Codificando binarios...")
        so_b64, _, _ = r36.run("base64 /tmp/libr36overlay_new.so")
        ui_b64, _, _ = r36.run("base64 /tmp/overlay_ui")

        # — Generar .sh —
        print("Generando R36 Overlay.sh...")
        sh_content = LAUNCHER_TEMPLATE.format(
            VERSION=VERSION,
            STAMP=STAMP,
            SO_DEST=SO_DEST,
            UI_DEST=UI_DEST,
            SO_B64=so_b64,
            UI_B64=ui_b64,
        )

        # Escribir .sh en /tmp en la consola
        stdin, _, _ = r36.client.exec_command("cat > '/tmp/R36 Overlay.sh'")
        stdin.write(sh_content.encode())
        stdin.channel.shutdown_write()

        # Instalar
        r36.run_sudo("mv '/tmp/R36 Overlay.sh' '/opt/system/R36 Overlay.sh'")
        r36.run_sudo("chmod 755 '/opt/system/R36 Overlay.sh'")

        # Limpiar /tmp
        r36.run("rm -f /tmp/r36_overlay.c /tmp/overlay_ui.c "
                "/tmp/libr36overlay_new.so /tmp/overlay_ui")

        # Verificar
        out, _, _ = r36.run("ls -lh '/opt/system/R36 Overlay.sh'")
        print(f"\nInstalado: {out}")
        print("Listo — lanza 'R36 Overlay' desde Options en dArkOSRE.")

if __name__ == "__main__":
    main()
