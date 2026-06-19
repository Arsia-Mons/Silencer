# ingame_drive_lib.sh — shared TUI gameplay-drive helpers for the in-game
# golden captures (sourced by cap_ingame_{origin,cppx}_extra.sh).
#
# Caller provides: BIN, CLI_DIR, PORT (control), FRAME_PORT, INPUT_PORT,
# OUT_DIR. Sets PID/SINK_PID (caller's cleanup kills them).
#
# Determinism: the sim is paused the whole time; every input lands on an
# explicit `step` tick, and every feedback loop converges on deterministic
# world state (message text / message_progress / player x / objects_count),
# so two independent runs replay the identical tick-for-tick world.

cli() { bun "$CLI_DIR/index.ts" --port "$PORT" "$@"; }

jget() { bun -e "const s=(JSON.parse(process.argv[1]).result)??JSON.parse(process.argv[1]);console.log($2)" "$1"; }

boot_tui() {
  # Throwaway frame sink: TUIBackend connects at init and streams every frame.
  bun -e '
    Bun.listen({ hostname: "127.0.0.1", port: Number(process.argv[1]), socket: { data() {} } });
    setInterval(() => {}, 60_000);
  ' "$FRAME_PORT" &
  SINK_PID=$!
  sleep 0.5
  SILENCER_TUI_FRAME_HOST=127.0.0.1 SILENCER_TUI_FRAME_PORT="$FRAME_PORT" \
    "$BIN" --headless --tui --tui-input-port "$INPUT_PORT" --control-port "$PORT" \
    >"/tmp/silencer-ingame-extra-$PORT.log" 2>&1 &
  PID=$!
}

# send_actions <u32 keymask> [weaponbyte] — one ACTION snapshot over the TUI
# input port (latest-wins, cached until replaced). Bit layout =
# net/inputserver.cpp (keyuse 1<<15, keyfire 1<<16, keyactivate 1<<14, ...).
send_actions() {
  bun -e '
    const [port, mask, wb] = process.argv.slice(1).map(Number);
    const frame = new Uint8Array(1 + 3 + 12);
    frame[0] = 1; frame[1] = 1; frame[2] = 12; frame[3] = 0;
    frame[4] = mask & 0xff; frame[5] = (mask >> 8) & 0xff;
    frame[6] = (mask >> 16) & 0xff; frame[7] = (mask >> 24) & 0xff;
    frame[8] = wb || 0;
    const sock = await Bun.connect({ hostname: "127.0.0.1", port,
      socket: { data() {}, error(_s, e) { console.error(e); process.exit(1); } } });
    sock.write(frame);
    await sock.flush?.();
    setTimeout(() => { sock.end(); process.exit(0); }, 150);
  ' "$INPUT_PORT" "$1" "${2:-0}"
}

# send_scancodes <byte_index> <byte_value> — scancode bitmask snapshot
# (ESC=41 -> byte 5 bit 1; F4=61 -> byte 7 bit 5).
send_scancodes() {
  bun -e '
    const [port, bi, bv] = process.argv.slice(1).map(Number);
    const mask = new Uint8Array(64);
    mask[bi] = bv;
    const frame = new Uint8Array(1 + 3 + 64);
    frame[0] = 1; frame[1] = 2; frame[2] = 64; frame[3] = 0;
    frame.set(mask, 4);
    const sock = await Bun.connect({ hostname: "127.0.0.1", port,
      socket: { data() {}, error(_s, e) { console.error(e); process.exit(1); } } });
    sock.write(frame);
    await sock.flush?.();
    setTimeout(() => { sock.end(); process.exit(0); }, 150);
  ' "$INPUT_PORT" "$1" "$2"
}

# act <mask> [weaponbyte] [hold_ticks] — hold an action mask, then release.
act() {
  local mask="$1" wb="${2:-0}" steps="${3:-1}"
  send_actions "$mask" "$wb"
  cli step --frames "$steps" >/dev/null
  send_actions 0 0
  cli step --frames 1 >/dev/null
}

mp_now() { jget "$(cli world_state)" 's.message_progress'; }
msg_now() { jget "$(cli world_state)" 's.message_text||""'; }
px_now() { jget "$(cli world_state)" 's.players[0].x'; }

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

expect_msg() { # expect_msg <glob> <label>
  local m
  m="$(msg_now)"
  case "$m" in
    $1) return 0 ;;
    *) echo "expected $2 (msg glob $1), got: $m" >&2; return 1 ;;
  esac
}

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
  local found=""
  for _ in $(seq 1 600); do
    ws="$(cli world_state)"
    if [ "$(jget "$ws" '(s.message_text||"").startsWith("Move your agent")&&s.message_progress>=1')" = "true" ]; then
      found="yes"; break
    fi
    cli step --frames 1 >/dev/null
  done
  [ -n "$found" ] || { echo "tutorial message never appeared" >&2; return 1; }
  [ "$(jget "$ws" 's.players[0].x===2784&&s.players[0].y===1350')" = "true" ] \
    || { echo "unexpected spawn: $(jget "$ws" 'JSON.stringify(s.players[0])') (goldens assume 2784,1350)" >&2; return 1; }
  echo "anchor ok (mp=$(jget "$ws" 's.message_progress'))"
}

