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

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"

snap() {
  local name="$1"
  cli --port "$PORT" wait_ms --n 1500 >/dev/null
  cli --port "$PORT" screenshot --out "$OUT_DIR/${name}.png" >/dev/null
  echo "  captured $name"
}

snap_now() {
  local name="$1"
  cli --port "$PORT" screenshot --out "$OUT_DIR/${name}.png" >/dev/null
  echo "  captured $name"
}

snap_port() {
  local port="$1"
  local name="$2"
  cli --port "$port" wait_ms --n 1500 >/dev/null
  cli --port "$port" screenshot --out "$OUT_DIR/${name}.png" >/dev/null
  echo "  captured $name"
}

resize() {
  cli --port "$PORT" resize --w "$1" --h "$2" >/dev/null
  cli --port "$PORT" wait_ms --n 1500 >/dev/null
}

go_back() {
  cli --port "$PORT" back >/dev/null
  cli --port "$PORT" wait_ms --n 1500 >/dev/null
}

wait_for_widget_port() {
  local port="$1"
  local label="$2"
  for _ in $(seq 1 100); do
    if cli --port "$port" inspect | LABEL="$label" bun -e '
      const text = await new Response(Bun.stdin.stream()).text();
      const response = JSON.parse(text);
      const label = process.env.LABEL;
      process.exit((response.widgets ?? []).some((w) => w.label === label) ? 0 : 1);
    ' >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.05
  done
  echo "widget '$label' never appeared" >&2
  cli --port "$port" inspect >&2 || true
  return 1
}

wait_for_lobby_state_port() {
  local port="$1"
  local target="$2"
  local state=""
  for _ in $(seq 1 100); do
    state="$(cli --port "$port" state | bun -e '
      const text = await new Response(Bun.stdin.stream()).text();
      console.log(JSON.parse(text).lobby_state || "");
    ')"
    if [ "$state" = "$target" ]; then
      return 0
    fi
    sleep 0.1
  done
  echo "lobby_state never became $target (last=$state)" >&2
  return 1
}

wait_character_create_port() {
  local port="$1"
  cli --port "$port" wait_for_ui --id-prefix character_create. --timeout-ms 15000 >/dev/null
}

wait_lobby_screen_port() {
  local port="$1"
  cli --port "$port" wait_for_ui --id lobby.go_back --timeout-ms 15000 >/dev/null
}

