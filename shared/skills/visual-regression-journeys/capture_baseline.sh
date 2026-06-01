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
# those journeys are marked failed by their individual capture without stopping
# the rest of the baseline run.

set +e

if [ -z "${WORKTREE:-}" ] || [ -z "${OUT_DIR:-}" ]; then
  echo "WORKTREE and OUT_DIR must both be set" >&2
  exit 1
fi
mkdir -p "$OUT_DIR"

VISUAL_HOME="$(mktemp -d "${TMPDIR:-/tmp}/silencer-visual-baseline-home.XXXXXX")"
mkdir -p "$VISUAL_HOME/Library/Application Support/Silencer" \
  "$VISUAL_HOME/.config/silencer"
cat > "$VISUAL_HOME/Library/Application Support/Silencer/config.cfg" <<EOF
lobbyhost=127.0.0.1
lobbyport=9
EOF
cp "$VISUAL_HOME/Library/Application Support/Silencer/config.cfg" \
  "$VISUAL_HOME/.config/silencer/config.cfg"

for candidate in \
  "$WORKTREE/clients/silencer/build/Silencer.app/Contents/MacOS/Silencer" \
  "$WORKTREE/clients/silencer/build/silencer" \
  "$WORKTREE/build/Silencer.app/Contents/MacOS/Silencer" \
  "$WORKTREE/build/silencer"; do
  if [ -x "$candidate" ]; then
    export SILENCER_BIN="$candidate"
    break
  fi
done
if [ ! -x "${SILENCER_BIN:-}" ]; then
  echo "no silencer binary in $WORKTREE/clients/silencer/build/ or $WORKTREE/build/" >&2
  exit 1
fi

