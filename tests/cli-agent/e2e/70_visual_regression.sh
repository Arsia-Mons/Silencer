#!/usr/bin/env bash
# Visual regression: menu cluster + modals + gallery vs golden baselines.
#
# Two kinds of baseline live in golden/ (see golden/ORIGIN_GOLDENS.md):
#   - ORIGIN goldens (the 13 origin/main captures): ground truth, NEVER
#     bless-able from a cppx render. Gated by tools/cap/pixdiff_tolerant.py's
#     printed verdict (global <1% AND zero hot tiles).
#   - cppx-only baselines (gallery, message_modal, password_modal — no origin
#     equivalent): regression-only, re-blessed from a cppx render with BLESS=1.
#
# Captures at 1920x1080 — the golden capture resolution.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

TOLERANT="$REPO_ROOT/tools/cap/pixdiff_tolerant.py"
PIXDIFF="$REPO_ROOT/tools/pixdiff/build/pixdiff"
GOLDEN_DIR="$SCRIPT_DIR/golden"
W=1920; H=1080
BLESS="${BLESS:-0}"
# Origin ground truth — blessing these from a cppx render destroys the spec.
ORIGIN_GOLDENS="mainmenu options options_audio options_display options_controls"

[ -x "$PIXDIFF" ] || { echo "pixdiff not built: $PIXDIFF" >&2; exit 1; }

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

is_origin_golden() {
  local n
  for n in $ORIGIN_GOLDENS; do [ "$n" = "$1" ] && return 0; done
  return 1
}

# Origin-golden screens: verdict comes from the tolerant tile gate.
check_origin() {
  local fresh="$1" name="$2"
  local golden="$GOLDEN_DIR/$name.png"
  [ -f "$golden" ] || { echo "  MISSING origin golden: $name.png" >&2; FAIL=$((FAIL+1)); return 0; }
  local line
  line="$(python3 "$TOLERANT" "$fresh" "$golden" | head -1)"
  case "$line" in
    *PASS*) ;;
    *) echo "  DIVERGED $name vs origin golden: $line" >&2; FAIL=$((FAIL+1)) ;;
  esac
}

# cppx-only surfaces: raw pixdiff vs the blessed cppx baseline (same renderer,
# so byte-exact comparison is meaningful). Threshold generous for text AA.
check_cppx() {
  local fresh="$1" name="$2"
  local golden="$GOLDEN_DIR/$name.png"
  if [ "$BLESS" = "1" ]; then cp "$fresh" "$golden"; return 0; fi
  [ -f "$golden" ] || { echo "  MISSING cppx baseline: $name.png (BLESS=1 to create)" >&2; FAIL=$((FAIL+1)); return 0; }
  local pct
  pct="$("$PIXDIFF" "$fresh" "$golden" 2>/dev/null || echo 100.0)"
  local over
  over="$(VR_PCT="$pct" bun -e 'console.log(Number(process.env.VR_PCT) > 0.40 ? "1" : "0")')"
  if [ "$over" = "1" ]; then
    echo "  REGRESSED $name: ${pct}% > 0.40%" >&2
    FAIL=$((FAIL+1))
  fi
}

cap() {
  local name="$1"
  cli --port "$PORT" wait_frames --n 3 >/dev/null
  local png="$WORK/$name.png"
  cli --port "$PORT" screenshot --out "$png" >/dev/null
  if is_origin_golden "$name"; then
    check_origin "$png" "$name"
  else
    check_cppx "$png" "$name"
  fi
}

# ---- MainMenu + options cluster (origin goldens) ----
# Land inside the logo HOLD window (~2.5s..7.5s after mount) so the wordmark
# is the stable full frame the golden was captured in (cf. cap_menus.sh).
sleep 3
cap mainmenu

cli --port "$PORT" click --label "Options" >/dev/null
cap options

cli --port "$PORT" click --label "OptionsAudio" >/dev/null
cap options_audio
cli --port "$PORT" click --label "OptionsCancel" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null

cli --port "$PORT" click --label "OptionsDisplay" >/dev/null
cap options_display
cli --port "$PORT" click --label "OptionsCancel" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null

cli --port "$PORT" click --label "OptionsControls" >/dev/null
cap options_controls
cli --port "$PORT" click --label "ControlsBack" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
cli --port "$PORT" click --label "OptionsGoBack" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null

# ---- Modals + gallery (cppx-only baselines) ----
# The mainmenu logo keeps cycling its wall-clock reveal BEHIND the modals
# (visible since the retained-tree cap bump let multi-layer frames commit);
# wait for the reveal HOLD (logo region stable across two frames) before each
# modal cap so the baselines stay deterministic.
wait_logo_hold() {
  local a="$WORK/logo_a.png" b="$WORK/logo_b.png"
  for _ in $(seq 1 30); do
    cli --port "$PORT" screenshot --out "$a" >/dev/null
    cli --port "$PORT" wait_ms --n 400 >/dev/null
    cli --port "$PORT" screenshot --out "$b" >/dev/null
    if python3 - "$a" "$b" <<'PY'
import sys
import numpy as np
from PIL import Image
a = np.array(Image.open(sys.argv[1]).convert("RGB"))[500:590, 250:850]
b = np.array(Image.open(sys.argv[2]).convert("RGB"))[500:590, 250:850]
sys.exit(0 if (a == b).all() else 1)
PY
    then return 0; fi
  done
  echo "logo never reached its HOLD window" >&2
  return 1
}

cli --port "$PORT" show_message_modal --title "Notice" --message "Visual regression sample message." >/dev/null
wait_logo_hold
cap message_modal
cli --port "$PORT" click --label "MessageModalOk" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null

cli --port "$PORT" show_password_modal --title "Password" >/dev/null
wait_logo_hold
cap password_modal
cli --port "$PORT" click --label "Ok" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null

cli --port "$PORT" ui_gallery >/dev/null
cap gallery
cli --port "$PORT" back >/dev/null

rm -rf "$WORK"

if [ "$FAIL" -ne 0 ]; then
  echo "70_visual_regression: $FAIL surface(s) diverged"
  exit 1
fi
echo "PASS 70_visual_regression"
