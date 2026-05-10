#!/usr/bin/env bash
# Drives the full lobby login flow: spawn a local lobby on an unprivileged
# port + a fresh silencer pointing at it, click through MainMenu →
# LobbyConnect → fill username/password → Login → LOBBY → Go Back →
# MainMenu. Exercises LobbyScreen end-to-end (chrome, character/chat
# panels, lobby pump, GoBack), the Lobby Go service's TCP auth path, and
# the CLI's set_text-on-TEXTINPUT support.
set -euo pipefail
. "$(dirname "$0")/lib.sh"

LOBBY_BIN="$REPO_ROOT/services/lobby/lobby"
if [ ! -x "$LOBBY_BIN" ]; then
  echo "lobby binary missing at $LOBBY_BIN — build it via 'cd services/lobby && go build'" >&2
  exit 1
fi

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

# Match the version baked into the silencer binary — without this the lobby
# rejects the handshake with "Wrong version".
SILENCER_VERSION=$(awk -F= '/^SILENCER_VERSION:STRING=/{print $2}' \
  "$REPO_ROOT/clients/silencer/build/CMakeCache.txt")
if [ -z "$SILENCER_VERSION" ]; then
  echo "could not read SILENCER_VERSION from CMakeCache.txt" >&2
  exit 1
fi

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

# Lobby on an unprivileged port. -version "" accepts any client version
# (the SILENCER_VERSION baked into the binary). -map-upload-key empty so
# we don't need to thread a key through the test.
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
# persisted to the dev's real config when CharacterPanel auto-saves on
# first lobby entry.
HOME="$SILENCER_HOME" "$SILENCER_BIN" \
  --headless \
  --control-port "$CTRL_PORT" \
  --lobby-host 127.0.0.1 \
  --lobby-port "$LOBBY_PORT" \
  >"/tmp/silencer-e2e-$CTRL_PORT.log" 2>&1 &
SILENCER_PID=$!
wait_alive "$CTRL_PORT"

# wait_for_state resolves the frame state flips, which is one frame BEFORE
# the case body runs PushScreen + sets currentinterface. Poll until the
# interface is built so set_text / click have something to dispatch onto.
wait_for_iface() {
  for i in $(seq 1 100); do
    iface=$(cli --port "$CTRL_PORT" state | bun -e \
      'const t=await new Response(Bun.stdin.stream()).text(); console.log(JSON.parse(t).current_interface_id||0);')
    if [ "${iface:-0}" != "0" ]; then return 0; fi
    sleep 0.05
  done
  echo "current_interface_id never became non-zero" >&2
  return 1
}

cli --port "$CTRL_PORT" wait_for_state --state MAINMENU --timeout-ms 15000
wait_for_iface
cli --port "$CTRL_PORT" click --label "Connect To Lobby" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000
wait_for_iface

# Type the credentials (auto-creates the account on first login).
cli --port "$CTRL_PORT" set_text --uid 1 --text "alice" >/dev/null
cli --port "$CTRL_PORT" set_text --uid 2 --text "secret" >/dev/null

# The Login button only dispatches credentials when the lobby state machine
# has advanced through Connect → version-check → AUTHENTICATING; a click
# before that is silently consumed. The `state` op exposes lobby_state for
# exactly this kind of synchronization.
wait_for_lobby_state() {
  local target="$1"
  for i in $(seq 1 80); do
    ls=$(cli --port "$CTRL_PORT" state | bun -e \
      'const t=await new Response(Bun.stdin.stream()).text(); console.log(JSON.parse(t).lobby_state||"");')
    if [ "$ls" = "$target" ]; then return 0; fi
    sleep 0.1
  done
  echo "lobby_state never became $target (last=$ls)" >&2
  return 1
}
wait_for_lobby_state AUTHENTICATING

cli --port "$CTRL_PORT" click --label "Login" >/dev/null

# Auth + lobby state pump can take a couple of seconds in CI.
cli --port "$CTRL_PORT" wait_for_state --state LOBBY --timeout-ms 15000
wait_for_iface

# Go Back from the lobby returns to MAINMENU (FADEOUT is a brief
# transient that wait_for_state will skip past).
cli --port "$CTRL_PORT" back >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state MAINMENU --timeout-ms 10000

echo "PASS 30_lobby_login"
