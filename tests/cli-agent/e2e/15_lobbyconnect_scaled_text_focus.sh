#!/usr/bin/env bash
# Regression: at uiScale > 1, LOBBYCONNECT text-input hit bounds must be
# resolved from Clay's virtual layout, not stale native/window rectangles.
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

TMP=$(mktemp -d)
LOBBY_LOG="$TMP/lobby.log"
LOBBY_DB="$TMP/lobby.json"
SILENCER_HOME="$TMP/home"
mkdir -p "$SILENCER_HOME" "$TMP/maps"

LOBBY_PORT=$(pick_port)
PLAYER_AUTH_PORT=$(pick_port)
MAP_API_PORT=$(pick_port)
CTRL_PORT=$(pick_port)

SILENCER_VERSION=""
for cache in \
  "$REPO_ROOT/build/CMakeCache.txt" \
  "$REPO_ROOT/clients/silencer/build/CMakeCache.txt" \
  "$REPO_ROOT/clients/silencer/build-unity/CMakeCache.txt" \
  "$REPO_ROOT/clients/silencer/build-release/CMakeCache.txt"; do
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
    cat "$LOBBY_LOG" >&2
    exit 1
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

cli --port "$CTRL_PORT" wait_for_state --state MAINMENU --timeout-ms 15000
cli --port "$CTRL_PORT" resize --w 2560 --h 1440 >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 3 >/dev/null
cli --port "$CTRL_PORT" click --label "Connect To Lobby" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000
cli --port "$CTRL_PORT" wait_frames --n 5 >/dev/null

target=$(cli --port "$CTRL_PORT" inspect | bun -e '
const t = await new Response(Bun.stdin.stream()).text();
const r = JSON.parse(t);
const w = r.widgets.find((x) => x.label === "Username" && x.kind === "textinput");
if (!w) {
  console.error("Username text input missing");
  process.exit(1);
}
if (w.x < 0 || w.x + w.w > 900) {
  console.error(`Username bounds are not in scaled virtual space: ${JSON.stringify(w)}`);
  process.exit(1);
}
console.log(`${Math.floor(w.x + w.w / 2)},${Math.floor(w.y + w.h / 2)}`);
')

IFS=, read -r x y <<<"$target"
cli --port "$CTRL_PORT" click_at --x "$x" --y "$y" >/dev/null
cli --port "$CTRL_PORT" key --key z >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 2 >/dev/null

typed=$(cli --port "$CTRL_PORT" inspect | bun -e '
const t = await new Response(Bun.stdin.stream()).text();
const r = JSON.parse(t);
const w = r.widgets.find((x) => x.label === "Username" && x.kind === "textinput");
console.log(w && w.text === "z" ? "ok" : JSON.stringify(w || null));
')

if [ "$typed" != "ok" ]; then
  echo "scaled click-to-focus did not route keyboard input to Username: $typed" >&2
  exit 1
fi

echo "PASS 15_lobbyconnect_scaled_text_focus"
