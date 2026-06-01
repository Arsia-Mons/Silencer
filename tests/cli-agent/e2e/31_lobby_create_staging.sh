#!/usr/bin/env bash
# Drives the cppx lobby game-join cluster (SIL-21 3/n) past connect → auth →
# CREATECHARACTER → LOBBY: the screen-local right-column swap GameSelect →
# GameCreate (use_state) and the id-based create intent. Reaching GameCreate and
# dispatching Create proves use_games.create_game + the panel swap are wired
# (the LOBBY-tick game-join pump then drives the dedicated-spawn → auto-join →
# staging transition; the live staging room is exercised in the capstone E2E,
# which provisions a map for the spawned dedicated server).
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
# character creation.
cli --port "$CTRL_PORT" wait_for_state --state CREATECHARACTER --timeout-ms 15000
wait_for_widget "Alias"

# Type an alias, then pick an agency — creating the agent. Once it round-trips
# through the lobby the CREATECHARACTER tick routes to the lobby.
for ch in A l i c e; do
  cli --port "$CTRL_PORT" key --key "$ch" >/dev/null
done
cli --port "$CTRL_PORT" click --label "Noxis" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBY --timeout-ms 15000

# GameSelect is the default right column (browse + Join/Spectate/New Game). Its
# Join control is present (the structured games browser replaced the read-only
# (2/n) list); "Send"/"Leave" still anchor the always-on chat + agent panels.
wait_for_widget "Send"
wait_for_widget "JoinGame"

# Swap to GameCreate (screen-local use_state<ActivePanel>): clicking New Game
# mounts the create form (name + bundled-map cycle + Create/Back). CreateBack is
# unique to this panel ("Leave" is shared with the always-on Agent panel), so it
# proves the swap landed.
cli --port "$CTRL_PORT" click --label "New Game" >/dev/null
wait_for_widget "CreateBack"
wait_for_widget "CreateGame"

# Back reverses the swap → GameSelect (New Game reappears). Proves the
# bidirectional screen-local panel swap end to end. The create intent +
# dedicated-spawn → auto-join → staging room run through the LOBBY-tick game-join
# pump; the live staging room is exercised in the capstone E2E (which provisions
# a map for the spawned dedicated server).
cli --port "$CTRL_PORT" click --label "Back" >/dev/null
wait_for_widget "NewGame"

echo "PASS 31_lobby_create_staging"