# Each capture is its own fresh silencer run with a hard timeout.
# Args: <name> <body — multiline string of cli commands>
run_capture() {
  local name="$1"
  local body="$2"
  HOME="$VISUAL_HOME" bash -c "
    set +e
    source $WORKTREE/tests/cli-agent/e2e/lib.sh
    PORT=\$(pick_port)
    PID=\$(start_silencer \"\$PORT\")
    trap 'stop_silencer \"\$PID\" \"\$PORT\"' EXIT
    wait_alive \"\$PORT\"
    cli --port \"\$PORT\" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
    $body
    cli --port \"\$PORT\" wait_ms --n 1500 >/dev/null 2>&1 || cli --port \"\$PORT\" wait_frames --n 30 >/dev/null 2>&1
    cli --port \"\$PORT\" screenshot --out \"$OUT_DIR/${name}.png\" >/dev/null 2>&1
  " >/dev/null 2>&1 &
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

run_capture "01_mainmenu_640x480" ""

run_capture "02_options_root_640x480" "
cli --port \"\$PORT\" click --label OPTIONS
cli --port \"\$PORT\" wait_for_state --state OPTIONS --timeout-ms 5000 >/dev/null
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
"

run_capture "03_options_controls_640x480" "
cli --port \"\$PORT\" click --label OPTIONS
cli --port \"\$PORT\" wait_for_state --state OPTIONS --timeout-ms 5000 >/dev/null
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
cli --port \"\$PORT\" click --label Controls
cli --port \"\$PORT\" wait_for_state --state OPTIONSCONTROLS --timeout-ms 5000 >/dev/null
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
"

run_capture "04_options_display_640x480" "
cli --port \"\$PORT\" click --label OPTIONS
cli --port \"\$PORT\" wait_for_state --state OPTIONS --timeout-ms 5000 >/dev/null
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
cli --port \"\$PORT\" click --label Display
cli --port \"\$PORT\" wait_for_state --state OPTIONSDISPLAY --timeout-ms 5000 >/dev/null
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
"

run_capture "05_options_audio_640x480" "
cli --port \"\$PORT\" click --label OPTIONS
cli --port \"\$PORT\" wait_for_state --state OPTIONS --timeout-ms 5000 >/dev/null
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
cli --port \"\$PORT\" click --label Audio
cli --port \"\$PORT\" wait_for_state --state OPTIONSAUDIO --timeout-ms 5000 >/dev/null
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
"

run_capture "06_lobby_connect_640x480" "
cli --port \"\$PORT\" click --label 'Connect To Lobby' 2>/dev/null || \\
  cli --port \"\$PORT\" click --label LOBBY 2>/dev/null || \\
  cli --port \"\$PORT\" click --label Online 2>/dev/null
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
"

run_capture "11_mainmenu_1280x720" "
cli --port \"\$PORT\" resize --w 1280 --h 720 >/dev/null
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
"

run_capture "12_options_root_1280x720" "
cli --port \"\$PORT\" resize --w 1280 --h 720 >/dev/null
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
cli --port \"\$PORT\" click --label OPTIONS
cli --port \"\$PORT\" wait_for_state --state OPTIONS --timeout-ms 5000 >/dev/null
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
"

run_capture "13_options_controls_1280x720" "
cli --port \"\$PORT\" resize --w 1280 --h 720 >/dev/null
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
cli --port \"\$PORT\" click --label OPTIONS
cli --port \"\$PORT\" wait_for_state --state OPTIONS --timeout-ms 5000 >/dev/null
cli --port \"\$PORT\" wait_ms --n 2000 >/dev/null
cli --port \"\$PORT\" click --label Controls
cli --port \"\$PORT\" wait_for_state --state OPTIONSCONTROLS --timeout-ms 5000 >/dev/null
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
  if cli --port \"\$PORT\" world_state 2>/dev/null | bun -e 'const t=await new Response(Bun.stdin.stream()).text();const r=JSON.parse(t);const s=r.result??r;if((s.objects_count??0)>0&&(s.players?.length??0)>0)process.exit(0);process.exit(1);' 2>/dev/null; then
    break
  fi
  cli --port \"\$PORT\" wait_frames --n 2 >/dev/null
done
cli --port \"\$PORT\" ingame_ui_mode --mode playerlist >/dev/null
cli --port \"\$PORT\" wait_ms --n 1500 >/dev/null
"

run_capture "22_ingame_buy_640x480" "
cli --port \"\$PORT\" click --label Tutorial
cli --port \"\$PORT\" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 20000 >/dev/null
for i in \$(seq 1 60); do
  if cli --port \"\$PORT\" world_state 2>/dev/null | bun -e 'const t=await new Response(Bun.stdin.stream()).text();const r=JSON.parse(t);const s=r.result??r;if((s.objects_count??0)>0&&(s.players?.length??0)>0)process.exit(0);process.exit(1);' 2>/dev/null; then
    break
  fi
  cli --port \"\$PORT\" wait_frames --n 2 >/dev/null
done
cli --port \"\$PORT\" ingame_ui_mode --mode buy >/dev/null
cli --port \"\$PORT\" wait_ms --n 1500 >/dev/null
"

run_capture "23_ingame_tech_640x480" "
cli --port \"\$PORT\" click --label Tutorial
cli --port \"\$PORT\" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 20000 >/dev/null
for i in \$(seq 1 60); do
  if cli --port \"\$PORT\" world_state 2>/dev/null | bun -e 'const t=await new Response(Bun.stdin.stream()).text();const r=JSON.parse(t);const s=r.result??r;if((s.objects_count??0)>0&&(s.players?.length??0)>0)process.exit(0);process.exit(1);' 2>/dev/null; then
    break
  fi
  cli --port \"\$PORT\" wait_frames --n 2 >/dev/null
done
cli --port \"\$PORT\" ingame_ui_mode --mode tech >/dev/null
cli --port \"\$PORT\" wait_ms --n 1500 >/dev/null
"

run_capture "24_ingame_chat_640x480" "
cli --port \"\$PORT\" click --label Tutorial
cli --port \"\$PORT\" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 20000 >/dev/null
for i in \$(seq 1 60); do
  if cli --port \"\$PORT\" world_state 2>/dev/null | bun -e 'const t=await new Response(Bun.stdin.stream()).text();const r=JSON.parse(t);const s=r.result??r;if((s.objects_count??0)>0&&(s.players?.length??0)>0)process.exit(0);process.exit(1);' 2>/dev/null; then
    break
  fi
  cli --port \"\$PORT\" wait_frames --n 2 >/dev/null
done
cli --port \"\$PORT\" ingame_ui_mode --mode chat >/dev/null
cli --port \"\$PORT\" wait_ms --n 1500 >/dev/null
"

run_capture "25_ingame_hud_1280x720" "
cli --port \"\$PORT\" click --label Tutorial
cli --port \"\$PORT\" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 20000 >/dev/null
for i in \$(seq 1 60); do
  if cli --port \"\$PORT\" world_state 2>/dev/null | bun -e 'const t=await new Response(Bun.stdin.stream()).text();const r=JSON.parse(t);const s=r.result??r;if((s.objects_count??0)>0&&(s.players?.length??0)>0)process.exit(0);process.exit(1);' 2>/dev/null; then
    break
  fi
  cli --port \"\$PORT\" wait_frames --n 2 >/dev/null
done
cli --port \"\$PORT\" ingame_ui_mode --mode clear >/dev/null
cli --port \"\$PORT\" resize --w 1280 --h 720 >/dev/null
cli --port \"\$PORT\" wait_ms --n 1500 >/dev/null
"

rm -rf "$VISUAL_HOME"
echo "DONE: $(ls "$OUT_DIR" 2>/dev/null | wc -l | tr -d ' ') captured in $OUT_DIR"
