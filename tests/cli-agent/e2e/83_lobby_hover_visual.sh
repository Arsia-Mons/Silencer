#!/usr/bin/env bash
# SIL-194: real LOBBY chrome buttons must show visible hover feedback.
set -euo pipefail
. "$(dirname "$0")/lib.sh"

LOBBY_BIN="$(lobby_bin)"

TMP=$(mktemp -d)
LOBBY_LOG="$TMP/lobby.log"
LOBBY_DB="$TMP/lobby.json"
SILENCER_HOME="$TMP/home"
mkdir -p "$SILENCER_HOME"

LOBBY_PORT=$(pick_port)
PLAYER_AUTH_PORT=$(pick_port)
MAP_API_PORT=$(pick_port)
CTRL_PORT=$(pick_port)
SILENCER_VERSION="$(silencer_version)"

cleanup() {
  if [ -n "${SILENCER_PID:-}" ]; then
    stop_silencer "$SILENCER_PID" "$CTRL_PORT" || true
  fi
  if [ -n "${LOBBY_PID:-}" ]; then
    kill "$LOBBY_PID" 2>/dev/null || true
    wait "$LOBBY_PID" 2>/dev/null || true
  fi
  rm -rf "$TMP"
}
trap cleanup EXIT

"$LOBBY_BIN" \
  -addr ":$LOBBY_PORT" \
  -db "$LOBBY_DB" \
  -version "$SILENCER_VERSION" \
  -game-binary "$SILENCER_BIN" \
  -maps-dir "$TMP/maps" \
  -player-auth-addr ":$PLAYER_AUTH_PORT" \
  -map-api-addr ":$MAP_API_PORT" \
  >"$LOBBY_LOG" 2>&1 &
LOBBY_PID=$!

for i in $(seq 1 60); do
  if (echo > "/dev/tcp/127.0.0.1/$LOBBY_PORT") 2>/dev/null; then
    break
  fi
  sleep 0.25
  if [ "$i" = 60 ]; then
    echo "lobby on :$LOBBY_PORT never came up" >&2
    cat "$LOBBY_LOG" >&2
    exit 1
  fi
done

HOME="$SILENCER_HOME" "$SILENCER_BIN" \
  --headless \
  --control-port "$CTRL_PORT" \
  --lobby-host 127.0.0.1 \
  --lobby-port "$LOBBY_PORT" \
  >"/tmp/silencer-e2e-$CTRL_PORT.log" 2>&1 &
SILENCER_PID=$!
wait_alive "$CTRL_PORT"

wait_for_widget() {
  local label="$1"
  for i in $(seq 1 100); do
    found=$(cli --port "$CTRL_PORT" inspect | LABEL="$label" bun -e \
      'const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
       const l = process.env.LABEL;
       console.log((r.nodes||[]).some((w)=>w.label===l||w.control_id===l) ? "yes" : "no");' 2>/dev/null || echo no)
    if [ "$found" = "yes" ]; then return 0; fi
    sleep 0.05
  done
  echo "widget '$label' never appeared" >&2
  cli --port "$CTRL_PORT" inspect >&2 || true
  return 1
}

wait_for_lobby_state() {
  local target="$1"
  for i in $(seq 1 80); do
    ls=$(cli --port "$CTRL_PORT" state | bun -e \
      'console.log(JSON.parse(await new Response(Bun.stdin.stream()).text()).lobby_state||"");')
    if [ "$ls" = "$target" ]; then return 0; fi
    sleep 0.1
  done
  echo "lobby_state never became $target (last=$ls)" >&2
  cat "/tmp/silencer-e2e-$CTRL_PORT.log" >&2 || true
  return 1
}

cli --port "$CTRL_PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
cli --port "$CTRL_PORT" resize --w 1920 --h 1080 >/dev/null
wait_for_widget "Connect To Lobby"
cli --port "$CTRL_PORT" click --label "Connect To Lobby" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000 >/dev/null
wait_for_widget "Username"

for ch in a l i c e; do
  cli --port "$CTRL_PORT" key --key "$ch" >/dev/null
done
cli --port "$CTRL_PORT" key --key tab >/dev/null
for ch in s e c r e t; do
  cli --port "$CTRL_PORT" key --key "$ch" >/dev/null
done
wait_for_lobby_state AUTHENTICATING
cli --port "$CTRL_PORT" click --label "Login/Create" >/dev/null
create_initial_character "Alice"
wait_for_widget "NewGame"

BEFORE_JSON="/tmp/sil194_hover_fixed_before.json"
AFTER_JSON="/tmp/sil194_hover_fixed_after.json"
BEFORE_PNG="/tmp/sil194_hover_fixed_before.png"
AFTER_PNG="/tmp/sil194_hover_fixed_after.png"

cli --port "$CTRL_PORT" hover_at --x 5 --y 5 >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 5 >/dev/null
cli --port "$CTRL_PORT" inspect >"$BEFORE_JSON"
NODE=$(TARGET="NewGame" BEFORE_JSON="$BEFORE_JSON" bun -e '
const r = JSON.parse(await Bun.file(process.env.BEFORE_JSON).text());
const n = (r.nodes || []).find((w) => w.control_id === process.env.TARGET && w.role === "button");
if (!n) { console.error("NewGame button missing"); process.exit(1); }
if (n.hovered) { console.error("NewGame unexpectedly hovered before hover_at"); process.exit(1); }
console.log(`${n.x} ${n.y} ${n.w} ${n.h}`);
')
read -r NX NY NW NH <<<"$NODE"
cli --port "$CTRL_PORT" screenshot --out "$BEFORE_PNG" >/dev/null

HX=$(bun -e "console.log(Math.round($NX + $NW / 2))")
HY=$(bun -e "console.log(Math.round($NY + $NH / 2))")
cli --port "$CTRL_PORT" hover_at --x "$HX" --y "$HY" >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 20 >/dev/null
cli --port "$CTRL_PORT" inspect >"$AFTER_JSON"
TARGET="NewGame" AFTER_JSON="$AFTER_JSON" bun -e '
const r = JSON.parse(await Bun.file(process.env.AFTER_JSON).text());
const n = (r.nodes || []).find((w) => w.control_id === process.env.TARGET && w.role === "button");
if (!n) { console.error("NewGame button missing after hover"); process.exit(1); }
if (!n.hovered) { console.error("NewGame did not report hovered=true"); process.exit(1); }
'
cli --port "$CTRL_PORT" screenshot --out "$AFTER_PNG" >/dev/null

python3 - "$NX" "$NY" "$NW" "$NH" "$BEFORE_PNG" "$AFTER_PNG" <<'PY'
import sys
from PIL import Image
import numpy as np

x, y, w, h = (float(a) for a in sys.argv[1:5])
before, after = sys.argv[5:7]
a = np.array(Image.open(before).convert("RGB"), dtype=int)
b = np.array(Image.open(after).convert("RGB"), dtype=int)
x0, x1 = int(x * 1.5) - 2, int((x + w) * 1.5) + 2
y0, y1 = int(y * 1.5) - 2, int((y + h) * 1.5) + 2
d = np.abs(a - b).sum(axis=2)[y0:y1, x0:x1]
diff_px = int((d > 0).sum())
diff_sum = int(d.sum())
diff_max = int(d.max() if d.size else 0)
print(f"NewGame hover region diff px {diff_px} sum {diff_sum} max {diff_max}")
if diff_px <= 0 or diff_sum <= 0:
    raise SystemExit("NewGame hover state reported true but button pixels did not change")
PY

echo "PASS 83_lobby_hover_visual"
