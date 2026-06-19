#!/usr/bin/env bash
# Infinite chat (issue #299): the lobby chat is a VIRTUALIZED per-message list.
# Asserts, against the real binary:
#  1. With N messages sent, only a small WINDOW of message rows commits to the
#     retained tree (virtualization) — not all N.
#  2. The well is pinned to the bottom: the newest message is in view.
#  3. Scrolling up reaches old history (the oldest message becomes visible).
#  4. Variable-height rows (some messages wrap to multiple lines) position
#     without dropping panels or failing the frame.
set -euo pipefail
. "$(dirname "$0")/lib.sh"

LOBBY_BIN="$(lobby_bin)"
W=1920; H=1080
NMSG="${NMSG:-80}"

TMP=$(mktemp -d)
LOBBY_DB="$TMP/lobby.json"; SILENCER_HOME="$TMP/home"; mkdir -p "$SILENCER_HOME"
export SILENCER_LOBBYD_DIR="$TMP/lobbyd"; mkdir -p "$SILENCER_LOBBYD_DIR"
LOBBY_PORT=$(pick_port); PLAYER_AUTH_PORT=$(pick_port); MAP_API_PORT=$(pick_port); CTRL_PORT=$(pick_port)
SILENCER_VERSION="$(silencer_version)"
WORK="${WORK:-$TMP/work}"; mkdir -p "$WORK"
GAME_LOG="/tmp/silencer-e2e-$CTRL_PORT.log"

cleanup() {
  bun "$REPO_ROOT/clients/cli/index.ts" lobby kill --all >/dev/null 2>&1 || true
  [ -n "${SILENCER_PID:-}" ] && stop_silencer "$SILENCER_PID" "$CTRL_PORT" || true
  [ -n "${LOBBY_PID:-}" ] && { kill "$LOBBY_PID" 2>/dev/null || true; wait "$LOBBY_PID" 2>/dev/null || true; }
  rm -rf "$TMP"
}
trap cleanup EXIT

"$LOBBY_BIN" -addr ":$LOBBY_PORT" -db "$LOBBY_DB" -version "$SILENCER_VERSION" \
  -game-binary "$SILENCER_BIN" -maps-dir "$TMP/maps" \
  -player-auth-addr ":$PLAYER_AUTH_PORT" -map-api-addr ":$MAP_API_PORT" >"$TMP/lobby.log" 2>&1 &
LOBBY_PID=$!
for i in $(seq 1 60); do (echo > "/dev/tcp/127.0.0.1/$LOBBY_PORT") 2>/dev/null && break; sleep 0.25
  [ "$i" = 60 ] && { echo "lobby never came up" >&2; cat "$TMP/lobby.log" >&2; exit 1; }; done

HOME="$SILENCER_HOME" "$SILENCER_BIN" --headless --control-port "$CTRL_PORT" \
  --lobby-host 127.0.0.1 --lobby-port "$LOBBY_PORT" >"$GAME_LOG" 2>&1 &
SILENCER_PID=$!
wait_alive "$CTRL_PORT"

