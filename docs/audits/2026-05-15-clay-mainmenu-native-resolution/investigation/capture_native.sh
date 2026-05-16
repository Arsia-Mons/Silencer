#!/usr/bin/env bash
# Investigation capture for 2026-05-15-clay-mainmenu-native-resolution.
# Drives the NON-UNITY release binary headless at the desktop's native
# 2560x1440 and screenshots every reachable screen. No baseline compare —
# the question is only "which screens degrade at native res on the branch".
set -uo pipefail

REPO_ROOT="C:/Users/Space Command/repos/zSilencer"
cd "$REPO_ROOT"

# Honor the build constraint: non-unity release build, not build/ (Debug) or build-unity (broken).
export SILENCER_BIN="$REPO_ROOT/clients/silencer/build-release/Silencer.exe"

OUT_DIR="$REPO_ROOT/docs/audits/2026-05-15-clay-mainmenu-native-resolution/investigation"
mkdir -p "$OUT_DIR"

source tests/cli-agent/e2e/lib.sh

W=2560
H=1440
SETTLE=3500   # fade-in settle; the audit warns 1500 yields a mid-fade frame

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT
wait_alive "$PORT"

snap() {
  local name="$1"
  cli --port "$PORT" wait_ms --n "$SETTLE" >/dev/null 2>&1 || true
  if cli --port "$PORT" screenshot --out "$OUT_DIR/${name}.png" >/dev/null 2>&1; then
    echo "  captured $name"
  else
    echo "  FAILED screenshot $name"
  fi
}

resize_native() {
  cli --port "$PORT" resize --w "$W" --h "$H" >/dev/null 2>&1 || echo "  resize failed"
  cli --port "$PORT" wait_ms --n 1200 >/dev/null 2>&1 || true
}

echo "== native capture ${W}x${H}, binary=$SILENCER_BIN =="
cli --port "$PORT" ping || { echo "no ping"; exit 1; }

cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null 2>&1 || true
resize_native
snap "10_mainmenu_${W}x${H}"

# Options root + 3 sub-tabs
( cli --port "$PORT" click --label OPTIONS >/dev/null 2>&1 \
  && cli --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000 >/dev/null 2>&1 \
  && snap "11_options_root_${W}x${H}" ) || echo "  skipped options root"

( cli --port "$PORT" click --label CONTROLS >/dev/null 2>&1 \
  && cli --port "$PORT" wait_for_state --state OPTIONSCONTROLS --timeout-ms 5000 >/dev/null 2>&1 \
  && snap "12_options_controls_${W}x${H}" \
  && cli --port "$PORT" click --label CANCEL >/dev/null 2>&1 \
  && cli --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000 >/dev/null 2>&1 ) || echo "  skipped options controls"

( cli --port "$PORT" click --label DISPLAY >/dev/null 2>&1 \
  && cli --port "$PORT" wait_for_state --state OPTIONSDISPLAY --timeout-ms 5000 >/dev/null 2>&1 \
  && snap "13_options_display_${W}x${H}" \
  && cli --port "$PORT" click --label CANCEL >/dev/null 2>&1 \
  && cli --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000 >/dev/null 2>&1 ) || echo "  skipped options display"

( cli --port "$PORT" click --label AUDIO >/dev/null 2>&1 \
  && cli --port "$PORT" wait_for_state --state OPTIONSAUDIO --timeout-ms 5000 >/dev/null 2>&1 \
  && snap "14_options_audio_${W}x${H}" \
  && cli --port "$PORT" click --label CANCEL >/dev/null 2>&1 \
  && cli --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000 >/dev/null 2>&1 ) || echo "  skipped options audio"

cli --port "$PORT" back >/dev/null 2>&1 || true
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 5000 >/dev/null 2>&1 || true

# Lobby connect (no live server; the connect-attempt UI is what we capture)
( ( cli --port "$PORT" click --label "Connect To Lobby" >/dev/null 2>&1 \
    || cli --port "$PORT" click --label Online >/dev/null 2>&1 \
    || cli --port "$PORT" click --label LOBBY >/dev/null 2>&1 ) \
  && cli --port "$PORT" wait_ms --n 1500 >/dev/null 2>&1 \
  && snap "15_lobby_connect_${W}x${H}" \
  && cli --port "$PORT" back >/dev/null 2>&1 \
  && cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 5000 >/dev/null 2>&1 ) || echo "  skipped lobby connect"

# In-game via Tutorial
( cli --port "$PORT" click --label Tutorial >/dev/null 2>&1 \
  && cli --port "$PORT" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 25000 >/dev/null 2>&1 ) || echo "  tutorial entry failed"

for _ in $(seq 1 60); do
  if cli --port "$PORT" world_state 2>/dev/null | bun -e '
    const t = await new Response(Bun.stdin.stream()).text();
    const r = JSON.parse(t); const s = r.result ?? r;
    if ((s.objects_count ?? 0) > 0 && (s.players?.length ?? 0) > 0) process.exit(0);
    process.exit(1);' 2>/dev/null; then
    break
  fi
  cli --port "$PORT" wait_ms --n 500 >/dev/null 2>&1 || true
done

cli --port "$PORT" ingame_ui_mode --mode clear >/dev/null 2>&1 || true
cli --port "$PORT" wait_frames --n 3 >/dev/null 2>&1 || true
snap "16_ingame_hud_${W}x${H}"

for mode in playerlist buy tech chat; do
  ( cli --port "$PORT" ingame_ui_mode --mode "$mode" >/dev/null 2>&1 \
    && cli --port "$PORT" wait_frames --n 3 >/dev/null 2>&1 \
    && snap "17_ingame_${mode}_${W}x${H}" ) || echo "  skipped ingame $mode"
done

echo "DONE: $(ls "$OUT_DIR"/*.png 2>/dev/null | wc -l | tr -d ' ') screenshots in investigation/"
