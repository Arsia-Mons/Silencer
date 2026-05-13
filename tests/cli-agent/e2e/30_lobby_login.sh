#!/usr/bin/env bash
# Drives the full lobby login flow: spawn a local lobby on an unprivileged
# port + a fresh silencer pointing at it, click through MainMenu →
# LobbyConnect → fill username/password → Login → LOBBY → Go Back →
# MainMenu. Exercises LobbyScreen end-to-end (chrome, character/chat
# panels, lobby pump, GoBack), the Lobby Go service's TCP auth path, and
# the normalized CLI key-to-text path.
set -euo pipefail
. "$(dirname "$0")/lib.sh"

LOBBY_BIN=""
for candidate in \
  "$REPO_ROOT/services/lobby/lobby" \
  "$REPO_ROOT/services/lobby/lobby.exe" \
  "$REPO_ROOT/services/lobby/silencer-lobby" \
  "$REPO_ROOT/services/lobby/silencer-lobby.exe"; do
  if [ -x "$candidate" ]; then LOBBY_BIN="$candidate"; break; fi
done
if [ -z "$LOBBY_BIN" ]; then
  echo "lobby binary missing under services/lobby/ — build it via 'cd services/lobby && go build'" >&2
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
# rejects the handshake with "Wrong version". Probe the same build dirs
# lib.sh probes for the binary.
SILENCER_VERSION=""
for cache in \
  "$REPO_ROOT/build/CMakeCache.txt" \
  "$REPO_ROOT/clients/silencer/build/CMakeCache.txt" \
  "$REPO_ROOT/clients/silencer/build-unity/CMakeCache.txt" \
  "$REPO_ROOT/clients/silencer/build-release/CMakeCache.txt"; do
  if [ -f "$cache" ]; then
    SILENCER_VERSION=$(awk -F= '/^SILENCER_VERSION:STRING=/{print $2}' "$cache")
    if [ -n "$SILENCER_VERSION" ]; then break; fi
  fi
done
if [ -z "$SILENCER_VERSION" ]; then
  echo "could not read SILENCER_VERSION from any CMakeCache.txt" >&2
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

wait_for_widget() {
  local label="$1"
  for i in $(seq 1 100); do
    found=$(cli --port "$CTRL_PORT" inspect | LABEL="$label" bun -e \
      'const t=await new Response(Bun.stdin.stream()).text();
       const r=JSON.parse(t);
       const label=process.env.LABEL;
       console.log(r.widgets.some((w)=>w.label===label) ? "yes" : "no");' 2>/dev/null || echo no)
    if [ "$found" = "yes" ]; then return 0; fi
    sleep 0.05
  done
  echo "widget '$label' never appeared" >&2
  cli --port "$CTRL_PORT" inspect >&2 || true
  return 1
}

cli --port "$CTRL_PORT" wait_for_state --state MAINMENU --timeout-ms 15000
wait_for_widget "Connect To Lobby"
cli --port "$CTRL_PORT" click --label "Connect To Lobby" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000
wait_for_widget "Login"

# Type the credentials through the same key path real text input uses
# (auto-creates the account on first login).
for ch in a l i c e; do
  cli --port "$CTRL_PORT" key --key "$ch" >/dev/null
done
cli --port "$CTRL_PORT" key --key tab >/dev/null
for ch in s e c r e t; do
  cli --port "$CTRL_PORT" key --key "$ch" >/dev/null
done

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
wait_for_widget "Create Game"

# Go Back from the lobby returns to MAINMENU (FADEOUT is a brief
# transient that wait_for_state will skip past).
cli --port "$CTRL_PORT" back >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state MAINMENU --timeout-ms 10000

echo "PASS 30_lobby_login"
