#!/usr/bin/env bash
# Visual regression: the gameplay-driven in-game surfaces vs the origin/main
# goldens (golden/ingame_{top_ticker,status_lines,secret_overlay}.png,
# 640x480 — provenance in golden/ORIGIN_GOLDENS.md "Gameplay-driven goldens").
#
# Capture runs through tools/cap/cap_ingame_cppx_extra.sh: the TUI-action
# tutorial drive (F4 ticker, base-door build fail/success at the deck data
# terminal, rocket purchase + flight). ingame_system_camera is captured but
# NOT gated: the origin golden carries origin's stale-temppalette UI dim
# (latched at base entry); the capture
# script still asserts the inset/frame are present on our side.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

TOLERANT="$REPO_ROOT/tools/cap/pixdiff_tolerant.py"
GOLDEN_DIR="$SCRIPT_DIR/golden"
WORK="$(mktemp -d /tmp/e2e-ingame-extra.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT

PORT="$(pick_port)" bash "$REPO_ROOT/tools/cap/cap_ingame_cppx_extra.sh" "$WORK"

# Masks: the three documented in-game nondeterminism regions (minimap inset,
# rain-ripple deck band, right-edge rain sliver) + per-surface extras below.
MASKS=(--mask 235,419,406,479 --mask 0,296,640,340 --mask 624,0,640,419)

FAIL=0
gate() { # gate <surface> [extra masks...]
  local s="$1"; shift
  local out verdict
  out="$(python3 "$TOLERANT" "$WORK/ingame_$s.png" \
         "$GOLDEN_DIR/ingame_$s.png" "${MASKS[@]}" "$@")"
  verdict="$(printf '%s\n' "$out" | head -1)"
  echo "ingame_$s: $verdict"
  case "$verdict" in
    *PASS*) ;;
    *) printf '%s\n' "$out"; FAIL=1 ;;
  esac
}

gate top_ticker
# wandering civilians: NPC behavior trees draw from the shared C rand() stream
# that wall-clock rain also consumes (origin civilian.cpp:61,
# behaviortree.cpp:81) — pose/x is nondeterministic in ANY two captures. One
# loiters by the status terminal, another on the deck right of the base door.
gate status_lines --mask 270,230,330,290
gate secret_overlay --mask 395,230,480,320

[ "$FAIL" -eq 0 ] || { echo "FAIL 76_visual_regression_ingame_extra" >&2; exit 1; }
echo "PASS 76_visual_regression_ingame_extra"