capture_live_lobby() {
  local lobby_bin tmp lobby_log lobby_db silencer_home lobby_port player_auth_port map_api_port ctrl_port
  lobby_bin="$(lobby_bin)"
  tmp="$(mktemp -d)"
  lobby_log="$tmp/lobby.log"
  lobby_db="$tmp/lobby.json"
  silencer_home="$tmp/home"
  mkdir -p "$silencer_home/Library/Application Support/Silencer" "$tmp/maps"

  lobby_port="$(pick_port)"
  player_auth_port="$(pick_port)"
  map_api_port="$(pick_port)"
  ctrl_port="$(pick_port)"

  cat > "$silencer_home/Library/Application Support/Silencer/config.cfg" <<EOF
mapapiurl=http://127.0.0.1:$map_api_port
EOF

  local silencer_version
  silencer_version="$(silencer_version)"

  "$lobby_bin" \
    -addr ":$lobby_port" \
    -db "$lobby_db" \
    -version "$silencer_version" \
    -game-binary "$SILENCER_BIN" \
    -maps-dir "$tmp/maps" \
    -player-auth-addr ":$player_auth_port" \
    -map-api-addr ":$map_api_port" \
    >"$lobby_log" 2>&1 &
  local lobby_pid=$!

  cleanup_live_lobby() {
    if [ -n "${silencer_pid:-}" ]; then
      stop_silencer "$silencer_pid" "$ctrl_port" || true
    fi
    kill "$lobby_pid" 2>/dev/null || true
    wait "$lobby_pid" 2>/dev/null || true
    rm -rf "$tmp"
  }
  trap cleanup_live_lobby EXIT

  for i in $(seq 1 60); do
    if (echo > "/dev/tcp/127.0.0.1/$lobby_port") 2>/dev/null; then
      break
    fi
    sleep 0.25
    if [ "$i" = 60 ]; then
      echo "lobby on :$lobby_port never came up" >&2
      cat "$lobby_log" >&2
      exit 1
    fi
  done

  HOME="$silencer_home" "$SILENCER_BIN" \
    --headless \
    --control-port "$ctrl_port" \
    --lobby-host 127.0.0.1 \
    --lobby-port "$lobby_port" \
    >"/tmp/silencer-e2e-$ctrl_port.log" 2>&1 &
  local silencer_pid=$!
  wait_alive "$ctrl_port"

  cli --port "$ctrl_port" wait_for_ui --id main_menu.options --timeout-ms 15000 >/dev/null
  cli --port "$ctrl_port" resize --w 640 --h 480 >/dev/null
  wait_for_widget_port "$ctrl_port" "Connect To Lobby"
  cli --port "$ctrl_port" click --label "Connect To Lobby" >/dev/null
  cli --port "$ctrl_port" wait_for_ui --id lobby_connect.login --timeout-ms 5000 >/dev/null
  wait_for_widget_port "$ctrl_port" "Login/Create"
  snap_port "$ctrl_port" "30_lobby_login_640x480"

  cli --port "$ctrl_port" set_text --uid 1 --text "visualcurrent" >/dev/null
  cli --port "$ctrl_port" set_text --uid 2 --text "secret" >/dev/null
  wait_for_lobby_state_port "$ctrl_port" AUTHENTICATING
  cli --port "$ctrl_port" click --label "Login/Create" >/dev/null
  wait_character_create_port "$ctrl_port"
  wait_for_widget_port "$ctrl_port" "Create New Character"
  snap_port "$ctrl_port" "31_character_create_640x480"

  cli --port "$ctrl_port" click --label "Create New Character" >/dev/null
  wait_for_widget_port "$ctrl_port" "Alias"
  snap_port "$ctrl_port" "32_character_alias_modal_640x480"
  cli --port "$ctrl_port" set_text --label "Alias" --text "VisualCurrent" >/dev/null
  cli --port "$ctrl_port" key --key enter >/dev/null
  wait_character_create_port "$ctrl_port"
  wait_for_widget_port "$ctrl_port" "Noxis"
  cli --port "$ctrl_port" click --label "Noxis" >/dev/null
  wait_lobby_screen_port "$ctrl_port"
  wait_for_widget_port "$ctrl_port" "Create Game"
  cli --port "$ctrl_port" step --frames 5 >/dev/null
  snap_port "$ctrl_port" "33_lobby_game_select_640x480"

  cli --port "$ctrl_port" set_text --label "Chat" --text "visual lobby hello" >/dev/null || true
  cli --port "$ctrl_port" click --label "Create Game" >/dev/null
  cli --port "$ctrl_port" step --frames 5 >/dev/null
  wait_for_widget_port "$ctrl_port" "Game name"
  snap_port "$ctrl_port" "34_lobby_game_create_640x480"

  cli --port "$ctrl_port" click --label "Create" >/dev/null
  cli --port "$ctrl_port" step --frames 5 >/dev/null
  wait_for_widget_port "$ctrl_port" OK
  snap_port "$ctrl_port" "35_lobby_create_message_modal_640x480"
  cli --port "$ctrl_port" click --label OK >/dev/null
  cli --port "$ctrl_port" step --frames 5 >/dev/null

  cli --port "$ctrl_port" resize --w 1280 --h 720 >/dev/null
  cli --port "$ctrl_port" wait_frames --n 3 >/dev/null
  snap_port "$ctrl_port" "36_lobby_game_create_1280x720"

  cleanup_live_lobby
  trap - EXIT
}

