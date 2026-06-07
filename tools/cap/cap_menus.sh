#!/usr/bin/env bash
# Persistent menu-cluster capture for cppx visual-parity work.
# Dumps live cppx renders (960x720) to $OUT (default /tmp/cppx_renders).
# No diffing, no cleanup of renders — pixdiff them against the goldens yourself.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/../../tests/cli-agent/e2e/lib.sh"

OUT="${OUT:-/tmp/cppx_renders}"
mkdir -p "$OUT"
W="${W:-960}"; H="${H:-720}"

FRESH_HOME="$(mktemp -d)"; export HOME="$FRESH_HOME"
PORT="$(pick_port)"; PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"; rm -rf "$FRESH_HOME"' EXIT
wait_alive "$PORT"
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
cli --port "$PORT" resize --w "$W" --h "$H" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null

shot() { # name
  cli --port "$PORT" wait_frames --n 3 >/dev/null
  cli --port "$PORT" screenshot --out "$OUT/$1.png" >/dev/null
  cli --port "$PORT" inspect > "$OUT/$1.json" 2>/dev/null || true
  echo "  captured $1"
}

shot mainmenu
cli --port "$PORT" click --label "Options" >/dev/null; shot options
cli --port "$PORT" click --label "OptionsAudio" >/dev/null; shot options_audio
cli --port "$PORT" click --label "OptionsCancel" >/dev/null; cli --port "$PORT" wait_frames --n 3 >/dev/null
cli --port "$PORT" click --label "OptionsDisplay" >/dev/null; shot options_display
cli --port "$PORT" click --label "OptionsCancel" >/dev/null; cli --port "$PORT" wait_frames --n 3 >/dev/null
cli --port "$PORT" click --label "OptionsControls" >/dev/null; shot options_controls
echo "menus -> $OUT"
