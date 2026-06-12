#!/usr/bin/env bash
# SIL-203: the cppx main menu must not hitch when the first high-resolution
# UI frame bakes chrome after startup/resize. This forces the same renderer
# reset path a high-DPI window takes and caps the first post-resize frame.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
SHOT=""
cleanup() {
  stop_silencer "$PID" "$PORT"
  [ -z "$SHOT" ] || rm -f "$SHOT"
}
trap cleanup EXIT

elapsed_ms() {
  perl -MTime::HiRes=time -e 'printf "%.1f", (time() * 1000) - $ARGV[0]' "$1"
}

now_ms() {
  perl -MTime::HiRes=time -e 'printf "%.3f", time() * 1000'
}

wait_alive "$PORT"
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

cli --port "$PORT" resize --w 640 --h 480 >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null

cli --port "$PORT" resize --w 1920 --h 1080 >/dev/null
SHOT="$(mktemp -t silencer-mainmenu-hitch.XXXXXX.png)"
start="$(now_ms)"
cli --port "$PORT" screenshot --out "$SHOT" >/dev/null
dt="$(elapsed_ms "$start")"

ok="$(DT="$dt" bun -e 'console.log(Number(process.env.DT) <= 750 ? "1" : "0")')"
if [ "$ok" != "1" ]; then
  echo "first 1920x1080 main-menu frame hitched: ${dt}ms > 750ms" >&2
  exit 1
fi

if cli --port "$PORT" inspect | bun -e '
const data = await new Response(Bun.stdin.stream()).json();
const fallback = (data.nodes ?? []).some((n) => n.role === "text" && n.value === "SILENCER");
process.exit(fallback ? 0 : 1);
'; then
  echo "main menu rendered the text logo fallback after chrome bake" >&2
  exit 1
fi

echo "PASS 89_main_menu_startup_no_hitch (${dt}ms)"
