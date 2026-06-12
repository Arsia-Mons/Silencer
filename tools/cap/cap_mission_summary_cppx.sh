#!/usr/bin/env bash
# cap_mission_summary_cppx.sh — render OUR client's mission_summary at
# 1920×1080 through the same deterministic match-end drive as the origin
# golden (tools/cap/cap_mission_summary_origin.sh): real Go lobby, 1-player
# game on STAR72.SIL, GAS timeLimitSecs=5 bundle edit (restored on exit),
# dedicated server SIGSTOPped on the lobby's "[stats]" log line so the
# client's own message cycle reaches CheckForEndOfGame before the
# connection-loss path. Our tree carries the GameStateObject replication fix
# (gamestateobject.cpp) permanently, so no patch step exists here.
#
# Usage: tools/cap/cap_mission_summary_cppx.sh [OUT_PNG]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/../../tests/cli-agent/e2e/lib.sh"

OUT_PNG="${1:-/tmp/cppx_renders/mission_summary.png}"
mkdir -p "$(dirname "$OUT_PNG")"
LOBBY_BIN="$(lobby_bin)"
GAS_JSON="$SILENCER_BUILD_DIR/Silencer.app/Contents/assets/gas/gamemodes.json"
[ -f "$GAS_JSON" ] || { echo "bundle gas missing: $GAS_JSON" >&2; exit 1; }

