#!/usr/bin/env bash
# cap_ingame_cppx_extra.sh — capture OUR (cppx) client's renders for the 4
# gameplay-driven in-game surfaces, mirroring cap_ingame_origin_extra.sh's
# drive tick-for-tick (same tutorial, same TUI action script, same anchors).
#
# Differences vs the origin script:
#   - our rain layer is disabled (`rain` op) — the goldens' frozen rain is the
#     residual the masks/tile tolerance absorb;
#   - the camera is phase-correlated to each golden before the shot (the
#     follow-cam rest position is render-cadence-dependent).
#
# Usage: tools/cap/cap_ingame_cppx_extra.sh [OUT_DIR]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PORT="${PORT:-63620}"
FRAME_PORT="$((PORT + 1))"
INPUT_PORT="$((PORT + 2))"
OUT_DIR="${1:-/tmp/cppx_renders}"
BIN="${SILENCER_BIN:-$REPO_ROOT/clients/silencer/build/Silencer.app/Contents/MacOS/Silencer}"
[ -x "$BIN" ] || BIN="$REPO_ROOT/clients/silencer/build/silencer"
[ -x "$BIN" ] || { echo "silencer binary missing under clients/silencer/build" >&2; exit 1; }
CLI_DIR="$REPO_ROOT/clients/cli"
GOLDEN_DIR="$REPO_ROOT/tests/cli-agent/e2e/golden"
mkdir -p "$OUT_DIR"

. "$SCRIPT_DIR/ingame_drive_lib.sh"

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

# align_camera <golden.png>: phase-correlate our world band against the golden
# and pin the camera (same as cap_ingame_cppx.sh).
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
    cli camera --x "$((cx - dx))" --y "$((cy - dy))" >/dev/null
    cli wait_frames --n 2 >/dev/null
  done
  echo "camera never aligned to $golden (last shift $dx,$dy)" >&2
  return 1
}

boot_tui
drive_to_anchor
cli rain --enabled 0 >/dev/null

# --- 1. top ticker (F4) ------------------------------------------------------
step_to_mp 254
send_scancodes 7 32
cli step --frames 1 >/dev/null
send_scancodes 7 0
cli step --frames 1 >/dev/null
probe_ink "top ticker" 264 8 460 24
align_camera "$GOLDEN_DIR/ingame_top_ticker.png"
shot ingame_top_ticker

# --- 2. tutorial cases 0-8 ---------------------------------------------------
run_tutorial_to_basedoor

# --- 3. status line at the deck terminal -------------------------------------
walk_to_x 1860 1820
for _ in $(seq 1 12); do
  [ "$(jget "$(cli world_state)" 's.topmessage_progress')" = "0" ] && break
  cli step --frames 32 >/dev/null
done
step_to_mp 60
send_actions 32768 0
cli step --frames 1 >/dev/null
send_actions 0 0
cli step --frames 1 >/dev/null
probe_ink "status line" 200 362 440 382
cli step --frames 64 >/dev/null
cli step --frames 26 >/dev/null
align_camera "$GOLDEN_DIR/ingame_status_lines.png"
shot ingame_status_lines

# --- 4. base built clear of the terminal -> secret hack panel ----------------
walk_to_x 2140 2100
send_actions 32768 0
cli step --frames 1 >/dev/null
send_actions 0 0
cli step --frames 1 >/dev/null
expect_msg "*enter your base*" "base door built"
step_to_mp 0
echo "secret: mp after step_to_mp = $(mp_now)"
align_camera "$GOLDEN_DIR/ingame_secret_overlay.png"
echo "secret: mp before shot = $(mp_now)"
shot ingame_secret_overlay

# --- 5. rockets from the base inventory station ------------------------------
for _ in $(seq 1 10); do
  act 16384 0 2
  cli step --frames 12 >/dev/null
  case "$(msg_now)" in *computer\ screen*) break;; esac
  send_actions 4 0; cli step --frames 2 >/dev/null
  send_actions 0 0; cli step --frames 1 >/dev/null
done
expect_msg "*computer\ screen*" "entered base"
for _ in $(seq 1 14); do
  send_actions 8 0; cli step --frames 8 >/dev/null
  send_actions 0 0; cli step --frames 1 >/dev/null
  act 16384 0 2
  case "$(msg_now)" in *Rocket\ ammo*|*purchase*) break;; esac
done
expect_msg "*purchase*" "buy menu open"
# our buy rows are keyboard-navigated focusables: Down selects Rocket, Enter
# purchases (world_session.buytech_purchase -> Player::BuyItem). One render
# frame between the keys — they share a UiInputFrame otherwise and confirm
# would activate the stale focus.
cli key --key down >/dev/null
cli wait_frames --n 2 >/dev/null
cli key --key enter >/dev/null
cli wait_frames --n 2 >/dev/null
cli step --frames 2 >/dev/null
expect_msg "*exit the menu*" "rocket purchased"
cli key --key enter >/dev/null || true
cli wait_frames --n 2 >/dev/null
cli step --frames 2 >/dev/null
act 4 0 2
for _ in $(seq 1 14); do
  send_actions 4 0; cli step --frames 16 >/dev/null
  send_actions 0 0; cli step --frames 1 >/dev/null
  case "$(msg_now)" in *playfield*|*data\ terminals*|*data\ port*) break;; esac
done
expect_msg "*data*" "left the base"

# --- 6. rocket in flight -> system camera inset ------------------------------
walk_to_x 2620 2580
send_actions 8 0; cli step --frames 2 >/dev/null
send_actions 0 0; cli step --frames 1 >/dev/null
act 0 4 2
step_to_mp 230
inset_on() {
  cli screenshot --out "/tmp/inset_probe_$PORT.png" >/dev/null
  python3 - "$PORT" <<'PY'
import sys
from PIL import Image
import numpy as np
a = np.array(Image.open(f"/tmp/inset_probe_{sys.argv[1]}.png").convert("RGB"), dtype=int)
band = a[300:360, 0:140]
g = (band[:,:,1] > band[:,:,0]+10) & (band[:,:,1] > band[:,:,2]+10) & (band[:,:,1] > 30)
sys.exit(0 if g.sum() >= 200 else 1)
PY
}
fired=""
for attempt in 1 2 3; do
  oc0="$(jget "$(cli world_state)" 's.objects_count')"
  send_actions 65536 0
  for _ in $(seq 1 8); do
    cli step --frames 1 >/dev/null
    [ "$(jget "$(cli world_state)" 's.objects_count')" -gt "$oc0" ] && break
  done
  send_actions 0 0
  cli step --frames 2 >/dev/null
  if inset_on; then fired=yes; break; fi
done
[ -n "$fired" ] || { echo "system camera inset never appeared" >&2; exit 1; }
cli step --frames 1 >/dev/null
align_camera "$GOLDEN_DIR/ingame_system_camera.png"
shot ingame_system_camera

echo "extra in-game cppx renders captured to $OUT_DIR"
