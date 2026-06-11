#!/usr/bin/env bash
# cap_ingame_origin.sh — capture origin/main in-game HUD goldens headlessly.
#
# Drives the PRISTINE origin binary (built from .worktrees/origin-capture at
# af4c50c5 WITHOUT the deterministic-fade patch — the patch corrupts the
# in-game palette: its FADEOUT branch writes an all-black temppalette whose
# stale high indices the in-game ambience path then re-exposes, blacking out
# all HUD/text colors; see ORIGIN_GOLDENS.md "In-game goldens") through the
# Tutorial single-player mission and captures each HUD surface at a
# sim-deterministic anchor tick.
#
# Determinism model:
#   - pause immediately on SINGLEPLAYERGAME entry, then advance the sim ONLY
#     via `step --frames N` (sim ticks are the only world clock).
#   - `step --frames N` is wall-clock-racy for multi-frame N (observed ±1-2
#     tick overshoot), so all anchors use FEEDBACK control on
#     message_progress (message_i, +1/tick, wraps mod 256): bulk-step to
#     ~30 ticks short of the target, then single-step (always exact) to it.
#     Landing on "first message_i==X after the message appears" identifies a
#     UNIQUE absolute sim tick, so the whole world (rain, agent, gauges,
#     message reveal) is byte-reproducible across runs.
#   - anchor = first ticks of the tutorial's 2nd message ("Move your agent
#     left and right..." — it loops until the player moves, which headless
#     control input cannot do, so it is stable for hundreds of ticks).
#   - base tick = first message_i == 0 after the anchor (message wraps mod
#     256 and is hidden at message_i == 0) → clean HUD frame, no message.
#   - overlays (chat/playerlist/buy) are toggled via `ingame_ui_mode` while
#     PAUSED at the base tick: renders happen per render-frame, sim stays
#     frozen, so every overlay sees the identical world.
#   - the chat caret blinks on WALL CLOCK ((SDL_GetTicks()/50)%32<16) even
#     while paused: chat captures retry until the caret strip is in the ON
#     state, so goldens are always caret-on.
#   - `ingame_ui_mode tech` mutates the world irreversibly (creates/joins a
#     team, teleports the player to its base) → captured LAST.
#
# Two sessions:
#   A) plain --headless: hud_base, player_list, buy_tech, chat_open,
#      chat_history, messages, tech_overlay (control ops only).
#   B) --headless --tui + a throwaway frame sink: quit_prompt. The quitstate
#      machine only listens to raw OS scancodes (game_input.cpp
#      OnScancodeDown), which headless control ops can't inject — but TUI
#      mode's input server accepts scancode bitmasks over TCP and routes them
#      through the same edge-detect handlers (ESC down→quitstate 1,
#      up→quitstate 2 → "Hit Enter To Quit" prompt persists).
#
# Usage: tools/cap/cap_ingame_origin.sh [OUT_DIR]
#   ORIGIN_ROOT  override origin worktree (default .worktrees/origin-capture
#                resolved against this repo's parent layout)
#   PORT         control port (default 63544; TUI session uses PORT+1/+2/+3)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
ORIGIN_ROOT="${ORIGIN_ROOT:-$(cd "$REPO_ROOT/.." && pwd)/origin-capture}"
PORT="${PORT:-63544}"
OUT_DIR="${1:-$REPO_ROOT/tests/cli-agent/e2e/golden}"
BIN="$ORIGIN_ROOT/clients/silencer/build/Silencer.app/Contents/MacOS/Silencer"
[ -x "$BIN" ] || { echo "origin binary missing: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

# The capture must run the PRISTINE renderer (no fade patch) — verify.
if ! git -C "$ORIGIN_ROOT" diff --quiet -- clients/silencer/src/game/render/game_renderer.cpp; then
  echo "origin-capture has the fade patch applied — rebuild pristine first:" >&2
  echo "  (cd $ORIGIN_ROOT && git stash) && ninja -C $ORIGIN_ROOT/clients/silencer/build" >&2
  exit 1
fi

cli() { bun "$ORIGIN_ROOT/clients/cli/index.ts" --port "$PORT" "$@"; }

jget() { # jget <json> <bun-expr over `s`>
  bun -e "const s=(JSON.parse(process.argv[1]).result)??JSON.parse(process.argv[1]);console.log($2)" "$1"
}

PID=""
SINK_PID=""
cleanup() {
  cli quit >/dev/null 2>&1 || true
  sleep 0.3
  [ -n "$PID" ] && kill "$PID" 2>/dev/null || true
  [ -n "$PID" ] && wait "$PID" 2>/dev/null || true
  [ -n "$SINK_PID" ] && kill "$SINK_PID" 2>/dev/null || true
}
trap cleanup EXIT

stop_session() {
  cli quit >/dev/null 2>&1 || true
  sleep 0.3
  [ -n "$PID" ] && kill "$PID" 2>/dev/null || true
  [ -n "$PID" ] && wait "$PID" 2>/dev/null || true
  PID=""
}

# drive_to_anchor: tutorial entry → paused → player spawned → first ticks of
# tutorial message 2. Leaves the sim paused at the anchor neighborhood.
drive_to_anchor() {
  for _ in $(seq 1 60); do cli ping >/dev/null 2>&1 && break; sleep 0.5; done
  cli ping >/dev/null

  cli wait_for_state --state MAINMENU --timeout-ms 20000 >/dev/null
  cli click --label Tutorial >/dev/null
  cli wait_for_state --state SINGLEPLAYERGAME --timeout-ms 20000 >/dev/null
  cli pause >/dev/null

  # 1. player spawn
  local ws
  for _ in $(seq 1 200); do
    ws="$(cli world_state)"
    [ "$(jget "$ws" '(s.players?.length??0)>0')" = "true" ] && break
    cli step --frames 4 >/dev/null
  done
  [ "$(jget "$ws" '(s.players?.length??0)>0')" = "true" ] || { echo "player never spawned" >&2; return 1; }

  # 2. first ticks of tutorial message 2 (single-step so we can't overshoot)
  local found=""
  for _ in $(seq 1 600); do
    ws="$(cli world_state)"
    if [ "$(jget "$ws" '(s.message_text||"").startsWith("Move your agent")&&s.message_progress>=1')" = "true" ]; then
      found="yes"
      break
    fi
    cli step --frames 1 >/dev/null
  done
  [ -n "$found" ] || { echo "tutorial message 2 never appeared" >&2; return 1; }
  echo "anchor: message 2 visible at progress $(jget "$ws" 's.message_progress')"

  # Guard the two latent nondeterminism sources in the tutorial bring-up:
  # RandomPlayerStartLocation (empirically always 2784,1350 on AGENCY04.SIL)
  # and the keymap-templated message text ("By tapping Left and Right." needs
  # the default profile). If either drifts, the goldens are invalid — fail.
  [ "$(jget "$ws" 's.players[0].x===2784&&s.players[0].y===1350')" = "true" ] \
    || { echo "unexpected spawn point: $(jget "$ws" 'JSON.stringify(s.players[0])') (goldens assume 2784,1350)" >&2; return 1; }
  [ "$(jget "$ws" 's.message_text.includes("tapping Left and Right")')" = "true" ] \
    || { echo "non-default keymap in tutorial message: $(jget "$ws" 's.message_text')" >&2; return 1; }
}

mp_now() { jget "$(cli world_state)" 's.message_progress'; }

# step_to_mp <target>: feedback-stepped convergence on message_i == target.
# Bulk steps stay 30 ticks short of the target (multi-frame steps can overrun
# by 1-2 ticks); single steps are exact and finish the approach. Never
# overshoots, so it always resolves the FIRST tick with message_i == target.
step_to_mp() {
  local target="$1" cur rem
  for _ in $(seq 1 400); do
    cur="$(mp_now)"
    rem="$(( (target - cur + 256) % 256 ))"
    [ "$rem" -eq 0 ] && return 0
    if [ "$rem" -gt 40 ]; then
      local bulk="$((rem - 30))"
      [ "$bulk" -gt 64 ] && bulk=64
      cli step --frames "$bulk" >/dev/null
    else
      cli step --frames 1 >/dev/null
    fi
  done
  echo "step_to_mp $target never converged (at $(mp_now))" >&2
  return 1
}

shot() { # shot <name>
  cli screenshot --out "$OUT_DIR/$1.png" >/dev/null
  bun -e '
    const b = new DataView(await Bun.file(process.argv[1]).arrayBuffer());
    if (b.getUint32(16) !== 640 || b.getUint32(20) !== 480) { console.error("not 640x480"); process.exit(1); }
  ' "$OUT_DIR/$1.png"
  echo "captured $1"
}

# chat captures must be caret-ON: retry until the input strip (x446-614,
# y288-302 — measured from the live overlay) gains the caret column.
shot_caret_on() { # shot_caret_on <name>
  local name="$1" tries=12
  cli screenshot --out "/tmp/caret_a_$PORT.png" >/dev/null
  for _ in $(seq 1 "$tries"); do
    cli wait_ms --n 420 >/dev/null
    cli screenshot --out "/tmp/caret_b_$PORT.png" >/dev/null
    local pick
    pick="$(python3 - "$PORT" <<'PY'
import sys
from PIL import Image
import numpy as np
port = sys.argv[1]
a = np.array(Image.open(f"/tmp/caret_a_{port}.png").convert("RGB"), dtype=int)[286:304, 440:620]
b = np.array(Image.open(f"/tmp/caret_b_{port}.png").convert("RGB"), dtype=int)[286:304, 440:620]
d = (np.abs(a - b).sum(axis=2) > 0)
if not d.any():
    print("same"); raise SystemExit
print("a" if a[d].sum() > b[d].sum() else "b")
PY
)"
    case "$pick" in
      a) cp "/tmp/caret_a_$PORT.png" "$OUT_DIR/$name.png"; echo "captured $name (caret on, frame a)"; return 0 ;;
      b) cp "/tmp/caret_b_$PORT.png" "$OUT_DIR/$name.png"; echo "captured $name (caret on, frame b)"; return 0 ;;
      same) cp "/tmp/caret_b_$PORT.png" "/tmp/caret_a_$PORT.png" ;;
    esac
  done
  echo "caret never blinked for $name" >&2
  return 1
}

