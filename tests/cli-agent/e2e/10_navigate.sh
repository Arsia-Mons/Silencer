#!/usr/bin/env bash
set -euo pipefail
. "$(dirname "$0")/lib.sh"

PORT=$(pick_port)
PID=$(start_silencer "$PORT")
trap "stop_silencer $PID $PORT" EXIT
wait_alive "$PORT"

# Main menu -> Options
wait_for_widget "Connect To Lobby"
cli --port "$PORT" click --label OPTIONS
wait_for_widget "Controls"
cli --port "$PORT" click --label CONTROLS
wait_for_widget "Preset"
cli --port "$PORT" inspect | bun -e \
  'const t=await new Response(Bun.stdin.stream()).text();
   const r=JSON.parse(t);
   if(!r.widgets.some((w)=>w.source==="ui" && w.label==="Preset")) process.exit(1);'
cli --port "$PORT" click --label CANCEL
wait_for_widget "Controls"
cli --port "$PORT" click --label DISPLAY
wait_for_widget "Smooth Scaling"
cli --port "$PORT" inspect | bun -e \
  'const t=await new Response(Bun.stdin.stream()).text();
   const r=JSON.parse(t);
   if(!r.widgets.some((w)=>w.source==="ui" && w.label==="Smooth Scaling")) process.exit(1);'
cli --port "$PORT" click --label CANCEL
wait_for_widget "Controls"
cli --port "$PORT" click --label AUDIO
wait_for_widget "Music"
cli --port "$PORT" inspect | bun -e \
  'const t=await new Response(Bun.stdin.stream()).text();
   const r=JSON.parse(t);
   if(!r.widgets.some((w)=>w.source==="ui" && w.label==="Music")) process.exit(1);'
cli --port "$PORT" click --label CANCEL
wait_for_widget "Controls"
# Options -> back -> main menu
cli --port "$PORT" back
wait_for_widget "Connect To Lobby"
echo "PASS 10_navigate"
