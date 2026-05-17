#!/usr/bin/env bash
# Verifies the real LobbyScreen uses the contiguous stepped right-pane chrome
# rather than two fully bordered sibling boxes.
#
# Captures the default GameSelect lobby state, then compares the right-pane crop
# against a committed reference from the intended UI. This is intentionally NOT
# a legacy-parity test: the stepped contour is a deliberate divergence.
#
# Pass gate: < 1.0% pixdiff vs reference.png over the right-pane crop.
#
# Usage:   bash tests/lobby-ui/lobby_stepped_pane_test/run.sh
# Updates: rerun with REGEN=1 to overwrite reference.png from the live binary.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REF="$OUT_DIR/reference.png"
PIXDIFF="$REPO_ROOT/tools/pixdiff/build/pixdiff"

if [ -x "$REPO_ROOT/build/Silencer.app/Contents/MacOS/Silencer" ]; then
  export SILENCER_BIN="$REPO_ROOT/build/Silencer.app/Contents/MacOS/Silencer"
elif [ -x "$REPO_ROOT/clients/silencer/build/Silencer.app/Contents/MacOS/Silencer" ]; then
  export SILENCER_BIN="$REPO_ROOT/clients/silencer/build/Silencer.app/Contents/MacOS/Silencer"
fi
. "$REPO_ROOT/tests/cli-agent/e2e/lib.sh"

LOBBY_BIN="$REPO_ROOT/services/lobby/lobby"
[ -x "$LOBBY_BIN" ] || { echo "lobby binary missing at $LOBBY_BIN" >&2; exit 1; }
[ -x "$PIXDIFF" ] || { echo "pixdiff binary missing at $PIXDIFF" >&2; exit 1; }

if [ ! -f "$REF" ] && [ "${REGEN:-0}" != "1" ]; then
  echo "reference missing at $REF — rerun with REGEN=1 after building the client" >&2
  exit 1
fi

TMP=$(mktemp -d)
LOBBY_LOG="$TMP/lobby.log"
LOBBY_DB="$TMP/lobby.json"
SILENCER_HOME="$TMP/home"
mkdir -p "$TMP/maps" "$SILENCER_HOME"

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

write_config() {
  local home="$1"
  local cfgdir="$home/Library/Application Support/Silencer"
  mkdir -p "$cfgdir"
  cat > "$cfgdir/config.cfg" <<EOF
mapapiurl=http://127.0.0.1:$MAP_API_PORT
EOF
}

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

write_config "$SILENCER_HOME"
HOME="$SILENCER_HOME" "$SILENCER_BIN" \
  --headless \
  --control-port "$CTRL_PORT" \
  --lobby-host 127.0.0.1 \
  --lobby-port "$LOBBY_PORT" \
  >"/tmp/silencer-lobby-stepped-pane-$CTRL_PORT.log" 2>&1 &
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

cli --port "$CTRL_PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
wait_for_widget "Connect To Lobby"
cli --port "$CTRL_PORT" click --label "Connect To Lobby" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000 >/dev/null
wait_for_widget "Login"
cli --port "$CTRL_PORT" set_text --uid 1 --text "alice" >/dev/null
cli --port "$CTRL_PORT" set_text --uid 2 --text "secret" >/dev/null
wait_for_lobby_state AUTHENTICATING
cli --port "$CTRL_PORT" click --label "Login" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBY --timeout-ms 15000 >/dev/null
wait_for_widget "Create Game"
cli --port "$CTRL_PORT" lobby_show_panel --panel select >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 60 >/dev/null
cli --port "$CTRL_PORT" step --frames 30 >/dev/null

SHOT="${LOBBY_UI_SHOT:-$OUT_DIR/screenshot.png}"
cli --port "$CTRL_PORT" screenshot --out "$SHOT" >/dev/null

if [ "${REGEN:-0}" = "1" ]; then
  cp "$SHOT" "$REF"
  echo "regenerated $REF"
  echo "re-run without REGEN to verify"
  exit 0
fi

CROP="235,60,405,395"
DIFF=$("$PIXDIFF" --crop "$CROP" "$REF" "$SHOT")
echo "pixdiff (crop $CROP) = ${DIFF}%"
if ! awk -v d="$DIFF" 'BEGIN { exit (d + 0 < 1.0) ? 0 : 1 }'; then
  echo "FAIL: pixdiff ${DIFF}% >= 1.0% threshold" >&2
  exit 1
fi
echo "PASS lobby_stepped_pane_test"
