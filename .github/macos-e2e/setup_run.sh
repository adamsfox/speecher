#!/usr/bin/env bash

# The setup assistant E2E: launches the packaged app with setup incomplete,
# walks the SwiftUI assistant with real AX clicks, and checks what setup is for
# — the settings it wrote, the shortcut it bound, and the window that follows.

source "$(dirname "$0")/common.sh"
TCC_SEED="$(dirname "$0")/tcc_seed.py"
KEY_POST="${KEY_POST:?}"
ASSISTANT_WINDOW='Speecher Setup Assistant'
USER_TCC_DB="$HOME/Library/Application Support/com.apple.TCC/TCC.db"
SYSTEM_TCC_DB='/Library/Application Support/com.apple.TCC/TCC.db'

# The steps as SetupStep.all orders them; the capture seam names its PNGs after
# these ids.
SETUP_STEP_IDS=(welcome transcription microphone accessibility delivery
                refinement profiles shortcut ready login)

seed_setup_tcc() {
  # osascript drives the assistant: AppleEvents to System Events and to the
  # app, and the system-db Accessibility right that synthetic clicks need.
  python3 "$TCC_SEED" "$USER_TCC_DB" \
    kTCCServiceAppleEvents /usr/bin/osascript 2 com.apple.systemevents 1 || return 1
  python3 "$TCC_SEED" "$USER_TCC_DB" \
    kTCCServiceAppleEvents /usr/bin/osascript 2 "$BUNDLE_ID" 1 || return 1
  sudo python3 "$TCC_SEED" "$SYSTEM_TCC_DB" \
    kTCCServiceAccessibility /usr/bin/osascript 2 UNUSED 1 || return 1
  # Finishing setup with a recorded single key installs an NSEvent monitor,
  # which needs the app itself trusted for Accessibility. The recovery case
  # (S5) flips this row to denied and back while the app runs.
  sudo python3 "$TCC_SEED" "$SYSTEM_TCC_DB" \
    kTCCServiceAccessibility "$BUNDLE_ID" 2 || return 1
  # key-post posts CGEvents from its own process; TCC gates posting on the
  # poster's path, exactly as it gates osascript's clicks above.
  sudo python3 "$TCC_SEED" "$SYSTEM_TCC_DB" \
    kTCCServicePostEvent "$KEY_POST" 2 UNUSED 1 || return 1
  sudo python3 "$TCC_SEED" "$SYSTEM_TCC_DB" \
    kTCCServiceAccessibility "$KEY_POST" 2 UNUSED 1 || return 1
  # The microphone step opens the input device; the seeded grant is what keeps
  # macOS from raising a consent sheet no one is there to click.
  python3 "$TCC_SEED" "$USER_TCC_DB" kTCCServiceMicrophone "$BUNDLE_ID" 2 || return 1
  sudo launchctl kickstart -k system/com.apple.tccd || sudo killall tccd || true
}

# Unlike common.sh's baseline_reset, setup must be INCOMPLETE, which is the
# whole reason the assistant appears.
fresh_reset() {
  stop_app
  defaults delete "$DOMAIN" >/dev/null 2>&1 || true
  # Sparkle's first-run prompt lives in the bundle-id domain and would steal
  # key status from the assistant.
  defaults write "$BUNDLE_ID" SUEnableAutomaticChecks -bool false
  unset SPEECHER_E2E_STUB SPEECHER_E2E_SKIP_MIC_GATE SPEECHER_E2E_REAL_AUDIO
  launchctl unsetenv SPEECHER_E2E_STUB >/dev/null 2>&1 || true
  launchctl unsetenv SPEECHER_E2E_SKIP_MIC_GATE >/dev/null 2>&1 || true
}

launch_setup() {
  if pgrep -x speecher >"$CASE_DIR/prelaunch-processes.txt" 2>&1; then
    return 1
  fi
  mkdir -p "$CASE_DIR/pages"
  SPEECHER_E2E_SETUP_CAPTURE_DIR="$CASE_DIR/pages" \
    DYLD_FRAMEWORK_PATH="${QT_ROOT_DIR:-}/lib" \
    "$APP_BIN" >"$CASE_DIR/process.out" 2>&1 &
  APP_PID=$!
  poll_process 20
}

assistant_ui() {
  bounded_osascript -e "tell application \"System Events\" to tell process \"speecher\" to $1"
}

