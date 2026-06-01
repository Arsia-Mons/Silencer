#!/usr/bin/env bash
# Enhanced visual regression (lobby surfaces) with Discord notifications.
# Similar to 71_visual_regression_lobby.sh but sends failure composites to Discord.
set -euo pipefail
. "$(dirname "$0")/lib.sh"

SCRIPT_DIR="$(dirname "$0")"
LOBBY_BIN="$(lobby_bin)"
PIXDIFF="$REPO_ROOT/tools/pixdiff/build/pixdiff"
GOLDEN_DIR="$(dirname "$0")/golden"
DISCORD_SKILL="$HOME/.claude/skills/discord-dm/send.ts"
W=960; H=720
BLESS="${BLESS:-0}"
[ -x "$PIXDIFF" ] || { echo "pixdiff not built: $PIXDIFF" >&2; exit 1; }
mkdir -p "$GOLDEN_DIR"

TMP=$(mktemp -d)
COMPOSITE_DIR="$(mktemp -d)"
LOBBY_DB="$TMP/lobby.json"; SILENCER_HOME="$TMP/home"; mkdir -p "$SILENCER_HOME"
LOBBY_PORT=$(pick_port); PLAYER_AUTH_PORT=$(pick_port); MAP_API_PORT=$(pick_port); CTRL_PORT=$(pick_port)
SILENCER_VERSION="$(silencer_version)"
WORK="$TMP/work"; mkdir -p "$WORK"
FAIL=0
REGRESSIONS=()

cleanup() {
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
  --lobby-host 127.0.0.1 --lobby-port "$LOBBY_PORT" >"/tmp/silencer-e2e-$CTRL_PORT.log" 2>&1 &
SILENCER_PID=$!
wait_alive "$CTRL_PORT"

wait_for_widget() {
  for i in $(seq 1 100); do
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
  done; echo "lobby_state never became $1 (last=$ls)" >&2; return 1
}

cap() {
  local name="$1" th="${2:-0.40}"
  cli --port "$CTRL_PORT" wait_frames --n 3 >/dev/null
  local png="$WORK/$name.png" golden="$GOLDEN_DIR/$name.png"
  cli --port "$CTRL_PORT" screenshot --out "$png" >/dev/null
  if [ "$BLESS" = "1" ]; then cp "$png" "$golden"; return 0; fi
  if [ ! -f "$golden" ]; then echo "  MISSING golden: $name.png" >&2; FAIL=$((FAIL+1)); return 0; fi
  local pct; pct="$("$PIXDIFF" "$png" "$golden" 2>/dev/null || echo 100.0)"
  local over; over="$(VR_PCT="$pct" VR_TH="$th" bun -e 'console.log(Number(process.env.VR_PCT)>Number(process.env.VR_TH)?"1":"0")')"
  if [ "$over" = 1 ]; then
    echo "  REGRESSED $name: ${pct}% > ${th}%" >&2
    REGRESSIONS+=("$name")
    FAIL=$((FAIL+1))
  fi
}

cli --port "$CTRL_PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
cli --port "$CTRL_PORT" resize --w "$W" --h "$H" >/dev/null
cli --port "$CTRL_PORT" click --label "Play Online" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000 >/dev/null
wait_for_widget "Username"
cap lobby_connect 2.5

for ch in a l i c e; do cli --port "$CTRL_PORT" key --key "$ch" >/dev/null; done
cli --port "$CTRL_PORT" key --key tab >/dev/null
for ch in s e c r e t; do cli --port "$CTRL_PORT" key --key "$ch" >/dev/null; done
wait_for_lobby_state AUTHENTICATING
cli --port "$CTRL_PORT" click --label "Login/Create" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state CREATECHARACTER --timeout-ms 15000 >/dev/null
wait_for_widget "Create New Character"
cap character_create 0.40

cli --port "$CTRL_PORT" click --label "Create New Character" >/dev/null
wait_for_widget "Alias"
cli --port "$CTRL_PORT" set_text --label "Alias" --text "Alice" >/dev/null
cli --port "$CTRL_PORT" click --label "AliasConfirm" >/dev/null
wait_for_widget "Black Rose"
cli --port "$CTRL_PORT" click --label "Noxis" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBY --timeout-ms 15000 >/dev/null
wait_for_widget "Send"
cap lobby_screen 1.0

# Handle results
if [ "$BLESS" = "1" ]; then
  echo "PASS 71_visual_regression_lobby (blessed)"
  rm -rf "$COMPOSITE_DIR"
  exit 0
fi

if [ "$FAIL" -eq 0 ]; then
  echo "PASS 71_visual_regression_lobby"
  rm -rf "$COMPOSITE_DIR"
  exit 0
fi

# Regressions detected - create composites and send to Discord
echo "⚠️  $FAIL visual regression(s) detected in lobby tests"
echo "📸 Creating side-by-side composites..."

composite_count=0
for regname in "${REGRESSIONS[@]}"; do
  regfile="$GOLDEN_DIR/$regname.png"
  if [ ! -f "$regfile" ]; then
    continue
  fi

  origin_file="$COMPOSITE_DIR/origin_$regname.png"
  if git show origin/main:tests/cli-agent/e2e/golden/"$regname.png" > "$origin_file" 2>/dev/null; then
    composite_file="$COMPOSITE_DIR/$regname.comparison.png"
    if bun "$REPO_ROOT/tools/compose-images.ts" \
      "$origin_file" \
      "$regfile" \
      "$composite_file" \
      "origin/main" \
      "current" 2>/dev/null; then
      composite_count=$((composite_count+1))
      echo "  ✓ $regname"
    fi
  fi
done

# Send composites to Discord if skill is available
if [ "$composite_count" -gt 0 ] && [ -f "$DISCORD_SKILL" ]; then
  echo "💬 Sending ${composite_count} comparison image(s) to Discord..."
  composites=()
  for f in "$COMPOSITE_DIR"/*.comparison.png; do
    [ -f "$f" ] && composites+=("$f")
  done

  if [ ${#composites[@]} -gt 0 ]; then
    msg="🔴 **Lobby Visual Regression Alert**
$FAIL regression(s) detected in lobby visual regression tests.
Compare the images below (left: origin/main, right: current)"

    bun "$DISCORD_SKILL" "$msg" "${composites[@]}" 2>/dev/null || true
  fi
fi

rm -rf "$COMPOSITE_DIR"
echo "71_visual_regression_lobby: $FAIL visual diff(s)" >&2
exit 1
