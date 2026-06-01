#!/usr/bin/env bash
# Captures every reachable UI journey on the CURRENT branch.
# Assumes the Clay-era CLI ops (resize, ingame_ui_mode, wait_ms)
# and the Clay-era uppercase labels ("OPTIONS", "CONTROLS", ...).
#
# Caller sets OUT_DIR. Working directory must be the repo root.
set -euo pipefail

if [ -z "${OUT_DIR:-}" ]; then
  echo "OUT_DIR must be set" >&2
  exit 1
fi
mkdir -p "$OUT_DIR"

source tests/cli-agent/e2e/lib.sh

VISUAL_HOME="$(mktemp -d "${TMPDIR:-/tmp}/silencer-visual-current-home.XXXXXX")"
mkdir -p "$VISUAL_HOME/Library/Application Support/Silencer" \
  "$VISUAL_HOME/.config/silencer"
cat > "$VISUAL_HOME/Library/Application Support/Silencer/config.cfg" <<EOF
lobbyhost=127.0.0.1
lobbyport=9
EOF
cp "$VISUAL_HOME/Library/Application Support/Silencer/config.cfg" \
  "$VISUAL_HOME/.config/silencer/config.cfg"
export HOME="$VISUAL_HOME"

PORT=""
PID=""

cleanup() {
  if [ -n "${PID:-}" ] && [ -n "${PORT:-}" ]; then
    stop_silencer "$PID" "$PORT" || true
  fi
  rm -rf "$VISUAL_HOME"
}

start_client() {
  PORT="$(pick_port)"
  PID="$(start_silencer "$PORT")"
  wait_alive "$PORT"
}

stop_client() {
  if [ -n "${PID:-}" ] && [ -n "${PORT:-}" ]; then
    stop_silencer "$PID" "$PORT" || true
  fi
  PID=""
  PORT=""
}

trap cleanup EXIT

snap() {
  local name="$1"
  cli --port "$PORT" wait_ms --n 1500 >/dev/null
  cli --port "$PORT" screenshot --out "$OUT_DIR/${name}.png" >/dev/null
  echo "  captured $name"
}

wait_for_current_widget() {
  wait_for_widget_on_port "$PORT" "$1"
}

resize() {
  cli --port "$PORT" resize --w "$1" --h "$2" >/dev/null
  cli --port "$PORT" wait_ms --n 1500 >/dev/null
}

go_back() {
  cli --port "$PORT" back >/dev/null
  cli --port "$PORT" wait_ms --n 1500 >/dev/null
}

wait_for_game_world() {
  for _ in $(seq 1 60); do
    if cli --port "$PORT" world_state 2>/dev/null | bun -e '
      const text = await new Response(Bun.stdin.stream()).text();
      const response = JSON.parse(text);
      const state = response.result ?? response;
      if ((state.objects_count ?? 0) > 0 && (state.players?.length ?? 0) > 0) process.exit(0);
      process.exit(1);
    ' 2>/dev/null; then
      return 0
    fi
    cli --port "$PORT" wait_ms --n 500 >/dev/null
  done
  return 1
}

capture_ingame() {
  local name="$1"
  local mode="$2"
  local width="${3:-640}"
  local height="${4:-480}"

  start_client
  wait_for_current_widget "Connect To Lobby"
  resize 640 480
  cli --port "$PORT" click --label Tutorial >/dev/null
  cli --port "$PORT" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 20000 >/dev/null
  wait_for_game_world
  cli --port "$PORT" ingame_ui_mode --mode "$mode" >/dev/null
  if [ "$width" != "640" ] || [ "$height" != "480" ]; then
    resize "$width" "$height"
  fi
  cli --port "$PORT" wait_frames --n 3 >/dev/null
  snap "$name"
  stop_client
}

start_client

# --- Phase 1: menu surfaces at 640x480 ---
wait_for_current_widget "Connect To Lobby"
cli --port "$PORT" resize --w 640 --h 480 >/dev/null
snap "01_mainmenu_640x480"

cli --port "$PORT" click --label Options >/dev/null
wait_for_current_widget "Controls"
snap "02_options_root_640x480"

cli --port "$PORT" click --label Controls >/dev/null
wait_for_current_widget "Cancel"
snap "03_options_controls_640x480"
cli --port "$PORT" click --label Cancel >/dev/null
wait_for_current_widget "Controls"

cli --port "$PORT" click --label Display >/dev/null
wait_for_current_widget "Cancel"
snap "04_options_display_640x480"
cli --port "$PORT" click --label Cancel >/dev/null
wait_for_current_widget "Controls"

cli --port "$PORT" click --label Audio >/dev/null
wait_for_current_widget "Cancel"
snap "05_options_audio_640x480"
cli --port "$PORT" click --label Cancel >/dev/null
wait_for_current_widget "Controls"

go_back
wait_for_current_widget "Connect To Lobby"

# Lobby connect (no live server needed — the connect-attempt UI is what we capture)
if cli --port "$PORT" click --label "Connect To Lobby" 2>/dev/null >/dev/null \
   || cli --port "$PORT" click --label Online 2>/dev/null >/dev/null \
   || cli --port "$PORT" click --label LOBBY 2>/dev/null >/dev/null; then
  cli --port "$PORT" wait_ms --n 1500 >/dev/null
  snap "06_lobby_connect_640x480"
  go_back
  wait_for_current_widget "Connect To Lobby"
else
  echo "  skipped lobby connect (no matching label)"
fi

# --- Phase 2: 1280x720 reflow ---
resize 1280 720
snap "11_mainmenu_1280x720"

cli --port "$PORT" click --label Options >/dev/null
wait_for_current_widget "Controls"
snap "12_options_root_1280x720"

cli --port "$PORT" click --label Controls >/dev/null
wait_for_current_widget "Cancel"
snap "13_options_controls_1280x720"
cli --port "$PORT" click --label Cancel >/dev/null
wait_for_current_widget "Controls"

go_back
wait_for_current_widget "Connect To Lobby"

resize 640 480
stop_client

# --- Phase 3: in-game via Tutorial ---
capture_ingame "20_ingame_hud_640x480" clear
capture_ingame "21_ingame_playerlist_640x480" playerlist
capture_ingame "22_ingame_buy_640x480" buy
capture_ingame "23_ingame_tech_640x480" tech
capture_ingame "24_ingame_chat_640x480" chat
capture_ingame "25_ingame_hud_1280x720" clear 1280 720

echo "DONE: $(ls "$OUT_DIR" | wc -l | tr -d ' ') screenshots in $OUT_DIR"
