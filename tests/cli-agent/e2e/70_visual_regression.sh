#!/usr/bin/env bash
# Visual regression tests with Discord notifications for failures.
# Creates side-by-side composites of origin/main vs current and sends to Discord.
#
#   bash tests/cli-agent/e2e/70_visual_regression_with_discord.sh
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PIXDIFF="$REPO_ROOT/tools/pixdiff/build/pixdiff"
GOLDEN_DIR="$SCRIPT_DIR/golden"
THRESH="${VR_THRESHOLD:-0.40}"
W=960; H=720
BLESS="${BLESS:-0}"
DISCORD_SKILL="$HOME/.claude/skills/discord-dm/send.ts"

if [ ! -x "$PIXDIFF" ]; then
  echo "pixdiff not built: $PIXDIFF" >&2
  exit 1
fi
mkdir -p "$GOLDEN_DIR"

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
COMPOSITE_DIR="$(mktemp -d)"
REGRESSIONS=()
FAIL=0

# Compare (or bless) percent-diff under THRESH.
diff_or_bless() {
  local fresh="$1" name="$2" crop="${3:-}"
  local golden="$GOLDEN_DIR/$name.png"
  if [ "$BLESS" = "1" ]; then
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
    REGRESSIONS+=("$name")
    FAIL=$((FAIL+1))
  fi
}

cap() {
  local name="$1"
  cli --port "$PORT" wait_frames --n 3 >/dev/null
  local png="$WORK/$name.png"
  cli --port "$PORT" screenshot --out "$png" >/dev/null
  cli --port "$PORT" inspect > "$WORK/$name.json"
  diff_or_bless "$png" "$name"
  echo "$png"
}

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

# ---- MainMenu ----
S="$(cap mainmenu)"
crop_check mainmenu ConnectToLobby
crop_check mainmenu Options
crop_check mainmenu Exit

# ---- Options cluster ----
cli --port "$PORT" click --label "Options" >/dev/null
cap options >/dev/null
crop_check options OptionsAudio
crop_check options OptionsDisplay
crop_check options OptionsControls
crop_check options OptionsGoBack

cli --port "$PORT" click --label "OptionsAudio" >/dev/null
cap options_audio >/dev/null
crop_check options_audio MusicToggle
crop_check options_audio OptionsSave
crop_check options_audio OptionsCancel
crop_check options_audio MusicToggle
cli --port "$PORT" click --label "OptionsCancel" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null

cli --port "$PORT" click --label "OptionsDisplay" >/dev/null
cap options_display >/dev/null
crop_check options_display FullscreenToggle
crop_check options_display SmoothScalingToggle
crop_check options_display OptionsSave
cli --port "$PORT" click --label "OptionsCancel" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null

cli --port "$PORT" click --label "OptionsControls" >/dev/null
cap options_controls >/dev/null
crop_check options_controls BindP0
crop_check options_controls CyclePreset
crop_check options_controls SaveBinds
crop_check options_controls ControlsBack
cli --port "$PORT" click --label "ControlsBack" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
cli --port "$PORT" click --label "OptionsGoBack" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null

# ---- Modals ----
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

# ---- Component gallery ----
cli --port "$PORT" ui_gallery >/dev/null
cap gallery >/dev/null
crop_check gallery gal_btn_primary
crop_check gallery gal_btn_secondary
crop_check gallery gal_btn_danger
crop_check gallery gal_btn_ghost
crop_check gallery gal_input
crop_check gallery gal_check_on
cli --port "$PORT" back >/dev/null

rm -rf "$WORK"

# Handle results and Discord notifications
if [ "$BLESS" = "1" ]; then
  echo "✅ PASS 70_visual_regression (blessed goldens in $GOLDEN_DIR)"
  rm -rf "$COMPOSITE_DIR"
  exit 0
fi

if [ "$FAIL" -eq 0 ]; then
  echo "✅ PASS 70_visual_regression"
  rm -rf "$COMPOSITE_DIR"
  exit 0
fi

# Regressions detected - create composites and send to Discord
echo "⚠️  $FAIL visual regression(s) detected"
echo "📸 Creating side-by-side composites..."

# Fetch origin/main goldens and create composites
composite_count=0
for regname in "${REGRESSIONS[@]}"; do
  regfile="$GOLDEN_DIR/$regname.png"
  if [ ! -f "$regfile" ]; then
    continue
  fi

  origin_file="$COMPOSITE_DIR/origin_$regname.png"
  if git show origin/main:tests/cli-agent/e2e/golden/"$regname.png" > "$origin_file" 2>/dev/null; then
    composite_file="$COMPOSITE_DIR/$regname.comparison.png"
    if bun "$REPO_ROOT/tools/compose-images.ts" \
      "$origin_file" \
      "$regfile" \
      "$composite_file" \
      "origin/main" \
      "current" 2>/dev/null; then
      composite_count=$((composite_count+1))
      echo "  ✓ $regname"
    fi
  fi
done

# Send composites to Discord if skill is available
if [ "$composite_count" -gt 0 ] && [ -f "$DISCORD_SKILL" ]; then
  echo "💬 Sending ${composite_count} comparison image(s) to Discord..."
  composites=()
  for f in "$COMPOSITE_DIR"/*.comparison.png; do
    [ -f "$f" ] && composites+=("$f")
  done

  if [ ${#composites[@]} -gt 0 ]; then
    msg="🔴 **Visual Regression Alert**
$FAIL regression(s) detected in visual regression tests.
Compare the images below (left: origin/main, right: current)"

    bun "$DISCORD_SKILL" "$msg" "${composites[@]}" 2>/dev/null || true
  fi
fi

rm -rf "$COMPOSITE_DIR"
echo "70_visual_regression: $FAIL visual diff(s) over ${THRESH}%" >&2
exit 1