# ============================================================================
# Session A — plain headless: all control-op-reachable surfaces
# ============================================================================
"$BIN" --headless --control-port "$PORT" >"/tmp/silencer-ingame-cap-$PORT.log" 2>&1 &
PID=$!
drive_to_anchor

# base tick: message_i == 0 (center message hidden)
step_to_mp 0

# wall-clock palette fade-in is long finished by now (>10s real time), but be
# explicit: give it one more beat before the first capture.
cli wait_ms --n 1000 >/dev/null

# --- captures at the base tick ---------------------------------------------
cli ingame_ui_mode --mode clear >/dev/null
cli wait_frames --n 2 >/dev/null
shot ingame_hud_base

cli ingame_ui_mode --mode playerlist >/dev/null
cli wait_frames --n 2 >/dev/null
shot ingame_player_list

cli ingame_ui_mode --mode clear >/dev/null
cli ingame_ui_mode --mode buy >/dev/null
cli wait_frames --n 2 >/dev/null
shot ingame_buy_tech

# chat input open, deterministic text
cli ingame_ui_mode --mode clear >/dev/null
cli ingame_ui_mode --mode chat >/dev/null
cli wait_frames --n 1 >/dev/null
CHAT_UID="$(jget "$(cli inspect)" '(s.widgets.find(w=>w.id==="ingame.chat")||{}).uid')"
[ "$CHAT_UID" != "undefined" ] || { echo "chat input not registered" >&2; exit 1; }
cli set_text --uid "$CHAT_UID" --text "parity check" >/dev/null
cli wait_frames --n 2 >/dev/null
shot_caret_on ingame_chat_open

