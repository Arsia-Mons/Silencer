#!/usr/bin/env bash
# Drives the cppx lobby GameCreate panel and proves it renders + stays usable
# across window sizes. The legacy Clay GameCreate had a scrollable "Game Options
# Form" with separate security/level option rows; the modern cppx GameCreate
# panel has no scrollable surface (CreateGameRequest defaults those options), so
# this is no longer a scroll test. Instead: connect → auth → create character →
# LOBBY → New Game, then assert the GameCreate controls (name input + bundled-map
# cycle + Create/Back) are present and laid out inside the virtual UI bounds at a
# desktop size, a small 640x480 size, and a tall mobile 390x844 size.
set -euo pipefail
. "$(dirname "$0")/lib.sh"

LOBBY_BIN="$(lobby_bin)"

# Per-run scratch — both for the lobby's user db (-db) and the silencer's
# config dir (HOME-rooted on macOS / Linux). Two separate pid/log files so
# the EXIT trap can tear both down cleanly.
TMP=$(mktemp -d)
LOBBY_LOG="$TMP/lobby.log"
LOBBY_DB="$TMP/lobby.json"
SILENCER_HOME="$TMP/home"
mkdir -p "$SILENCER_HOME"

LOBBY_PORT=$(pick_port)
PLAYER_AUTH_PORT=$(pick_port)
MAP_API_PORT=$(pick_port)
CTRL_PORT=$(pick_port)

# Match the version baked into the selected silencer binary — without this the
# lobby rejects the handshake with "Wrong version".
SILENCER_VERSION="$(silencer_version)"

cleanup() {
  if [ -n "${SILENCER_PID:-}" ]; then
    stop_silencer "$SILENCER_PID" "$CTRL_PORT" || true
  fi
  if [ -n "${LOBBY_PID:-}" ]; then
    kill "$LOBBY_PID" 2>/dev/null || true
    wait "$LOBBY_PID" 2>/dev/null || true
  fi
  rm -rf "$TMP"
}
trap cleanup EXIT

# Lobby on an unprivileged port. -version matches the binary so the handshake
# passes; -maps-dir/-map-api-addr keep map traffic local.
"$LOBBY_BIN" \
  -addr ":$LOBBY_PORT" \
  -db "$LOBBY_DB" \
  -version "$SILENCER_VERSION" \
  -game-binary "$SILENCER_BIN" \
  -maps-dir "$TMP/maps" \
  -player-auth-addr ":$PLAYER_AUTH_PORT" \
  -map-api-addr ":$MAP_API_PORT" \
  >"$LOBBY_LOG" 2>&1 &
LOBBY_PID=$!

# Wait for the lobby's TCP listener to come up.
for i in $(seq 1 60); do
  if (echo > "/dev/tcp/127.0.0.1/$LOBBY_PORT") 2>/dev/null; then
    break
  fi
  sleep 0.25
  if [ "$i" = 60 ]; then
    echo "lobby on :$LOBBY_PORT never came up" >&2
    cat "$LOBBY_LOG" >&2
    exit 1
  fi
done

# Silencer with a fresh HOME so the lobby host/port override doesn't get
# persisted to the dev's real config.
HOME="$SILENCER_HOME" "$SILENCER_BIN" \
  --headless \
  --control-port "$CTRL_PORT" \
  --lobby-host 127.0.0.1 \
  --lobby-port "$LOBBY_PORT" \
  >"/tmp/silencer-e2e-$CTRL_PORT.log" 2>&1 &
SILENCER_PID=$!
wait_alive "$CTRL_PORT"

