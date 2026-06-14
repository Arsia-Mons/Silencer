#!/usr/bin/env bash
# SIL-231: the Create-Game Select-Map hover preview must caption the minimap with
# the map NAME and the wrapped DESCRIPTION (origin game_create_panel_map_form
# BuildHoverPreviewOverlay: name -> minimap -> description). A regression once
# rendered the minimap only. Drive to Create Game, hover a bundled map row that
# has a minimap (STAR72.SIL / THET06e.SIL), and assert the preview's name + desc
# text nodes are present at the cursor-following preview (distinct from the row).
#
# Harness (lobby spawn + connect/auth/route) mirrors 40_lobby_basic.sh.
set -euo pipefail
. "$(dirname "$0")/lib.sh"

LOBBY_BIN="$(lobby_bin)"

TMP=$(mktemp -d)
LOBBY_LOG="$TMP/lobby.log"
LOBBY_DB="$TMP/lobby.json"
SILENCER_HOME="$TMP/home"
MAPS_DIR="$TMP/maps"
mkdir -p "$SILENCER_HOME" "$MAPS_DIR"
# The preview is baked only for maps with a LOCAL file + a stored minimap; copy
# the bundled .SIL set into the lobby's maps dir so STAR72/THET06 resolve.
cp "$REPO_ROOT"/shared/assets/level/*.SIL "$MAPS_DIR"/ 2>/dev/null || true

LOBBY_PORT=$(pick_port)
PLAYER_AUTH_PORT=$(pick_port)
MAP_API_PORT=$(pick_port)
CTRL_PORT=$(pick_port)
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
  -maps-dir "$MAPS_DIR" \
  -player-auth-addr ":$PLAYER_AUTH_PORT" \
  -map-api-addr ":$MAP_API_PORT" \
  >"$LOBBY_LOG" 2>&1 &
LOBBY_PID=$!

for i in $(seq 1 60); do
  if (echo > "/dev/tcp/127.0.0.1/$LOBBY_PORT") 2>/dev/null; then break; fi
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

# Poll the retained cppx tree for a node whose control id OR label matches (the
# `inspect` op returns `nodes`). create_initial_character (lib.sh) also calls
# this, so it must be defined before that helper runs.
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

cli --port "$CTRL_PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
cli --port "$CTRL_PORT" resize --w 1920 --h 1080 >/dev/null
wait_for_widget "Connect To Lobby"
cli --port "$CTRL_PORT" click --label "Connect To Lobby" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000 >/dev/null
wait_for_widget "Username"
for ch in a l i c e; do cli --port "$CTRL_PORT" key --key "$ch" >/dev/null; done
cli --port "$CTRL_PORT" key --key tab >/dev/null
for ch in s e c r e t; do cli --port "$CTRL_PORT" key --key "$ch" >/dev/null; done
wait_for_lobby_state AUTHENTICATING
cli --port "$CTRL_PORT" click --label "Login/Create" >/dev/null
create_initial_character "Alice"

# Enter Create Game; the Select-Map list anchors with focusable MapRow rows.
wait_for_widget "NewGame"
cli --port "$CTRL_PORT" click --label "NewGame" >/dev/null
wait_for_widget "MapRow"

# Pick a bundled map row that HAS a stored minimap (community maps do not).
cli --port "$CTRL_PORT" inspect >"$TMP/maps.json"
ROW=$(MAPS_JSON="$TMP/maps.json" bun -e '
const r = JSON.parse(await Bun.file(process.env.MAPS_JSON).text());
const rows = (r.nodes || []).filter((w) => w.control_id === "MapRow");
const want = ["STAR72.SIL", "THET06e.SIL"];
const hit = rows.find((w) => want.includes(w.label));
if (!hit) { console.error("no minimap-bearing MapRow (STAR72/THET06) present"); process.exit(1); }
console.log(JSON.stringify({ label: hit.label, x: hit.x, y: hit.y, w: hit.w, h: hit.h }));
')
MAP_LABEL=$(echo "$ROW" | bun -e 'console.log(JSON.parse(await new Response(Bun.stdin.stream()).text()).label)')
HX=$(echo "$ROW" | bun -e 'const j=JSON.parse(await new Response(Bun.stdin.stream()).text());console.log(Math.round(j.x+j.w/2))')
HY=$(echo "$ROW" | bun -e 'const j=JSON.parse(await new Response(Bun.stdin.stream()).text());console.log(Math.round(j.y+j.h/2))')
RX=$(echo "$ROW" | bun -e 'console.log(Math.round(JSON.parse(await new Response(Bun.stdin.stream()).text()).x))')

cli --port "$CTRL_PORT" hover_at --x "$HX" --y "$HY" >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 20 >/dev/null
cli --port "$CTRL_PORT" inspect >"$TMP/hovered.json"

# Assert the floating preview captions the minimap: there is a `text` node whose
# value equals the hovered map filename, AND a separate `text` node carrying a
# non-empty description — both positioned LEFT of the map row (the preview floats
# up-left of the cursor, so its x is well below the row's left edge). This fails
# if the preview ever regresses to the minimap-only card.
HOVERED_JSON="$TMP/hovered.json" MAP_LABEL="$MAP_LABEL" ROW_X="$RX" bun -e '
const r = JSON.parse(await Bun.file(process.env.HOVERED_JSON).text());
const nodes = (r.nodes || []).filter((w) => w.role === "text" && w.value);
const label = process.env.MAP_LABEL;
const rowX = Number(process.env.ROW_X);
// The preview sits left of the map list (origin offset -185); require x notably
// left of the row so we never match the list rows themselves.
const inPreview = (w) => w.x < rowX - 40;
const nameNode = nodes.find((w) => w.value === label && inPreview(w));
if (!nameNode) {
  console.error(`map-preview NAME caption missing for ${label} (expected a text node = "${label}" left of the row at x<${rowX - 40})`);
  console.error("text nodes:", JSON.stringify(nodes.map((w) => ({ v: w.value, x: Math.round(w.x), y: Math.round(w.y) }))));
  process.exit(1);
}
// The description: a text node in the preview column (same x band as the name),
// below the name, that is not the name and not a bare map filename.
const descNode = nodes.find((w) =>
  inPreview(w) &&
  Math.abs(w.x - nameNode.x) < 30 &&
  w.y > nameNode.y &&
  w.value !== label &&
  w.value.length > 0 &&
  !/\.sil$/i.test(w.value.trim()));
if (!descNode) {
  console.error(`map-preview DESCRIPTION caption missing for ${label}`);
  console.error("text nodes:", JSON.stringify(nodes.map((w) => ({ v: w.value, x: Math.round(w.x), y: Math.round(w.y) }))));
  process.exit(1);
}
console.log(`preview caption OK: name="${nameNode.value}" desc="${descNode.value.slice(0, 40)}..."`);
'

echo "PASS 90_lobby_map_preview_caption"
