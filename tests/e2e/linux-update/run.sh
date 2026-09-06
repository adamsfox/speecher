#!/usr/bin/env bash
# Linux AppImage auto-update end-to-end test.
#
# Builds two E2E-hooks AppImages one build number apart, serves the newer one
# from a local TLS manifest, and drives the whole update UX through real AT-SPI
# clicks inside a virtual KDE Wayland session:
#   * popup flow: update chip during a dictation, one-click install+restart,
#     dictation restored, what's-new chip (auto-hide + opens What's New page).
#   * settings flow: banner one-click install+restart, settings window restored,
#     installed banner afterwards.
#
# Usage: tests/e2e/linux-update/run.sh [--reuse-build]
# Requires: appimagetool, patchelf, kwin_wayland, dbus-run-session, bwrap,
#           python3 with the Atspi GI bindings, a Qt 6.8 the AppImage bundles.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
HERE="$ROOT_DIR/tests/e2e/linux-update"
WORK="${E2E_WORK:-$ROOT_DIR/.scratch/update-flow/linux-e2e}"
TOOLS="$ROOT_DIR/.scratch/update-flow/tools"
PORT="${E2E_PORT:-8443}"
export PATH="$TOOLS:$PATH"

mkdir -p "$WORK"
FIXTURE="$WORK/fixture"
CERTS="$WORK/trust-certs"

# AppImages self-extract into $TMPDIR, so it has to be on disk (a full RAM-backed
# /tmp truncates a bundled library mid-extraction) yet short: the single-instance
# IPC puts its Unix socket under $TMPDIR, and a deep path overflows sun_path's
# 108 bytes. A short dir directly under $HOME satisfies both.
E2E_TMP_BASE="$HOME/.spe-e2e"
rm -rf "$E2E_TMP_BASE"
mkdir -p "$E2E_TMP_BASE/build"
export TMPDIR="$E2E_TMP_BASE/build"

log() { printf '\n=== %s ===\n' "$*"; }

# --- 1. Two AppImages, one build number apart ------------------------------
build_appimage() {
  local tag="$1"
  local outdir="$WORK/$tag"
  if [[ -x "$outdir/Speecher.AppImage" && -n "${E2E_KEEP_BUILDS:-}" ]]; then
    log "reusing existing $tag AppImage"
    return
  fi
  log "building $tag AppImage"
  rm -rf "$outdir"
  mkdir -p "$outdir"
  SPEECHER_BUILD_DIR="$WORK/build-$tag" \
  SPEECHER_APPDIR="$WORK/appdir-$tag" \
  SPEECHER_OUTPUT_DIR="$outdir" \
  SPEECHER_BUILD_TYPE=Release \
  SPEECHER_APPIMAGE_CMAKE_EXTRA="-DSPEECHER_E2E_HOOKS=ON -DSPEECHER_WITH_KDE=OFF -DSPEECHER_RELEASE_BUILD=OFF" \
    bash "$ROOT_DIR/packaging/build-appimage.sh" > "$outdir/build.log" 2>&1
  mv "$outdir"/Speecher*x86_64.AppImage "$outdir/Speecher.AppImage"
  chmod +x "$outdir/Speecher.AppImage"
}

if [[ "${1:-}" != "--reuse-build" ]]; then
  # OLD is the committed HEAD; NEW is HEAD plus an empty build-number bump.
  build_appimage old
  git -C "$ROOT_DIR" stash create >/dev/null 2>&1 || true
  NEW_WT="$WORK/new-src"
  rm -rf "$NEW_WT"
  git -C "$ROOT_DIR" worktree remove --force "$NEW_WT" 2>/dev/null || true
  git -C "$ROOT_DIR" worktree add --detach "$NEW_WT" HEAD >/dev/null
  git -C "$NEW_WT" commit --allow-empty -m "e2e: advance build number" >/dev/null
  mkdir -p "$WORK/new"
  SPEECHER_BUILD_DIR="$WORK/build-new" \
  SPEECHER_APPDIR="$WORK/appdir-new" \
  SPEECHER_OUTPUT_DIR="$WORK/new" \
  SPEECHER_BUILD_TYPE=Release \
  SPEECHER_APPIMAGE_CMAKE_EXTRA="-DSPEECHER_E2E_HOOKS=ON -DSPEECHER_WITH_KDE=OFF -DSPEECHER_RELEASE_BUILD=OFF" \
    bash "$NEW_WT/packaging/build-appimage.sh" > "$WORK/new/build.log" 2>&1
  mv "$WORK"/new/Speecher*x86_64.AppImage "$WORK/new/Speecher.AppImage"
  chmod +x "$WORK/new/Speecher.AppImage"
  git -C "$ROOT_DIR" worktree remove --force "$NEW_WT"
fi

