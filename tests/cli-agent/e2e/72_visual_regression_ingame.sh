#!/usr/bin/env bash
# Visual regression: the 8 in-game HUD surfaces vs the origin/main goldens
# (golden/ingame_*.png, 640x480 — see golden/ORIGIN_GOLDENS.md).
#
# Capture runs through tools/cap/cap_ingame_cppx.sh (deterministic tutorial
# anchor, camera pinning, caret/pulse matching, median-of-5 rain suppression).
# Gate: pixdiff_tolerant.py printed verdict with the documented
# nondeterminism masks (minimap inset; rain-ripple deck band; right-edge rain
# sliver — ORIGIN_GOLDENS.md "Nondeterministic regions").
#
# One bounded RE-CAPTURE on failure (same pattern as 74's race retry): the
# goldens' frozen rain + rand()-driven NPC wander leave marginal world tiles
# flapping around the 5% line under full-suite load (observed 3.6→5.2% on the
# same quit_prompt tile run-to-run); a fresh drive re-rolls them. The gate
# itself is untouched — a real regression fails both captures deterministically.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

TOLERANT="$REPO_ROOT/tools/cap/pixdiff_tolerant.py"
GOLDEN_DIR="$SCRIPT_DIR/golden"
WORK="$(mktemp -d /tmp/e2e-ingame.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT

# Masks (full-res input coords): minimap inset (wall-clock dot blink + live
# world), the rain-impact ripple band on the bridge deck, and the half-width
# right-edge tile where sparse rain reads double per tile.
MASKS=(--mask 235,419,406,479 --mask 0,296,640,340 --mask 624,0,640,419)

gate_all() {
  local fail=0 s out verdict
  for s in hud_base chat_open chat_history player_list buy_tech tech_overlay \
           messages quit_prompt; do
    # quit_prompt's long TUI drive lets the rand()-driven civilians wander the
    # whole deck (two flapping tiles at y242 across runs); mask their traversal
    # band above the ripple band — ORIGIN_GOLDENS.md "NPC wander".
    # (${extra[@]+...}: macOS bash 3.2 + set -u abort on empty-array expansion.)
    local extra=()
    [ "$s" = quit_prompt ] && extra=(--mask 0,242,624,296)
    out="$(python3 "$TOLERANT" "$WORK/ingame_$s.png" \
           "$GOLDEN_DIR/ingame_$s.png" "${MASKS[@]}" ${extra[@]+"${extra[@]}"})"
    verdict="$(printf '%s\n' "$out" | head -1)"
    echo "ingame_$s: $verdict"
    case "$verdict" in
      *PASS*) ;;
      *) printf '%s\n' "$out"; fail=1 ;;
    esac
  done
  return "$fail"
}

OK=""
for attempt in 1 2; do
  PORT="$(pick_port)" bash "$REPO_ROOT/tools/cap/cap_ingame_cppx.sh" "$WORK"
  if gate_all; then OK="yes"; break; fi
  [ "$attempt" = 1 ] && echo "  marginal-tile flap; re-capturing once" >&2
done

[ -n "$OK" ] || { echo "FAIL 72_visual_regression_ingame" >&2; exit 1; }
echo "PASS 72_visual_regression_ingame"
