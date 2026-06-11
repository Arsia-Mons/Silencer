#!/usr/bin/env bash
# Drives the cppx lobby GameCreate panel and proves it renders + stays usable
# across window sizes. The legacy Clay GameCreate had a scrollable "Game Options
# Form" with separate security/level option rows; the modern cppx GameCreate
# panel has no scrollable surface (CreateGameRequest defaults those options —
# they render as a read-only "Game Options" table), so this is no longer a
# scroll test. Instead: connect → auth → create character → LOBBY → New Game,
# then assert the GameCreate panel (Select Map list of focusable MapRow entries,
# Game name display, Create button, Go Back) is present and laid out inside the
# UI viewport at a desktop size, a small 640x480 size, and a tall 1000x1100
# portrait size. The logical canvas pins height at 720 (width = aspect*720,
# game_ui_pipeline.cpp) and the lobby cockpit's authored min logical width is
# ~640, so a phone-narrow 390x844 window crops horizontally BY DESIGN — at that
# size we assert the panel still mounts (controls exist, positive size,
# vertically in-bounds) rather than full horizontal containment.
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

# step1: set the alias (the Alias input is not autofocused, so set_text by
# accessibility label rather than raw key chars), then enter confirms
# (AliasConfirm is disabled until the alias is non-empty) and advances to the
# agency picker.
wait_for_widget "Alias"
cli --port "$CTRL_PORT" set_text --label "Alias" --text "Alice" >/dev/null
cli --port "$CTRL_PORT" key --key enter >/dev/null

# step2: pick an agency — creating the agent. Once it round-trips through the
# lobby the CREATECHARACTER tick routes to the lobby.
wait_for_widget "Black Rose"
cli --port "$CTRL_PORT" click --label "Noxis" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBY --timeout-ms 15000

# GameSelect is the default right column; swap to GameCreate (screen-local
# use_state<ActivePanel>) by clicking New Game — this mounts the create form.
# The NewGame button's visible label is "Create Game"; click by control_id.
wait_for_widget "NewGame"
cli --port "$CTRL_PORT" click --label "NewGame" >/dev/null
wait_for_widget "CreateGame"

# Assert the GameCreate panel is present and laid out within the UI viewport at
# the current window size. The modern origin-parity panel (lobby_screen.cppx)
# is: a "Game Options" read-only table, a "Select Map" list of focusable MapRow
# entries, the "Game name:" display (plain text by design — no Input chrome),
# the wide "Create" button (control_id CreateGame), and the cockpit "Go Back"
# button (LobbyGoBack) that exits the create panel. Buttons are matched by
# control_id (stable across copy changes); the viewport is the retained tree's
# Root node (the logical UI canvas). Each interactive control must have
# positive size and sit fully inside the viewport — proving the panel renders
# intact and never collapses or drifts off-canvas as the window resizes.
# Second arg "vertical" relaxes the horizontal containment (for windows
# narrower than the cockpit's authored min logical width, where horizontal
# cropping is the designed behavior) while still requiring presence, positive
# size, and vertical containment.
assert_create_panel_in_bounds() {
  local what="$1"
  local mode="${2:-full}"
  local inspect_out
  inspect_out="$(mktemp)"
  cli --port "$CTRL_PORT" inspect > "$inspect_out"

  bun -e '
  const inspectPath = process.argv[1];
  const what = process.argv[2];
  const fullBounds = process.argv[3] !== "vertical";
  const inspect = JSON.parse(await Bun.file(inspectPath).text());
  const nodes = inspect.nodes ?? [];
  if (nodes.length === 0) {
    console.error(`${what}: inspect returned no nodes`);
    process.exit(1);
  }
  // UI viewport = the retained tree root (the logical UI canvas).
  const root = nodes.find((n) => n.type === "Root") ?? nodes[0];
  const maxX = root.x + root.w, maxY = root.y + root.h;
  if (!(maxX > 0) || !(maxY > 0)) {
    console.error(`${what}: degenerate UI viewport ${maxX}x${maxY}`);
    process.exit(1);
  }
  const eps = 1;
  const inBounds = (n) =>
    n.y >= -eps && n.y + n.h <= maxY + eps &&
    (!fullBounds || (n.x >= -eps && n.x + n.w <= maxX + eps));
  const assertControl = (n, id) => {
    if (!(n.w > 0) || !(n.h > 0)) {
      console.error(`${what}: control ${id} has non-positive size ${n.w}x${n.h}`);
      process.exit(1);
    }
    if (!inBounds(n)) {
      console.error(`${what}: control ${id} outside UI viewport ${maxX}x${maxY}: ${JSON.stringify(n)}`);
      process.exit(1);
    }
  };
  // Required buttons: the Create submit + the cockpit Go Back that leaves the
  // create panel.
  for (const id of ["CreateGame", "LobbyGoBack"]) {
    const n = nodes.find((w) => w.control_id === id);
    if (!n) {
      console.error(`${what}: GameCreate control ${id} missing`);
      process.exit(1);
    }
    if (!n.focusable) {
      console.error(`${what}: control ${id} not focusable`);
      process.exit(1);
    }
    assertControl(n, id);
  };
  // The Select Map list: at least one focusable MapRow entry, all in-bounds
  // (the maplist box clips with Overflow::Hidden, but the capped row count
  // must still lay out on-canvas).
  const rows = nodes.filter((w) => w.control_id === "MapRow");
  if (rows.length === 0) {
    console.error(`${what}: no MapRow entries in the Select Map list`);
    process.exit(1);
  }
  rows.forEach((n, i) => assertControl(n, `MapRow[${i}] ${n.label ?? ""}`));
  // Panel copy that anchors the create form: the options table title, the map
  // list title, and the game-name field label.
  for (const text of ["Game Options", "Select Map", "Game name:"]) {
    if (!nodes.some((w) => w.role === "text" && w.value === text)) {
      console.error(`${what}: GameCreate text "${text}" missing`);
      process.exit(1);
    }
  }
  console.log(`${what}: CreateGame+LobbyGoBack+${rows.length} MapRows ${fullBounds ? "in-bounds" : "mounted + vertically in-bounds"} within ${maxX}x${maxY}`);
  ' "$inspect_out" "$what" "$mode"

  rm -f "$inspect_out"
}

# Desktop size (headless default is small, so resize explicitly).
cli --port "$CTRL_PORT" resize --w 1280 --h 720 >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 3 >/dev/null
wait_for_widget "CreateGame"
assert_create_panel_in_bounds "1280x720"

# Small desktop size.
cli --port "$CTRL_PORT" resize --w 640 --h 480 >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 3 >/dev/null
wait_for_widget "CreateGame"
assert_create_panel_in_bounds "640x480"

# Tall portrait size whose logical width (1000/(1100/720) ≈ 654) still clears
# the cockpit's authored min width — full containment must hold.
cli --port "$CTRL_PORT" resize --w 1000 --h 1100 >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 3 >/dev/null
wait_for_widget "CreateGame"
assert_create_panel_in_bounds "1000x1100"

# Phone-narrow portrait: the logical canvas (≈333x720) is narrower than the
# cockpit's authored min width, so horizontal cropping is by design — the panel
# must still mount with positive-size controls inside the vertical bounds.
cli --port "$CTRL_PORT" resize --w 390 --h 844 >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 3 >/dev/null
wait_for_widget "CreateGame"
assert_create_panel_in_bounds "390x844" vertical

echo "PASS 53_lobby_create_options_scroll"
