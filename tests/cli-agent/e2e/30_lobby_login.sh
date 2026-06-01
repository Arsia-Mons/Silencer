#!/usr/bin/env bash
# Drives the cppx lobby connect → auth → route path end-to-end: spawn a local
# lobby on an unprivileged port + a fresh silencer pointing at it, click through
# MainMenu → LobbyConnect → fill username/password → Login → real TCP connect +
# version-check + auth → route to CREATECHARACTER (fresh account). Exercises the
# game-tick connect flow (LobbyConnectFlow), the cppx LobbyConnect screen +
# use_lobby_session intents, the Lobby Go service's TCP auth path, and the
# normalized CLI key-to-text path. The character-create + lobby flows themselves
# are SIL-21; reaching CREATECHARACTER proves connect→auth→route.
set -euo pipefail
. "$(dirname "$0")/lib.sh"

LOBBY_BIN="$(lobby_bin)"

# Per-run scratch — both for the lobby's user db (-db) and the silencer's
# config dir (HOME-rooted on macOS / Linux). Two separate pid/log files so
# the EXIT trap can tear both down cleanly.
TMP=$(mktemp -d)
LOBBY_LOG="$TMP/lobby.log"
LOBBY_DB="$TMP/lobby.json"
SILENCER_HOME="$TMP/home"
mkdir -p "$SILENCER_HOME"

LOBBY_PORT=$(pick_port)
PLAYER_AUTH_PORT=$(pick_port)
MAP_API_PORT=$(pick_port)
CTRL_PORT=$(pick_port)

# Match the version baked into the selected silencer binary — without this the
# lobby rejects the handshake with "Wrong version".
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

# Lobby on an unprivileged port. -version matches the binary so the handshake
# passes; -maps-dir/-map-api-addr keep map traffic local.
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

# Wait for the lobby's TCP listener to come up.
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

# Silencer with a fresh HOME so the lobby host/port override doesn't get
# persisted to the dev's real config.
HOME="$SILENCER_HOME" "$SILENCER_BIN" \
  --headless \
  --control-port "$CTRL_PORT" \
  --lobby-host 127.0.0.1 \
  --lobby-port "$LOBBY_PORT" \
  >"/tmp/silencer-e2e-$CTRL_PORT.log" 2>&1 &
SILENCER_PID=$!
wait_alive "$CTRL_PORT"

# Poll the retained cppx tree for a node whose control id OR accessibility label
# equals the argument (the introspection `inspect` op returns `nodes`).
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

cli --port "$CTRL_PORT" wait_for_state --state MAINMENU --timeout-ms 15000
wait_for_widget "Play Online"
cli --port "$CTRL_PORT" click --label "Play Online" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000
wait_for_widget "Username"

# Type the credentials through the same key path real text input uses
# (auto-creates the account on first login). Username is autofocused; Tab moves
# focus to the password field.
for ch in a l i c e; do
  cli --port "$CTRL_PORT" key --key "$ch" >/dev/null
done
cli --port "$CTRL_PORT" key --key tab >/dev/null
for ch in s e c r e t; do
  cli --port "$CTRL_PORT" key --key "$ch" >/dev/null
done

# The Login button only submits credentials once the connect flow has advanced
# (on the game tick) through Connect → version-check → AUTHENTICATING; a click
# before that is silently consumed. `state` exposes lobby_state for this sync.
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
wait_for_lobby_state AUTHENTICATING

cli --port "$CTRL_PORT" click --label "Login/Create" >/dev/null

# Fresh account → server reports no characters → the connect flow routes to
# character creation. The 3-step wizard opens on the roster screen (step0): click
# "Create New Character" to reach the alias step (step1).
cli --port "$CTRL_PORT" wait_for_state --state CREATECHARACTER --timeout-ms 15000
wait_for_widget "Create New Character"
cli --port "$CTRL_PORT" click --label "Create New Character" >/dev/null

# step1: type an alias into the "Alias" input, then "Continue" (AliasConfirm,
# which is disabled until the alias is non-empty) advances to the agency step.
wait_for_widget "Alias"
for ch in A l i c e; do
  cli --port "$CTRL_PORT" key --key "$ch" >/dev/null
done
cli --port "$CTRL_PORT" click --label "Continue" >/dev/null

# step2: pick an agency — creating the agent. Once it round-trips through the
# lobby the CREATECHARACTER tick routes to the lobby.
wait_for_widget "Black Rose"
cli --port "$CTRL_PORT" click --label "Noxis" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBY --timeout-ms 15000

# The lobby renders the real read cluster (agent + chat + games panels), not the
# old scaffold: its Send/Leave controls are present.
wait_for_widget "Send"
wait_for_widget "Leave"

echo "PASS 30_lobby_login"
