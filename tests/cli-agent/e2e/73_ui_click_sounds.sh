#!/usr/bin/env bash
# UI interaction sounds (origin ClientUi.cpp PlayMenuButtonSound): the click
# edge fires on (a) hover-ENTER of an audible button (deduped while hovering
# the same one), and (b) keyboard navigation landing on a button. Headless
# audio is disabled, so the assertion reads the edge counter (`ui_audio` op).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT
wait_alive "$PORT"
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

clicks() {
  cli --port "$PORT" ui_audio | bun -e \
    'console.log(JSON.parse(await Bun.stdin.text()).clicks)'
}

cli --port "$PORT" inspect > "/tmp/sounds-ins-$PORT.json"
python3 - "$PORT" <<'PY' > "/tmp/sounds-xy-$PORT.txt"
import json, sys
ns = [n for n in json.load(open(f"/tmp/sounds-ins-{sys.argv[1]}.json"))["nodes"]
      if n.get("role") == "button" and not n.get("disabled")]
assert len(ns) >= 2, "need two buttons on MAINMENU"
for n in ns[:2]:
    print(int(n["x"] + n["w"] / 2), int(n["y"] + n["h"] / 2))
PY
X1=$(sed -n 1p "/tmp/sounds-xy-$PORT.txt" | cut -d' ' -f1)
Y1=$(sed -n 1p "/tmp/sounds-xy-$PORT.txt" | cut -d' ' -f2)
X2=$(sed -n 2p "/tmp/sounds-xy-$PORT.txt" | cut -d' ' -f1)
Y2=$(sed -n 2p "/tmp/sounds-xy-$PORT.txt" | cut -d' ' -f2)

BASE="$(clicks)"

# hover-ENTER edge on button 1
cli --port "$PORT" hover_at --x "$X1" --y "$Y1" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
C1="$(clicks)"
[ "$C1" -eq "$((BASE + 1))" ] || { echo "hover-enter edge expected +1 (got $BASE -> $C1)" >&2; exit 1; }

# steady hover on the same button: NO new edge
cli --port "$PORT" wait_frames --n 10 >/dev/null
C2="$(clicks)"
[ "$C2" -eq "$C1" ] || { echo "steady hover must not re-fire (got $C1 -> $C2)" >&2; exit 1; }

# hover-ENTER edge on a different button
cli --port "$PORT" hover_at --x "$X2" --y "$Y2" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
C3="$(clicks)"
[ "$C3" -eq "$((C2 + 1))" ] || { echo "second hover-enter expected +1 (got $C2 -> $C3)" >&2; exit 1; }

# keyboard navigation onto a button
cli --port "$PORT" key --key down >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
C4="$(clicks)"
[ "$C4" -gt "$C3" ] || { echo "nav onto a button expected an edge (got $C3 -> $C4)" >&2; exit 1; }

echo "PASS 73_ui_click_sounds"