wait_for_assistant() {
  local deadline=$((SECONDS + 30))
  while (( SECONDS < deadline )); do
    if assistant_ui "get name of window \"$ASSISTANT_WINDOW\"" \
        >>"$CASE_DIR/assistant-ax.out" 2>&1; then
      return 0
    fi
    sleep 0.2
  done
  return 1
}

click_button() {
  # The root SwiftUI group exposes the navigation HStack's buttons in order.
  # Skip is first; Continue/Finish is last. Step captures and persisted setup
  # state verify the action, without depending on SwiftUI's AX names.
  local button
  case "$1" in
    "Skip Setup") button='first button' ;;
    Continue|Finish) button='last button' ;;
    *) return 1 ;;
  esac
  assistant_ui "click $button of group 1 of window \"$ASSISTANT_WINDOW\"" \
    >>"$CASE_DIR/clicks.out" 2>&1
}

# The seam writes the PNG after the step renders; wait for the atomic write.
wait_for_page_capture() {
  local index="$1" id="$2" count=0
  local file="$CASE_DIR/pages/step-$index-$id.png"
  while (( count < 50 )); do
    [[ -s "$file" ]] && return 0
    sleep 0.2
    count=$((count + 1))
  done
  return 1
}

setup_completed() {
  [[ "$(defaults read "$DOMAIN" app.setupCompleted 2>/dev/null)" == 1 ]]
}

check_page_captures() {
  # A nonempty PNG can still be the previous step. Read the actual pixels,
  # independently of the flow model that picked the capture's filename.
  swift - "$CASE_DIR/pages" >"$CASE_DIR/page-checks.out" 2>&1 <<'SWIFT'
import Foundation
import ImageIO
import Vision

let pages = [
    ("welcome", "Welcome to Speecher"), ("transcription", "Transcription"),
    ("microphone", "Microphone"), ("accessibility", "Accessibility"),
    ("delivery", "Text delivery"), ("refinement", "Refinement"),
    ("profiles", "Writing profiles"), ("shortcut", "Dictation shortcut"),
    ("ready", "Ready to dictate"), ("login", "Start at login"),
]
for (index, page) in pages.enumerated() {
    let filename = "step-\(index + 1)-\(page.0).png"
    let url = URL(fileURLWithPath: CommandLine.arguments[1]).appendingPathComponent(filename)
    let data = try Data(contentsOf: url)
    guard data.count > 8192,
          let source = CGImageSourceCreateWithData(data as CFData, nil),
          let image = CGImageSourceCreateImageAtIndex(source, 0, nil),
          image.width >= 600, image.height >= 500 else {
        print("FAIL: \(filename) is missing a full-size rendering")
        exit(1)
    }
    let request = VNRecognizeTextRequest()
    request.recognitionLevel = .accurate
    try VNImageRequestHandler(cgImage: image).perform([request])
    let text = (request.results ?? []).compactMap { $0.topCandidates(1).first?.string }
        .joined(separator: " ")
    print("\(filename): \(image.width)x\(image.height), \(data.count) bytes\n\(text)")
    guard text.contains(page.1), text.contains("Step \(index + 1) of 10") else {
        print("FAIL: \(filename) does not show its expected title and step number")
        exit(1)
    }
}
print("PASS: all ten captures show the expected title and step number")
SWIFT
}

# The stored shortcut, flattened from SettingsKeys::GlobalShortcut
# ("shortcuts/toggleDictation") the way QSettings writes groups to defaults.
stored_shortcut() {
  defaults read "$DOMAIN" shortcuts.toggleDictation 2>/dev/null
}

# The settings window's AX name is the pane it shows, so any other window — an
# alert, the updater — does not satisfy this.
SETTINGS_PANE_TITLES='General|What.s New|Dictation|Shortcut|Text|Delivery|Apps|Vocabulary|Settings'

settings_window_present() {
  local deadline=$((SECONDS + 20))
  while (( SECONDS < deadline )); do
    if assistant_ui 'get name of windows' 2>/dev/null \
        | tr ',' '\n' | grep -qE "^\s*(${SETTINGS_PANE_TITLES})\s*$"; then
      return 0
    fi
    sleep 0.2
  done
  return 1
}

assistant_gone() {
  local deadline=$((SECONDS + 10))
  while (( SECONDS < deadline )); do
    if ! assistant_ui "get name of window \"$ASSISTANT_WINDOW\"" >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.2
  done
  return 1
}