# chat history: two known lines, input closed (Esc), display ticks frozen >0
cli ingame_ui_mode --mode clear >/dev/null
cli ingame_ui_mode --mode chat --chat-line "Recon: parity check one" >/dev/null
cli ingame_ui_mode --mode chat --chat-line "Recon: parity check two" >/dev/null
cli key --key escape >/dev/null
cli wait_frames --n 2 >/dev/null
shot ingame_chat_history

cli ingame_ui_mode --mode clear >/dev/null
cli wait_frames --n 2 >/dev/null

# --- center message (base tick + 94 → message_i == 94, fully revealed) -----
step_to_mp 94
shot ingame_messages

# --- tech overlay LAST (irreversibly joins a team + teleports the player) --
step_to_mp 0   # hide the center message again
cli ingame_ui_mode --mode tech >/dev/null
cli wait_frames --n 2 >/dev/null
shot ingame_tech_overlay

stop_session

# ============================================================================
# Session B — TUI input channel: quit prompt (needs a real ESC scancode)
# ============================================================================
PORT="$((PORT + 1))"
FRAME_PORT="$((PORT + 1))"
INPUT_PORT="$((PORT + 2))"

# Throwaway frame sink: TUIBackend connects here at init and writes every
# presented frame; we accept and discard.
bun -e '
  Bun.listen({ hostname: "127.0.0.1", port: Number(process.argv[1]), socket: { data() {} } });
  setInterval(() => {}, 60_000);
