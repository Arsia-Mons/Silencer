#!/usr/bin/env bash
# Drives the migrated lobby through gameselect → gamecreate using only
# `cli click --label` and `cli set_text --label`, ending with a Create.
# Exercises P18 (CLI inspect compatibility for UI tree): the UI panels
# register their interactive widgets in UiInteractionRegistry, and controldispatch
# routes click / set_text through that registry when the active iface has
# no matching object.
set -euo pipefail
. "$(dirname "$0")/lib.sh"

LOBBY_BIN="$(lobby_bin)"

TMP=$(mktemp -d)
LOBBY_LOG="$TMP/lobby.log"
LOBBY_DB="$TMP/lobby.json"
SILENCER_HOME="$TMP/home"
mkdir -p "$SILENCER_HOME" "$TMP/maps"

# Pin mapapiurl at a local empty endpoint to avoid hitting production.
mkdir -p "$SILENCER_HOME/Library/Application Support/Silencer"

LOBBY_PORT=$(pick_port)
PLAYER_AUTH_PORT=$(pick_port)
MAP_API_PORT=$(pick_port)
CTRL_PORT=$(pick_port)

cat > "$SILENCER_HOME/Library/Application Support/Silencer/config.cfg" <<EOF
mapapiurl=http://127.0.0.1:$MAP_API_PORT
EOF

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

HOME="$SILENCER_HOME" "$SILENCER_BIN" \
  --headless \
  --control-port "$CTRL_PORT" \
  --lobby-host 127.0.0.1 \
  --lobby-port "$LOBBY_PORT" \
  >"/tmp/silencer-e2e-$CTRL_PORT.log" 2>&1 &
SILENCER_PID=$!
wait_alive "$CTRL_PORT"

