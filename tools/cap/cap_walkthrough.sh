#!/usr/bin/env bash
# Full e2e video walkthrough: boot → menus → options tour → lobby login →
# character wizard → lobby → create game → staging → tech select, then a
# second in-game segment (tutorial: HUD, chat, player list, buy/tech, quit).
# A background sampler polls screenshots (~10fps) while the journey runs
# semantically (per shared/skills/cli/SKILL.md "Capture sampled E2E videos").
# Frames normalize to 1920x1080 (in-game 640x480 stretches non-uniformly =
# origin's present behavior) and encode to one mp4.
set -euo pipefail
cd "$(dirname "$0")/../.."   # repo root (script lives in tools/cap when installed; adjust if run from /tmp)
[ -f tests/cli-agent/e2e/lib.sh ] || cd /Users/hv/repos/Silencer/.worktrees/cppx-migration-cc
. tests/cli-agent/e2e/lib.sh

W=1920 H=1080
OUT=${OUT:-/tmp/silencer_walkthrough.mp4}
WORK=$(mktemp -d /tmp/walkthrough.XXXXXX)
FRAMES="$WORK/frames"; mkdir -p "$FRAMES"
STOP="$WORK/stop"

LOBBY_BIN="$(lobby_bin)"
SILENCER_VERSION="$(silencer_version)"
LOBBY_PORT=$(pick_port); PLAYER_AUTH_PORT=$(pick_port); MAP_API_PORT=$(pick_port)
CTRL_PORT=$(pick_port)
SILENCER_HOME="$WORK/home"; mkdir -p "$SILENCER_HOME"

cleanup() {
  touch "$STOP" 2>/dev/null || true
  [ -n "${SAMPLER_PID:-}" ] && wait "$SAMPLER_PID" 2>/dev/null || true
  [ -n "${SILENCER_PID:-}" ] && stop_silencer "$SILENCER_PID" "$CTRL_PORT" 2>/dev/null || true
  [ -n "${LOBBY_PID:-}" ] && { kill "$LOBBY_PID" 2>/dev/null || true; wait "$LOBBY_PID" 2>/dev/null || true; }
}
trap cleanup EXIT

