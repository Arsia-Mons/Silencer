#!/usr/bin/env bash
# Visual regression: mission_summary vs its origin golden (1920×1080).
#
# Drives a REAL deterministic lobby match end (tools/cap/
# cap_mission_summary_cppx.sh): Go lobby + 1-player game on STAR72.SIL, GAS
# timeLimitSecs=5 bundle edit, dedicated server SIGSTOPped on the lobby's
# [stats] line so the client's own message cycle reaches CheckForEndOfGame →
# MISSIONSUMMARY (exercises the GameStateObject replication fix end to end).
# The capture loses a ~6% message-cycle race occasionally (exit 2) — one retry.
#
# Gate: pixdiff_tolerant printed verdict (global <1% AND zero hot tiles).
set -euo pipefail
. "$(dirname "$0")/lib.sh"

GOLDEN="$REPO_ROOT/tests/cli-agent/e2e/golden/mission_summary.png"
RENDER="/tmp/cppx_renders/mission_summary_e2e.png"
[ -f "$GOLDEN" ] || { echo "golden missing: $GOLDEN" >&2; exit 1; }

rc=0
bash "$REPO_ROOT/tools/cap/cap_mission_summary_cppx.sh" "$RENDER" || rc=$?
if [ "$rc" = 2 ]; then
  echo "  message-cycle race lost; retrying once"
  rc=0
  bash "$REPO_ROOT/tools/cap/cap_mission_summary_cppx.sh" "$RENDER" || rc=$?
fi
[ "$rc" = 0 ] || { echo "mission_summary capture failed (rc=$rc)" >&2; exit 1; }

out="$(python3 "$REPO_ROOT/tools/cap/pixdiff_tolerant.py" "$RENDER" "$GOLDEN")"
echo "mission_summary: $out"
echo "$out" | head -1 | grep -q "PASS" || { echo "mission_summary diverged from golden" >&2; exit 1; }

echo "PASS 74_visual_regression_mission_summary"
