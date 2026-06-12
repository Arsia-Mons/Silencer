#!/usr/bin/env bash
# cap_ingame_origin_extra.sh — capture the 4 gameplay-driven in-game goldens
# (top_ticker, status_lines, secret_overlay, system_camera) from the PRISTINE
# origin binary. Companion to cap_ingame_origin.sh (same determinism model:
# paused sim + feedback-stepped control, TUI input channel for raw input).
#
# Drive (one TUI session, tutorial AGENCY04.SIL):
#   1. anchor (tutorial message 1 visible, paused) → F4 scancode edge →
#      ShowTopMessage("          *MUSIC PAUSED*") (game_input.cpp F4 handler;
#      headless audio is disabled so MusicPaused() is always false and the
#      text is stable) → ingame_top_ticker at topmessage_i == 2.
#   2. tutorial cases 0-8 via TUI ACTION snapshots (run/jump/jetpack/crouch/
#      roll/disguise x2/fire/switch-weapon — all near the spawn deck); case 9
#      grants INV_BASEDOOR.
#   3. walk LEFT to the deck data terminal at x=1824 (parsed from
#      AGENCY04.SIL actor records); keyuse with the base-door item →
#      Player::CanCreateBase fails on TERMINAL-in-AABB → ShowStatus("Can't
#      build a base here", 208) → ingame_status_lines captured 92 ticks after
#      the push (status time == 8 → brightness 64, mid-fade; trigger pinned at
#      message_i == 60 so the capture lands at 152 where the case-10 center
#      message has faded to black).
#   4. walk RIGHT clear of the terminal (~2100), keyuse → base door built →
#      team.basedoorid set → secret hack panel visible → ingame_secret_overlay
#      at message_i == 0 (center message hidden).
#   5. enter the base (keyactivate at the just-built door), walk right to the
#      inventory station, keyactivate → buy menu → control-op click on the
#      Rocket row (purchase via ActivateBuyTechSelection) x2 → exit menu
#      (moveleft) → walk left out of the base.
#   6. weapon 3 (rocket), hold fire until the projectile spawns
#      (objects_count edge), +3 ticks → rocket in flight keeps
#      world.SetSystemCamera(0, rocket) alive → ingame_system_camera, fired
#      at message_i == 200 (case-16 center message faded).
#
# NOT captured here: hud_trace_time (needs team.secretprogress >= 180 =
# draining 4+ data terminals across the map; see PARITY.md).
#
# Usage: tools/cap/cap_ingame_origin_extra.sh [OUT_DIR]
#   ORIGIN_ROOT  override origin worktree
#   PORT         control port (default 63554; +1 frame sink, +2 TUI input)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
ORIGIN_ROOT="${ORIGIN_ROOT:-$(cd "$REPO_ROOT/.." && pwd)/origin-capture}"
PORT="${PORT:-63554}"
FRAME_PORT="$((PORT + 1))"
INPUT_PORT="$((PORT + 2))"
OUT_DIR="${1:-$REPO_ROOT/tests/cli-agent/e2e/golden}"
BIN="$ORIGIN_ROOT/clients/silencer/build/Silencer.app/Contents/MacOS/Silencer"
CLI_DIR="$ORIGIN_ROOT/clients/cli"
[ -x "$BIN" ] || { echo "origin binary missing: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

# The capture must run the PRISTINE renderer (no fade patch) — verify.
if ! git -C "$ORIGIN_ROOT" diff --quiet -- clients/silencer/src/game/render/game_renderer.cpp; then
  echo "origin-capture has the fade patch applied — stash + rebuild pristine first" >&2
  exit 1
fi

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

boot_tui
drive_to_anchor

# --- 1. top ticker (F4 = scancode 61 -> mask byte 7 bit 5) ------------------
step_to_mp 254
send_scancodes 7 32
cli step --frames 1 >/dev/null
send_scancodes 7 0
cli step --frames 1 >/dev/null
probe_ink "top ticker" 264 8 460 24
shot ingame_top_ticker

# --- 2. tutorial cases 0-8 ---------------------------------------------------
run_tutorial_to_basedoor

# --- 3. status line at the deck terminal (x=1824) ---------------------------
walk_to_x 1860 1820
# The F4 ticker scrolls for 254 ticks; make sure it expired before capturing.
for _ in $(seq 1 12); do
  [ "$(jget "$(cli world_state)" 's.topmessage_progress')" = "0" ] && break
  cli step --frames 32 >/dev/null
done
step_to_mp 60
send_actions 32768 0   # keyuse edge: CanCreateBase fails -> status pushed
cli step --frames 1 >/dev/null
send_actions 0 0
cli step --frames 1 >/dev/null
probe_ink "status line" 200 362 440 382
cli step --frames 64 >/dev/null
cli step --frames 26 >/dev/null   # push tick + 92 => status time 8 (mid-fade)
shot ingame_status_lines

# --- 4. base built clear of the terminal -> secret hack panel ---------------
walk_to_x 2140 2100
send_actions 32768 0
cli step --frames 1 >/dev/null
send_actions 0 0
cli step --frames 1 >/dev/null
expect_msg "*enter your base*" "base door built"
step_to_mp 0
shot ingame_secret_overlay

# --- 5. rockets from the base inventory station ------------------------------
# keyactivate at the door -> WALKIN; nudge across the door cell until it takes.
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
cli click --label Rocket >/dev/null
cli step --frames 2 >/dev/null
expect_msg "*exit the menu*" "rocket purchased"
cli click --label Rocket >/dev/null || true   # 2nd purchase if credits allow
cli step --frames 2 >/dev/null
act 4 0 2       # moveleft closes the buy menu
for _ in $(seq 1 14); do
  send_actions 4 0; cli step --frames 16 >/dev/null
  send_actions 0 0; cli step --frames 1 >/dev/null
  case "$(msg_now)" in *playfield*|*data\ terminals*|*data\ port*) break;; esac
done
expect_msg "*data*" "left the base"

# --- 6. rocket in flight -> system camera inset ------------------------------
# Open deck, facing right (last stride right): the rocket gets ~600px of
# clear flight toward the bridge tower. The case-17 center message has
# time=255 (never dark) — pin its reveal at mp 230 (fully revealed, pulse
# deterministic) instead.
walk_to_x 2620 2580
send_actions 8 0; cli step --frames 2 >/dev/null
send_actions 0 0; cli step --frames 1 >/dev/null
act 0 4 2       # weapon 3 = rocket
step_to_mp 230
# The slot-0 frame (bank 95 idx 2, "SYSTEM") covers x0-140 y~300-420; its
# bright green label/divider rows are the reliable presence signal (the
# viewport itself is near-black at night).
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
    oc1="$(jget "$(cli world_state)" 's.objects_count')"
    echo "  fire attempt $attempt: objects $oc0 -> $oc1"
    [ "$oc1" -gt "$oc0" ] && break
  done
  send_actions 0 0
  cli step --frames 2 >/dev/null
  if inset_on; then fired=yes; break; fi
done
[ -n "$fired" ] || { echo "system camera inset never appeared" >&2; exit 1; }
cli step --frames 1 >/dev/null
shot ingame_system_camera

echo "extra in-game origin goldens captured to $OUT_DIR"
