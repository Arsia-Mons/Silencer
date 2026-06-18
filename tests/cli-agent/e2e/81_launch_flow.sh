#!/usr/bin/env bash
# Launch flow end to end vs the lobby-spawned dedicated server:
#   staging room → (Choose Tech hop) → Ready → LAUNCH → INGAME.
#
# Closes the PARITY "staging → tech_select → launch full flow" row: scenario 31
# stops at the GameCreate panel swap and the mission_summary drive (tools/cap)
# only passes through launch — this scenario ASSERTS each leg:
#   1. Create Game on STAR72.SIL → the staging cockpit mounts (ChooseTech /
#      ChangeTeam / StagingReady + the host's roster row). There is no separate
#      GAMESTAGING game state — staging is the LOBBY-state cockpit swap, so the
#      leg is asserted via the staging widgets while state stays LOBBY.
#   2. Choose Tech swaps to the tech panel (BackToTeams + TechRow) and back.
#   3. Ready (host gate open: label "Ready", not "Waiting...") → the client
#      leaves LOBBY and reaches INGAME against the dedicated server the lobby
#      spawned (lobby log carries the spawn line).
#   4. In-game: world_state shows the launched map loaded (STAR72.SIL), the
#      dedicated server as a remote AUTHORITY peer, and our player spawned
#      (players non-empty, hp > 0).
# Teardown kills client + dedicated server + lobby.
set -euo pipefail
. "$(dirname "$0")/lib.sh"

LOBBY_BIN="$(lobby_bin)"