# --- Phase 1: menu surfaces at 640x480 ---
cli --port "$PORT" wait_for_ui --id main_menu.options --timeout-ms 15000 >/dev/null
resize 640 480
snap "01_mainmenu_640x480"

cli --port "$PORT" click --label OPTIONS >/dev/null
cli --port "$PORT" wait_for_ui --id options.controls --timeout-ms 5000 >/dev/null
snap "02_options_root_640x480"

cli --port "$PORT" click --label CONTROLS >/dev/null
cli --port "$PORT" wait_for_ui --id options_controls.preset --timeout-ms 5000 >/dev/null
snap "03_options_controls_640x480"
cli --port "$PORT" click --label CANCEL >/dev/null
cli --port "$PORT" wait_for_ui --id options.controls --timeout-ms 5000 >/dev/null

cli --port "$PORT" click --label DISPLAY >/dev/null
cli --port "$PORT" wait_for_ui --id options_display.fullscreen --timeout-ms 5000 >/dev/null
snap "04_options_display_640x480"
cli --port "$PORT" click --label CANCEL >/dev/null
cli --port "$PORT" wait_for_ui --id options.controls --timeout-ms 5000 >/dev/null

cli --port "$PORT" click --label AUDIO >/dev/null
cli --port "$PORT" wait_for_ui --id options_audio.music --timeout-ms 5000 >/dev/null
snap "05_options_audio_640x480"
cli --port "$PORT" click --label CANCEL >/dev/null
cli --port "$PORT" wait_for_ui --id options.controls --timeout-ms 5000 >/dev/null

go_back
cli --port "$PORT" wait_for_ui --id main_menu.options --timeout-ms 5000 >/dev/null

# Lobby connect (no live server needed — the connect-attempt UI is what we capture)
if cli --port "$PORT" click --label "Connect To Lobby" 2>/dev/null >/dev/null \
   || cli --port "$PORT" click --label Online 2>/dev/null >/dev/null \
   || cli --port "$PORT" click --label LOBBY 2>/dev/null >/dev/null; then
  cli --port "$PORT" wait_ms --n 1500 >/dev/null
  snap "06_lobby_connect_640x480"
  go_back
  cli --port "$PORT" wait_for_ui --id main_menu.options --timeout-ms 5000 >/dev/null || true
else
  echo "  skipped lobby connect (no matching label)"
fi

cli --port "$PORT" show_password_modal >/dev/null
cli --port "$PORT" wait_ms --n 500 >/dev/null
snap "07_password_modal_640x480"
cli --port "$PORT" click --label OK >/dev/null
cli --port "$PORT" wait_for_ui --id main_menu.options --timeout-ms 5000 >/dev/null

# --- Phase 2: 1280x720 reflow ---
resize 1280 720
snap "11_mainmenu_1280x720"

cli --port "$PORT" click --label OPTIONS >/dev/null
cli --port "$PORT" wait_for_ui --id options.controls --timeout-ms 5000 >/dev/null
snap "12_options_root_1280x720"

cli --port "$PORT" click --label CONTROLS >/dev/null
cli --port "$PORT" wait_for_ui --id options_controls.preset --timeout-ms 5000 >/dev/null
snap "13_options_controls_1280x720"
cli --port "$PORT" click --label CANCEL >/dev/null
cli --port "$PORT" wait_for_ui --id options.controls --timeout-ms 5000 >/dev/null

go_back
cli --port "$PORT" wait_for_ui --id main_menu.options --timeout-ms 5000 >/dev/null

stop_silencer "$PID" "$PORT"
trap - EXIT

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT
wait_alive "$PORT"
cli --port "$PORT" wait_for_ui --id main_menu.options --timeout-ms 15000 >/dev/null
resize 640 480

# --- Phase 3: in-game via Tutorial ---
cli --port "$PORT" click --label Tutorial >/dev/null
cli --port "$PORT" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 20000 >/dev/null

