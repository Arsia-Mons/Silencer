#!/usr/bin/env bash
# Captures the comparable subset of UI journeys from a BASELINE worktree.
# Restarts the binary per screen because legacy UI back-navigation is
# fragile via the CLI on older refs.
#
# Required env:
#   WORKTREE — absolute path to the baseline worktree
#   OUT_DIR  — where to write captures
#
# Labels assume legacy-era casing ("Controls", "Display", "Audio").
# Tolerates absent features — older refs lack resize, ingame_ui_mode, etc.;
# those journeys are skipped, not failed.

set +e

if [ -z "${WORKTREE:-}" ] || [ -z "${OUT_DIR:-}" ]; then
  echo "WORKTREE and OUT_DIR must both be set" >&2
  exit 1
fi
mkdir -p "$OUT_DIR"

if [ -z "${SILENCER_BIN:-}" ] || [ ! -x "$SILENCER_BIN" ]; then
  for candidate in \
    "$WORKTREE/clients/silencer/build/Silencer.app/Contents/MacOS/Silencer" \
    "$WORKTREE/clients/silencer/build/silencer" \
    "$WORKTREE/clients/silencer/build/Silencer.exe" \
    "$WORKTREE/build/Silencer.app/Contents/MacOS/Silencer" \
    "$WORKTREE/build/silencer" \
    "$WORKTREE/build/Silencer.exe"; do
    if [ -x "$candidate" ]; then
      export SILENCER_BIN="$candidate"
      break
    fi
  done
fi
if [ -z "${SILENCER_BIN:-}" ] || [ ! -x "$SILENCER_BIN" ]; then
  echo "no silencer binary in $WORKTREE build outputs" >&2
  exit 1
fi

# Each capture is its own fresh silencer run with a hard timeout.
# Args: <name> <body — multiline string of cli commands>
run_capture() {
  local name="$1"
  local body="$2"
  (
    set -euo pipefail
    source "$WORKTREE/tests/cli-agent/e2e/lib.sh"
    wait_state() {
      cli --port "$PORT" wait_for_state --state "$1" --timeout-ms "$2" >/dev/null
    }
    wait_main_menu() {
      cli --port "$PORT" wait_for_ui --id main_menu.options --timeout-ms 15000 >/dev/null 2>&1 ||
        wait_state MAINMENU 15000
    }
    wait_options_root() {
      cli --port "$PORT" wait_for_ui --id options.controls --timeout-ms 5000 >/dev/null 2>&1 ||
        wait_state OPTIONS 5000
    }
    wait_options_controls() {
      cli --port "$PORT" wait_for_ui --id options_controls.preset --timeout-ms 5000 >/dev/null 2>&1 ||
        wait_state OPTIONSCONTROLS 5000
    }
    wait_options_display() {
      cli --port "$PORT" wait_for_ui --id options_display.fullscreen --timeout-ms 5000 >/dev/null 2>&1 ||
        wait_state OPTIONSDISPLAY 5000
    }
    wait_options_audio() {
      cli --port "$PORT" wait_for_ui --id options_audio.music --timeout-ms 5000 >/dev/null 2>&1 ||
        wait_state OPTIONSAUDIO 5000
    }
    PORT="$(pick_port)"
    PID="$(start_silencer "$PORT")"
    trap 'stop_silencer "$PID" "$PORT"' EXIT
    wait_alive "$PORT"
    wait_main_menu
    eval "$body"
    cli --port "$PORT" wait_ms --n 1500 >/dev/null 2>&1 || cli --port "$PORT" wait_frames --n 30 >/dev/null 2>&1
    cli --port "$PORT" screenshot --out "$OUT_DIR/${name}.png" >/dev/null
  ) >/dev/null 2>&1 &
  local pid=$!
  local waited=0
  while kill -0 "$pid" 2>/dev/null && [ $waited -lt 90 ]; do
    sleep 1
    waited=$((waited+1))
  done
  if kill -0 "$pid" 2>/dev/null; then
    echo "  TIMEOUT $name"
    kill -9 $(pgrep -P $pid 2>/dev/null) 2>/dev/null
    kill -9 $pid 2>/dev/null
  fi
  pkill -9 -f "$SILENCER_BIN" 2>/dev/null
  sleep 1
  if [ -f "$OUT_DIR/${name}.png" ]; then
    echo "  captured $name"
  else
    echo "  FAILED $name"
  fi
}

