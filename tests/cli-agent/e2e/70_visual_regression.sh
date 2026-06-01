#!/usr/bin/env bash
# SIL-24 visual-regression suite (no-lobby surfaces): captures golden PNGs of the
# gallery + MainMenu + the Options cluster + the two modals at a fixed viewport,
# and pixel-diffs each fresh screenshot against its committed golden. Full-screen
# diffs cover the [SCREEN] tickets; per-control crops (rect from `inspect`) cover
# the [VISUAL] element + [COMP] component-variant tickets.
#
#   BLESS=1 bash tests/cli-agent/e2e/70_visual_regression.sh   # (re)write goldens
#   bash tests/cli-agent/e2e/70_visual_regression.sh           # compare (CI)
#
# Goldens live in tests/cli-agent/e2e/golden/. Regenerate intentionally after a
# deliberate UI change; review the PNG diff in the PR.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PIXDIFF="$REPO_ROOT/tools/pixdiff/build/pixdiff"
GOLDEN_DIR="$SCRIPT_DIR/golden"
THRESH="${VR_THRESHOLD:-0.40}"   # percent of bytes allowed to differ
W=960; H=720
BLESS="${BLESS:-0}"

if [ ! -x "$PIXDIFF" ]; then
  echo "pixdiff not built: $PIXDIFF (cmake -B build && cmake --build build in tools/pixdiff)" >&2
  exit 1
fi
mkdir -p "$GOLDEN_DIR"

# Fresh HOME so config-derived screen content (keybinds, volume, toggles) is at
# its defaults — goldens stay portable across machines.
FRESH_HOME="$(mktemp -d)"
export HOME="$FRESH_HOME"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"; rm -rf "$FRESH_HOME"' EXIT
wait_alive "$PORT"
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
cli --port "$PORT" resize --w "$W" --h "$H" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null

WORK="$(mktemp -d)"
FAIL=0

# Compare (or bless) percent-diff under THRESH. $1 fresh png, $2 golden basename,
# $3 optional "--crop x,y,w,h".
diff_or_bless() {
  local fresh="$1" name="$2" crop="${3:-}"
  local golden="$GOLDEN_DIR/$name.png"
  if [ "$BLESS" = "1" ]; then
    # Only the full-screen capture writes a golden; crops reuse the screen golden.
    [ -z "$crop" ] && cp "$fresh" "$golden"
    return 0
  fi
  if [ ! -f "$golden" ]; then
    echo "  MISSING golden: $name.png (run with BLESS=1)" >&2
    FAIL=$((FAIL+1)); return 0
  fi
  local pct
  if [ -n "$crop" ]; then
    pct="$("$PIXDIFF" --crop "$crop" "$fresh" "$golden" 2>/dev/null || echo 100.0)"
  else
    pct="$("$PIXDIFF" "$fresh" "$golden" 2>/dev/null || echo 100.0)"
  fi
  local over
  over="$(VR_PCT="$pct" VR_TH="$THRESH" bun -e 'console.log(Number(process.env.VR_PCT) > Number(process.env.VR_TH) ? "1" : "0")')"
  if [ "$over" = "1" ]; then
    echo "  REGRESSED $name${crop:+ [crop $crop]}: ${pct}% > ${THRESH}%" >&2
    FAIL=$((FAIL+1))
  fi
}

# Screenshot the current frame and full-diff it against golden <name>.
cap() {
  local name="$1"
  cli --port "$PORT" wait_frames --n 3 >/dev/null
  local png="$WORK/$name.png"
  cli --port "$PORT" screenshot --out "$png" >/dev/null
  cli --port "$PORT" inspect > "$WORK/$name.json"
  diff_or_bless "$png" "$name"
  echo "$png"
}

