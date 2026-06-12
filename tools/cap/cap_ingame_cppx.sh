#!/usr/bin/env bash
# cap_ingame_cppx.sh — capture OUR (cppx) client's in-game HUD renders headlessly,
# mirroring tools/cap/cap_ingame_origin.sh's deterministic drive so each capture
# lands on the SAME absolute sim tick as the corresponding origin golden.
#
# Differences vs the origin script (richer cppx control surface):
#   - chat compose text seeds via `ingame_ui_mode --mode chat --chat-text ...`
#     (no widget-uid set_text needed).
#   - chat history seeds via `--chat-line ...` which appends WITHOUT opening
#     the input (no Esc keystroke needed).
#   - output goes to /tmp/cppx_renders by default — NEVER the golden dir.
#
# Determinism model is identical: pause on SINGLEPLAYERGAME entry, advance the
# sim only via feedback-stepped `step --frames N` converging on
# message_progress (+1/tick, mod 256); all overlays toggled while paused.
#
# Usage: tools/cap/cap_ingame_cppx.sh [OUT_DIR]
#   PORT control port (default 63600; TUI session uses PORT+1/+2/+3)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PORT="${PORT:-63600}"
OUT_DIR="${1:-/tmp/cppx_renders}"
BIN="${SILENCER_BIN:-$REPO_ROOT/clients/silencer/build/Silencer.app/Contents/MacOS/Silencer}"
[ -x "$BIN" ] || BIN="$REPO_ROOT/clients/silencer/build/silencer"
[ -x "$BIN" ] || { echo "silencer binary missing under clients/silencer/build" >&2; exit 1; }
mkdir -p "$OUT_DIR"

cli() { bun "$REPO_ROOT/clients/cli/index.ts" --port "$PORT" "$@"; }

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
# tutorial message 2 (same anchor as the origin goldens).
drive_to_anchor() {
  for _ in $(seq 1 60); do cli ping >/dev/null 2>&1 && break; sleep 0.5; done
  cli ping >/dev/null

  cli wait_for_state --state MAINMENU --timeout-ms 20000 >/dev/null
  cli click --label Tutorial >/dev/null
  cli wait_for_state --state SINGLEPLAYERGAME --timeout-ms 20000 >/dev/null
  cli pause >/dev/null

  local ws
  for _ in $(seq 1 200); do
    ws="$(cli world_state)"
    [ "$(jget "$ws" '(s.players?.length??0)>0')" = "true" ] && break
    cli step --frames 4 >/dev/null
  done
  [ "$(jget "$ws" '(s.players?.length??0)>0')" = "true" ] || { echo "player never spawned" >&2; return 1; }

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

  # Same determinism guards as the origin capture: pinned spawn + default keymap.
  [ "$(jget "$ws" 's.players[0].x===2784&&s.players[0].y===1350')" = "true" ] \
    || { echo "unexpected spawn point: $(jget "$ws" 'JSON.stringify(s.players[0])') (goldens assume 2784,1350)" >&2; return 1; }
  [ "$(jget "$ws" 's.message_text.includes("tapping Left and Right")')" = "true" ] \
    || { echo "non-default keymap in tutorial message: $(jget "$ws" 's.message_text')" >&2; return 1; }
}

mp_now() { jget "$(cli world_state)" 's.message_progress'; }

# Feedback-stepped convergence on message_progress == target (never overshoots).
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

