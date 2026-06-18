#!/usr/bin/env bash
# cap_hover_cppx.sh — render OUR client's hovered-mainmenu-oval state at
# 1920×1080 (gate target: tests/cli-agent/e2e/golden/hover_mainmenu_oval.png).
# Drive: MAINMENU → hover the first oval (Tutorial) via hover_at → the
# AppButton ramp settles at phase 4 (~210ms) → stable-pair capture (two
# consecutive byte-identical frames only happen inside the logo HOLD window
# with the ramp settled).
#
# Usage: tools/cap/cap_hover_cppx.sh [OUT_PNG]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/../../tests/cli-agent/e2e/lib.sh"

OUT_PNG="${1:-/tmp/cppx_renders/hover_mainmenu_oval.png}"
mkdir -p "$(dirname "$OUT_PNG")"
TMP=$(mktemp -d)
FRESH_HOME="$TMP/home"; mkdir -p "$FRESH_HOME"
PORT="$(pick_port)"
HOME="$FRESH_HOME" "$SILENCER_BIN" --headless --control-port "$PORT" \
  >"/tmp/silencer-e2e-$PORT.log" 2>&1 &
PID=$!
trap 'kill "$PID" 2>/dev/null || true; wait "$PID" 2>/dev/null || true; rm -rf "$TMP"' EXIT
wait_alive "$PORT"

c() { cli --port "$PORT" "$@"; }
c wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
c resize --w 1920 --h 1080 >/dev/null
c wait_frames --n 3 >/dev/null

FIRST=$(c inspect | bun -e '
  const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
  const btns = (r.nodes || []).filter((n) => n.role === "button");
  if (!btns.length) { console.error("no buttons"); process.exit(1); }
  const b = btns.reduce((a, x) => (x.y < a.y ? x : a));
  console.log(`${Math.round(b.x + b.w / 2)} ${Math.round(b.y + b.h / 2)}`)')
read -r HX HY <<<"$FIRST"
c hover_at --x "$HX" --y "$HY" >/dev/null
sleep 1 # phase 4 settles in 5 legacy frames (~210ms); generous

c screenshot --out "$TMP/a.png" >/dev/null
for _ in $(seq 1 40); do
  sleep 0.25
  c screenshot --out "$TMP/b.png" >/dev/null
  if cmp -s "$TMP/a.png" "$TMP/b.png"; then
    cp "$TMP/b.png" "$OUT_PNG"
    echo "captured $OUT_PNG"
    exit 0
  fi
  mv "$TMP/b.png" "$TMP/a.png"
done
echo "frame never stabilized" >&2
exit 1