TMP=$(mktemp -d)
PORT=$(pick_port); LOBBY_PORT=$(pick_port); AUTH_PORT=$(pick_port); MAPAPI_PORT=$(pick_port)
mkdir -p "$TMP/home" "$TMP/maps"
cp "$REPO_ROOT"/shared/assets/level/*.SIL "$TMP/maps/"

cp -p "$GAS_JSON" "$TMP/gamemodes.bak.json"
python3 - "$GAS_JSON" <<'PY'
import json, sys
p = sys.argv[1]
j = json.load(open(p))
for m in j["gameModes"]:
    if m["id"] == 0: m["timeLimitSecs"] = 5
json.dump(j, open(p, "w"), indent=2)
PY

LOBBY_PID=""; CLIENT_PID=""; SERVER_PID=""
cleanup() {
  cp -p "$TMP/gamemodes.bak.json" "$GAS_JSON"
  [ -n "$SERVER_PID" ] && kill -KILL "$SERVER_PID" 2>/dev/null || true
  [ -n "$CLIENT_PID" ] && kill "$CLIENT_PID" 2>/dev/null || true
  [ -n "$LOBBY_PID" ] && kill "$LOBBY_PID" 2>/dev/null || true
  wait 2>/dev/null || true
  rm -rf "$TMP"
}
trap cleanup EXIT

"$LOBBY_BIN" -addr ":$LOBBY_PORT" -db "$TMP/lobby.json" -version "$(silencer_version)" \
  -game-binary "$SILENCER_BIN" -maps-dir "$TMP/maps" \
  -player-auth-addr ":$AUTH_PORT" -map-api-addr ":$MAPAPI_PORT" >"$TMP/lobby.log" 2>&1 &
LOBBY_PID=$!
for i in $(seq 1 60); do (echo > "/dev/tcp/127.0.0.1/$LOBBY_PORT") 2>/dev/null && break; sleep 0.25
  [ "$i" = 60 ] && { echo "lobby never came up" >&2; exit 1; }; done

HOME="$TMP/home" "$SILENCER_BIN" --headless --control-port "$PORT" \
  --lobby-host 127.0.0.1 --lobby-port "$LOBBY_PORT" >"/tmp/silencer-e2e-$PORT.log" 2>&1 &
CLIENT_PID=$!
wait_alive "$PORT"

c() { cli --port "$PORT" "$@"; }

c wait_for_state --state MAINMENU --timeout-ms 20000 >/dev/null
c resize --w 1920 --h 1080 >/dev/null
c click --label "Connect To Lobby" >/dev/null
c wait_for_state --state LOBBYCONNECT --timeout-ms 5000 >/dev/null
sleep 0.3
for ch in a l i c e; do c key --key "$ch" >/dev/null; done
c key --key tab >/dev/null
for ch in s e c r e t; do c key --key "$ch" >/dev/null; done
for _ in $(seq 1 80); do
  ls=$(c state | bun -e 'console.log(JSON.parse(await new Response(Bun.stdin.stream()).text()).lobby_state||"")')
  [ "$ls" = "AUTHENTICATING" ] && break; sleep 0.1
done
c click --label "Login/Create" >/dev/null
c wait_for_state --state CREATECHARACTER --timeout-ms 15000 >/dev/null
sleep 0.5
c click --label "Create New Character" >/dev/null; sleep 0.5
for ch in A l i c e; do c key --key "$ch" >/dev/null; done
c key --key enter >/dev/null; sleep 0.5
c click --label "Noxis" >/dev/null
c wait_for_state --state LOBBY --timeout-ms 15000 >/dev/null
sleep 1

c click --label "NewGame" >/dev/null; sleep 0.8
c click --label "STAR72.SIL" >/dev/null; sleep 0.3
c click --label "CreateGame" >/dev/null
for _ in $(seq 1 100); do
  c inspect | grep -q '"Ready"' && break; sleep 0.2
done
sleep 0.5
c click --label "Ready" >/dev/null
c wait_for_state --state INGAME --timeout-ms 30000 >/dev/null
echo "in match; waiting for the server's stats registration..."

SERVER_PID=$(grep -oE 'spawned dedicated server for game [0-9]+ \(pid=[0-9]+\)' "$TMP/lobby.log" | tail -1 | grep -oE '[0-9]+\)$' | tr -d ')')
[ -n "$SERVER_PID" ] || { echo "no dedicated server pid in lobby.log" >&2; exit 1; }

for i in $(seq 1 240); do grep -q '\[stats\]' "$TMP/lobby.log" && break; sleep 0.25
  [ "$i" = 240 ] && { echo "server never registered stats" >&2; exit 1; }; done
sleep 0.3
kill -STOP "$SERVER_PID"
echo "server SIGSTOPped (pid=$SERVER_PID); waiting for MISSIONSUMMARY..."

if ! c wait_for_state --state MISSIONSUMMARY --timeout-ms 30000 >/dev/null; then
  echo "client never reached MISSIONSUMMARY (message-cycle race lost — rerun)" >&2
  exit 2
fi
# In-game pinned the headless surface to 640×480; re-assert the menu size
# AFTER the state-new tick has unloaded the map (resize while map.loaded
# re-pins to 640×480), then settle past the fade ramp.
c wait_frames --n 5 >/dev/null
c resize --w 1920 --h 1080 >/dev/null
sleep 3

c screenshot --out "$OUT_PNG" >/dev/null
bun -e '
  const b = new DataView(await Bun.file(process.argv[1]).arrayBuffer());
  if (b.getUint32(16) !== 1920 || b.getUint32(20) !== 1080) { console.error("not 1920x1080"); process.exit(1); }
' "$OUT_PNG"
c inspect > "${OUT_PNG%.png}.json"
echo "captured $OUT_PNG"

# Functional wheel probe — AFTER the rest capture so the PNG stays rest-state.
# origin mission_summary wheel (scrollDelta accumulate + clamp, screen.cpp
# :254-258/:415-417): scenario 74 asserts the window from these three dumps.
read -r SX SY <<<"$(bun -e '
  const r = JSON.parse(await Bun.file(process.argv[1]).text());
  const b = (r.nodes||[]).find((n)=>n.control_id==="SummaryScroll");
  if (!b) { console.error("no SummaryScroll node"); process.exit(1); }
  console.log(`${Math.round(b.x+b.w/2)} ${Math.round(b.y+b.h/2)}`);
' "${OUT_PNG%.png}.json")"
probe() { # probe <dy> <out_suffix>
  c scroll --x "$SX" --y "$SY" --dy "$1" >/dev/null
  c wait_frames --n 2 >/dev/null
  c inspect > "${OUT_PNG%.png}.$2.json"
}
probe -3   scrolled
probe -100 end
probe 200  top
echo "scroll probes dumped beside $OUT_PNG"
