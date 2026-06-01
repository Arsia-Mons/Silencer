#!/usr/bin/env bash
# Regression: at a resized (scaled) resolution, the cppx LobbyConnect Username
# field's hit bounds must be resolved from the retained Yoga layout, not a stale
# native/window rectangle. We resize, read the field's layout bounds from the
# introspection tree, click its center, type, and confirm the keystroke routed
# to that field — proving click-to-focus uses the same layout space the renderer
# laid out.
set -euo pipefail
. "$(dirname "$0")/lib.sh"

LOBBY_BIN="$(lobby_bin)"

TMP=$(mktemp -d)
LOBBY_LOG="$TMP/lobby.log"
LOBBY_DB="$TMP/lobby.json"
SILENCER_HOME="$TMP/home"
mkdir -p "$SILENCER_HOME" "$TMP/maps"

LOBBY_PORT=$(pick_port)
PLAYER_AUTH_PORT=$(pick_port)
MAP_API_PORT=$(pick_port)
CTRL_PORT=$(pick_port)

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
cli --port "$CTRL_PORT" click --label "Play Online" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000
cli --port "$CTRL_PORT" wait_frames --n 5 >/dev/null

# The Username input bounds must be in the scaled layout space (not a stale
# native rect): x within [0, render width], positive size. Emit its center.
target=$(cli --port "$CTRL_PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const w = (r.nodes||[]).find((x) => x.label === "Username" && x.role === "input");
if (!w) { console.error("Username input missing"); process.exit(1); }
if (w.x < 0 || w.w <= 0 || w.h <= 0 || w.x + w.w > 4000) {
  console.error(`Username bounds not in scaled layout space: ${JSON.stringify(w)}`);
  process.exit(1);
}
console.log(`${Math.floor(w.x + w.w / 2)},${Math.floor(w.y + w.h / 2)}`);
')

IFS=, read -r x y <<<"$target"
cli --port "$CTRL_PORT" click_at --x "$x" --y "$y" >/dev/null
cli --port "$CTRL_PORT" key --key z >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 2 >/dev/null

typed=$(cli --port "$CTRL_PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const w = (r.nodes||[]).find((x) => x.label === "Username" && x.role === "input");
console.log(w && w.value === "z" ? "ok" : JSON.stringify(w || null));
')

if [ "$typed" != "ok" ]; then
  echo "scaled click-to-focus did not route keyboard input to Username: $typed" >&2
  exit 1
fi

echo "PASS 15_lobbyconnect_scaled_text_focus"
