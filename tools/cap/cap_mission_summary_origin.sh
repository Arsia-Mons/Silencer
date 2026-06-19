#!/usr/bin/env bash
# cap_mission_summary_origin.sh — capture origin/main's MISSIONSUMMARY screen
# headlessly at 1920×1080 (menu-cluster golden pipeline).
#
# Drive: real Go lobby + origin client (alice/secret → Alice/Noxis) → Create
# Game on STAR72.SIL → Ready → lobby-spawned dedicated server runs the match.
# Match end is forced deterministically via GAS: this script temporarily sets
# gameModes[id=0].timeLimitSecs=5 in the BUILT BUNDLE's gas/gamemodes.json
# (Contents/assets/gas — both the client and the spawned dedicated server read
# it; restored on exit). The authority (dedicated server) ends the match at
# tickcount/tps >= 5s with winningteamid=0xFFFF (1-team draw).
#
# REQUIRED CAPTURE PATCH (uncommitted, in ORIGIN_ROOT — like the fade patch):
# GameStateObject::Tick must early-return on World::REPLICA after adopting the
# replicated winningTeamId into world.winningteamid. Stock origin clobbers the
# replicated field with the local (never-set) winningteamid every TickObjects,
# so a time-limit match end NEVER reaches CheckForEndOfGame on a lobby client
# (it drops to LOBBY via connection-loss when the server exits). The secrets
# win path sets replica winningteamid naturally via Team::Tick — the patch
# only makes the time-limit end take the same, already-reachable screen path.
# This script verifies the patch is applied (git diff non-empty on the file).
#
# Server-lifetime control: the dedicated server quits ~7s after it registers
# stats (its message_i==72 → lobby.log "[stats]" line; quit at message_i>=240),
# and its MSG_DISCONNECT would put the client on the CONNECTION LOST → LOBBY
# path before the client's own message_i reaches 240. So on the "[stats]" log
# line the script SIGSTOPs the server: snapshots stop (client world freezes —
# irrelevant, the client already adopted winningteamid) but the client's local
# messaging keeps ticking to >=240 → GoToState(MISSIONSUMMARY) (client is
# lobby-AUTHENTICATED). The 10s peer timeout loses to the <=10.6s worst-case
# message cycle ~94% of runs; rerun on the rare CONNECTION LOST loss.
#
# The summary screen is MENU-side: MISSIONSUMMARY unloads the game + map, so
# the headless surface returns to the window size (1920×1080) like the other
# 13 menu goldens. Binary may be pristine OR fade-patched: the capture settles
# ~3s after the state flips, well past either fade ramp. Determinism: fresh
# lobby db every run → fresh account, 1-player draw, zero stats, "+ 0 XP";
# two independent runs measured byte-identical.
#
# Usage: tools/cap/cap_mission_summary_origin.sh [OUT_PNG]
#   ORIGIN_ROOT  override origin worktree (default .worktrees/origin-capture)
#
# The fresh-account draw ALREADY renders the upgrade state: "+ 0 XP",
# "*NEW UPGRADE AVAILABLE*" banner and all six Upgrade buttons (measured) —
# no seeded-XP variant needed.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
ORIGIN_ROOT="${ORIGIN_ROOT:-$(cd "$REPO_ROOT/.." && pwd)/origin-capture}"
OUT_PNG="${1:-/tmp/mission_summary_origin.png}"
BIN="$ORIGIN_ROOT/clients/silencer/build/Silencer.app/Contents/MacOS/Silencer"
GAS_JSON="$ORIGIN_ROOT/clients/silencer/build/Silencer.app/Contents/assets/gas/gamemodes.json"
LOBBY_BIN="$REPO_ROOT/services/lobby/lobby"
[ -x "$BIN" ] || { echo "origin binary missing: $BIN" >&2; exit 1; }
[ -x "$LOBBY_BIN" ] || { echo "lobby binary missing: $LOBBY_BIN" >&2; exit 1; }

if git -C "$ORIGIN_ROOT" diff --quiet -- clients/silencer/src/game/state/gamestateobject.cpp; then
  echo "origin-capture lacks the replica winningTeamId capture patch (see header)" >&2
  exit 1
fi