# run_tutorial_to_basedoor: cases 0-8 (movement curriculum) + case 9 grants
# INV_BASEDOOR. Each step retries its action until the next case's message
# text appears — the retry counts re-converge identically across runs.
run_tutorial_to_basedoor() {
  local m
  tut_step() { # tut_step <glob-of-next-msg> <label> <action-fn>
    local glob="$1" label="$2" fn="$3"
    for _ in $(seq 1 12); do
      m="$(msg_now)"
      case "$m" in $glob) echo "tutorial: $label done"; return 0;; esac
      "$fn"
      cli step --frames 6 >/dev/null
    done
    echo "tutorial step '$label' never completed (msg: $(msg_now))" >&2
    return 1
  }
  a_run()      { act 8 0 4; }
  a_jump()     { act 4096 0 3; cli step --frames 12 >/dev/null; }
  a_jetpack()  { act 8192 0 8; cli step --frames 24 >/dev/null; }
  a_crouch()   { act 2 0 8; }
  a_roll()     { send_actions 2 0; cli step --frames 10 >/dev/null;
                 send_actions 10 0; cli step --frames 4 >/dev/null;
                 send_actions 0 0; cli step --frames 10 >/dev/null; }
  a_disguise() { act 131072 0 3; cli step --frames 8 >/dev/null; }
  a_fire()     { act 65536 0 3; cli step --frames 8 >/dev/null; }
  a_weapon2()  { act 0 2 3; cli step --frames 8 >/dev/null; }
  tut_step "*jump*"          "run"        a_run
  tut_step "*jet-pack*"      "jump"       a_jump
  tut_step "*kneel*"         "jetpack"    a_jetpack
  tut_step "*roll*"          "crouch"     a_crouch
  tut_step "*disguise*"      "roll"       a_roll
  tut_step "*return\ to\ normal*" "disguise on"  a_disguise
  tut_step "*fires\ your\ current*" "disguise off" a_disguise
  tut_step "*change\ weapons*" "fire"     a_fire
  # ("|" alternation doesn't survive variable expansion in case patterns)
  tut_step "*base*" "switch weapon" a_weapon2
  # case 9 -> 10 is automatic (grants the base-door item).
  for _ in $(seq 1 6); do
    case "$(msg_now)" in *build\ your\ base*) break;; esac
    cli step --frames 32 >/dev/null
  done
}

# walk_to_x <upper> <lower>: walk left/right until player x is inside
# [lower, upper] (coarse 8-tick strides, then 2-tick fine steps).
walk_to_x() {
  local upper="$1" lower="$2" x
  for _ in $(seq 1 80); do
    x="$(px_now)"
    if [ "$x" -le "$upper" ] && [ "$x" -ge "$lower" ]; then return 0; fi
    local mask=4 # left
    [ "$x" -lt "$lower" ] && mask=8
    local dist=$(( x - upper )); [ "$mask" = 8 ] && dist=$(( lower - x ))
    local ticks=8
    [ "$dist" -lt 120 ] && ticks=2
    send_actions "$mask" 0; cli step --frames "$ticks" >/dev/null
    send_actions 0 0; cli step --frames 1 >/dev/null
  done
  echo "walk_to_x [$lower,$upper] never converged (x=$(px_now))" >&2
  return 1
}

# probe_ink <label> <x0> <y0> <x1> <y1>: assert the band has lit pixels.
probe_ink() {
  local label="$1"
  cli screenshot --out "/tmp/ink_probe_$PORT.png" >/dev/null
  python3 - "$PORT" "$2" "$3" "$4" "$5" "$label" <<'PY'
import sys
from PIL import Image
import numpy as np
port, x0, y0, x1, y1 = sys.argv[1], *map(int, sys.argv[2:6])
a = np.array(Image.open(f"/tmp/ink_probe_{port}.png").convert("RGB"), dtype=int)
band = a[y0:y1, x0:x1]
if (band.sum(axis=2) > 90).sum() < 40:
    print(f"{sys.argv[6]} not visible in x{x0}-{x1} y{y0}-{y1}", file=sys.stderr)
    sys.exit(1)
PY
}

shot() { # shot <name> — 640x480 screenshot into OUT_DIR
  cli screenshot --out "$OUT_DIR/$1.png" >/dev/null
  bun -e '
    const b = new DataView(await Bun.file(process.argv[1]).arrayBuffer());
    if (b.getUint32(16) !== 640 || b.getUint32(20) !== 480) { console.error("not 640x480"); process.exit(1); }
  ' "$OUT_DIR/$1.png"
  echo "captured $1"
}
