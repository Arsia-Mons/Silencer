#!/usr/bin/env bash
# Windowed in-game stretch verification (REQUIRES a logged-in GUI session —
# SDL needs WindowServer; headless gates cannot see this path).
#
# Drives a REAL windowed client at 1920x1080 to the cap_ingame_cppx tutorial
# anchor and captures the composited swapchain. In-game, origin renders world
# + HUD into ONE 640x480 screenbuffer and stretches it NON-uniformly (x3.0 /
# x2.25, NEAREST) to the window; our pipeline renders the in-game cppx UI at
# the 640x480 surface on all paths and the backend composite (fullscreen quad,
# nearest) stretches it like the world quad. PASS criteria:
#   1. The downsampled (exact src-map inverse) capture camera-aligns and
#      matches the ingame_hud_base golden under the standard masks.
#   2. The right-anchored inventory-frame band (virtual x612-640 -> device
#      x1836-1920) matches the golden — the pre-fix uniform scale put that
#      chrome at device ~x1377, nowhere near the window edge.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PORT="${PORT:-63800}"
BIN="${SILENCER_BIN:-$REPO_ROOT/clients/silencer/build/Silencer.app/Contents/MacOS/Silencer}"
[ -x "$BIN" ] || BIN="$REPO_ROOT/clients/silencer/build/silencer"
OUT="${1:-/tmp/cppx_renders/ingame_windowed.png}"
mkdir -p "$(dirname "$OUT")"

cli() { bun "$REPO_ROOT/clients/cli/index.ts" --port "$PORT" "$@"; }
jget() { bun -e "const s=(JSON.parse(process.argv[1]).result)??JSON.parse(process.argv[1]);console.log($2)" "$1"; }

PID=""
cleanup() {
  cli quit >/dev/null 2>&1 || true
  sleep 0.3
  [ -n "$PID" ] && kill "$PID" 2>/dev/null || true
}
trap cleanup EXIT

# WINDOWED (no --headless): the real divergence scenario.
"$BIN" --control-port "$PORT" >"/tmp/silencer-winverify-$PORT.log" 2>&1 &
PID=$!
for _ in $(seq 1 60); do cli ping >/dev/null 2>&1 && break; sleep 0.5; done
cli ping >/dev/null
cli wait_for_state --state MAINMENU --timeout-ms 20000 >/dev/null
cli resize --w 1920 --h 1080 >/dev/null
cli wait_frames --n 3 >/dev/null
cli click --label Tutorial >/dev/null
cli wait_for_state --state SINGLEPLAYERGAME --timeout-ms 20000 >/dev/null
cli pause >/dev/null

ws=""
for _ in $(seq 1 200); do
  ws="$(cli world_state)"
  [ "$(jget "$ws" '(s.players?.length??0)>0')" = "true" ] && break
  cli step --frames 4 >/dev/null
done
[ "$(jget "$ws" '(s.players?.length??0)>0')" = "true" ] || { echo "player never spawned" >&2; exit 1; }

found=""
for _ in $(seq 1 600); do
  ws="$(cli world_state)"
  if [ "$(jget "$ws" '(s.message_text||"").startsWith("Move your agent")&&s.message_progress>=1')" = "true" ]; then
    found="yes"; break
  fi
  cli step --frames 1 >/dev/null
done
[ -n "$found" ] || { echo "tutorial message 2 never appeared" >&2; exit 1; }

# step to mp 0 like cap_ingame_cppx (hud_base anchor)
for _ in $(seq 1 400); do
  cur="$(jget "$(cli world_state)" 's.message_progress')"
  rem="$(( (0 - cur + 256) % 256 ))"
  [ "$rem" -eq 0 ] && break
  if [ "$rem" -gt 40 ]; then
    bulk="$((rem - 30))"; [ "$bulk" -gt 64 ] && bulk=64
    cli step --frames "$bulk" >/dev/null
  else
    cli step --frames 1 >/dev/null
  fi
done

cli rain --enabled 0 >/dev/null
cli ingame_ui_mode --mode clear >/dev/null
cli wait_frames --n 3 >/dev/null

# Camera-align against the golden using the DOWNSAMPLED windowed capture
# (pick one device pixel inside each 640x480 src cell — exact for a NEAREST
# fullscreen-quad stretch).
GOLDEN="$REPO_ROOT/tests/cli-agent/e2e/golden/ingame_hud_base.png"
dx=99; dy=99
for _ in 1 2 3 4 5 6 7 8; do
  cli screenshot --out "$OUT" >/dev/null
  shift_xy="$(python3 - "$OUT" "$GOLDEN" <<'PY'
import sys
import numpy as np
from PIL import Image
full = np.array(Image.open(sys.argv[1]).convert("L"), dtype=float)
assert full.shape == (1080, 1920), full.shape
xs = np.array([int(np.ceil(vx * 1920 / 640)) for vx in range(640)])
ys = np.array([int(np.ceil(vy * 1080 / 480)) for vy in range(480)])
a = full[np.ix_(ys, xs)]
b = np.array(Image.open(sys.argv[2]).convert("L"), dtype=float)
a = a[100:380, 80:600]; b = b[100:380, 80:600]
fa = np.fft.fft2(a - a.mean()); fb = np.fft.fft2(b - b.mean())
r = fb * np.conj(fa); r /= np.abs(r) + 1e-9
c = np.fft.ifft2(r).real
dy, dx = np.unravel_index(np.argmax(c), c.shape)
if dy > a.shape[0]//2: dy -= a.shape[0]
if dx > a.shape[1]//2: dx -= a.shape[1]
print(dx, dy)
PY
)"
  dx="${shift_xy%% *}"; dy="${shift_xy##* }"
  if [ "$dx" = "0" ] && [ "$dy" = "0" ]; then break; fi
  cam="$(cli camera)"
  cx="$(jget "$cam" 's.x')"; cy="$(jget "$cam" 's.y')"
  cli camera --x "$((cx - dx))" --y "$((cy - dy))" >/dev/null
  cli wait_frames --n 2 >/dev/null
done
[ "$dx" = "0" ] && [ "$dy" = "0" ] || { echo "camera never aligned ($dx,$dy)" >&2; exit 1; }

cli screenshot --out "$OUT" >/dev/null

python3 - "$OUT" "$GOLDEN" <<'PY'
import sys
import numpy as np
from PIL import Image
full = np.array(Image.open(sys.argv[1]).convert("RGB"))
g = np.array(Image.open(sys.argv[2]).convert("RGB"))
xs = np.array([int(np.ceil(vx * 1920 / 640)) for vx in range(640)])
ys = np.array([int(np.ceil(vy * 1080 / 480)) for vy in range(480)])
down = full[np.ix_(ys, xs)]
Image.fromarray(down).save("/tmp/ingame_windowed_down.png")
mae = np.abs(down[:, 612:640].astype(int) - g[:, 612:640].astype(int)).mean()
print(f"inventory band (virtual x612-640 = device x1836-1920) mae vs golden: {mae:.2f}")
assert mae < 8.0, "right-anchored HUD band does not match origin placement"
PY
python3 "$REPO_ROOT/tools/cap/pixdiff_tolerant.py" /tmp/ingame_windowed_down.png "$GOLDEN" \
  --mask 235,419,406,479 --mask 0,296,640,340 --mask 624,0,640,419
echo "windowed capture: $OUT (downsample: /tmp/ingame_windowed_down.png)"