# align_camera <golden.png>: pin the world camera to the golden's. The
# follow-camera has a 100px y-hysteresis (Camera::Follow h=100/yoffset=30) so
# its rest position depends on render cadence during the spawn fall; phase-
# correlate our world band against the golden and set the camera until the
# measured shift is zero.
align_camera() {
  local golden="$1"
  for _ in 1 2 3 4 5; do
    cli screenshot --out "/tmp/camalign_$PORT.png" >/dev/null
    local shift
    shift="$(python3 - "$PORT" "$golden" <<'PY'
import sys
import numpy as np
from PIL import Image
port, golden = sys.argv[1], sys.argv[2]
a = np.array(Image.open(f"/tmp/camalign_{port}.png").convert("L"), dtype=float)
b = np.array(Image.open(golden).convert("L"), dtype=float)
# world band, away from HUD chrome / team strip / minimap
a = a[100:380, 80:600]; b = b[100:380, 80:600]
fa = np.fft.fft2(a - a.mean()); fb = np.fft.fft2(b - b.mean())
r = fb * np.conj(fa)
r /= np.abs(r) + 1e-9
c = np.fft.ifft2(r).real
peak = np.unravel_index(np.argmax(c), c.shape)
dy, dx = peak
if dy > a.shape[0] // 2: dy -= a.shape[0]
if dx > a.shape[1] // 2: dx -= a.shape[1]
print(dx, dy)
PY
)"
    local dx dy
    dx="${shift%% *}"; dy="${shift##* }"
    if [ "$dx" = "0" ] && [ "$dy" = "0" ]; then return 0; fi
    local cam cx cy
    cam="$(cli camera)"
    cx="$(jget "$cam" 's.x')"; cy="$(jget "$cam" 's.y')"
    # screen content moves by (dx,dy) when the camera moves by (-dx,-dy)
    cli camera --x "$((cx - dx))" --y "$((cy - dy))" >/dev/null
    cli wait_frames --n 2 >/dev/null
  done
  echo "camera never aligned to $golden (last shift $dx,$dy)" >&2
  return 1
}

# shot_pulse_match <name> <x0> <y0> <x1> <y1>: the buy/tech selected row pulses
# its brightness on the WALL clock (origin HudView selectedBrightness,
# SDL_GetTicks/50 period 16); sample frames ~100ms apart and keep the one whose
# region best matches the golden.
shot_pulse_match() {
  local name="$1" x0="$2" y0="$3" x1="$4" y1="$5"
  for i in $(seq 1 40); do
    cli screenshot --out "/tmp/pulse_${PORT}_$i.png" >/dev/null
    cli wait_ms --n 90 >/dev/null
  done
  python3 - "$PORT" "$REPO_ROOT/tests/cli-agent/e2e/golden/$name.png" \
    "$OUT_DIR/$name.png" "$x0" "$y0" "$x1" "$y1" <<'PY'
import sys
import numpy as np
from PIL import Image
port, golden, out = sys.argv[1:4]
x0, y0, x1, y1 = map(int, sys.argv[4:8])
b = np.array(Image.open(golden).convert("RGB"), dtype=int)
frames, resids = [], []
for i in range(1, 41):
    a = np.array(Image.open(f"/tmp/pulse_{port}_{i}.png").convert("RGB"))
    frames.append(a)
    resids.append(int(np.abs(a[y0:y1, x0:x1].astype(int) - b[y0:y1, x0:x1]).sum()))
order = np.argsort(resids)
# Median the 5 best pulse-phase matches (same brightness recurs every 800ms)
# so transient rain streaks drop out like the plain-shot median.
pick = [frames[i] for i in order[:5]]
med = np.median(np.stack(pick), axis=0).astype(np.uint8)
Image.fromarray(med).save(out)
print(f"captured {sys.argv[2].split('/')[-1].replace('.png','')} "
      f"(pulse-matched median, best resid {resids[order[0]]})")
PY
}

shot() { # shot <name> — single frame; our rain layer is disabled (`rain` op)
  cli screenshot --out "$OUT_DIR/$1.png" >/dev/null
  bun -e '
    const b = new DataView(await Bun.file(process.argv[1]).arrayBuffer());
    if (b.getUint32(16) !== 640 || b.getUint32(20) !== 480) { console.error("not 640x480"); process.exit(1); }
  ' "$OUT_DIR/$1.png"
  echo "captured $1"
}

# chat captures must be caret-ON (wall-clock blink): keep the brighter of a
# blink-pair over the input strip x446-614 y288-302.
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
"$BIN" --headless --control-port "$PORT" >"/tmp/silencer-ingame-cppx-$PORT.log" 2>&1 &
PID=$!
drive_to_anchor

step_to_mp 0
# Our rain/puddle layer is rand()-driven on the wall clock and can never match
# the goldens' frozen streaks — disable it; the golden's own rain is absorbed
# by the documented masks + tile tolerance.
cli rain --enabled 0 >/dev/null
cli wait_ms --n 1000 >/dev/null