wait_for_widget() {
  local label="$1"
  for i in $(seq 1 100); do
    found=$(cli --port "$CTRL_PORT" inspect | LABEL="$label" bun -e \
      'const t=await new Response(Bun.stdin.stream()).text();
       const r=JSON.parse(t);
       const label=process.env.LABEL;
       console.log(r.widgets.some((w)=>w.label===label) ? "yes" : "no");' 2>/dev/null || echo no)
    if [ "$found" = "yes" ]; then return 0; fi
    sleep 0.05
  done
  echo "widget '$label' never appeared" >&2
  cli --port "$CTRL_PORT" inspect >&2 || true
  return 1
}

wait_for_lobby_state() {
  local target="$1"
  for i in $(seq 1 80); do
    ls=$(cli --port "$CTRL_PORT" state | bun -e \
      'const t=await new Response(Bun.stdin.stream()).text(); console.log(JSON.parse(t).lobby_state||"");')
    if [ "$ls" = "$target" ]; then return 0; fi
    sleep 0.1
  done
  echo "lobby_state never became $target (last=$ls)" >&2
  return 1
}

cli --port "$CTRL_PORT" wait_for_state --state MAINMENU --timeout-ms 15000
wait_for_widget "Connect To Lobby"
cli --port "$CTRL_PORT" click --label "Connect To Lobby" >/dev/null
wait_for_widget "Login/Create"

cli --port "$CTRL_PORT" set_text --uid 1 --text "UIbob" >/dev/null
cli --port "$CTRL_PORT" set_text --uid 2 --text "secret" >/dev/null
wait_for_lobby_state AUTHENTICATING
cli --port "$CTRL_PORT" click --label "Login/Create" >/dev/null
create_initial_character "UIBob"
wait_for_widget "Create Game"

# Drive a few frames so the UI panels run their first Build pass and
# register widgets into the inspector.
cli --port "$CTRL_PORT" step --frames 5 >/dev/null

# inspect should now list UI widgets including "Create Game".
got=$(cli --port "$CTRL_PORT" inspect | bun -e \
  'const t=await new Response(Bun.stdin.stream()).text();
   const r=JSON.parse(t);
   const has=(lbl)=>r.widgets.some((w)=>w.label===lbl && w.source==="ui");
   console.log(has("Create Game") ? "ok" : "missing");')
if [ "$got" != "ok" ]; then
  echo "inspect did not surface UI 'Create Game' widget" >&2
  cli --port "$CTRL_PORT" inspect >&2 || true
  exit 1
fi

# Step from gameselect → gamecreate via the UI registry path.
cli --port "$CTRL_PORT" click --label "Create Game" >/dev/null
cli --port "$CTRL_PORT" step --frames 5 >/dev/null

# inspect should now list the gamecreate widgets ("Create", "Game name").
got=$(cli --port "$CTRL_PORT" inspect | bun -e \
  'const t=await new Response(Bun.stdin.stream()).text();
   const r=JSON.parse(t);
   const has=(lbl)=>r.widgets.some((w)=>w.label===lbl && w.source==="ui");
   console.log(has("Create") && has("Game name") ? "ok" : "missing");')
if [ "$got" != "ok" ]; then
  echo "inspect did not surface gamecreate UI widgets" >&2
  cli --port "$CTRL_PORT" inspect >&2 || true
  exit 1
fi

# Set the game name via UI set_text.
cli --port "$CTRL_PORT" set_text --label "Game name" --text "UITest" >/dev/null

# Creating without a selected map should show a modal. While it is up,
# inspect/click routes must be modal-scoped, not leak through to the
# game-create panel below it.
cli --port "$CTRL_PORT" click --label "Create" >/dev/null
cli --port "$CTRL_PORT" step --frames 5 >/dev/null
modal_scope=$(cli --port "$CTRL_PORT" inspect | bun -e \
  'const t=await new Response(Bun.stdin.stream()).text();
   const r=JSON.parse(t);
   const hasOk=r.widgets.some((w)=>w.label==="OK" && w.source==="ui");
   const leaks=r.widgets.some((w)=>w.label==="Create" || w.label==="Game name");
   console.log(hasOk && !leaks ? "ok" : "bad");')
if [ "$modal_scope" != "ok" ]; then
  echo "message modal did not block underlying game-create widgets" >&2
  cli --port "$CTRL_PORT" inspect >&2 || true
  exit 1
fi
cli --port "$CTRL_PORT" click --label OK >/dev/null
cli --port "$CTRL_PORT" step --frames 5 >/dev/null

# Select the first list row through the UI-aware select control op.
first_map=$(cli --port "$CTRL_PORT" inspect | bun -e \
  'const t=await new Response(Bun.stdin.stream()).text();
   const r=JSON.parse(t);
   const row=r.widgets.find((w)=>w.kind==="listrow" && w.source==="ui" && w.row_index===0);
   console.log(row && row.label ? row.label : "");')
if [ -z "$first_map" ]; then
  echo "no UI listrow with row_index=0 in inspect" >&2
  exit 1
fi
cli --port "$CTRL_PORT" select --index 0 >/dev/null
cli --port "$CTRL_PORT" step --frames 5 >/dev/null

# Verify the row got marked selected.
sel=$(cli --port "$CTRL_PORT" inspect | bun -e \
  'const t=await new Response(Bun.stdin.stream()).text();
   const r=JSON.parse(t);
   const row=r.widgets.find((w)=>w.kind==="listrow" && w.source==="ui" && w.row_index===0);
   console.log(row && row.selected ? "yes" : "no");')
if [ "$sel" != "yes" ]; then
  echo "map row click did not mark the row selected" >&2
  exit 1
fi

# Verify the text input now reads "UITest".
name=$(cli --port "$CTRL_PORT" inspect | bun -e \
  'const t=await new Response(Bun.stdin.stream()).text();
   const r=JSON.parse(t);
   const w=r.widgets.find((w)=>w.kind==="textinput" && w.label==="Game name" && w.source==="ui");
   console.log(w ? (w.text || "") : "");')
if [ "$name" != "UITest" ]; then
  echo "set_text on Game name did not stick (got: '$name')" >&2
  exit 1
fi

# Click Create. The GameCreatePanelTick consumes the flag and runs the legacy
# CreateGame kickoff (with bundled map → mapUploadState=2 → CreateGame). The
# client pushes a MessageModal::Progress("Uploading map...") modal.
cli --port "$CTRL_PORT" click --label "Create" >/dev/null

# Wait a few frames for either the progress modal to push (success path) or
# an immediate error modal ("No map selected", "Could not create game"). The
# UI panel teardown only happens AFTER the Create flow completes, so for
# this test we just want to confirm Create dispatched without hitting the
# WIDGET_NOT_FOUND path.
cli --port "$CTRL_PORT" step --frames 30 >/dev/null
for i in $(seq 1 100); do
  if cli --port "$CTRL_PORT" inspect | bun -e '
    const t = await new Response(Bun.stdin.stream()).text();
    const r = JSON.parse(t);
    const agents = (r.widgets ?? []).find((w) => w.id === "lobby.character.agents" && w.kind === "button");
    process.exit(agents && agents.enabled === false ? 0 : 1);
  ' >/dev/null 2>&1; then
    echo "PASS 40_lobby_basic"
    exit 0
  fi
  sleep 0.1
done

echo "Agents button was not disabled after joining created game" >&2
cli --port "$CTRL_PORT" inspect >&2 || true
exit 1