# A local NSEvent monitor (the recorders) only sees events routed to the app,
# so it must be frontmost before key-post posts anything at it.
app_frontmost() {
  assistant_ui "set frontmost to true" >>"$CASE_DIR/clicks.out" 2>&1
}

# Finds a button anywhere inside the named window by its AX name and clicks it
# (action=click) or merely confirms it exists (action=find). SwiftUI nests
# controls in AX groups, so this walks the window's entire contents, the
# drive_picker pattern from provider_steps_run.sh.
button_ax() {
  local action="$1" name="$2" window="$3"
  osascript - "$action" "$name" "$window" >>"$CASE_DIR/clicks.out" 2>&1 <<'OSA' &
on run argv
  set theAction to item 1 of argv
  set buttonName to item 2 of argv
  set windowName to item 3 of argv
  tell application "System Events" to tell process "speecher"
    set allElements to entire contents of window windowName
    repeat with e in allElements
      try
        if class of e is button and name of e is buttonName then
          if theAction is "click" then click e
          return
        end if
      end try
    end repeat
    error "no button named '" & buttonName & "' in window '" & windowName & "'"
  end tell
end run
OSA
  local pid=$! count=0
  while kill -0 "$pid" >/dev/null 2>&1 && (( count < 150 )); do
    sleep 0.2
    count=$((count + 1))
  done
  if kill -0 "$pid" >/dev/null 2>&1; then
    kill -9 "$pid" >/dev/null 2>&1 || true
    wait "$pid" 2>/dev/null || true
    return 124
  fi
  wait "$pid"
}

wait_for_button() {
  local name="$1" window="$2" deadline=$((SECONDS + 15))
  while (( SECONDS < deadline )); do
    if button_ax find "$name" "$window"; then
      return 0
    fi
    sleep 0.5
  done
  return 1
}

poll_stored_shortcut() {
  local wanted="$1" deadline=$((SECONDS + 15))
  while (( SECONDS < deadline )); do
    [[ "$(stored_shortcut)" == "$wanted" ]] && return 0
    sleep 0.5
  done
  printf 'stored shortcut: %s (wanted %s)\n' "$(stored_shortcut)" "$wanted" \
    >>"$CASE_DIR/assertions-failed.txt"
  return 1
}

current_status() {
  cli status 2>/dev/null | tail -1
}

if ! seed_setup_tcc; then
  log "TCC seeding failed; AX driving cannot work without it"
  record_verdict SETUP-TCC FAIL "The runner refused the TCC seeds the AX driver needs."
  exit 1
fi

# S1: a fresh profile walks every step to Finish. Setup completes, the default
# shortcut binds, and the settings window follows the assistant out.
fresh_reset
case_begin S1
if ! launch_setup; then
  fail_case "The app did not launch within 20 seconds."
elif ! wait_for_assistant; then
  fail_case "The setup assistant window never appeared on a fresh profile."