run_live_lobby_capture() {
  (
    set -euo pipefail
    source "$WORKTREE/tests/cli-agent/e2e/lib.sh"

    wait_for_widget() {
      local label="$1"
      for _ in $(seq 1 100); do
        if cli --port "$CTRL_PORT" inspect | LABEL="$label" bun -e '
          const text = await new Response(Bun.stdin.stream()).text();
          const response = JSON.parse(text);
          const label = process.env.LABEL;
          process.exit((response.widgets ?? []).some((w) => w.label === label) ? 0 : 1);
        ' >/dev/null 2>&1; then
          return 0
        fi
        sleep 0.05
      done
      return 1
    }

    wait_lobby_state() {
      local target="$1"
      local state=""
      for _ in $(seq 1 100); do
        state="$(cli --port "$CTRL_PORT" state | bun -e '
          const text = await new Response(Bun.stdin.stream()).text();
          console.log(JSON.parse(text).lobby_state || "");
        ')"
        [ "$state" = "$target" ] && return 0
        sleep 0.1
      done
      return 1
    }

    wait_control_state() {
      cli --port "$CTRL_PORT" wait_for_state --state "$1" --timeout-ms "$2" >/dev/null
    }

    wait_character_create() {
      cli --port "$CTRL_PORT" wait_for_ui --id-prefix character_create. --timeout-ms 15000 >/dev/null 2>&1 ||
        wait_control_state CREATECHARACTER 15000
    }

    wait_main_menu() {
      cli --port "$CTRL_PORT" wait_for_ui --id main_menu.options --timeout-ms 15000 >/dev/null 2>&1 ||
        wait_control_state MAINMENU 15000
    }

    wait_lobby_connect() {
      cli --port "$CTRL_PORT" wait_for_ui --id lobby_connect.login --timeout-ms 5000 >/dev/null 2>&1 ||
        wait_control_state LOBBYCONNECT 5000
    }

    wait_lobby_screen() {
      cli --port "$CTRL_PORT" wait_for_ui --id lobby.go_back --timeout-ms 15000 >/dev/null 2>&1 ||
        wait_control_state LOBBY 15000
    }

    snap() {
      local name="$1"
      cli --port "$CTRL_PORT" wait_ms --n 1500 >/dev/null 2>&1 || cli --port "$CTRL_PORT" wait_frames --n 30 >/dev/null
      cli --port "$CTRL_PORT" screenshot --out "$OUT_DIR/${name}.png" >/dev/null
      echo "  captured $name"
    }

    LOBBY_BIN="$(lobby_bin)"
    TMP="$(mktemp -d)"
    LOBBY_LOG="$TMP/lobby.log"
    LOBBY_DB="$TMP/lobby.json"
    SILENCER_HOME="$TMP/home"
    mkdir -p "$SILENCER_HOME/Library/Application Support/Silencer" "$TMP/maps"

    LOBBY_PORT="$(pick_port)"
    PLAYER_AUTH_PORT="$(pick_port)"
    MAP_API_PORT="$(pick_port)"
    CTRL_PORT="$(pick_port)"

    cat > "$SILENCER_HOME/Library/Application Support/Silencer/config.cfg" <<EOF
mapapiurl=http://127.0.0.1:$MAP_API_PORT
EOF

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
      if (echo > "/dev/tcp/127.0.0.1/$LOBBY_PORT") 2>/dev/null; then
        break
      fi
      sleep 0.25
      [ "$i" = 60 ] && exit 1
    done

    HOME="$SILENCER_HOME" "$SILENCER_BIN" \
      --headless \
      --control-port "$CTRL_PORT" \
      --lobby-host 127.0.0.1 \
      --lobby-port "$LOBBY_PORT" \
      >"/tmp/silencer-e2e-$CTRL_PORT.log" 2>&1 &
    SILENCER_PID=$!
    wait_alive "$CTRL_PORT"

    wait_main_menu
    cli --port "$CTRL_PORT" resize --w 640 --h 480 >/dev/null
    wait_for_widget "Connect To Lobby"
    cli --port "$CTRL_PORT" click --label "Connect To Lobby" >/dev/null
    wait_lobby_connect
    wait_for_widget "Login/Create"
    snap "30_lobby_login_640x480"

    cli --port "$CTRL_PORT" set_text --uid 1 --text "visualbase" >/dev/null
    cli --port "$CTRL_PORT" set_text --uid 2 --text "secret" >/dev/null
    wait_lobby_state AUTHENTICATING
    cli --port "$CTRL_PORT" click --label "Login/Create" >/dev/null
    wait_character_create
    wait_for_widget "Create New Character"
    snap "31_character_create_640x480"

    cli --port "$CTRL_PORT" click --label "Create New Character" >/dev/null
    wait_for_widget "Alias"
    snap "32_character_alias_modal_640x480"
    cli --port "$CTRL_PORT" set_text --label "Alias" --text "VisualBase" >/dev/null
    cli --port "$CTRL_PORT" key --key enter >/dev/null
    wait_character_create
    wait_for_widget "Noxis"
    cli --port "$CTRL_PORT" click --label "Noxis" >/dev/null
    wait_lobby_screen
    wait_for_widget "Create Game"
    cli --port "$CTRL_PORT" step --frames 5 >/dev/null
    snap "33_lobby_game_select_640x480"

    cli --port "$CTRL_PORT" click --label "Create Game" >/dev/null
    cli --port "$CTRL_PORT" step --frames 5 >/dev/null
    wait_for_widget "Game name"
    snap "34_lobby_game_create_640x480"

    cli --port "$CTRL_PORT" click --label "Create" >/dev/null
    cli --port "$CTRL_PORT" step --frames 5 >/dev/null
    wait_for_widget OK
    snap "35_lobby_create_message_modal_640x480"
    cli --port "$CTRL_PORT" click --label OK >/dev/null
    cli --port "$CTRL_PORT" step --frames 5 >/dev/null

    cli --port "$CTRL_PORT" resize --w 1280 --h 720 >/dev/null
    cli --port "$CTRL_PORT" wait_frames --n 3 >/dev/null
    snap "36_lobby_game_create_1280x720"
  )
  local status=$?
  if [ "$status" -ne 0 ]; then
    echo "  FAILED live lobby flow"
  fi
}