' "$FRAME_PORT" &
SINK_PID=$!
sleep 0.5

SILENCER_TUI_FRAME_HOST=127.0.0.1 SILENCER_TUI_FRAME_PORT="$FRAME_PORT" \
  "$BIN" --headless --tui --tui-input-port "$INPUT_PORT" --control-port "$PORT" \
  >"/tmp/silencer-ingame-cap-$PORT.log" 2>&1 &
PID=$!
drive_to_anchor
# Land 2 ticks BEFORE the message-hidden tick: the ESC press/release edges
# below each consume one sim tick, so the capture itself sits at
# message_i == 0 (no center message over the quit prompt).
step_to_mp 254

# send_scancodes <hex byte for mask byte 5> — ESC is scancode 41 = byte 5 bit 1.
# One-shot connection: version handshake then a single SCANCODE_SNAPSHOT
# (latest-wins, cached server-side after we disconnect).
send_scancodes() {
  bun -e '
    const [port, byte5] = process.argv.slice(1).map(Number);
    const mask = new Uint8Array(64);
    mask[5] = byte5;
    const frame = new Uint8Array(1 + 3 + 64);
    frame[0] = 1;           // protocol version
    frame[1] = 2;           // SCANCODE_SNAPSHOT
    frame[2] = 64; frame[3] = 0; // len LE
    frame.set(mask, 4);
    const sock = await Bun.connect({
      hostname: "127.0.0.1", port,
      socket: { data() {}, error(_s, e) { console.error(e); process.exit(1); } },
    });
    sock.write(frame);
    await sock.flush?.();
    setTimeout(() => { sock.end(); process.exit(0); }, 200);
  ' "$INPUT_PORT" "$1"
}

# ESC press edge (quitstate 0→1), one sim tick to consume it, release edge
# (1→2: prompt latched), one more tick.
send_scancodes 2
cli step --frames 1 >/dev/null
send_scancodes 0
cli step --frames 1 >/dev/null
cli wait_frames --n 2 >/dev/null

# Verify the prompt actually rendered ("Hit Enter To Quit", centered ~y200)
cli screenshot --out "/tmp/quit_probe_$PORT.png" >/dev/null
python3 - "$PORT" <<'PY'
import sys
from PIL import Image
import numpy as np
port = sys.argv[1]
a = np.array(Image.open(f"/tmp/quit_probe_{port}.png").convert("RGB"), dtype=int)
band = a[190:230, 150:490]
if (band.sum(axis=2) > 90).sum() < 50:
    print("quit prompt not visible in y190-230 band", file=sys.stderr)
    sys.exit(1)
PY
cp "/tmp/quit_probe_$PORT.png" "$OUT_DIR/ingame_quit_prompt.png"
echo "captured ingame_quit_prompt"

stop_session
echo "all in-game origin goldens captured to $OUT_DIR"
