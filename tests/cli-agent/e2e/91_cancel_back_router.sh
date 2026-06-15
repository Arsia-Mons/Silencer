#!/usr/bin/env bash
# UI-layer cancel/back router (use_cancel) + PauseScreen, end to end through the
# control socket. Proves the converged cancel edge: keyboard ESC (the `key
# --key escape` op → InjectKeyOp) and the `back` op (Game::GoBack injects the
# SAME cancel_pressed edge a controller "back" sets) both route through the one
# UI-layer router (ClientUi::end_layout), not a game-layer GoToState.
#
# Covers:
#   1. Overlay (Options over MainMenu): ESC pops it; `back` pops it.
#   2. Lobby phase screen (the reported bug): ESC / `back` returns toward the
#      main menu (use_cancel(on_go_back) -> leave_to_menu).
#   3. In match: ESC closes chat then buy/tech first (use_ingame_chat().cancel /
#      use_tech().close); with nothing open ESC pushes PauseScreen; ESC again
#      pops it; the PauseScreen primary action ("Hit Enter To Quit") leaves the
#      match (leave_match -> origin CheckForQuit outcome) and dismisses itself.
#
# A physical gamepad can't be driven headless. The `back` op injects the exact
# cancel edge the gamepad UiCancel binding sets in events.cpp, so the router is
# covered; the gamepad button->edge mapping is covered by reasoning, not a pad.
set -euo pipefail
. "$(dirname "$0")/lib.sh"

LOBBY_BIN="$(lobby_bin)"

TMP=$(mktemp -d)
SILENCER_HOME="$TMP/home"
mkdir -p "$SILENCER_HOME"

LOBBY_PORT=$(pick_port)
PLAYER_AUTH_PORT=$(pick_port)
MAP_API_PORT=$(pick_port)
PORT=$(pick_port)
SILENCER_VERSION="$(silencer_version)"

cleanup() {
  [ -n "${SILENCER_PID:-}" ] && stop_silencer "$SILENCER_PID" "$PORT" || true
  [ -n "${LOBBY_PID:-}" ] && { kill "$LOBBY_PID" 2>/dev/null || true; wait "$LOBBY_PID" 2>/dev/null || true; }
  rm -rf "$TMP"
}
trap cleanup EXIT

"$LOBBY_BIN" \
  -addr ":$LOBBY_PORT" \
  -db "$TMP/lobby.json" \
  -version "$SILENCER_VERSION" \
  -game-binary "$SILENCER_BIN" \
  -maps-dir "$TMP/maps" \
  -player-auth-addr ":$PLAYER_AUTH_PORT" \
  -map-api-addr ":$MAP_API_PORT" \
  >"$TMP/lobby.log" 2>&1 &
LOBBY_PID=$!
for i in $(seq 1 60); do
  (echo > "/dev/tcp/127.0.0.1/$LOBBY_PORT") 2>/dev/null && break
  sleep 0.25
  [ "$i" = 60 ] && { echo "lobby never came up" >&2; cat "$TMP/lobby.log" >&2; exit 1; }
done

HOME="$SILENCER_HOME" "$SILENCER_BIN" \
  --headless \
  --control-port "$PORT" \
  --lobby-host 127.0.0.1 \
  --lobby-port "$LOBBY_PORT" \
  >"/tmp/silencer-e2e-$PORT.log" 2>&1 &
SILENCER_PID=$!
wait_alive "$PORT"