run_capture "01_mainmenu_640x480" ""

run_capture "02_options_root_640x480" "
cli --port \"\$PORT\" click --label OPTIONS
wait_options_root
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
"

run_capture "03_options_controls_640x480" "
cli --port \"\$PORT\" click --label OPTIONS
wait_options_root
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
cli --port \"\$PORT\" click --label Controls
wait_options_controls
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
"

run_capture "04_options_display_640x480" "
cli --port \"\$PORT\" click --label OPTIONS
wait_options_root
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
cli --port \"\$PORT\" click --label Display
wait_options_display
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
"

run_capture "05_options_audio_640x480" "
cli --port \"\$PORT\" click --label OPTIONS
wait_options_root
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
cli --port \"\$PORT\" click --label Audio
wait_options_audio
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
"

run_capture "06_lobby_connect_640x480" "
cli --port \"\$PORT\" click --label 'Connect To Lobby' 2>/dev/null || \\
  cli --port \"\$PORT\" click --label LOBBY 2>/dev/null || \\
  cli --port \"\$PORT\" click --label Online 2>/dev/null
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
"

run_capture "07_password_modal_640x480" "
cli --port \"\$PORT\" show_password_modal
cli --port \"\$PORT\" wait_ms --n 500 >/dev/null
"

run_capture "11_mainmenu_1280x720" "
cli --port \"\$PORT\" resize --w 1280 --h 720
"

run_capture "12_options_root_1280x720" "
cli --port \"\$PORT\" resize --w 1280 --h 720
cli --port \"\$PORT\" click --label OPTIONS
wait_options_root
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
"

run_capture "13_options_controls_1280x720" "
cli --port \"\$PORT\" resize --w 1280 --h 720
cli --port \"\$PORT\" click --label OPTIONS
wait_options_root
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
cli --port \"\$PORT\" click --label Controls
wait_options_controls
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
"