wait_for_widget() {
  for i in $(seq 1 200); do
    found=$(cli --port "$CTRL_PORT" inspect | LABEL="$1" bun -e \
      'const r=JSON.parse(await new Response(Bun.stdin.stream()).text());const l=process.env.LABEL;
       console.log((r.nodes||[]).some((w)=>w.label===l||w.control_id===l)?"yes":"no")' 2>/dev/null || echo no)
    [ "$found" = yes ] && return 0; sleep 0.05
  done; echo "widget '$1' never appeared" >&2; return 1
}
wait_for_lobby_state() {
  for i in $(seq 1 80); do
    ls=$(cli --port "$CTRL_PORT" state | bun -e 'console.log(JSON.parse(await new Response(Bun.stdin.stream()).text()).lobby_state||"")')
    [ "$ls" = "$1" ] && return 0; sleep 0.1
  done; return 1
}

# Min/max chat-row message number COMMITTED to the tree, and the committed count.
# Chat rows are text nodes with value "bob: m<N>[ ...]".
chat_rows() { # prints "<count> <min> <max>"
  cli --port "$CTRL_PORT" inspect | bun -e '
const r=JSON.parse(await new Response(Bun.stdin.stream()).text());
const ns=(r.nodes||[]).map(n=>n.value||"").map(v=>{const m=/bob:\s*m(\d+)/.exec(v);return m?+m[1]:null}).filter(x=>x!==null);
if(!ns.length){console.log("0 0 0");process.exit(0)}
console.log(`${ns.length} ${Math.min(...ns)} ${Math.max(...ns)}`)'
}

# --- drive into lobby ---
cli --port "$CTRL_PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
cli --port "$CTRL_PORT" resize --w "$W" --h "$H" >/dev/null
cli --port "$CTRL_PORT" click --label "Connect To Lobby" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000 >/dev/null
wait_for_widget "Username"
wait_for_lobby_state AUTHENTICATING 2>/dev/null || true
for ch in a l i c e; do cli --port "$CTRL_PORT" key --key "$ch" >/dev/null; done
cli --port "$CTRL_PORT" key --key tab >/dev/null
for ch in s e c r e t; do cli --port "$CTRL_PORT" key --key "$ch" >/dev/null; done
cli --port "$CTRL_PORT" click --label "Login/Create" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state CREATECHARACTER --timeout-ms 15000 >/dev/null
wait_for_widget "Create New Character"
cli --port "$CTRL_PORT" click --label "Create New Character" >/dev/null
wait_for_widget "Alias"
cli --port "$CTRL_PORT" set_text --label "Alias" --text "Alice" >/dev/null
cli --port "$CTRL_PORT" key --key enter >/dev/null
wait_for_widget "Noxis"
cli --port "$CTRL_PORT" click --label "Noxis" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBY --timeout-ms 15000 >/dev/null
wait_for_widget "Agents"

# --- spam a mix of short + wrapping messages (every 5th wraps) ---
bun "$REPO_ROOT/clients/cli/index.ts" lobby spawn --as bob --host 127.0.0.1 \
  --port "$LOBBY_PORT" --version "$SILENCER_VERSION" --user bob --pass bob >/dev/null
bun "$REPO_ROOT/clients/cli/index.ts" lobby join_channel --as bob --channel Lobby >/dev/null
for i in $(seq 1 "$NMSG"); do
  if [ $((i % 5)) -eq 0 ]; then
    txt="m$i the quick brown fox jumps over the lazy dog and keeps on running to wrap"
  else
    txt="m$i"
  fi
  bun "$REPO_ROOT/clients/cli/index.ts" lobby chat --as bob --channel Lobby --text "$txt" >/dev/null
done
cli --port "$CTRL_PORT" wait_frames --n 30 >/dev/null
cli --port "$CTRL_PORT" screenshot --out "$WORK/chat_bottom.png" >/dev/null

FAILS=$(grep -c "failed to update retained runtime" "$GAME_LOG" || true)
read -r CNT MIN MAX <<<"$(chat_rows)"
echo "bottom: committed rows=$CNT min=m$MIN max=m$MAX (sent $NMSG); frame failures=$FAILS"

FAIL=0
[ "$FAILS" -eq 0 ] || { echo "FAIL: $FAILS frame-transcribe failures"; FAIL=1; }
# Virtualization: far fewer rows committed than sent.
[ "$CNT" -gt 0 ] && [ "$CNT" -lt "$NMSG" ] || { echo "FAIL: committed rows=$CNT not a bounded window of $NMSG"; FAIL=1; }
# Stick-to-bottom: newest message in view.
[ "$MAX" -ge $((NMSG - 3)) ] || { echo "FAIL: newest (m$NMSG) not in view (max=m$MAX)"; FAIL=1; }

# --- scroll to the top, confirm oldest history is reachable ---
read -r LX LY <<<"$(cli --port "$CTRL_PORT" inspect | bun -e '
const r=JSON.parse(await new Response(Bun.stdin.stream()).text());
const l=(r.nodes||[]).find(n=>n.control_id==="ChatLog");
if(!l){console.error("no ChatLog viewport");process.exit(1)}
console.log(`${Math.round(l.x+l.w/2)} ${Math.round(l.y+l.h/2)}`)')"
for s in $(seq 1 40); do
  cli --port "$CTRL_PORT" scroll --x "$LX" --y "$LY" --dy 6 >/dev/null
  cli --port "$CTRL_PORT" wait_frames --n 1 >/dev/null
done
cli --port "$CTRL_PORT" wait_frames --n 5 >/dev/null
cli --port "$CTRL_PORT" screenshot --out "$WORK/chat_top.png" >/dev/null
read -r TCNT TMIN TMAX <<<"$(chat_rows)"
echo "top:    committed rows=$TCNT min=m$TMIN max=m$TMAX"
# After scrolling up, the oldest messages are reachable (window moved to the head).
[ "$TMIN" -le 3 ] || { echo "FAIL: scroll-up did not reach old history (min=m$TMIN)"; FAIL=1; }

echo "screens: $WORK/chat_bottom.png  $WORK/chat_top.png"
[ "$FAIL" -eq 0 ] || { echo "FAIL 93_lobby_chat_virtualization"; exit 1; }
echo "PASS 93_lobby_chat_virtualization"
