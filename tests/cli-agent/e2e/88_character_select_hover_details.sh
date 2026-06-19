#!/usr/bin/env bash
# SIL-202: hovering an existing character row previews that character's details.
set -euo pipefail
. "$(dirname "$0")/lib.sh"

LOBBY_BIN="$(lobby_bin)"

TMP=$(mktemp -d)
LOBBY_LOG="$TMP/lobby.log"
LOBBY_DB="$TMP/lobby.json"
SILENCER_HOME="$TMP/home"
mkdir -p "$SILENCER_HOME"

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
      'const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
       const l = process.env.LABEL;
       console.log((r.nodes||[]).some((w)=>w.label===l||w.control_id===l||w.value===l) ? "yes" : "no");' 2>/dev/null || echo no)
    if [ "$found" = "yes" ]; then return 0; fi
    sleep 0.05
  done
  echo "widget '$label' never appeared" >&2
  cli --port "$CTRL_PORT" inspect >&2 || true
  return 1
}

wait_for_lobby_state() {
  local target="$1"
  local ls=""
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

assert_detail() {
  local want="$1"
  local reject="$2"
  cli --port "$CTRL_PORT" inspect | WANT="$want" REJECT="$reject" bun -e '
    const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
    const text = (r.nodes ?? []).map((n) => `${n.value || ""} ${n.label || ""}`).join("\n");
    if (!text.includes(process.env.WANT)) {
      console.error(`missing detail text: ${process.env.WANT}`);
      process.exit(1);
    }
    if (process.env.REJECT && text.includes(process.env.REJECT)) {
      console.error(`stale detail text still visible: ${process.env.REJECT}`);
      process.exit(1);
    }
  '
}

cli --port "$CTRL_PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
wait_for_widget "Connect To Lobby"
cli --port "$CTRL_PORT" click --label "Connect To Lobby" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000 >/dev/null
wait_for_widget "Username"

for ch in a l i c e; do
  cli --port "$CTRL_PORT" key --key "$ch" >/dev/null
done
cli --port "$CTRL_PORT" key --key tab >/dev/null
for ch in s e c r e t; do
  cli --port "$CTRL_PORT" key --key "$ch" >/dev/null
done

wait_for_lobby_state AUTHENTICATING
cli --port "$CTRL_PORT" click --label "Login/Create" >/dev/null
create_initial_character "Alice"

wait_for_widget "LobbyAgents"
cli --port "$CTRL_PORT" click --label "LobbyAgents" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state CREATECHARACTER --timeout-ms 5000 >/dev/null
wait_for_widget "CreateNewCharacter"
cli --port "$CTRL_PORT" click --label "CreateNewCharacter" >/dev/null
wait_for_widget "Alias"
cli --port "$CTRL_PORT" set_text --label "Alias" --text "Bob" >/dev/null
cli --port "$CTRL_PORT" key --key enter >/dev/null
wait_for_widget "Lazarus"
cli --port "$CTRL_PORT" click --label "Lazarus" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBY --timeout-ms 15000 >/dev/null

wait_for_widget "LobbyAgents"
cli --port "$CTRL_PORT" click --label "LobbyAgents" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state CREATECHARACTER --timeout-ms 5000 >/dev/null
wait_for_widget "Alice"
wait_for_widget "Bob"
assert_detail "Endurance" "Resurrection Ability"

read -r HX HY < <(cli --port "$CTRL_PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const row = (r.nodes ?? []).find((w) => w.role === "button" && w.label === "Bob" && w.w > 0 && w.h > 0);
if (!row) { console.error("Bob row missing"); process.exit(1); }
console.log(`${Math.round(row.x + row.w / 2)} ${Math.round(row.y + row.h / 2)}`);
')

cli --port "$CTRL_PORT" hover_at --x "$HX" --y "$HY" >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 2 >/dev/null
cli --port "$CTRL_PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const hovered = (r.nodes ?? []).filter((w) => w.role === "button" && w.hovered === true);
if (hovered.length !== 1 || hovered[0].label !== "Bob") {
  console.error(`expected only Bob hovered, got ${JSON.stringify(hovered.map((w) => w.label))}`);
  process.exit(1);
}
'
assert_detail "Resurrection Ability" "Endurance"

echo "PASS 88_character_select_hover_details"
