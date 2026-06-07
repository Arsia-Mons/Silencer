#!/usr/bin/env bash
# Persistent lobby-cluster capture for cppx visual-parity work.
# Boots the Go lobby + a headless silencer, drives connect -> auth ->
# character-create wizard -> lobby -> create-game -> (best-effort) staging/tech,
# dumping live cppx renders (960x720) to $OUT (default /tmp/cppx_renders).
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/../../tests/cli-agent/e2e/lib.sh"

OUT="${OUT:-/tmp/cppx_renders}"; mkdir -p "$OUT"
W="${W:-960}"; H="${H:-720}"
LOBBY_BIN="$(lobby_bin)"

TMP=$(mktemp -d)
LOBBY_DB="$TMP/lobby.json"; SILENCER_HOME="$TMP/home"; mkdir -p "$SILENCER_HOME" "$TMP/maps"
# Provision bundled maps so Create -> dedicated-spawn -> staging can succeed.
cp "$REPO_ROOT"/shared/assets/level/*.SIL "$TMP/maps/" 2>/dev/null || true
LOBBY_PORT=$(pick_port); PLAYER_AUTH_PORT=$(pick_port); MAP_API_PORT=$(pick_port); CTRL_PORT=$(pick_port)
SILENCER_VERSION="$(silencer_version)"

cleanup() {
  [ -n "${SILENCER_PID:-}" ] && stop_silencer "$SILENCER_PID" "$CTRL_PORT" || true
  [ -n "${LOBBY_PID:-}" ] && { kill "$LOBBY_PID" 2>/dev/null || true; wait "$LOBBY_PID" 2>/dev/null || true; }
  rm -rf "$TMP"
}
trap cleanup EXIT

"$LOBBY_BIN" -addr ":$LOBBY_PORT" -db "$LOBBY_DB" -version "$SILENCER_VERSION" \
  -game-binary "$SILENCER_BIN" -maps-dir "$TMP/maps" \
  -player-auth-addr ":$PLAYER_AUTH_PORT" -map-api-addr ":$MAP_API_PORT" >"$TMP/lobby.log" 2>&1 &
LOBBY_PID=$!
for i in $(seq 1 60); do (echo > "/dev/tcp/127.0.0.1/$LOBBY_PORT") 2>/dev/null && break; sleep 0.25
  [ "$i" = 60 ] && { echo "lobby never came up" >&2; cat "$TMP/lobby.log" >&2; exit 1; }; done

HOME="$SILENCER_HOME" "$SILENCER_BIN" --headless --control-port "$CTRL_PORT" \
  --lobby-host 127.0.0.1 --lobby-port "$LOBBY_PORT" >"/tmp/silencer-e2e-$CTRL_PORT.log" 2>&1 &
SILENCER_PID=$!
wait_alive "$CTRL_PORT"

wait_for_widget() {
  for i in $(seq 1 120); do
    found=$(cli --port "$CTRL_PORT" inspect | LABEL="$1" bun -e \
      'const r=JSON.parse(await new Response(Bun.stdin.stream()).text());const l=process.env.LABEL;
       console.log((r.nodes||[]).some((w)=>w.label===l||w.control_id===l)?"yes":"no")' 2>/dev/null || echo no)
    [ "$found" = yes ] && return 0; sleep 0.05
  done; echo "  widget '$1' never appeared" >&2; return 1
}
wait_for_lobby_state() {
  for i in $(seq 1 80); do
    ls=$(cli --port "$CTRL_PORT" state | bun -e 'console.log(JSON.parse(await new Response(Bun.stdin.stream()).text()).lobby_state||"")')
    [ "$ls" = "$1" ] && return 0; sleep 0.1
  done; echo "  lobby_state never became $1 (last=$ls)" >&2; return 1
}
shot() {
  cli --port "$CTRL_PORT" wait_frames --n 3 >/dev/null
  cli --port "$CTRL_PORT" screenshot --out "$OUT/$1.png" >/dev/null
  cli --port "$CTRL_PORT" inspect > "$OUT/$1.json" 2>/dev/null || true
  echo "  captured $1"
}

cli --port "$CTRL_PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
cli --port "$CTRL_PORT" resize --w "$W" --h "$H" >/dev/null
cli --port "$CTRL_PORT" click --label "Connect To Lobby" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000 >/dev/null
wait_for_widget "Username"
# The connect handshake reaches AUTHENTICATING on its own (no creds needed),
# populating the status log; capture then so the log well matches the golden.
wait_for_lobby_state AUTHENTICATING 2>/dev/null || true
shot lobby_connect

for ch in a l i c e; do cli --port "$CTRL_PORT" key --key "$ch" >/dev/null; done
cli --port "$CTRL_PORT" key --key tab >/dev/null
for ch in s e c r e t; do cli --port "$CTRL_PORT" key --key "$ch" >/dev/null; done
cli --port "$CTRL_PORT" click --label "Login/Create" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state CREATECHARACTER --timeout-ms 15000 >/dev/null
wait_for_widget "Create New Character"
shot character_create

cli --port "$CTRL_PORT" click --label "Create New Character" >/dev/null
wait_for_widget "Alias"
shot cc_alias
for ch in A l i c e; do cli --port "$CTRL_PORT" key --key "$ch" >/dev/null; done
cli --port "$CTRL_PORT" key --key enter >/dev/null
wait_for_widget "Black Rose"
shot cc_select_agency

cli --port "$CTRL_PORT" click --label "Noxis" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBY --timeout-ms 15000 >/dev/null
wait_for_widget "Send"
shot lobby_screen

# create_game panel
cli --port "$CTRL_PORT" click --label "NewGame" >/dev/null
wait_for_widget "CreateBack" && shot create_game

# best-effort staging + tech (needs the dedicated server to spawn from provisioned maps)
if cli --port "$CTRL_PORT" click --label "CreateGame" >/dev/null 2>&1; then
  if wait_for_lobby_state STAGING 2>/dev/null || wait_for_widget "Ready" 2>/dev/null; then
    shot game_staging
    if cli --port "$CTRL_PORT" click --label "TechSelect" >/dev/null 2>&1 || cli --port "$CTRL_PORT" click --label "Tech" >/dev/null 2>&1; then
      wait_for_widget "Tech" 2>/dev/null && shot tech_select
    fi
  else
    echo "  (staging not reached — capture later)"
  fi
fi
echo "lobby -> $OUT"
