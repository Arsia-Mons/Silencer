#!/usr/bin/env bash
# On the character-creation SELECT AGENCY stage (a list of oval rows),
# mouse hover and keyboard navigation must share ONE focus highlight:
# keyboard-focus an agency row, then hover a different one, and only the
# hovered row stays focused. Reproduces the user-reported "two highlights"
# bug on the real character-creation screen, end-to-end through the lobby.
set -euo pipefail
. "$(dirname "$0")/lib.sh"

LOBBY_BIN=""
for candidate in \
  "$REPO_ROOT/services/lobby/lobby" \
  "$REPO_ROOT/services/lobby/lobby.exe" \
  "$REPO_ROOT/services/lobby/silencer-lobby" \
  "$REPO_ROOT/services/lobby/silencer-lobby.exe"; do
  if [ -x "$candidate" ]; then LOBBY_BIN="$candidate"; break; fi
done
if [ -z "$LOBBY_BIN" ]; then
  echo "lobby binary missing under services/lobby/ — build it via 'cd services/lobby && go build'" >&2
  exit 1
fi

TMP=$(mktemp -d)
LOBBY_LOG="$TMP/lobby.log"
LOBBY_DB="$TMP/lobby.json"
SILENCER_HOME="$TMP/home"
mkdir -p "$SILENCER_HOME"

LOBBY_PORT=$(pick_port)
PLAYER_AUTH_PORT=$(pick_port)
MAP_API_PORT=$(pick_port)
CTRL_PORT=$(pick_port)

SILENCER_VERSION=""
for cache in \
  "$REPO_ROOT/build/CMakeCache.txt" \
  "$REPO_ROOT/clients/silencer/build/CMakeCache.txt" \
  "$REPO_ROOT/clients/silencer/build-unity/CMakeCache.txt" \
  "$REPO_ROOT/clients/silencer/build-release/CMakeCache.txt"; do
  if [ -f "$cache" ]; then
    SILENCER_VERSION=$(awk -F= '/^SILENCER_VERSION:STRING=/{print $2}' "$cache")
    if [ -n "$SILENCER_VERSION" ]; then break; fi
  fi
done
if [ -z "$SILENCER_VERSION" ]; then
  echo "could not read SILENCER_VERSION from any CMakeCache.txt" >&2
  exit 1
fi

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

wait_for_widget() {
  local label="$1"
  for i in $(seq 1 100); do
    found=$(cli --port "$CTRL_PORT" inspect | LABEL="$label" bun -e \
      'const t=await new Response(Bun.stdin.stream()).text();
       const r=JSON.parse(t);
       console.log(r.widgets.some((w)=>w.label===process.env.LABEL) ? "yes" : "no");' 2>/dev/null || echo no)
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

# MainMenu -> LobbyConnect -> login (auto-creates account) -> CREATECHARACTER.
cli --port "$CTRL_PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
wait_for_widget "Connect To Lobby"
cli --port "$CTRL_PORT" click --label "Connect To Lobby" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000 >/dev/null
wait_for_widget "Login"
for ch in a l i c e; do cli --port "$CTRL_PORT" key --key "$ch" >/dev/null; done
cli --port "$CTRL_PORT" key --key tab >/dev/null
for ch in s e c r e t; do cli --port "$CTRL_PORT" key --key "$ch" >/dev/null; done
wait_for_lobby_state AUTHENTICATING
cli --port "$CTRL_PORT" click --label "Login" >/dev/null

# Walk the create flow up to the SELECT AGENCY stage (5 oval agency rows).
cli --port "$CTRL_PORT" wait_for_state --state CREATECHARACTER --timeout-ms 15000 >/dev/null
wait_for_widget "Create New Character"
cli --port "$CTRL_PORT" click --label "Create New Character" >/dev/null
wait_for_widget "Alias"
cli --port "$CTRL_PORT" set_text --label "Alias" --text "Alice" >/dev/null
cli --port "$CTRL_PORT" key --key enter >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state CREATECHARACTER --timeout-ms 15000 >/dev/null
wait_for_widget "Black Rose"
cli --port "$CTRL_PORT" wait_frames --n 2 >/dev/null

OUT_DIR="$(mktemp -d)"
KB="$OUT_DIR/inspect-keyboard.json"
HOVER="$OUT_DIR/inspect-hover.json"

# Keyboard-navigate a few rows down to focus an agency row that is NOT the
# first one, then capture the focused state.
cli --port "$CTRL_PORT" key --key down >/dev/null
cli --port "$CTRL_PORT" key --key down >/dev/null
cli --port "$CTRL_PORT" key --key down >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 2 >/dev/null
cli --port "$CTRL_PORT" inspect > "$KB"

# Compute the center of an agency row the keyboard did NOT focus.
read -r HX HY < <(bun -e '
const r = JSON.parse(await Bun.file(process.argv[1]).text());
const names = new Set(["Noxis","Lazarus","Caliber","Static","Black Rose"]);
const rows = (r.widgets ?? []).filter((w) => w.kind === "button" && names.has(w.label));
const focused = rows.filter((w) => w.focused === true);
if (focused.length !== 1) { console.error(`expected one keyboard-focused agency, got ${focused.length}`); process.exit(1); }
const target = rows.find((w) => w.label !== focused[0].label && w.w > 0 && w.h > 0);
if (!target) { console.error("no alternate agency row to hover"); process.exit(1); }
console.log(`${Math.round(target.x + target.w/2)} ${Math.round(target.y + target.h/2)}`);
' "$KB")

cli --port "$CTRL_PORT" hover_at --x "$HX" --y "$HY" >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 2 >/dev/null
cli --port "$CTRL_PORT" inspect > "$HOVER"

bun -e '
const names = new Set(["Noxis","Lazarus","Caliber","Static","Black Rose"]);
const rowsOf = (j) => (j.widgets ?? []).filter((w) => w.kind === "button" && names.has(w.label));
const kb = JSON.parse(await Bun.file(process.argv[1]).text());
const hover = JSON.parse(await Bun.file(process.argv[2]).text());
const hx = Number(process.argv[3]), hy = Number(process.argv[4]);

const kbFocused = rowsOf(kb).filter((w) => w.focused === true);
if (kbFocused.length !== 1) {
  console.error(`expected one keyboard-focused agency, got ${kbFocused.length}`);
  process.exit(1);
}
const hovered = rowsOf(hover).find((w) => hx >= w.x && hx < w.x + w.w && hy >= w.y && hy < w.y + w.h);
if (!hovered) { console.error("hover point did not land on an agency row"); process.exit(1); }

const hoverFocused = rowsOf(hover).filter((w) => w.focused === true);
if (hoverFocused.length !== 1) {
  console.error(`expected exactly one focused agency after hover, got ${hoverFocused.length}: ${JSON.stringify(rowsOf(hover).map((w)=>({label:w.label,focused:w.focused})))}`);
  process.exit(1);
}
if (hoverFocused[0].label !== hovered.label) {
  console.error(`hover did not move focus: hovering "${hovered.label}" but "${hoverFocused[0].label}" is focused`);
  process.exit(1);
}
if (hoverFocused[0].label === kbFocused[0].label) {
  console.error(`focus did not move off the keyboard row "${kbFocused[0].label}"`);
  process.exit(1);
}
' "$KB" "$HOVER" "$HX" "$HY"

echo "PASS 17_character_create_focus"