# --- helpers ----------------------------------------------------------------
buttons() {
  cli --port "$PORT" inspect 2>/dev/null | bun -e \
    'const s=JSON.parse(await new Response(Bun.stdin.stream()).text());const r=s.result??s;
     console.log((r.nodes||[]).filter(n=>n.role==="button").map(n=>n.label).join(","));'
}
has_button() { # has_button <label>
  cli --port "$PORT" inspect 2>/dev/null | LABEL="$1" bun -e \
    'const s=JSON.parse(await new Response(Bun.stdin.stream()).text());const r=s.result??s;const l=process.env.LABEL;
     process.exit((r.nodes||[]).some(n=>n.role==="button"&&n.label===l)?0:1);'
}
stateof() {
  cli --port "$PORT" state 2>/dev/null | bun -e \
    'console.log(JSON.parse(await new Response(Bun.stdin.stream()).text()).state||"");'
}
ui_flags() { # ui_flags <field>  (chat_active|buy_active|tech_active)
  cli --port "$PORT" ingame_ui_mode --mode status 2>/dev/null | bun -e \
    'const s=JSON.parse(await new Response(Bun.stdin.stream()).text());const r=s.result??s;console.log(r[process.argv[1]]);' "$1"
}
expect() { if [ "$2" != "$3" ]; then echo "FAIL 91: $1 = '$2', want '$3'" >&2; cli --port "$PORT" inspect >&2 || true; exit 1; fi; }
wfw() { # wait for a widget by control-id/label
  for i in $(seq 1 100); do
    f=$(cli --port "$PORT" inspect | LABEL="$1" bun -e \
      'const r=JSON.parse(await new Response(Bun.stdin.stream()).text());const l=process.env.LABEL;
       console.log((r.nodes||[]).some(w=>w.label===l||w.control_id===l)?"yes":"no");' 2>/dev/null || echo no)
    [ "$f" = yes ] && return 0; sleep 0.05
  done
  echo "widget '$1' never appeared" >&2; return 1
}
wls() { # wait for lobby_state
  for i in $(seq 1 80); do
    ls=$(cli --port "$PORT" state | bun -e 'console.log(JSON.parse(await new Response(Bun.stdin.stream()).text()).lobby_state||"");')
    [ "$ls" = "$1" ] && return 0; sleep 0.1
  done
  return 1
}

cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

# --- 1. Overlay: ESC pops; back pops ----------------------------------------
cli --port "$PORT" click --label "Options" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
if ! has_button "Go Back"; then echo "FAIL 91: Options overlay did not open" >&2; exit 1; fi
cli --port "$PORT" key --key escape >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
if has_button "Go Back"; then echo "FAIL 91: ESC did not pop the Options overlay" >&2; exit 1; fi

cli --port "$PORT" click --label "Options" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
if ! has_button "Go Back"; then echo "FAIL 91: Options overlay did not reopen" >&2; exit 1; fi
cli --port "$PORT" back >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
if has_button "Go Back"; then echo "FAIL 91: back op did not pop the Options overlay" >&2; exit 1; fi
echo "ok: overlay ESC + back both pop (converged cancel edge)"

# --- 2. Lobby (the reported bug): ESC returns toward the main menu -----------
wfw "Connect To Lobby"; cli --port "$PORT" click --label "Connect To Lobby" >/dev/null
cli --port "$PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000 >/dev/null
wfw "Username"
for ch in a l i c e; do cli --port "$PORT" key --key "$ch" >/dev/null; done
cli --port "$PORT" key --key tab >/dev/null
for ch in s e c r e t; do cli --port "$PORT" key --key "$ch" >/dev/null; done
wls AUTHENTICATING; cli --port "$PORT" click --label "Login/Create" >/dev/null
cli --port "$PORT" wait_for_state --state CREATECHARACTER --timeout-ms 15000 >/dev/null
wfw "Create New Character"; cli --port "$PORT" click --label "Create New Character" >/dev/null
wfw "Alias"; cli --port "$PORT" set_text --label "Alias" --text "Alice" >/dev/null
cli --port "$PORT" key --key enter >/dev/null
wfw "Noxis"; cli --port "$PORT" click --label "Noxis" >/dev/null
cli --port "$PORT" wait_for_state --state LOBBY --timeout-ms 15000 >/dev/null

cli --port "$PORT" key --key escape >/dev/null
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
echo "ok: Lobby ESC returns toward the main menu (state=$(stateof))"

# --- 3. In match: chat/buy close first, then PauseScreen, then leave ---------
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 10000 >/dev/null
wfw "Tutorial"; cli --port "$PORT" click --label "Tutorial" >/dev/null
cli --port "$PORT" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 20000 >/dev/null
# Let the world reach a state with a viewed player so the overlays exist.
cli --port "$PORT" pause >/dev/null
for i in $(seq 1 100); do
  if cli --port "$PORT" world_state 2>/dev/null | bun -e \
    'const s=JSON.parse(await new Response(Bun.stdin.stream()).text());const r=s.result??s;process.exit((r.players?.length??0)>0?0:1);'; then break; fi
  cli --port "$PORT" step --frames 4 >/dev/null
done
cli --port "$PORT" resume >/dev/null

# chat open -> ESC closes chat (no PauseScreen).
cli --port "$PORT" ingame_ui_mode --mode chat >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
expect "chat opened" "$(ui_flags chat_active)" true
cli --port "$PORT" key --key escape >/dev/null
cli --port "$PORT" wait_frames --n 4 >/dev/null
expect "ESC closes chat first" "$(ui_flags chat_active)" false
if has_button "Hit Enter To Quit"; then echo "FAIL 91: ESC pushed PauseScreen while chat was open" >&2; exit 1; fi

# buy open -> ESC closes buy (no PauseScreen).
cli --port "$PORT" ingame_ui_mode --mode buy >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
expect "buy opened" "$(ui_flags buy_active)" true
cli --port "$PORT" key --key escape >/dev/null
cli --port "$PORT" wait_frames --n 4 >/dev/null
expect "ESC closes buy first" "$(ui_flags buy_active)" false
if has_button "Hit Enter To Quit"; then echo "FAIL 91: ESC pushed PauseScreen while buy was open" >&2; exit 1; fi

# nothing open -> ESC pushes PauseScreen.
cli --port "$PORT" key --key escape >/dev/null
cli --port "$PORT" wait_frames --n 4 >/dev/null
if ! has_button "Hit Enter To Quit"; then echo "FAIL 91: ESC did not push PauseScreen" >&2; exit 1; fi
echo "ok: in-match ESC closes chat/buy first, then pushes PauseScreen"

# ESC again pops the PauseScreen (default overlay pop), still in match.
cli --port "$PORT" key --key escape >/dev/null
cli --port "$PORT" wait_frames --n 4 >/dev/null
if has_button "Hit Enter To Quit"; then echo "FAIL 91: second ESC did not pop PauseScreen" >&2; exit 1; fi
expect "still in match after dismiss" "$(stateof)" SINGLEPLAYERGAME

# Re-push, then the primary action leaves the match (and pops itself).
cli --port "$PORT" key --key escape >/dev/null
cli --port "$PORT" wait_frames --n 4 >/dev/null
if ! has_button "Hit Enter To Quit"; then echo "FAIL 91: PauseScreen did not re-open" >&2; exit 1; fi
cli --port "$PORT" key --key enter >/dev/null
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
expect "leave_match landed on MAINMENU" "$(stateof)" MAINMENU
if has_button "Hit Enter To Quit"; then echo "FAIL 91: PauseScreen lingered after leaving the match" >&2; exit 1; fi
echo "ok: PauseScreen primary action leaves the match and dismisses itself"

echo "PASS 91_cancel_back_router"
