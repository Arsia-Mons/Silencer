#!/usr/bin/env bash
# Visual regression: the 8 in-game HUD surfaces vs the origin/main goldens
# (golden/ingame_*.png, 640x480 — see golden/ORIGIN_GOLDENS.md).
#
# Capture runs through tools/cap/cap_ingame_cppx.sh (deterministic tutorial
# anchor, camera pinning, caret/pulse matching, median-of-5 rain suppression).
# Gate: pixdiff_tolerant.py printed verdict with the documented
# nondeterminism masks (minimap inset; rain-ripple deck band; right-edge rain
# sliver — ORIGIN_GOLDENS.md "Nondeterministic regions").
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

TOLERANT="$REPO_ROOT/tools/cap/pixdiff_tolerant.py"
GOLDEN_DIR="$SCRIPT_DIR/golden"
WORK="$(mktemp -d /tmp/e2e-ingame.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT

PORT="$(pick_port)" bash "$REPO_ROOT/tools/cap/cap_ingame_cppx.sh" "$WORK"

# Masks (full-res input coords): minimap inset (wall-clock dot blink + live
# world), the rain-impact ripple band on the bridge deck, and the half-width
# right-edge tile where sparse rain reads double per tile.
MASKS=(--mask 235,419,406,479 --mask 0,296,640,340 --mask 624,0,640,419)

FAIL=0
for s in hud_base chat_open chat_history player_list buy_tech tech_overlay \
         messages quit_prompt; do
  out="$(python3 "$TOLERANT" "$WORK/ingame_$s.png" \
         "$GOLDEN_DIR/ingame_$s.png" "${MASKS[@]}")"
  verdict="$(printf '%s\n' "$out" | head -1)"
  echo "ingame_$s: $verdict"
  case "$verdict" in
    *PASS*) ;;
    *) printf '%s\n' "$out"; FAIL=1 ;;
  esac
done

[ "$FAIL" -eq 0 ] || { echo "FAIL 72_visual_regression_ingame" >&2; exit 1; }
echo "PASS 72_visual_regression_ingame"