# Crop-diff control <cid> on screen <name> (rect from the saved inspect) vs the
# screen golden. Maps a [VISUAL]/[COMP] ticket to a region of the screen golden.
crop_check() {
  local name="$1" cid="$2"
  [ "$BLESS" = "1" ] && return 0
  local rect
  rect="$(CID="$cid" bun -e '
    const r = JSON.parse(await Bun.file(process.argv[1]).text());
    const n = (r.nodes||[]).find((x)=>x.control_id===process.env.CID);
    if (!n) { console.log(""); process.exit(0); }
    const cx=Math.max(0,Math.round(n.x)), cy=Math.max(0,Math.round(n.y));
    console.log(`${cx},${cy},${Math.round(n.w)},${Math.round(n.h)}`);
  ' "$WORK/$name.json")"
  if [ -z "$rect" ]; then
    echo "  MISSING control for crop: $cid on $name" >&2
    FAIL=$((FAIL+1)); return 0
  fi
  diff_or_bless "$WORK/$name.png" "$name" "$rect"
}

# ---- MainMenu (SIL-50; elements SIL-62..65) -------------------------------
S="$(cap mainmenu)"
crop_check mainmenu PlayOnline   # Play button (SIL-63)
crop_check mainmenu Options      # Options button (SIL-64)
crop_check mainmenu Quit         # Quit button (SIL-65)

# ---- Options cluster (SIL-54..57; OptionsMenu elements SIL-66..70,
#       OptionsAudio SIL-71..74, OptionsDisplay SIL-75..78, Controls SIL-79..83)
cli --port "$PORT" click --label "Options" >/dev/null
cap options >/dev/null
crop_check options OptionsAudio
crop_check options OptionsDisplay
crop_check options OptionsControls
crop_check options OptionsDone

# Each sub-screen returns to Options via its own Back button (deterministic),
# then a settle wait before the next push.
cli --port "$PORT" click --label "OptionsAudio" >/dev/null
cap options_audio >/dev/null
crop_check options_audio MusicToggle
crop_check options_audio VolumeDown   # "Master Volume" is a +/- pair in the modern UI (SIL-71)
crop_check options_audio VolumeUp
crop_check options_audio OptionsBack
cli --port "$PORT" click --label "OptionsBack" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null

cli --port "$PORT" click --label "OptionsDisplay" >/dev/null
cap options_display >/dev/null
crop_check options_display FullscreenToggle
crop_check options_display SmoothScalingToggle
crop_check options_display OptionsBack
cli --port "$PORT" click --label "OptionsBack" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null

cli --port "$PORT" click --label "OptionsControls" >/dev/null
cap options_controls >/dev/null
crop_check options_controls RebindFire    # keybind entry (SIL-80)
crop_check options_controls CyclePreset   # preset/capture (SIL-81)
crop_check options_controls RevertBinds   # reset (SIL-82)
crop_check options_controls ControlsBack
cli --port "$PORT" click --label "ControlsBack" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
cli --port "$PORT" click --label "OptionsCancel" >/dev/null   # Options -> MainMenu
cli --port "$PORT" wait_frames --n 3 >/dev/null

# ---- Modals (SIL-58 MessageModal, SIL-59 PasswordModal) -------------------
cli --port "$PORT" show_message_modal --title "Notice" --message "Visual regression sample message." >/dev/null
cap message_modal >/dev/null
crop_check message_modal MessageModalOk
cli --port "$PORT" click --label "MessageModalOk" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null

cli --port "$PORT" show_password_modal --title "Password" >/dev/null
cap password_modal >/dev/null
crop_check password_modal Ok
cli --port "$PORT" click --label "Ok" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null

# ---- Component gallery (SIL-25..40 component variants) ---------------------
cli --port "$PORT" ui_gallery >/dev/null
cap gallery >/dev/null
crop_check gallery gal_btn_primary     # SIL-25
crop_check gallery gal_btn_secondary   # SIL-26
crop_check gallery gal_btn_danger      # SIL-27
crop_check gallery gal_btn_ghost       # SIL-28
crop_check gallery gal_input           # SIL-29
crop_check gallery gal_check_on        # SIL-30
cli --port "$PORT" back >/dev/null

rm -rf "$WORK"
if [ "$BLESS" = "1" ]; then
  echo "PASS 70_visual_regression (blessed goldens in $GOLDEN_DIR)"
  exit 0
fi
if [ "$FAIL" -ne 0 ]; then
  echo "70_visual_regression: $FAIL visual diff(s) over ${THRESH}%" >&2
  exit 1
fi
echo "PASS 70_visual_regression"
