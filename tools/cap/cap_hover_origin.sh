#!/usr/bin/env bash
# cap_hover_origin.sh — origin/main hover-state goldens (1920×1080).
#
# 1) hover_mainmenu_oval.png — the mainmenu's FIRST oval button hovered and
#    settled at the legacy ramp's steady state (origin button.cpp: hover
#    targets phase 4 — sprite base+4 at brightness 136, label at 136; one
#    phase per 42ms frame, so ≥5 frames settles it). The logo reveal is
#    wall-clock driven: the capture polls for two consecutive byte-identical
#    screenshots, which only happens inside the ~5s logo HOLD window with the
#    hover ramp settled — self-stabilizing, no fade patch needed.
# 2) /tmp/hover_lobby_connect_origin.png — the lobby_connect Login (Chrome)
#    button hovered: origin Chrome buttons change NOTHING on hover/focus
#    (button.cpp SpriteIndexForFrame idx24 every phase, brightness pinned
#    128), so outside the focus-driven caret this must equal the rest-state
#    lobby_connect golden. Measured + documented in ORIGIN_GOLDENS.md rather
#    than stored (the rest golden IS the chrome-hover golden).
#
# Origin binary: .worktrees/origin-capture build (fade patch state
# irrelevant — both captures settle past the fade; the GSO capture patch
# does not affect menus).
#
# Usage: tools/cap/cap_hover_origin.sh [OUT_DIR]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
ORIGIN_ROOT="${ORIGIN_ROOT:-$(cd "$REPO_ROOT/.." && pwd)/origin-capture}"
OUT_DIR="${1:-/tmp}"
BIN="$ORIGIN_ROOT/clients/silencer/build/Silencer.app/Contents/MacOS/Silencer"
LOBBY_BIN="$REPO_ROOT/services/lobby/lobby"
[ -x "$BIN" ] || { echo "origin binary missing: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

PORT=$((30000 + RANDOM % 20000))
LOBBY_PORT=$((PORT + 1)); AUTH_PORT=$((PORT + 2)); MAPAPI_PORT=$((PORT + 3))
TMP=$(mktemp -d)
mkdir -p "$TMP/home"

CLIENT_PID=""; LOBBY_PID=""
cleanup() {
  [ -n "$CLIENT_PID" ] && kill "$CLIENT_PID" 2>/dev/null || true
  [ -n "$LOBBY_PID" ] && kill "$LOBBY_PID" 2>/dev/null || true
  wait 2>/dev/null || true
  rm -rf "$TMP"
}
trap cleanup EXIT

cli() { bun "$ORIGIN_ROOT/clients/cli/index.ts" --port "$PORT" "$@"; }

# stable_shot <out>: capture pairs until two consecutive frames are
# byte-identical (logo hold + ramp settled + fade done).
stable_shot() {
  local out="$1"
  cli screenshot --out "$TMP/a.png" >/dev/null
  for _ in $(seq 1 40); do
    sleep 0.25
    cli screenshot --out "$TMP/b.png" >/dev/null
    if cmp -s "$TMP/a.png" "$TMP/b.png"; then
      cp "$TMP/b.png" "$out"
      return 0
    fi
    mv "$TMP/b.png" "$TMP/a.png"
  done
  echo "frame never stabilized for $out" >&2
  return 1
}

# ---- 1. mainmenu first oval hovered ----------------------------------------
HOME="$TMP/home" "$BIN" --headless --control-port "$PORT" >"$TMP/client.log" 2>&1 &
CLIENT_PID=$!
for _ in $(seq 1 60); do cli ping >/dev/null 2>&1 && break; sleep 0.5; done
cli wait_for_state --state MAINMENU --timeout-ms 20000 >/dev/null
cli resize --w 1920 --h 1080 >/dev/null
sleep 1
FIRST=$(cli inspect | bun -e '
  const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
  const btns = (r.widgets || []).filter((w) => w.kind === "button");
  if (!btns.length) { console.error("no buttons"); process.exit(1); }
  const b = btns.reduce((a, c) => (c.y < a.y ? c : a));
  console.log(JSON.stringify({label: b.label, x: Math.round(b.x + b.w / 2), y: Math.round(b.y + b.h / 2)}))')
echo "hovering first mainmenu button: $FIRST"
HX=$(echo "$FIRST" | bun -e 'console.log(JSON.parse(await new Response(Bun.stdin.stream()).text()).x)')
HY=$(echo "$FIRST" | bun -e 'console.log(JSON.parse(await new Response(Bun.stdin.stream()).text()).y)')
cli hover_at --x "$HX" --y "$HY" >/dev/null
sleep 1  # ramp settles in 5 legacy frames (~210ms); generous
stable_shot "$OUT_DIR/hover_mainmenu_oval.png"
echo "captured $OUT_DIR/hover_mainmenu_oval.png"
cli quit >/dev/null 2>&1 || true
sleep 0.3; kill "$CLIENT_PID" 2>/dev/null || true; wait "$CLIENT_PID" 2>/dev/null || true
CLIENT_PID=""

# ---- 2. lobby_connect Login (Chrome) hovered — negative check --------------
[ -x "$LOBBY_BIN" ] || { echo "lobby binary missing — skipping chrome-hover check" >&2; exit 0; }
"$LOBBY_BIN" -addr ":$LOBBY_PORT" -db "$TMP/lobby.json" -version 00058 \
  -game-binary "$BIN" -maps-dir "$TMP" \
  -player-auth-addr ":$AUTH_PORT" -map-api-addr ":$MAPAPI_PORT" >"$TMP/lobby.log" 2>&1 &
LOBBY_PID=$!
for i in $(seq 1 60); do (echo > "/dev/tcp/127.0.0.1/$LOBBY_PORT") 2>/dev/null && break; sleep 0.25
  [ "$i" = 60 ] && { echo "lobby never came up" >&2; exit 1; }; done

# The rest-state lobby_connect golden pinned port 63532 (the connect log
# renders it). The hover comparison masks the log region anyway, but keep the
# same flow: connect → AUTHENTICATING → hover Login → settle → capture.
HOME="$TMP/home" "$BIN" --headless --control-port "$PORT" \
  --lobby-host 127.0.0.1 --lobby-port "$LOBBY_PORT" >"$TMP/client2.log" 2>&1 &
CLIENT_PID=$!
for _ in $(seq 1 60); do cli ping >/dev/null 2>&1 && break; sleep 0.5; done
cli wait_for_state --state MAINMENU --timeout-ms 20000 >/dev/null
cli resize --w 1920 --h 1080 >/dev/null
cli click --label "Connect To Lobby" >/dev/null
cli wait_for_state --state LOBBYCONNECT --timeout-ms 5000 >/dev/null
for _ in $(seq 1 80); do
  ls=$(cli state | bun -e 'console.log(JSON.parse(await new Response(Bun.stdin.stream()).text()).lobby_state||"")')
  [ "$ls" = "AUTHENTICATING" ] && break; sleep 0.1
done
LOGIN=$(cli inspect | bun -e '
  const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
  const b = (r.widgets || []).find((w) => (w.label || "").startsWith("Login"));
  if (!b) { console.error("no Login button"); process.exit(1); }
  console.log(JSON.stringify({x: Math.round(b.x + b.w / 2), y: Math.round(b.y + b.h / 2), bx: b.x, by: b.y, bw: b.w, bh: b.h}))')
echo "hovering Login: $LOGIN"
HX=$(echo "$LOGIN" | bun -e 'console.log(JSON.parse(await new Response(Bun.stdin.stream()).text()).x)')
HY=$(echo "$LOGIN" | bun -e 'console.log(JSON.parse(await new Response(Bun.stdin.stream()).text()).y)')
cli hover_at --x "$HX" --y "$HY" >/dev/null
sleep 1
stable_shot "$OUT_DIR/hover_lobby_connect_origin.png"
echo "captured $OUT_DIR/hover_lobby_connect_origin.png (compare to the rest golden)"
