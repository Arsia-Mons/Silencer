#!/usr/bin/env bash
# Captures the seven lobby baselines from the LEGACY build by driving the
# headless silencer through the CLI. Outputs into the script's directory.
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

wait_for_iface() {
  for i in $(seq 1 100); do
    iface=$(cli --port "$CTRL_PORT" state | bun -e \
      'const t=await new Response(Bun.stdin.stream()).text(); console.log(JSON.parse(t).current_interface_id||0);')
    [ "${iface:-0}" != "0" ] && return 0
    sleep 0.05
  done
  return 1
}

# Lobby panels live in child interfaces nested under the screen's
# top-level interface. `click --label` only searches the current
# interface, so to click a panel button we walk into each child
# interface via `inspect --interface-id` and click by --id (which
# resolves through the world object table, not the iface's objects).
# Prints the button's global object id to stdout, or empty on miss.
find_button_id() {
  local label="$1"
  # object_type=1 is INTERFACE (clients/silencer/src/actors/objecttypes.h)
  local children
  children=$(cli --port "$CTRL_PORT" inspect \
    | jq -r '.widgets[] | select(.kind=="other" and .object_type==1) | .id')
  for child in $children; do
    local hit
    hit=$(cli --port "$CTRL_PORT" inspect --interface-id "$child" \
      | jq -r --arg L "$label" \
        '.widgets[] | select(.kind=="button" and (.label|ascii_downcase)==($L|ascii_downcase)) | .id' \
      | head -n1)
    if [ -n "$hit" ]; then echo "$hit"; return 0; fi
  done
  return 1
}

click_by_label_recursive() {
  local label="$1"
  local id
  id=$(find_button_id "$label" || true)
  if [ -z "$id" ]; then
    echo "WIDGET_NOT_FOUND (recursive): $label" >&2
    return 1
  fi
  cli --port "$CTRL_PORT" click --id "$id" >/dev/null
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
wait_for_iface
cli --port "$CTRL_PORT" click --label "Connect To Lobby" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000
wait_for_iface
cli --port "$CTRL_PORT" set_text --uid 1 --text "alice" >/dev/null
cli --port "$CTRL_PORT" set_text --uid 2 --text "secret" >/dev/null
wait_for_lobby_state AUTHENTICATING
cli --port "$CTRL_PORT" click --label "Login" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBY --timeout-ms 15000
wait_for_iface

# Let the lobby pump a few frames so panel content stabilizes.
cli --port "$CTRL_PORT" wait_frames --n 30 >/dev/null

# Default lobby right-pane is GameSelect. The full-frame screenshot
# covers title chrome + character + chat + gameselect at once; each
# panel-specific baseline crops the same source frame at a known
# region (panel coordinates are immutable across builds).
cli --port "$CTRL_PORT" screenshot --out "$OUT_DIR/title_chrome.png" >/dev/null
cp "$OUT_DIR/title_chrome.png" "$OUT_DIR/character.png"
cp "$OUT_DIR/title_chrome.png" "$OUT_DIR/chat.png"
cp "$OUT_DIR/title_chrome.png" "$OUT_DIR/gameselect.png"

# Click "Create Game" → GameCreatePanel
click_by_label_recursive "Create Game"
cli --port "$CTRL_PORT" wait_frames --n 10 >/dev/null
cli --port "$CTRL_PORT" screenshot --out "$OUT_DIR/gamecreate.png" >/dev/null

# NOTE: gamejoin.png / gametech.png are NOT captured here yet — they
# require a successful CreateGame round-trip (dedicated server spawn
# + UDP heartbeat). Two blockers identified during P2:
#   (1) cli set_text/select --label coerces a purely-numeric string
#       arg to a JSON number, and the C++ control dispatch's
#       `args["label"].get<std::string>()` throws type_error 302
#       (silencer crashes with libc++abi terminate). This must be
#       fixed before any nested-iface widget can be addressed by
#       global object id.
#   (2) The dedicated server process spawned by services/lobby/proc.go
#       needs to find its map assets when launched out of $TMP. Path
#       resolution for the spawned child needs verification.
# Until both are fixed, the two baselines remain uncaptured.

echo "captured baselines:"
for f in title_chrome character chat gameselect gamecreate; do
  if [ -s "$OUT_DIR/$f.png" ]; then
    size=$(stat -f '%z' "$OUT_DIR/$f.png" 2>/dev/null || stat -c '%s' "$OUT_DIR/$f.png")
    echo "  $f.png ($size bytes)"
  else
    echo "  $f.png MISSING OR EMPTY" >&2
    exit 1
  fi
done
echo "DEFERRED: gamejoin.png, gametech.png — see comment above and README.md"
