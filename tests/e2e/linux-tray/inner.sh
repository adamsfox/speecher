#!/usr/bin/env bash
# Runs inside the virtual KDE Wayland session (started by run.sh). Owns a
# minimal StatusNotifierWatcher, launches the AppImage as a daemon with a
# completed-setup profile, then hands control to the driver.
set -euo pipefail

FLOW_DIR="$E2E_FLOW_DIR"
APP="$FLOW_DIR/Speecher.AppImage"
APP_TMPDIR="$E2E_APP_TMPDIR"
mkdir -p "$APP_TMPDIR"

# Seed settings: setup complete and stub dictation, so the tray's Start
# Dictation starts a real session without vendor credentials or a network.
mkdir -p "$FLOW_DIR/config/${SPEECHER_ORG:-io.github.firemonster612}"
cat > "$FLOW_DIR/config/${SPEECHER_ORG:-io.github.firemonster612}/speecher.conf" <<INI
[app]
setupCompleted=true

[stt]
provider=e2e-stub

[refinement]
provider=e2e-stub

[audio]
vadEnabled=false

[updates]
autoCheck=false
INI

# A short silent WAV that WavFileAudioInput loops as room tone; the stub
# transcriber produces deterministic text regardless.
python3 - "$FLOW_DIR/mic.wav" <<'PY'
import struct, sys, wave
with wave.open(sys.argv[1], "wb") as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(16000)
    w.writeframes(struct.pack("<" + "h" * 16000, *([0] * 16000)))
PY

# Qt only publishes QSystemTrayIcon over StatusNotifierItem if a watcher owns
# org.kde.StatusNotifierWatcher when the icon is created, so the watcher must
# be on the bus before the app starts.
EVENTS="$FLOW_DIR/sni-events.log"
: > "$EVENTS"
python3 "$E2E_HERE/sni_watcher.py" "$EVENTS" > "$FLOW_DIR/watcher.log" 2>&1 &
for _ in $(seq 50); do
  grep -q "watcher-ready" "$EVENTS" && break
  sleep 0.2
done
grep -q "watcher-ready" "$EVENTS" || { echo "SNI watcher never came up" >&2; exit 1; }

COMMON_ENV=(
  APPIMAGE_EXTRACT_AND_RUN=1
  TMPDIR="$APP_TMPDIR"
  QT_QPA_PLATFORM=wayland
  LIBGL_ALWAYS_SOFTWARE=1
  SPEECHER_E2E_STUB=1
  SPEECHER_E2E_SKIP_MIC_GATE=1
  SPEECHER_AUDIO_WAV="$FLOW_DIR/mic.wav"
  XDG_CONFIG_HOME="$FLOW_DIR/config"
  XDG_DATA_HOME="$FLOW_DIR/data"
  XDG_CACHE_HOME="$FLOW_DIR/cache"
  XDG_RUNTIME_DIR="$E2E_RUNTIME_DIR"
)

sleep 1
bwrap \
  --ro-bind / / \
  --dev-bind /dev /dev \
  --proc /proc \
  --bind /tmp /tmp \
  --bind "$FLOW_DIR" "$FLOW_DIR" \
  --bind "$APP_TMPDIR" "$APP_TMPDIR" \
  env "${COMMON_ENV[@]}" "$APP" --daemon \
  > "$FLOW_DIR/app-stdio.log" 2>&1 &

E2E_APP="$APP" \
E2E_FLOW_DIR="$FLOW_DIR" \
APPIMAGE_EXTRACT_AND_RUN=1 \
TMPDIR="$APP_TMPDIR" \
QT_QPA_PLATFORM=wayland \
XDG_RUNTIME_DIR="$E2E_RUNTIME_DIR" \
  python3 "$E2E_HERE/drive.py" > "$FLOW_DIR/driver.log" 2>&1