for _ in $(seq 1 60); do
  if cli --port "$PORT" world_state 2>/dev/null | bun -e '
    const text = await new Response(Bun.stdin.stream()).text();
    const response = JSON.parse(text);
    const state = response.result ?? response;
    if ((state.objects_count ?? 0) > 0 && (state.players?.length ?? 0) > 0) process.exit(0);
    process.exit(1);
  ' 2>/dev/null; then
    break
  fi
  cli --port "$PORT" wait_ms --n 500 >/dev/null
done

# In-game overlays. Note: tutorial mode may not surface buy/tech because those
# need world stations the tutorial doesn't have. Caller should eyeball the
# images and treat 21/22/23 as best-effort.
cli --port "$PORT" ingame_ui_mode --mode clear >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
snap "20_ingame_hud_640x480"

stop_silencer "$PID" "$PORT"
trap - EXIT

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT
wait_alive "$PORT"
cli --port "$PORT" wait_for_ui --id main_menu.options --timeout-ms 15000 >/dev/null
resize 640 480
cli --port "$PORT" click --label Tutorial >/dev/null
cli --port "$PORT" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 20000 >/dev/null
for _ in $(seq 1 60); do
  if cli --port "$PORT" world_state 2>/dev/null | bun -e '
    const text = await new Response(Bun.stdin.stream()).text();
    const response = JSON.parse(text);
    const state = response.result ?? response;
    if ((state.objects_count ?? 0) > 0 && (state.players?.length ?? 0) > 0) process.exit(0);
    process.exit(1);
  ' 2>/dev/null; then
    break
  fi
  cli --port "$PORT" wait_ms --n 500 >/dev/null
done

cli --port "$PORT" ingame_ui_mode --mode playerlist >/dev/null
snap_now "21_ingame_playerlist_640x480"

cli --port "$PORT" ingame_ui_mode --mode clear >/dev/null
cli --port "$PORT" wait_frames --n 1 >/dev/null
cli --port "$PORT" ingame_ui_mode --mode buy >/dev/null
snap_now "22_ingame_buy_640x480"

cli --port "$PORT" ingame_ui_mode --mode clear >/dev/null
cli --port "$PORT" wait_frames --n 1 >/dev/null
cli --port "$PORT" ingame_ui_mode --mode chat >/dev/null
snap_now "24_ingame_chat_640x480"

cli --port "$PORT" ingame_ui_mode --mode clear >/dev/null
cli --port "$PORT" wait_frames --n 1 >/dev/null
cli --port "$PORT" ingame_ui_mode --mode tech >/dev/null
snap_now "23_ingame_tech_640x480"

# 1280x720 in-game reflow
stop_silencer "$PID" "$PORT"
trap - EXIT

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT
wait_alive "$PORT"
cli --port "$PORT" wait_for_ui --id main_menu.options --timeout-ms 15000 >/dev/null
resize 1280 720
cli --port "$PORT" click --label Tutorial >/dev/null
cli --port "$PORT" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 20000 >/dev/null
for _ in $(seq 1 60); do
  if cli --port "$PORT" world_state 2>/dev/null | bun -e '
    const text = await new Response(Bun.stdin.stream()).text();
    const response = JSON.parse(text);
    const state = response.result ?? response;
    if ((state.objects_count ?? 0) > 0 && (state.players?.length ?? 0) > 0) process.exit(0);
    process.exit(1);
  ' 2>/dev/null; then
    break
  fi
  cli --port "$PORT" wait_frames --n 2 >/dev/null
done
snap "25_ingame_hud_1280x720"

stop_silencer "$PID" "$PORT"
trap - EXIT

# --- Phase 4: authenticated lobby/menu flow against a local lobby ---
capture_live_lobby

echo "DONE: $(ls "$OUT_DIR" | wc -l | tr -d ' ') screenshots in $OUT_DIR"