OLD_VERSION="$(APPIMAGE_EXTRACT_AND_RUN=1 "$WORK/old/Speecher.AppImage" --version | head -1)"
NEW_VERSION="$(APPIMAGE_EXTRACT_AND_RUN=1 "$WORK/new/Speecher.AppImage" --version | head -1)"
log "OLD: $OLD_VERSION"
log "NEW: $NEW_VERSION"
NEW_BUILD="$(sed -E 's/.*build ([0-9]+).*/\1/' <<<"$NEW_VERSION")"
NEW_SHORT="$(sed -E 's/speecher ([^ ]+).*/\1/' <<<"$NEW_VERSION")"
export E2E_OLD_SHORT="$(sed -E 's/speecher ([^ ]+).*/\1/' <<<"$OLD_VERSION")"
export E2E_OLD_BUILD="$(sed -E 's/.*build ([0-9]+).*/\1/' <<<"$OLD_VERSION")"

# --- 2. Fixture: TLS cert + manifest pointing at the NEW AppImage ----------
log "building fixture"
rm -rf "$FIXTURE" "$CERTS"
mkdir -p "$FIXTURE"
cp "$WORK/new/Speecher.AppImage" "$FIXTURE/Speecher-x86_64.AppImage"
NEW_SHA="$(sha256sum "$FIXTURE/Speecher-x86_64.AppImage" | cut -d' ' -f1)"
openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout "$FIXTURE/key.pem" -out "$FIXTURE/cert.pem" -days 2 \
  -subj "/CN=localhost" \
  -addext "subjectAltName=DNS:localhost,IP:127.0.0.1" >/dev/null 2>&1
cat > "$FIXTURE/update-manifest.json" <<JSON
{
  "version": "$NEW_SHORT",
  "buildNumber": $NEW_BUILD,
  "linux-x86_64": {
    "appimage": "https://localhost:$PORT/Speecher-x86_64.AppImage",
    "sha256": "$NEW_SHA"
  }
}
JSON

# Qt 6.8 ignores SSL_CERT_FILE, so overlay a CA dir holding our cert for the
# app process only. Copy the host trust store and add ours.
mkdir -p "$CERTS"
cp -a /etc/ssl/certs/. "$CERTS/" 2>/dev/null || true
cp "$FIXTURE/cert.pem" "$CERTS/speecher-e2e-fixture.pem"
( cd "$CERTS" && c_rehash . >/dev/null 2>&1 || openssl rehash . >/dev/null 2>&1 || true )

# --- 3. TLS server ---------------------------------------------------------
python3 "$HERE/serve.py" "$FIXTURE" "$FIXTURE/cert.pem" "$FIXTURE/key.pem" "$PORT" \
  > "$WORK/server.log" 2>&1 &
SERVER_PID=$!
trap 'kill $SERVER_PID 2>/dev/null || true' EXIT
sleep 1

# --- 4. Run both flows -----------------------------------------------------
export E2E_NEW_VERSION="$NEW_SHORT"
export E2E_FIXTURE="$FIXTURE"
export E2E_CERTS="$CERTS"
export E2E_PORT="$PORT"
export E2E_WORK="$WORK"
export E2E_HERE="$HERE"

overall=0
for flow in ${E2E_FLOWS:-popup settings}; do
  log "flow: $flow"
  FLOW_DIR="$WORK/flow-$flow"
  rm -rf "$FLOW_DIR"
  mkdir -p "$FLOW_DIR"/{config,data,cache,frames,grabs}
  # A Wayland/IPC socket path over 108 bytes overflows sun_path, so the runtime
  # dir has to be short. kwin (here) and the app (inner.sh) must share it.
  RUNTIME_DIR="/tmp/spe-e2e-$flow"
  rm -rf "$RUNTIME_DIR"
  mkdir -p "$RUNTIME_DIR"
  chmod 700 "$RUNTIME_DIR"
  export XDG_RUNTIME_DIR="$RUNTIME_DIR"
  # Short, disk-backed, and shared by the daemon (bound into bwrap) and the
  # driver CLI so both resolve the IPC socket to the same path.
  APP_TMPDIR="$E2E_TMP_BASE/$flow"
  rm -rf "$APP_TMPDIR"
  mkdir -p "$APP_TMPDIR"
  # A fresh install of the OLD AppImage for this flow, so the swap is real.
  cp "$WORK/old/Speecher.AppImage" "$FLOW_DIR/Speecher.AppImage"
  chmod +x "$FLOW_DIR/Speecher.AppImage"
  export E2E_FLOW="$flow" E2E_FLOW_DIR="$FLOW_DIR" E2E_RUNTIME_DIR="$RUNTIME_DIR" \
         E2E_APP_TMPDIR="$APP_TMPDIR"
  if dbus-run-session -- kwin_wayland --virtual --width 1280 --height 900 \
       --no-lockscreen --no-global-shortcuts \
       --exit-with-session="$HERE/inner.sh" > "$FLOW_DIR/session.log" 2>&1; then
    if grep -q "E2E-RESULT: PASS" "$FLOW_DIR/driver.log" 2>/dev/null; then
      log "flow $flow PASSED"
    else
      log "flow $flow FAILED"; overall=1
    fi
  else
    log "flow $flow session exited non-zero"; overall=1
  fi
done

log "done (exit $overall). Evidence in $WORK/flow-*"
exit "$overall"
