#!/usr/bin/env bash
set -euo pipefail
. "$(dirname "$0")/lib.sh"

PORT=$(pick_port)
PID=$(start_silencer "$PORT")
trap "stop_silencer $PID $PORT" EXIT
wait_alive "$PORT"

# MAINMENU -> OPTIONS
cli --port "$PORT" wait_for_ui --id main_menu.options --timeout-ms 15000
cli --port "$PORT" click --label OPTIONS
cli --port "$PORT" wait_for_ui --id options.controls --timeout-ms 5000
cli --port "$PORT" click --label CONTROLS
cli --port "$PORT" wait_for_ui --id options_controls.preset --timeout-ms 5000
cli --port "$PORT" inspect | bun -e \
  'const t=await new Response(Bun.stdin.stream()).text();
   const r=JSON.parse(t);
   if(!r.widgets.some((w)=>w.source==="clay" && w.id==="options_controls.preset")) process.exit(1);'
cli --port "$PORT" click --label CANCEL
cli --port "$PORT" wait_for_ui --id options.controls --timeout-ms 5000
cli --port "$PORT" click --label DISPLAY
cli --port "$PORT" wait_for_ui --id options_display.fullscreen --timeout-ms 5000
cli --port "$PORT" inspect | bun -e \
  'const t=await new Response(Bun.stdin.stream()).text();
   const r=JSON.parse(t);
   if(!r.widgets.some((w)=>w.source==="clay" && w.label==="Smooth Scaling")) process.exit(1);'
cli --port "$PORT" click --label CANCEL
cli --port "$PORT" wait_for_ui --id options.controls --timeout-ms 5000
cli --port "$PORT" click --label AUDIO
cli --port "$PORT" wait_for_ui --id options_audio.music --timeout-ms 5000
cli --port "$PORT" inspect | bun -e \
  'const t=await new Response(Bun.stdin.stream()).text();
   const r=JSON.parse(t);
   if(!r.widgets.some((w)=>w.source==="clay" && w.label==="Music")) process.exit(1);'
cli --port "$PORT" click --label CANCEL
cli --port "$PORT" wait_for_ui --id options.controls --timeout-ms 5000
# OPTIONS -> back -> MAINMENU
cli --port "$PORT" back
cli --port "$PORT" wait_for_ui --id main_menu.options --timeout-ms 5000
# Gameplay back -> fade clear -> MAINMENU. This covers the post-fade
# frontend entry path used by session exits after the legacy state-screen map
# and interim route enum were removed.
cli --port "$PORT" click --label Tutorial
cli --port "$PORT" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 15000
cli --port "$PORT" back
cli --port "$PORT" wait_for_ui --id main_menu.options --timeout-ms 15000
echo "PASS 10_navigate"