c() { cli --port "$CTRL_PORT" "$@"; }
type_keys() { local s="$1"; local i; for ((i=0;i<${#s};i++)); do c key --key "${s:$i:1}" >/dev/null; sleep 0.08; done; }
wait_lobby_state() {
  local target="$1"; local ls=""
  for i in $(seq 1 80); do
    ls=$(c state | bun -e 'console.log(JSON.parse(await new Response(Bun.stdin.stream()).text()).lobby_state||"");')
    [ "$ls" = "$target" ] && return 0; sleep 0.1
  done
  echo "lobby_state never $target (last=$ls)" >&2; return 1
}
click_if() { c click --label "$1" >/dev/null 2>&1 || echo "  (skip: no widget '$1')" >&2; }

start_sampler() {
  rm -f "$STOP"
  ( i="$1"
    while [ ! -f "$STOP" ]; do
      out=$(printf "%s/%06d.png" "$FRAMES" "$i"); tmp="$out.tmp"
      if c screenshot --out "$tmp" >/dev/null 2>&1 && [ -s "$tmp" ]; then mv "$tmp" "$out"; i=$((i+1)); else rm -f "$tmp"; fi
      sleep 0.1
    done ) &
  SAMPLER_PID=$!
}
stop_sampler() { touch "$STOP"; wait "$SAMPLER_PID" 2>/dev/null || true; SAMPLER_PID=""; }

# ---------- lobby server ----------
"$LOBBY_BIN" -addr ":$LOBBY_PORT" -db "$WORK/lobby.json" -version "$SILENCER_VERSION" \
  -game-binary "$SILENCER_BIN" -maps-dir "$WORK/maps" \
  -player-auth-addr ":$PLAYER_AUTH_PORT" -map-api-addr ":$MAP_API_PORT" \
  >"$WORK/lobby.log" 2>&1 &
LOBBY_PID=$!
for i in $(seq 1 60); do (echo > "/dev/tcp/127.0.0.1/$LOBBY_PORT") 2>/dev/null && break; sleep 0.25; done

# ---------- SEGMENT 1: menus + lobby flow @1920x1080 ----------
HOME="$SILENCER_HOME" "$SILENCER_BIN" --headless --control-port "$CTRL_PORT" \
  --lobby-host 127.0.0.1 --lobby-port "$LOBBY_PORT" \
  >"/tmp/silencer-walkthrough-$CTRL_PORT.log" 2>&1 &
SILENCER_PID=$!
wait_alive "$CTRL_PORT"
c resize --w $W --h $H >/dev/null
start_sampler 1

c wait_for_state --state MAINMENU --timeout-ms 20000 >/dev/null
sleep 3.5                                   # logo HOLD + fade

echo "== options tour"
click_if "Options";        sleep 1.5
click_if "Audio";          sleep 1.5
click_if "Cancel";         sleep 0.8
click_if "Display";        sleep 1.5
click_if "Cancel";         sleep 0.8
click_if "Controls";       sleep 2.0
click_if "Cancel";         sleep 0.8
click_if "Go Back";        sleep 1.2     # close the Options overlay

echo "== lobby login"
c click --label "Connect To Lobby" >/dev/null
c wait_for_state --state LOBBYCONNECT --timeout-ms 8000 >/dev/null
sleep 1.0
type_keys "alice"
c key --key tab >/dev/null; sleep 0.3
type_keys "secret"
wait_lobby_state AUTHENTICATING
sleep 0.6
c click --label "Login/Create" >/dev/null

echo "== character wizard"
c wait_for_state --state CREATECHARACTER --timeout-ms 15000 >/dev/null
sleep 1.5
click_if "Create New Character"; sleep 1.2
type_keys "Alice"; sleep 0.6                 # alias dialog (Enter submits)
c key --key enter >/dev/null; sleep 1.8      # agency select step
click_if "Noxis"; sleep 1.0

echo "== lobby + create game"
c wait_for_state --state LOBBY --timeout-ms 15000 >/dev/null
sleep 2.5
click_if "NewGame"; sleep 2.0
click_if "STAR72.SIL"; sleep 1.2
click_if "CreateGame"; sleep 1.0             # dispatch → dedicated spawn → staging
for i in $(seq 1 60); do
  if c inspect 2>/dev/null | grep -q '"Ready"'; then break; fi
  sleep 0.5
done
sleep 2.5

echo "== tech select"
click_if "ChooseTech"; sleep 2.5
click_if "BackToTeams"; sleep 2.0            # back to staging
sleep 1.0
stop_sampler
SEG1_LAST=$(ls "$FRAMES" | tail -1 | sed 's/\.png//')
stop_silencer "$SILENCER_PID" "$CTRL_PORT"; SILENCER_PID=""

# ---------- SEGMENT 2: in-game via tutorial @640x480 ----------
CTRL_PORT=$(pick_port)
HOME="$SILENCER_HOME" "$SILENCER_BIN" --headless --control-port "$CTRL_PORT" \
  >"/tmp/silencer-walkthrough2-$CTRL_PORT.log" 2>&1 &
SILENCER_PID=$!
wait_alive "$CTRL_PORT"
start_sampler $((10#$SEG1_LAST + 1))

c wait_for_state --state MAINMENU --timeout-ms 20000 >/dev/null
sleep 1.0
c click --label Tutorial >/dev/null
c wait_for_state --state SINGLEPLAYERGAME --timeout-ms 20000 >/dev/null
sleep 6                                      # spawn + tutorial messages, real-time

echo "== chat"
c key --key t >/dev/null; sleep 0.8
type_keys "hello from the walkthrough"
sleep 0.5
c key --key enter >/dev/null; sleep 2.0

echo "== player list"
c ingame_ui_mode --mode playerlist >/dev/null 2>&1 || c key --key f1 >/dev/null 2>&1 || true
sleep 2.0
c ingame_ui_mode --mode none >/dev/null 2>&1 || true; sleep 0.6

echo "== buy/tech overlay"
c ingame_ui_mode --mode buy >/dev/null 2>&1 || true
sleep 2.5
c ingame_ui_mode --mode none >/dev/null 2>&1 || true; sleep 0.6

echo "== quit prompt"
c key --key escape >/dev/null 2>&1 || true
sleep 2.0
stop_sampler
stop_silencer "$SILENCER_PID" "$CTRL_PORT"; SILENCER_PID=""

# ---------- normalize + encode ----------
echo "== encoding $(ls "$FRAMES" | wc -l | tr -d ' ') frames"
python3 - "$FRAMES" $W $H <<'EOF'
import sys, os
from PIL import Image
d, W, H = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
for f in sorted(os.listdir(d)):
    if not f.endswith('.png'): continue
    p = os.path.join(d, f)
    im = Image.open(p)
    if im.size != (W, H):
        im = im.resize((W, H), Image.NEAREST)   # origin's present = non-uniform NEAREST stretch
        im.save(p)
EOF
ffmpeg -y -framerate 10 -pattern_type glob -i "$FRAMES/*.png" \
  -vf "format=yuv420p" -c:v libx264 -preset veryfast -crf 21 -movflags +faststart \
  "$OUT" 2>"$WORK/ffmpeg.log" || { tail -5 "$WORK/ffmpeg.log"; exit 1; }
echo "walkthrough video -> $OUT ($(du -h "$OUT" | cut -f1))"
