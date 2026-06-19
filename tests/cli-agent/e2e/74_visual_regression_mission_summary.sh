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

# Functional: stats scrollbox wheel (origin scrollDelta accumulate + clamp,
# mission_summary_screen.cpp:254-258/:415-417 — 1 row/notch, window clamped to
# [0, lines−27]). The capture script wheels AFTER the rest screenshot and dumps
# inspect at rest / −3 notches / clamped end / wheeled back to top; the first
# (resp. last) visible line in the 180×308 stats column must track the window.
bun -e '
const lines = async (p) => {
  const r = JSON.parse(await Bun.file(p).text());
  const b = (r.nodes||[]).find((n)=>n.control_id==="SummaryScroll");
  if (!b) throw new Error(`no SummaryScroll node in ${p}`);
  return (r.nodes||[]).filter((n)=>n.role==="text" &&
      n.x >= b.x-1 && n.x < b.x+b.w && n.y >= b.y-1 && n.y < b.y+b.h)
    .sort((a,c)=>a.y-c.y).map((n)=>n.value);
};
const fail = (m) => { console.error(`mission_summary scroll: ${m}`); process.exit(1); };
const base = process.argv[1];
const rest = await lines(`${base}.json`);
const scrolled = await lines(`${base}.scrolled.json`);
const end = await lines(`${base}.end.json`);
const top = await lines(`${base}.top.json`);
if (!/^Kills:/.test(rest[0] ?? "")) fail(`rest first line = ${rest[0]}`);
// −3 notches: window starts at line 3 (blank) ⇒ first visible text = the
// "Secrets" header (line 4).
if (scrolled[0] !== "Secrets") fail(`after -3 notches first line = ${scrolled[0]}`);
// Clamped end: the last summary line (Flamer "  Player kills:") is visible.
if (!/^ {2}Player kills:/.test(end[end.length-1] ?? ""))
  fail(`clamped-end last line = ${end[end.length-1]}`);
// Wheel back past the top clamps at the rest window.
if (top[0] !== rest[0] || top.length !== rest.length)
  fail(`top clamp mismatch: ${top[0]} vs ${rest[0]}`);
' "${RENDER%.png}"

echo "PASS 74_visual_regression_mission_summary"
