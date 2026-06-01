#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"

cli --port "$PORT" wait_for_ui --id main_menu.options --timeout-ms 15000 >/dev/null
cli --port "$PORT" show_password_modal >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null

cli --port "$PORT" inspect | bun -e '
const text = await new Response(Bun.stdin.stream()).text();
const response = JSON.parse(text);
const widgets = response.widgets ?? [];
const password = widgets.find((w) => w.source === "clay" && w.label === "Password");
const ok = widgets.find((w) => w.source === "clay" && w.label === "OK");
const leaked = widgets.some((w) => ["Tutorial", "Options", "Exit"].includes(w.label));
if (!password || password.kind !== "textinput" || !password.password || !ok || leaked) {
  console.error("password modal did not expose only modal-scoped Clay widgets");
  process.exit(1);
}
'

for ch in s w o r d f i s h; do
  cli --port "$PORT" key --key "$ch" >/dev/null
done
cli --port "$PORT" key --key enter >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null
cli --port "$PORT" password_modal_result | bun -e '
const text = await new Response(Bun.stdin.stream()).text();
const response = JSON.parse(text);
const result = response.result ?? response;
if (!result.submitted || result.value !== "swordfish") {
  console.error(`password modal submit mismatch: ${JSON.stringify(result)}`);
  process.exit(1);
}
'

echo "PASS 13_password_modal"
