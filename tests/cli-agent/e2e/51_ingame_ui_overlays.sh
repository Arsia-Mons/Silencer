#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"

OUT_DIR="$(mktemp -d)"

cli --port "$PORT" wait_for_ui --id main_menu.options --timeout-ms 15000 >/dev/null
cli --port "$PORT" click --label Tutorial >/dev/null
cli --port "$PORT" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 15000 >/dev/null

for _ in $(seq 1 60); do
  if cli --port "$PORT" world_state | bun -e '
    const text = await new Response(Bun.stdin.stream()).text();
    const response = JSON.parse(text);
    const state = response.result ?? response;
    if ((state.objects_count ?? 0) > 0 && (state.players?.length ?? 0) > 0) process.exit(0);
    process.exit(1);
  '; then
    break
  fi
  cli --port "$PORT" wait_frames --n 1 >/dev/null
done

check_png_size() {
  local path="$1"
  bun -e '
  const path = process.argv[1];
  const bytes = await Bun.file(path).arrayBuffer();
  const view = new DataView(bytes);
  const width = view.getUint32(16);
  const height = view.getUint32(20);
  if (width !== 640 || height !== 480) {
    console.error(`expected 640x480 screenshot, got ${width}x${height}`);
    process.exit(1);
  }
  ' "$path"
}

assert_mode_result() {
  local mode="$1" path="$2"
  bun -e '
  const mode = process.argv[1];
  const path = process.argv[2];
  const response = JSON.parse(await Bun.file(path).text());
  const result = response.result ?? response;
  const fail = (message) => {
    console.error(`${mode}: ${message}`);
    process.exit(1);
  };
  if (!result.ok || result.mode !== mode) fail(`unexpected result ${JSON.stringify(result)}`);
  if (mode === "chat" && (!result.chat_active || result.show_chat_ticks <= 0)) fail("chat overlay not active");
  if (mode === "buy" && (!result.buy_active || result.buy_item_count <= 0)) fail("buy overlay has no rows");
  if (mode === "tech" && (!result.tech_active || result.tech_item_count <= 0)) fail("tech overlay has no rows");
  if (mode === "playerlist" && !result.show_player_list) fail("player list not active");
  ' "$mode" "$path"
}

assert_overlay_changes_frame() {
  local mode="$1" before="$2" after="$3"
  local crop=""
  case "$mode" in
    chat) crop="360,300,280,180" ;;
    buy|tech) crop="120,0,400,220" ;;
    playerlist) crop="100,0,440,260" ;;
  esac
  local diff
  diff="$(tools/pixdiff/build/pixdiff --crop "$crop" "$before" "$after")"
  bun -e '
  const mode = process.argv[1];
  const diff = Number(process.argv[2]);
  if (!Number.isFinite(diff) || diff <= 0.01) {
    console.error(`${mode}: expected visible overlay diff, got ${diff}`);
    process.exit(1);
  }
  ' "$mode" "$diff"
}

check_mode() {
  local mode="$1"
  local result="$OUT_DIR/$mode.json"
  local before="$OUT_DIR/$mode-before.png"
  local after="$OUT_DIR/$mode-after.png"

  cli --port "$PORT" ingame_ui_mode --mode clear >/dev/null
  cli --port "$PORT" wait_frames --n 1 >/dev/null
  cli --port "$PORT" screenshot --out "$before" >/dev/null
  check_png_size "$before"

  cli --port "$PORT" ingame_ui_mode --mode "$mode" > "$result"
  assert_mode_result "$mode" "$result"
  cli --port "$PORT" wait_frames --n 1 >/dev/null
  cli --port "$PORT" screenshot --out "$after" >/dev/null
  check_png_size "$after"
  assert_overlay_changes_frame "$mode" "$before" "$after"
}

check_mode chat
check_mode buy
cli --port "$PORT" key --key down >/dev/null
cli --port "$PORT" ingame_ui_mode --mode status | bun -e '
const text = await new Response(Bun.stdin.stream()).text();
const response = JSON.parse(text);
const result = response.result ?? response;
if (!result.buy_active || result.buy_selected_index < 1 || result.buy_item_count <= 1) {
  console.error(`buy menu selection did not remain inspectable: ${JSON.stringify(result)}`);
  process.exit(1);
}
'
check_mode tech
cli --port "$PORT" key --key down >/dev/null
cli --port "$PORT" ingame_ui_mode --mode status | bun -e '
const text = await new Response(Bun.stdin.stream()).text();
const response = JSON.parse(text);
const result = response.result ?? response;
if (!result.tech_active || result.tech_selected_index < 1 || result.tech_item_count <= 1) {
  console.error(`tech menu selection did not remain inspectable: ${JSON.stringify(result)}`);
  process.exit(1);
}
'
check_mode playerlist

echo "PASS 51_ingame_ui_overlays ($OUT_DIR)"