TMP=$(mktemp -d)
LOBBY_LOG="$TMP/lobby.log"
SILENCER_HOME="$TMP/home"
mkdir -p "$SILENCER_HOME" "$TMP/maps"
# The staging Ready gate (AllPeersDownloadedMap) needs the map servable.
cp "$REPO_ROOT"/shared/assets/level/*.SIL "$TMP/maps/"

LOBBY_PORT=$(pick_port)
PLAYER_AUTH_PORT=$(pick_port)
MAP_API_PORT=$(pick_port)
CTRL_PORT=$(pick_port)
SILENCER_VERSION="$(silencer_version)"

SERVER_PID=""
cleanup() {
  if [ -n "${SILENCER_PID:-}" ]; then
    stop_silencer "$SILENCER_PID" "$CTRL_PORT" || true
  fi
  [ -n "$SERVER_PID" ] && kill -KILL "$SERVER_PID" 2>/dev/null || true
  if [ -n "${LOBBY_PID:-}" ]; then
    kill "$LOBBY_PID" 2>/dev/null || true
    wait "$LOBBY_PID" 2>/dev/null || true
  fi
  rm -rf "$TMP"
}
trap cleanup EXIT

"$LOBBY_BIN" \
  -addr ":$LOBBY_PORT" \
  -db "$TMP/lobby.json" \
  -version "$SILENCER_VERSION" \
  -game-binary "$SILENCER_BIN" \
  -maps-dir "$TMP/maps" \
  -player-auth-addr ":$PLAYER_AUTH_PORT" \
  -map-api-addr ":$MAP_API_PORT" \
  >"$LOBBY_LOG" 2>&1 &
LOBBY_PID=$!
for i in $(seq 1 60); do
  (echo > "/dev/tcp/127.0.0.1/$LOBBY_PORT") 2>/dev/null && break
  sleep 0.25
  [ "$i" = 60 ] && { echo "lobby on :$LOBBY_PORT never came up" >&2; cat "$LOBBY_LOG" >&2; exit 1; }
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
    sleep 0.1
  done
  echo "widget '$label' never appeared" >&2
  cli --port "$CTRL_PORT" inspect >&2 || true
  return 1
}

# --- Lobby login + character (same drive as 31/74) ---
cli --port "$CTRL_PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
wait_for_widget "Connect To Lobby"
cli --port "$CTRL_PORT" click --label "Connect To Lobby" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000 >/dev/null
wait_for_widget "Username"
for ch in a l i c e; do cli --port "$CTRL_PORT" key --key "$ch" >/dev/null; done
cli --port "$CTRL_PORT" key --key tab >/dev/null
for ch in s e c r e t; do cli --port "$CTRL_PORT" key --key "$ch" >/dev/null; done
for _ in $(seq 1 80); do
  ls=$(cli --port "$CTRL_PORT" state | bun -e \
    'console.log(JSON.parse(await new Response(Bun.stdin.stream()).text()).lobby_state||"")')
  [ "$ls" = "AUTHENTICATING" ] && break
  sleep 0.1
done
cli --port "$CTRL_PORT" click --label "Login/Create" >/dev/null
create_initial_character "Alice"

# --- Leg 1: create the game, land in the staging cockpit ---
wait_for_widget "NewGame"
cli --port "$CTRL_PORT" click --label "NewGame" >/dev/null
wait_for_widget "MapRow"
cli --port "$CTRL_PORT" click --label "STAR72.SIL" >/dev/null
cli --port "$CTRL_PORT" click --label "CreateGame" >/dev/null

wait_for_widget "StagingReady"
wait_for_widget "ChooseTech"
wait_for_widget "ChangeTeam"
state=$(cli --port "$CTRL_PORT" state | bun -e \
  'console.log(JSON.parse(await new Response(Bun.stdin.stream()).text()).state)')
[ "$state" = "LOBBY" ] || { echo "staging cockpit should live in LOBBY state, got $state" >&2; exit 1; }
# Roster row: our agent appears in the staging roster.
cli --port "$CTRL_PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
if (!(r.nodes||[]).some((n)=>n.role==="text" && (n.value||"").includes("Alice")))
  { console.error("staging roster missing the host agent row"); process.exit(1); }'
grep -q "spawned dedicated server" "$LOBBY_LOG" || {
  echo "lobby never spawned the dedicated server" >&2; cat "$LOBBY_LOG" >&2; exit 1; }

# --- Leg 2: tech-select hop and back ---
cli --port "$CTRL_PORT" click --label "ChooseTech" >/dev/null
wait_for_widget "BackToTeams"
wait_for_widget "TechRow"
cli --port "$CTRL_PORT" click --label "BackToTeams" >/dev/null
wait_for_widget "StagingReady"

# --- Leg 3: Ready → launch. The host Ready gate (AllPeersDownloadedMap)
# resolves once the map round-trips; the button label flips Waiting... → Ready.
for _ in $(seq 1 100); do
  cli --port "$CTRL_PORT" inspect | grep -q '"Ready"' && break
  sleep 0.2
done
sleep 0.3
cli --port "$CTRL_PORT" click --label "Ready" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state INGAME --timeout-ms 30000 >/dev/null

# --- Leg 4: in the match — map loaded, dedicated authority, player spawned ---
SERVER_PID=$(grep -oE 'spawned dedicated server for game [0-9]+ \(pid=[0-9]+\)' "$LOBBY_LOG" \
  | tail -1 | grep -oE '[0-9]+\)$' | tr -d ')')
[ -n "$SERVER_PID" ] || { echo "no dedicated server pid in lobby.log" >&2; exit 1; }

for i in $(seq 1 100); do
  if cli --port "$CTRL_PORT" world_state | bun -e '
    const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
    process.exit((r.players?.length ?? 0) > 0 ? 0 : 1);' 2>/dev/null; then break; fi
  sleep 0.2
  [ "$i" = 100 ] && { echo "player never spawned in-match" >&2; exit 1; }
done

cli --port "$CTRL_PORT" world_state | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const fail = (m) => { console.error(m); process.exit(1); };
if (r.map !== "STAR72.SIL") fail(`launched map not loaded: ${r.map}`);
if (!((r.peerlist?.length ?? 0) >= 2)) fail(`expected client + dedicated in peerlist, got ${JSON.stringify(r.peerlist)}`);
if (r.authoritypeer === r.localpeerid) fail("dedicated server should hold authority");
const p = r.players?.[0];
if (!p || !(p.hp > 0)) fail(`player not spawned alive: ${JSON.stringify(p)}`);
const me = (r.peerlist||[]).find((q) => q.id === r.localpeerid);
if (!me || !(me.controlledlist||[]).includes(p.id))
  fail(`local peer does not control the spawned player: ${JSON.stringify(me)}`);'

echo "PASS 81_launch_flow"