cli ingame_ui_mode --mode clear >/dev/null
cli wait_frames --n 2 >/dev/null
align_camera "$REPO_ROOT/tests/cli-agent/e2e/golden/ingame_hud_base.png"
shot ingame_hud_base

cli ingame_ui_mode --mode playerlist >/dev/null
cli wait_frames --n 2 >/dev/null
shot ingame_player_list

cli ingame_ui_mode --mode clear >/dev/null
cli ingame_ui_mode --mode buy >/dev/null
cli wait_frames --n 2 >/dev/null
# selected row 0 (icon at ~169..222, name/price y145-160) pulses on wall clock
shot_pulse_match ingame_buy_tech 160 139 460 165

cli ingame_ui_mode --mode clear >/dev/null
cli ingame_ui_mode --mode chat --chat-text "parity check" >/dev/null
cli wait_frames --n 2 >/dev/null
shot_caret_on ingame_chat_open

cli ingame_ui_mode --mode clear >/dev/null
cli ingame_ui_mode --mode chat --chat-line "Recon: parity check one" >/dev/null
cli ingame_ui_mode --mode chat --chat-line "Recon: parity check two" >/dev/null
cli wait_frames --n 2 >/dev/null
shot ingame_chat_history

cli ingame_ui_mode --mode clear >/dev/null
cli wait_frames --n 2 >/dev/null

step_to_mp 94
shot ingame_messages

# tech overlay LAST (irreversibly joins a team + teleports the player)
step_to_mp 0
cli ingame_ui_mode --mode tech >/dev/null
cli wait_frames --n 2 >/dev/null
align_camera "$REPO_ROOT/tests/cli-agent/e2e/golden/ingame_tech_overlay.png"
shot_pulse_match ingame_tech_overlay 160 139 460 165

stop_session

# ============================================================================
# Session B — TUI input channel: quit prompt (needs a real ESC scancode)
# ============================================================================
PORT="$((PORT + 1))"
FRAME_PORT="$((PORT + 1))"
INPUT_PORT="$((PORT + 2))"

bun -e '
  Bun.listen({ hostname: "127.0.0.1", port: Number(process.argv[1]), socket: { data() {} } });
  setInterval(() => {}, 60_000);
' "$FRAME_PORT" &
SINK_PID=$!
sleep 0.5

send_scancodes() { # <byte 5 of the 64-byte scancode mask> — ESC=41 = byte 5 bit 1
  bun -e '
    const [port, byte5] = process.argv.slice(1).map(Number);
    const mask = new Uint8Array(64);
    mask[5] = byte5;
    const frame = new Uint8Array(1 + 3 + 64);
    frame[0] = 1;
    frame[1] = 2;
    frame[2] = 64; frame[3] = 0;
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

# Bounded re-drive: under full-suite load the follow-camera's rest position
# occasionally lands where the x correction is outside Camera::Follow's ±7
# window (w=15) and align_camera sticks at a constant shift — a fresh drive
# re-rolls the rest position. A visual gate must not be flaky.
quit_prompt_attempt() {
  SILENCER_TUI_FRAME_HOST=127.0.0.1 SILENCER_TUI_FRAME_PORT="$FRAME_PORT" \
    "$BIN" --headless --tui --tui-input-port "$INPUT_PORT" --control-port "$PORT" \
    >"/tmp/silencer-ingame-cppx-$PORT.log" 2>&1 &
  PID=$!
  drive_to_anchor || return 1
  # ESC press/release below consume one sim tick each → capture sits at mp==0.
  step_to_mp 254 || return 1
  send_scancodes 2
  cli step --frames 1 >/dev/null
  send_scancodes 0
  cli step --frames 1 >/dev/null
  cli wait_frames --n 2 >/dev/null
  align_camera "$REPO_ROOT/tests/cli-agent/e2e/golden/ingame_quit_prompt.png" || return 1
  shot ingame_quit_prompt
}

captured=""
for attempt in 1 2 3; do
  if quit_prompt_attempt; then captured="yes"; break; fi
  echo "quit_prompt attempt $attempt failed; tearing down + re-driving" >&2
  stop_session
done
[ -n "$captured" ] || { echo "quit_prompt capture failed after 3 drives" >&2; exit 1; }

stop_session
echo "all in-game cppx renders captured to $OUT_DIR"