# Poll the retained cppx tree for a node whose control id OR accessibility label
# equals the argument (the introspection `inspect` op returns `nodes`).
wait_for_widget() {
  local label="$1"
  for i in $(seq 1 100); do
    found=$(cli --port "$CTRL_PORT" inspect | LABEL="$label" bun -e \
      'const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
       const l = process.env.LABEL;
       console.log((r.nodes||[]).some((w)=>w.label===l||w.control_id===l) ? "yes" : "no");' 2>/dev/null || echo no)
    if [ "$found" = "yes" ]; then return 0; fi
    sleep 0.05
  done
  echo "widget '$label' never appeared" >&2
  cli --port "$CTRL_PORT" inspect >&2 || true
  return 1
}

# The Login button only submits credentials once the connect flow has advanced
# (on the game tick) through Connect → version-check → AUTHENTICATING; a click
# before that is silently consumed. `state` exposes lobby_state for this sync.
wait_for_lobby_state() {
  local target="$1"
  for i in $(seq 1 80); do
    ls=$(cli --port "$CTRL_PORT" state | bun -e \
      'console.log(JSON.parse(await new Response(Bun.stdin.stream()).text()).lobby_state||"");')
    if [ "$ls" = "$target" ]; then return 0; fi
    sleep 0.1
  done
  echo "lobby_state never became $target (last=$ls)" >&2
  cat "/tmp/silencer-e2e-$CTRL_PORT.log" >&2 || true
  return 1
}

cli --port "$CTRL_PORT" wait_for_state --state MAINMENU --timeout-ms 15000
wait_for_widget "Connect To Lobby"
cli --port "$CTRL_PORT" click --label "Connect To Lobby" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000
wait_for_widget "Username"

# Type the credentials through the same key path real text input uses
# (auto-creates the account on first login). Username is autofocused; Tab moves
# focus to the password field.
for ch in a l i c e; do
  cli --port "$CTRL_PORT" key --key "$ch" >/dev/null
done
cli --port "$CTRL_PORT" key --key tab >/dev/null
for ch in s e c r e t; do
  cli --port "$CTRL_PORT" key --key "$ch" >/dev/null
done

wait_for_lobby_state AUTHENTICATING
cli --port "$CTRL_PORT" click --label "Login/Create" >/dev/null

# Fresh account → server reports no characters → the connect flow routes to
# character creation. The 3-step wizard: roster (step0) → alias (step1) →
# agency (step2).
cli --port "$CTRL_PORT" wait_for_state --state CREATECHARACTER --timeout-ms 15000

# step0: roster — start a new character.
wait_for_widget "Create New Character"
cli --port "$CTRL_PORT" click --label "Create New Character" >/dev/null

# step1: type an alias, then confirm to advance to the agency picker.
wait_for_widget "Alias"
for ch in A l i c e; do
  cli --port "$CTRL_PORT" key --key "$ch" >/dev/null
done
cli --port "$CTRL_PORT" click --label "Continue" >/dev/null

# step2: pick an agency — creating the agent. Once it round-trips through the
# lobby the CREATECHARACTER tick routes to the lobby.
wait_for_widget "Black Rose"
cli --port "$CTRL_PORT" click --label "Noxis" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBY --timeout-ms 15000

# GameSelect is the default right column; swap to GameCreate (screen-local
# use_state<ActivePanel>) by clicking New Game — this mounts the create form.
wait_for_widget "NewGame"
cli --port "$CTRL_PORT" click --label "New Game" >/dev/null
wait_for_widget "CreateBack"

# Assert the GameCreate panel controls are present and laid out within the UI
# content viewport at the current window size. Controls are matched by
# control_id (stable across copy changes). The viewport reference is the
# bounding box of the whole retained tree (inspect emits every node's Yoga
# layout x/y/w/h in one UI-space): each control must sit at non-negative
# on-canvas coordinates with positive size, fully inside that content box. The
# lobby lays out against a fixed virtual canvas, so this verifies the panel
# renders intact and never collapses or drifts off-canvas as the window resizes.
assert_create_panel_in_bounds() {
  local what="$1"
  local inspect_out
  inspect_out="$(mktemp)"
  cli --port "$CTRL_PORT" inspect > "$inspect_out"

  bun -e '
  const inspectPath = process.argv[1];
  const what = process.argv[2];
  const inspect = JSON.parse(await Bun.file(inspectPath).text());
  const nodes = inspect.nodes ?? [];
  if (nodes.length === 0) {
    console.error(`${what}: inspect returned no nodes`);
    process.exit(1);
  }
  // UI content viewport = bounding box of every laid-out node.
  let maxX = 0, maxY = 0;
  for (const n of nodes) {
    if (Number.isFinite(n.x) && Number.isFinite(n.w)) maxX = Math.max(maxX, n.x + n.w);
    if (Number.isFinite(n.y) && Number.isFinite(n.h)) maxY = Math.max(maxY, n.y + n.h);
  }
  if (!(maxX > 0) || !(maxY > 0)) {
    console.error(`${what}: degenerate UI content viewport ${maxX}x${maxY}`);
    process.exit(1);
  }
  // The GameCreate panel: game-name Input + bundled-map cycle + Create/Back.
  const required = ["GameName", "MapPrev", "MapNext", "CreateGame", "CreateBack"];
  const eps = 1;
  for (const id of required) {
    const n = nodes.find((w) => w.control_id === id);
    if (!n) {
      console.error(`${what}: GameCreate control ${id} missing`);
      process.exit(1);
    }
    if (!(n.w > 0) || !(n.h > 0)) {
      console.error(`${what}: control ${id} has non-positive size ${n.w}x${n.h}`);
      process.exit(1);
    }
    if (n.x < -eps || n.y < -eps || n.x + n.w > maxX + eps || n.y + n.h > maxY + eps) {
      console.error(`${what}: control ${id} outside UI viewport ${maxX}x${maxY}: ${JSON.stringify(n)}`);
      process.exit(1);
    }
  }
  console.log(`${what}: GameName+MapPrev+MapNext+CreateGame+CreateBack in-bounds within ${maxX}x${maxY}`);
  ' "$inspect_out" "$what"

  rm -f "$inspect_out"
}

# Default desktop size.
cli --port "$CTRL_PORT" wait_frames --n 3 >/dev/null
assert_create_panel_in_bounds "default"

# Small desktop size.
cli --port "$CTRL_PORT" resize --w 640 --h 480 >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 3 >/dev/null
wait_for_widget "CreateBack"
assert_create_panel_in_bounds "640x480"

# Tall mobile size.
cli --port "$CTRL_PORT" resize --w 390 --h 844 >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 3 >/dev/null
wait_for_widget "CreateBack"
assert_create_panel_in_bounds "390x844"

echo "PASS 53_lobby_create_options_scroll"