else
  errors=()
  for index in "${!SETUP_STEP_IDS[@]}"; do
    step=$((index + 1))
    id="${SETUP_STEP_IDS[$index]}"
    log "S1 step $step ($id)"
    if ! wait_for_page_capture "$step" "$id"; then
      errors+=("step $step ($id) was never captured")
      break
    fi
    if [[ "$id" == shortcut ]]; then
      # Record a single key in the wizard: the runner posts right Option from
      # key-post's own process, which the recorder's local monitor must take.
      # This is also the live proof that NSEvent.keyCode arrives usable on
      # flagsChanged events.
      app_frontmost
      if ! button_ax click "Record a Single Key" "$ASSISTANT_WINDOW"; then
        errors+=("the single-key record button did not click")
        break
      fi
      sleep 0.5
      "$KEY_POST" right-option tap
      if ! wait_for_button "Right Option" "$ASSISTANT_WINDOW"; then
        errors+=("recording right Option did not update the single-key button")
        break
      fi
    fi
    if (( step < ${#SETUP_STEP_IDS[@]} )); then
      if ! click_button Continue; then
        errors+=("Continue did not click on step $step ($id)")
        break
      fi
      sleep 0.5
    fi
  done
  if (( ${#errors[@]} == 0 )); then
    click_button Finish || errors+=("Finish did not click on the last step")
  fi
  sleep 2
  setup_completed || errors+=("app.setupCompleted was not written")
  kill -0 "$APP_PID" 2>/dev/null || errors+=("the app quit after Finish")
  assistant_gone || errors+=("the assistant window stayed open after Finish")
  settings_window_present || errors+=("no settings window followed the assistant")
  # Finish registers the single key recorded on the shortcut step; a single
  # key is stored in ShortcutBinding's key: spelling. (The default ⌃⌥D combo
  # used to be asserted here; recording replaces it, and S6 below proves the
  # combo path live instead.)
  stored_shortcut > "$CASE_DIR/shortcut-defaults.txt"
  [[ "$(stored_shortcut)" == "key:AltRight" ]] \
    || errors+=("Finish did not store the recorded single key (got '$(stored_shortcut)')")
  check_page_captures || errors+=("page rendering checks failed; see page-checks.out")
  if (( ${#errors[@]} )); then
    fail_case "$(IFS='; '; echo "${errors[*]}")"
  else
    pass_case "All ten steps rendered and clicked through; Finish registered the recorded single key and opened the settings window."
  fi
fi

# S2: Skip Setup from the first step completes setup without touching the
# settings the later steps would have written.
fresh_reset
case_begin S2
if ! launch_setup; then
  fail_case "The app did not launch."
elif ! wait_for_assistant; then
  fail_case "The setup assistant window never appeared."
else
  errors=()
  wait_for_page_capture 1 welcome || errors+=("the welcome step was never captured")
  click_button "Skip Setup" || errors+=("Skip Setup did not click")
  sleep 2
  setup_completed || errors+=("app.setupCompleted was not written after skipping")
  kill -0 "$APP_PID" 2>/dev/null || errors+=("the app quit after skipping")
  assistant_gone || errors+=("the assistant window stayed open after skipping")
  settings_window_present || errors+=("no settings window followed skipping")
  # Skipping must not bind or store what the later steps would have.
  [[ -z "$(stored_shortcut)" ]] \
    || errors+=("skipping still stored a shortcut ('$(stored_shortcut)')")
  if (( ${#errors[@]} )); then
    fail_case "$(IFS='; '; echo "${errors[*]}")"
  else
    pass_case "Skip Setup completed setup from the first step."
  fi
fi

# S3: a completed profile launches without the assistant.
stop_app
case_begin S3
if ! launch_setup; then
  fail_case "The app did not relaunch on the completed profile."
else
  sleep 3
  if ! kill -0 "$APP_PID" 2>/dev/null; then
    fail_case "The app quit after relaunching on the completed profile."
  elif ! settings_window_present; then
    fail_case "No settings window appeared on the completed profile."
  elif assistant_ui "get name of window \"$ASSISTANT_WINDOW\"" >/dev/null 2>&1; then
    fail_case "The assistant reappeared although setup is complete."
  else
    pass_case "A completed profile launches without the assistant."
  fi
fi

# S4: the bound single key drives a whole dictation cycle in a running app.
# key-post posts right Option from its own process, so this passing is also
# the proof that the binder's self-PID filter (which drops only Speecher's own
# posts) does not eat the harness's input.
stop_app
baseline_reset
defaults write "$DOMAIN" shortcuts.toggleDictation "key:AltRight"
defaults write "$DOMAIN" shortcuts.activationMode hybrid
case_begin S4
if ! launch_app; then
  fail_case "The app did not launch for the single-key dictation cycle."
else
  errors=()
  poll_status idle 10 >"$CASE_DIR/launch-idle.out" || errors+=("the app never reached idle")
  "$KEY_POST" right-option down
  poll_status listening 10 >"$CASE_DIR/listening.out" \
    || errors+=("holding right Option did not start dictation")
  # Past the hybrid hold threshold (250 ms), so the release must stop it.
  sleep 0.6
  "$KEY_POST" right-option up
  poll_status idle 25 >"$CASE_DIR/idle.out" \
    || errors+=("releasing right Option did not stop dictation")
  if (( ${#errors[@]} )); then
    fail_case "$(IFS='; '; echo "${errors[*]}")"
  else
    pass_case "Right Option, posted from another process, started and stopped a dictation."
  fi
fi

# S5: recovery on grant, without a relaunch. The binder polls
# AXIsProcessTrusted once a second when it starts untrusted and must begin
# watching as soon as the grant lands.
stop_app
sudo python3 "$TCC_SEED" "$SYSTEM_TCC_DB" kTCCServiceAccessibility "$BUNDLE_ID" 0 \
  >>"$EVIDENCE_ROOT/tcc-recovery.log" 2>&1
restart_tcc
case_begin S5
if ! launch_app; then
  fail_case "The app did not launch for the grant-recovery case."
else
  errors=()
  poll_status idle 10 >"$CASE_DIR/launch-idle.out" || errors+=("the app never reached idle")
  # Denied: the key must do nothing.
  "$KEY_POST" right-option tap
  sleep 2
  [[ "$(current_status)" == idle ]] \
    || errors+=("the key started dictation although Accessibility is denied")
  # The grant arrives while the app is running.
  sudo python3 "$TCC_SEED" "$SYSTEM_TCC_DB" kTCCServiceAccessibility "$BUNDLE_ID" 2 \
    >>"$EVIDENCE_ROOT/tcc-recovery.log" 2>&1
  restart_tcc
  # In hybrid a short tap toggles dictation on, so the first tap the re-bound
  # monitor sees is the success signal; retry while the 1 s grant poll lands.
  recovered=0
  for attempt in 1 2 3 4 5; do
    "$KEY_POST" right-option tap
    if poll_status listening 3 >"$CASE_DIR/recovered-listening.out"; then
      recovered=1
      break
    fi
  done
  (( recovered )) || errors+=("the binder did not start watching after the grant")
  cli stop >/dev/null 2>&1 || true
  poll_status idle 25 >"$CASE_DIR/recovered-idle.out" \
    || errors+=("dictation did not stop after the recovery check")
  if (( ${#errors[@]} )); then
    fail_case "$(IFS='; '; echo "${errors[*]}")"
  else
    pass_case "The binder ignored the key while denied and re-bound within the grant poll after seeding."
  fi
fi

# S6: switching combo<->single in one live session leaves exactly one binding
# firing. In particular the Carbon hot key must actually unregister when a
# single key takes the binding over (removeRegistration).
stop_app
defaults write "$BUNDLE_ID" settingsPane shortcut
case_begin S6
if ! launch_setup; then
  fail_case "The app did not relaunch for the switching case."
elif ! settings_window_present; then
  fail_case "The settings window did not open on the Shortcut pane."
else
  errors=()
  app_frontmost
  # The combo recorder button is captioned with the current binding's display.
  button_ax click "Right Option" "Shortcut" \
    || errors+=("the combination record button did not click")
  sleep 0.5
  "$KEY_POST" control-option-d
  poll_stored_shortcut "Meta+Alt+D" \
    || errors+=("recording the ⌃⌥D combination did not store it")
  # The replaced single key must be dead...
  "$KEY_POST" right-option tap
  sleep 2
  [[ "$(current_status)" == idle ]] \
    || errors+=("the replaced single key still starts dictation")
  # ...and the combination alive (Carbon hot key; a second chord toggles off).
  "$KEY_POST" control-option-d
  poll_status listening 10 >"$CASE_DIR/combo-listening.out" \
    || errors+=("the recorded combination does not start dictation")
  "$KEY_POST" control-option-d
  poll_status idle 25 >"$CASE_DIR/combo-idle.out" \
    || errors+=("the combination did not toggle dictation off")
  # Back to the single key.
  if (( ${#errors[@]} == 0 )); then
    button_ax click "Record a Single Key" "Shortcut" \
      || errors+=("the single-key record button did not click")
    sleep 0.5
    "$KEY_POST" right-option tap
    poll_stored_shortcut "key:AltRight" \
      || errors+=("re-recording right Option did not store the single key")
    # The Carbon hot key must be gone...
    "$KEY_POST" control-option-d
    sleep 2
    [[ "$(current_status)" == idle ]] \
      || errors+=("the replaced combination still fires: the Carbon hot key was not unregistered")
    # ...and the key watching again.
    "$KEY_POST" right-option down
    poll_status listening 10 >"$CASE_DIR/single-listening.out" \
      || errors+=("the re-recorded single key does not start dictation")
    sleep 0.6
    "$KEY_POST" right-option up
    poll_status idle 25 >"$CASE_DIR/single-idle.out" \
      || errors+=("the re-recorded single key did not stop dictation on release")
  fi
  if (( ${#errors[@]} )); then
    fail_case "$(IFS='; '; echo "${errors[*]}")"
  else
    pass_case "Combo and single key swapped twice in one session with exactly one binding firing each time."
  fi
fi

stop_app
log "Setup E2E finished"
cat "$VERDICTS"
if grep -q 'FAIL\|BLOCKED' "$VERDICTS"; then
  exit 1
fi
