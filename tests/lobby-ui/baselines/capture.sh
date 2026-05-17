#!/usr/bin/env bash
# Captures the remaining legacy-parity lobby baselines by driving headless
# Silencer through the CLI. Outputs into the script's directory.
#
# Requires:
#   - clients/silencer/build/Silencer.app (or platform equivalent)
#   - services/lobby/lobby
# Re-run after rebuilding either binary to refresh the baselines.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

. "$REPO_ROOT/tests/cli-agent/e2e/lib.sh"

LOBBY_BIN="$REPO_ROOT/services/lobby/lobby"
if [ ! -x "$LOBBY_BIN" ]; then
  echo "lobby binary missing at $LOBBY_BIN — build it via 'cd services/lobby && go build -o lobby .'" >&2
  exit 1
fi

TMP=$(mktemp -d)
LOBBY_LOG="$TMP/lobby.log"
LOBBY_DB="$TMP/lobby.json"
SILENCER_HOME="$TMP/home"
mkdir -p "$SILENCER_HOME"

LOBBY_PORT=$(pick_port)
PLAYER_AUTH_PORT=$(pick_port)
MAP_API_PORT=$(pick_port)
CTRL_PORT=$(pick_port)

SILENCER_VERSION=""
for d in build build-unity build-release; do
  cache="$REPO_ROOT/clients/silencer/$d/CMakeCache.txt"
  if [ -f "$cache" ]; then
    SILENCER_VERSION=$(awk -F= '/^SILENCER_VERSION:STRING=/{print $2}' "$cache")
    [ -n "$SILENCER_VERSION" ] && break
  fi
done
[ -z "$SILENCER_VERSION" ] && { echo "no SILENCER_VERSION in CMakeCache.txt" >&2; exit 1; }

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
  if (echo > "/dev/tcp/127.0.0.1/$LOBBY_PORT") 2>/dev/null; then break; fi
  sleep 0.25
  if [ "$i" = 60 ]; then
    echo "lobby on :$LOBBY_PORT never came up" >&2
    cat "$LOBBY_LOG" >&2; exit 1
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
      'const t=await new Response(Bun.stdin.stream()).text();
       const r=JSON.parse(t);
       const label=process.env.LABEL;
       console.log(r.widgets.some((w)=>w.label===label) ? "yes" : "no");' 2>/dev/null || echo no)
    [ "$found" = "yes" ] && return 0
    sleep 0.05
  done
  return 1
}

wait_for_lobby_state() {
  local target="$1"
  for i in $(seq 1 80); do
    ls=$(cli --port "$CTRL_PORT" state | bun -e \
      'const t=await new Response(Bun.stdin.stream()).text(); console.log(JSON.parse(t).lobby_state||"");')
    [ "$ls" = "$target" ] && return 0
    sleep 0.1
  done
  return 1
}

# MainMenu → LobbyConnect → Login → LOBBY
cli --port "$CTRL_PORT" wait_for_state --state MAINMENU --timeout-ms 15000
wait_for_widget "Connect To Lobby"
cli --port "$CTRL_PORT" click --label "Connect To Lobby" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000
wait_for_widget "Login"
cli --port "$CTRL_PORT" set_text --uid 1 --text "alice" >/dev/null
cli --port "$CTRL_PORT" set_text --uid 2 --text "secret" >/dev/null
wait_for_lobby_state AUTHENTICATING
cli --port "$CTRL_PORT" click --label "Login" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBY --timeout-ms 15000
wait_for_widget "Create Game"

# Let the lobby pump a few frames so panel content stabilizes.
cli --port "$CTRL_PORT" wait_frames --n 30 >/dev/null

# The full-frame screenshot covers the title chrome and character panel at once.
cli --port "$CTRL_PORT" screenshot --out "$OUT_DIR/title_chrome.png" >/dev/null
cp "$OUT_DIR/title_chrome.png" "$OUT_DIR/character.png"

echo "captured baselines:"
for f in title_chrome character; do
  if [ -s "$OUT_DIR/$f.png" ]; then
    size=$(stat -f '%z' "$OUT_DIR/$f.png" 2>/dev/null || stat -c '%s' "$OUT_DIR/$f.png")
    echo "  $f.png ($size bytes)"
  else
    echo "  $f.png MISSING OR EMPTY" >&2
    exit 1
  fi
done
