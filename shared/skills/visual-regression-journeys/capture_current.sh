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

resize() {
  cli --port "$PORT" resize --w "$1" --h "$2" >/dev/null
  cli --port "$PORT" wait_ms --n 1500 >/dev/null
}

go_back() {
  cli --port "$PORT" back >/dev/null
  cli --port "$PORT" wait_ms --n 1500 >/dev/null
}

# --- Phase 1: menu surfaces at 640x480 ---
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
resize 640 480
snap "01_mainmenu_640x480"

cli --port "$PORT" click --label OPTIONS >/dev/null
cli --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000 >/dev/null
snap "02_options_root_640x480"

cli --port "$PORT" click --label CONTROLS >/dev/null
cli --port "$PORT" wait_for_state --state OPTIONSCONTROLS --timeout-ms 5000 >/dev/null
snap "03_options_controls_640x480"
cli --port "$PORT" click --label CANCEL >/dev/null
cli --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000 >/dev/null

cli --port "$PORT" click --label DISPLAY >/dev/null
cli --port "$PORT" wait_for_state --state OPTIONSDISPLAY --timeout-ms 5000 >/dev/null
snap "04_options_display_640x480"
cli --port "$PORT" click --label CANCEL >/dev/null
cli --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000 >/dev/null

cli --port "$PORT" click --label AUDIO >/dev/null
cli --port "$PORT" wait_for_state --state OPTIONSAUDIO --timeout-ms 5000 >/dev/null
snap "05_options_audio_640x480"
cli --port "$PORT" click --label CANCEL >/dev/null
cli --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000 >/dev/null

go_back
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 5000 >/dev/null

# Lobby connect (no live server needed — the connect-attempt UI is what we capture)
if cli --port "$PORT" click --label "Connect To Lobby" 2>/dev/null >/dev/null \
   || cli --port "$PORT" click --label Online 2>/dev/null >/dev/null \
   || cli --port "$PORT" click --label LOBBY 2>/dev/null >/dev/null; then
  cli --port "$PORT" wait_ms --n 1500 >/dev/null
  snap "06_lobby_connect_640x480"
  go_back
  cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 5000 >/dev/null || true
else
  echo "  skipped lobby connect (no matching label)"
fi

# --- Phase 2: 1280x720 reflow ---
resize 1280 720
snap "11_mainmenu_1280x720"

cli --port "$PORT" click --label OPTIONS >/dev/null
cli --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000 >/dev/null
snap "12_options_root_1280x720"

cli --port "$PORT" click --label CONTROLS >/dev/null
cli --port "$PORT" wait_for_state --state OPTIONSCONTROLS --timeout-ms 5000 >/dev/null
snap "13_options_controls_1280x720"
cli --port "$PORT" click --label CANCEL >/dev/null
cli --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000 >/dev/null

go_back
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 5000 >/dev/null

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

cli --port "$PORT" ingame_ui_mode --mode playerlist >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
snap "21_ingame_playerlist_640x480"

cli --port "$PORT" ingame_ui_mode --mode buy >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
snap "22_ingame_buy_640x480"

cli --port "$PORT" ingame_ui_mode --mode tech >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
snap "23_ingame_tech_640x480"

cli --port "$PORT" ingame_ui_mode --mode chat >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
snap "24_ingame_chat_640x480"

# 1280x720 in-game reflow
cli --port "$PORT" ingame_ui_mode --mode clear >/dev/null
resize 1280 720
cli --port "$PORT" wait_frames --n 3 >/dev/null
snap "25_ingame_hud_1280x720"

echo "DONE: $(ls "$OUT_DIR" | wc -l | tr -d ' ') screenshots in $OUT_DIR"