TMP=$(mktemp -d)
PORT=$((20000 + RANDOM % 20000))
LOBBY_PORT=$((PORT + 1)); AUTH_PORT=$((PORT + 2)); MAPAPI_PORT=$((PORT + 3))
mkdir -p "$TMP/home" "$TMP/maps"
cp "$ORIGIN_ROOT"/shared/assets/level/*.SIL "$TMP/maps/"

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

"$LOBBY_BIN" -addr ":$LOBBY_PORT" -db "$TMP/lobby.json" -version 00058 \
  -game-binary "$BIN" -maps-dir "$TMP/maps" \
  -player-auth-addr ":$AUTH_PORT" -map-api-addr ":$MAPAPI_PORT" >"$TMP/lobby.log" 2>&1 &
LOBBY_PID=$!
for i in $(seq 1 60); do (echo > "/dev/tcp/127.0.0.1/$LOBBY_PORT") 2>/dev/null && break; sleep 0.25
  [ "$i" = 60 ] && { echo "lobby never came up" >&2; exit 1; }; done

HOME="$TMP/home" "$BIN" --headless --control-port "$PORT" \
  --lobby-host 127.0.0.1 --lobby-port "$LOBBY_PORT" >"$TMP/client.log" 2>&1 &
CLIENT_PID=$!

cli() { bun "$ORIGIN_ROOT/clients/cli/index.ts" --port "$PORT" "$@"; }
for _ in $(seq 1 60); do cli ping >/dev/null 2>&1 && break; sleep 0.5; done

cli wait_for_state --state MAINMENU --timeout-ms 20000 >/dev/null
cli resize --w 1920 --h 1080 >/dev/null
cli click --label "Connect To Lobby" >/dev/null
cli wait_for_state --state LOBBYCONNECT --timeout-ms 5000 >/dev/null
for ch in a l i c e; do cli key --key "$ch" >/dev/null; done
cli key --key tab >/dev/null
for ch in s e c r e t; do cli key --key "$ch" >/dev/null; done
for _ in $(seq 1 80); do
  ls=$(cli state | bun -e 'console.log(JSON.parse(await new Response(Bun.stdin.stream()).text()).lobby_state||"")')
  [ "$ls" = "AUTHENTICATING" ] && break; sleep 0.1
done
cli click --label "Login/Create" >/dev/null
cli wait_for_state --state CREATECHARACTER --timeout-ms 15000 >/dev/null
sleep 0.5
cli click --label "Create New Character" >/dev/null; sleep 0.5
for ch in A l i c e; do cli key --key "$ch" >/dev/null; done
cli key --key enter >/dev/null; sleep 0.5
cli click --label "Noxis" >/dev/null
cli wait_for_state --state LOBBY --timeout-ms 15000 >/dev/null
sleep 1

cli click --label "Create Game" >/dev/null; sleep 0.8
cli click --label "STAR72.SIL" >/dev/null; sleep 0.3
cli click --label "Create" >/dev/null
for _ in $(seq 1 100); do
  cli inspect | grep -q '"Ready"' && break; sleep 0.2
done
sleep 0.5
cli click --label "Ready" >/dev/null
cli wait_for_state --state INGAME --timeout-ms 30000 >/dev/null
echo "in match; waiting for the server's stats registration..."

SERVER_PID=$(grep -oE 'spawned dedicated server for game [0-9]+ \(pid=[0-9]+\)' "$TMP/lobby.log" | tail -1 | grep -oE '[0-9]+\)$' | tr -d ')')
[ -n "$SERVER_PID" ] || { echo "no dedicated server pid in lobby.log" >&2; exit 1; }

for i in $(seq 1 240); do grep -q '\[stats\]' "$TMP/lobby.log" && break; sleep 0.25
  [ "$i" = 240 ] && { echo "server never registered stats" >&2; exit 1; }; done
sleep 0.3
kill -STOP "$SERVER_PID"
echo "server SIGSTOPped (pid=$SERVER_PID); waiting for MISSIONSUMMARY..."

if ! cli wait_for_state --state MISSIONSUMMARY --timeout-ms 30000 >/dev/null; then
  echo "client never reached MISSIONSUMMARY (message-cycle race lost — rerun)" >&2
  exit 2
fi
# In-game pinned the headless surface to 640×480; re-assert the menu window
# size AFTER the state-new tick has unloaded the map (resize while map.loaded
# re-pins to 640×480), then settle past the fade ramp.
cli wait_frames --n 5 >/dev/null
cli resize --w 1920 --h 1080 >/dev/null
sleep 3

cli screenshot --out "$OUT_PNG" >/dev/null
bun -e '
  const b = new DataView(await Bun.file(process.argv[1]).arrayBuffer());
  if (b.getUint32(16) !== 1920 || b.getUint32(20) !== 1080) { console.error("not 1920x1080"); process.exit(1); }
' "$OUT_PNG"
cli inspect > "${OUT_PNG%.png}.json" 2>/dev/null || true
echo "captured $OUT_PNG"
