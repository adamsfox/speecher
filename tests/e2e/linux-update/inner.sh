#!/usr/bin/env bash
# Runs inside the virtual KDE Wayland session (started by run.sh). Launches the
# OLD AppImage as a daemon with the update check pointed at the local manifest,
# a stubbed provider, and a looping WAV mic, then hands control to the driver.
set -euo pipefail

FLOW_DIR="$E2E_FLOW_DIR"
APP="$FLOW_DIR/Speecher.AppImage"

# Seed settings: setup already done, automatic check on, restart-safe stub
# dictation, so a "start" opens the popup and stays listening.
mkdir -p "$FLOW_DIR/config/${SPEECHER_ORG:-io.github.firemonster612}"
CONF="$FLOW_DIR/config/${SPEECHER_ORG:-io.github.firemonster612}/speecher.conf"
cat > "$CONF" <<INI
[app]
setupCompleted=true

[stt]
provider=e2e-stub

[refinement]
provider=e2e-stub

[audio]
vadEnabled=false

[updates]
autoCheck=true
autoInstall=false
channel=stable
lastCheckTime=0
INI

# A short silent WAV that WavFileAudioInput loops as room tone; the stub
# transcriber produces deterministic text regardless.
python3 - "$FLOW_DIR/mic.wav" <<'PY'
import struct, sys, wave
with wave.open(sys.argv[1], "wb") as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(16000)
    w.writeframes(struct.pack("<" + "h" * 16000, *([0] * 16000)))
PY

COMMON_ENV=(
  APPIMAGE_EXTRACT_AND_RUN=1
  QT_QPA_PLATFORM=wayland
  QT_ACCESSIBILITY=1
  QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1
  LIBGL_ALWAYS_SOFTWARE=1
  SPEECHER_E2E_STUB=1
  SPEECHER_E2E_SKIP_MIC_GATE=1
  SPEECHER_AUDIO_WAV="$FLOW_DIR/mic.wav"
  SSL_CERT_FILE="$E2E_FIXTURE/cert.pem"
  SPEECHER_UPDATE_MANIFEST_URL="https://localhost:$E2E_PORT/update-manifest.json"
  SPEECHER_POPUP_CAPTURE_DIR="$FLOW_DIR/frames"
  SPEECHER_GRAB_DIR="$FLOW_DIR/grabs"
  XDG_CONFIG_HOME="$FLOW_DIR/config"
  XDG_DATA_HOME="$FLOW_DIR/data"
  XDG_CACHE_HOME="$FLOW_DIR/cache"
  XDG_RUNTIME_DIR="$FLOW_DIR/runtime"
)

sleep 1
# The app process gets the fixture CA overlaid over /etc/ssl/certs; nothing
# else on the host is affected.
bwrap \
  --ro-bind / / \
  --dev-bind /dev /dev \
  --proc /proc \
  --bind /tmp /tmp \
  --bind "$FLOW_DIR" "$FLOW_DIR" \
  --ro-bind "$E2E_CERTS" /etc/ssl/certs \
  env "${COMMON_ENV[@]}" "$APP" --daemon \
  > "$FLOW_DIR/app-stdio.log" 2>&1 &

sleep 3
E2E_APP="$APP" \
E2E_FLOW="$E2E_FLOW" \
E2E_FLOW_DIR="$FLOW_DIR" \
E2E_NEW_VERSION="$E2E_NEW_VERSION" \
SPEECHER_POPUP_CAPTURE_DIR="$FLOW_DIR/frames" \
APPIMAGE_EXTRACT_AND_RUN=1 \
QT_QPA_PLATFORM=wayland \
XDG_RUNTIME_DIR="$FLOW_DIR/runtime" \
SPEECHER_UPDATE_MANIFEST_URL="https://localhost:$E2E_PORT/update-manifest.json" \
SSL_CERT_FILE="$E2E_FIXTURE/cert.pem" \
SPEECHER_AUDIO_WAV="$FLOW_DIR/mic.wav" \
SPEECHER_E2E_STUB=1 \
SPEECHER_GRAB_DIR="$FLOW_DIR/grabs" \
  python3 "$E2E_HERE/drive.py" > "$FLOW_DIR/driver.log" 2>&1