run_capture "20_ingame_hud_640x480" "
cli --port \"\$PORT\" click --label Tutorial
cli --port \"\$PORT\" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 20000 >/dev/null
for i in \$(seq 1 60); do
  if cli --port \"\$PORT\" world_state 2>/dev/null | bun -e 'const t=await new Response(Bun.stdin.stream()).text();const r=JSON.parse(t);const s=r.result??r;if((s.objects_count??0)>0&&(s.players?.length??0)>0)process.exit(0);process.exit(1);' 2>/dev/null; then
    break
  fi
  cli --port \"\$PORT\" wait_frames --n 2 >/dev/null
done
cli --port \"\$PORT\" wait_ms --n 1500 >/dev/null
"

run_capture "21_ingame_playerlist_640x480" "
cli --port \"\$PORT\" click --label Tutorial
cli --port \"\$PORT\" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 20000 >/dev/null
for i in \$(seq 1 60); do
  if cli --port \"\$PORT\" world_state 2>/dev/null | bun -e 'const t=await new Response(Bun.stdin.stream()).text();const r=JSON.parse(t);const s=r.result??r;if((s.objects_count??0)>0&&(s.players?.length??0)>0)process.exit(0);process.exit(1);' 2>/dev/null; then break; fi
  cli --port \"\$PORT\" wait_frames --n 2 >/dev/null
done
cli --port \"\$PORT\" ingame_ui_mode --mode playerlist >/dev/null
"

run_capture "22_ingame_buy_640x480" "
cli --port \"\$PORT\" click --label Tutorial
cli --port \"\$PORT\" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 20000 >/dev/null
for i in \$(seq 1 60); do
  if cli --port \"\$PORT\" world_state 2>/dev/null | bun -e 'const t=await new Response(Bun.stdin.stream()).text();const r=JSON.parse(t);const s=r.result??r;if((s.objects_count??0)>0&&(s.players?.length??0)>0)process.exit(0);process.exit(1);' 2>/dev/null; then break; fi
  cli --port \"\$PORT\" wait_frames --n 2 >/dev/null
done
cli --port \"\$PORT\" ingame_ui_mode --mode buy >/dev/null
"

run_capture "23_ingame_tech_640x480" "
cli --port \"\$PORT\" click --label Tutorial
cli --port \"\$PORT\" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 20000 >/dev/null
for i in \$(seq 1 60); do
  if cli --port \"\$PORT\" world_state 2>/dev/null | bun -e 'const t=await new Response(Bun.stdin.stream()).text();const r=JSON.parse(t);const s=r.result??r;if((s.objects_count??0)>0&&(s.players?.length??0)>0)process.exit(0);process.exit(1);' 2>/dev/null; then break; fi
  cli --port \"\$PORT\" wait_frames --n 2 >/dev/null
done
cli --port \"\$PORT\" ingame_ui_mode --mode tech >/dev/null
"

run_capture "24_ingame_chat_640x480" "
cli --port \"\$PORT\" click --label Tutorial
cli --port \"\$PORT\" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 20000 >/dev/null
for i in \$(seq 1 60); do
  if cli --port \"\$PORT\" world_state 2>/dev/null | bun -e 'const t=await new Response(Bun.stdin.stream()).text();const r=JSON.parse(t);const s=r.result??r;if((s.objects_count??0)>0&&(s.players?.length??0)>0)process.exit(0);process.exit(1);' 2>/dev/null; then break; fi
  cli --port \"\$PORT\" wait_frames --n 2 >/dev/null
done
cli --port \"\$PORT\" ingame_ui_mode --mode chat >/dev/null
"

run_capture "25_ingame_hud_1280x720" "
cli --port \"\$PORT\" resize --w 1280 --h 720
cli --port \"\$PORT\" click --label Tutorial
cli --port \"\$PORT\" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 20000 >/dev/null
for i in \$(seq 1 60); do
  if cli --port \"\$PORT\" world_state 2>/dev/null | bun -e 'const t=await new Response(Bun.stdin.stream()).text();const r=JSON.parse(t);const s=r.result??r;if((s.objects_count??0)>0&&(s.players?.length??0)>0)process.exit(0);process.exit(1);' 2>/dev/null; then break; fi
  cli --port \"\$PORT\" wait_frames --n 2 >/dev/null
done
cli --port \"\$PORT\" wait_ms --n 1500 >/dev/null
"

run_live_lobby_capture

echo "DONE: $(ls "$OUT_DIR" 2>/dev/null | wc -l | tr -d ' ') captured in $OUT_DIR"
