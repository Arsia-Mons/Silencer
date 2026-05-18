#!/usr/bin/env bash
set -euo pipefail
. "$(dirname "$0")/lib.sh"

PORT=$(pick_port)
PID=$(start_silencer "$PORT")
trap "stop_silencer $PID $PORT" EXIT
wait_alive "$PORT"

# MAINMENU -> OPTIONS
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000
cli --port "$PORT" click --label OPTIONS
cli --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000
cli --port "$PORT" click --label CONTROLS
cli --port "$PORT" wait_for_state --state OPTIONSCONTROLS --timeout-ms 5000
cli --port "$PORT" inspect | bun -e \
  'const t=await new Response(Bun.stdin.stream()).text();
   const r=JSON.parse(t);
   if(!r.widgets.some((w)=>w.source==="clay" && w.label==="Preset")) process.exit(1);'
cli --port "$PORT" click --label CANCEL
cli --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000
cli --port "$PORT" click --label DISPLAY
cli --port "$PORT" wait_for_state --state OPTIONSDISPLAY --timeout-ms 5000
cli --port "$PORT" inspect | bun -e \
  'const t=await new Response(Bun.stdin.stream()).text();
   const r=JSON.parse(t);
   if(!r.widgets.some((w)=>w.source==="clay" && w.label==="Smooth Scaling")) process.exit(1);'
cli --port "$PORT" click --label CANCEL
cli --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000
cli --port "$PORT" click --label AUDIO
cli --port "$PORT" wait_for_state --state OPTIONSAUDIO --timeout-ms 5000
cli --port "$PORT" inspect | bun -e \
  'const t=await new Response(Bun.stdin.stream()).text();
   const r=JSON.parse(t);
   if(!r.widgets.some((w)=>w.source==="clay" && w.label==="Music")) process.exit(1);'
cli --port "$PORT" click --label CANCEL
cli --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000
# OPTIONS -> back -> MAINMENU
cli --port "$PORT" back
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 5000
echo "PASS 10_navigate"
