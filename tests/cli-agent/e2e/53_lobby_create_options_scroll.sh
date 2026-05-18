#!/usr/bin/env bash
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

PIXDIFF="$REPO_ROOT/tools/pixdiff/build/pixdiff"
[ -x "$PIXDIFF" ] || {
  echo "pixdiff binary missing at $PIXDIFF" >&2
  exit 1
}

TMP="$(mktemp -d)"
LOBBY_LOG="$TMP/lobby.log"
LOBBY_DB="$TMP/lobby.json"
SILENCER_HOME="$TMP/home"
OUT_DIR="$TMP/out"
mkdir -p "$SILENCER_HOME/Library/Application Support/Silencer" "$TMP/maps" "$OUT_DIR"

LOBBY_PORT="$(pick_port)"
PLAYER_AUTH_PORT="$(pick_port)"
MAP_API_PORT="$(pick_port)"
CTRL_PORT="$(pick_port)"

cat > "$SILENCER_HOME/Library/Application Support/Silencer/config.cfg" <<EOF
mapapiurl=http://127.0.0.1:$MAP_API_PORT
EOF

SILENCER_VERSION=""
for cache in \
  "$REPO_ROOT/build/CMakeCache.txt" \
  "$REPO_ROOT/clients/silencer/build/CMakeCache.txt" \
  "$REPO_ROOT/clients/silencer/build-unity/CMakeCache.txt" \
  "$REPO_ROOT/clients/silencer/build-release/CMakeCache.txt"; do
  if [ -f "$cache" ]; then
    SILENCER_VERSION="$(awk -F= '/^SILENCER_VERSION:STRING=/{print $2}' "$cache")"
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
    found="$(cli --port "$CTRL_PORT" inspect | LABEL="$label" bun -e \
      'const t=await new Response(Bun.stdin.stream()).text();
       const r=JSON.parse(t);
       const label=process.env.LABEL;
       console.log(r.widgets.some((w)=>w.label===label) ? "yes" : "no");' 2>/dev/null || echo no)"
    if [ "$found" = "yes" ]; then return 0; fi
    sleep 0.05
  done
  echo "widget '$label' never appeared" >&2
  cli --port "$CTRL_PORT" inspect >&2 || true
  return 1
}

wait_for_lobby_state() {
  local target="$1"
  for i in $(seq 1 80); do
    ls="$(cli --port "$CTRL_PORT" state | bun -e \
      'const t=await new Response(Bun.stdin.stream()).text(); console.log(JSON.parse(t).lobby_state||"");')"
    if [ "$ls" = "$target" ]; then return 0; fi
    sleep 0.1
  done
  echo "lobby_state never became $target (last=$ls)" >&2
  return 1
}

cli --port "$CTRL_PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
wait_for_widget "Connect To Lobby"
cli --port "$CTRL_PORT" click --label "Connect To Lobby" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000 >/dev/null
wait_for_widget "Login"
cli --port "$CTRL_PORT" set_text --uid 1 --text "scrolltest" >/dev/null
cli --port "$CTRL_PORT" set_text --uid 2 --text "secret" >/dev/null
wait_for_lobby_state AUTHENTICATING
cli --port "$CTRL_PORT" click --label "Login" >/dev/null
create_initial_character "Scrolltest"
wait_for_widget "Create Game"
cli --port "$CTRL_PORT" click --label "Create Game" >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 5 >/dev/null

cli --port "$CTRL_PORT" resize --w 640 --h 360 >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 3 >/dev/null

scroll_crop="$(cli --port "$CTRL_PORT" inspect | bun -e '
const text = await new Response(Bun.stdin.stream()).text();
const response = JSON.parse(text);
const widgets = response.widgets ?? [];
const elements = response.elements ?? [];
const scroll = elements.find((e) =>
  e.source === "clay" &&
  e.kind === "container" &&
  e.label === "Game Options Form" &&
  e.value === "scroll" &&
  e.w > 0 &&
  e.h > 0
);
if (!scroll) {
  console.error("missing game options scroll element");
  process.exit(1);
}
const hasSecurity = widgets.some((w) => w.id === "lobby.game_create.security");
const hasSpectatable = widgets.some((w) => w.id === "lobby.game_create.spectatable");
if (!hasSecurity || hasSpectatable) {
  console.error("tight viewport did not clip the options rows as expected");
  process.exit(1);
}
console.log([
  Math.max(0, Math.floor(scroll.x)),
  Math.max(0, Math.floor(scroll.y)),
  Math.max(1, Math.ceil(scroll.w)),
  Math.max(1, Math.ceil(scroll.h)),
].join(","));
')"
tight_h="${scroll_crop##*,}"

before="$OUT_DIR/options-before.png"
after="$OUT_DIR/options-after.png"
cli --port "$CTRL_PORT" screenshot --out "$before" >/dev/null
cli --port "$CTRL_PORT" scroll --label "Game Options Form" --amount 10 >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 2 >/dev/null
cli --port "$CTRL_PORT" screenshot --out "$after" >/dev/null

cli --port "$CTRL_PORT" inspect | bun -e '
const text = await new Response(Bun.stdin.stream()).text();
const response = JSON.parse(text);
const widgets = response.widgets ?? [];
const hasSecurity = widgets.some((w) => w.id === "lobby.game_create.security");
const hasSpectatable = widgets.some((w) => w.id === "lobby.game_create.spectatable");
if (hasSecurity || !hasSpectatable) {
  console.error("scrolling did not reveal the lower game options row");
  process.exit(1);
}
'

diff="$("$PIXDIFF" --crop "$scroll_crop" "$before" "$after")"
bun -e '
const diff = Number(process.argv[1]);
if (!Number.isFinite(diff) || diff <= 0.01) {
  console.error(`expected game options viewport to visibly scroll, got diff ${diff}`);
  process.exit(1);
}
' "$diff"

cli --port "$CTRL_PORT" resize --w 1280 --h 720 >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 3 >/dev/null

cli --port "$CTRL_PORT" inspect | bun -e '
const text = await new Response(Bun.stdin.stream()).text();
const response = JSON.parse(text);
const elements = response.elements ?? [];
const scroll = elements.find((e) =>
  e.source === "clay" &&
  e.kind === "container" &&
  e.label === "Game Options Form" &&
  e.value === "scroll"
);
const tightH = Number(process.argv[1]);
if (!scroll || !(scroll.h > tightH)) {
  console.error(`expected game options viewport to grow from ${tightH}, got ${scroll?.h}`);
  process.exit(1);
}
' "$tight_h"

echo "PASS 53_lobby_create_options_scroll ($OUT_DIR)"
