#!/usr/bin/env bash
# On the character-creation SELECT AGENCY stage (a list of oval rows),
# mouse hover and keyboard navigation must share one transient focus highlight:
# keyboard-focus an agency row, hover a different one, verify hover does not
# commit selection, then verify pointer-origin focus clears on hover-out.
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

read -r CREATE_HX CREATE_HY < <(cli --port "$CTRL_PORT" inspect | bun -e '
const t = await new Response(Bun.stdin.stream()).text();
const r = JSON.parse(t);
const row = (r.widgets ?? []).find((w) => w.label === "Create New Character" && w.w > 0 && w.h > 0);
if (!row) { console.error("Create New Character row missing while alias modal is open"); process.exit(1); }
console.log(`${Math.round(row.x + row.w / 2)} ${Math.round(row.y + row.h / 2)}`);
')
cli --port "$CTRL_PORT" hover_at --x "$CREATE_HX" --y "$CREATE_HY" >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 2 >/dev/null
ALIAS_HOVER="$TMP/alias-hover.json"
cli --port "$CTRL_PORT" inspect > "$ALIAS_HOVER"
bun -e '
const r = JSON.parse(await Bun.file(process.argv[1]).text());
const alias = (r.widgets ?? []).find((w) => w.label === "Alias");
if (!alias) { console.error("Alias text input missing after hover"); process.exit(1); }
if (alias.focused !== true) {
  console.error(`Alias lost focus after hovering Create New Character: ${JSON.stringify(alias)}`);
  process.exit(1);
}
' "$ALIAS_HOVER"

cli --port "$CTRL_PORT" set_text --label "Alias" --text "Alice" >/dev/null
cli --port "$CTRL_PORT" key --key enter >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state CREATECHARACTER --timeout-ms 15000 >/dev/null
wait_for_widget "Black Rose"
cli --port "$CTRL_PORT" wait_frames --n 2 >/dev/null

OUT_DIR="$(mktemp -d)"
KB="$OUT_DIR/inspect-keyboard.json"
HOVER="$OUT_DIR/inspect-hover.json"
HOVER_OUT="$OUT_DIR/inspect-hover-out.json"

# Keyboard-navigate a few rows down to focus an agency row that is NOT the
# first one, then capture the focused state.
cli --port "$CTRL_PORT" key --key down >/dev/null
cli --port "$CTRL_PORT" key --key down >/dev/null
cli --port "$CTRL_PORT" key --key down >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 2 >/dev/null
cli --port "$CTRL_PORT" inspect > "$KB"

# Compute the center of an agency row the keyboard did NOT focus and that is
# not already selected. Hover should preview/focus it without committing it.
read -r HX HY < <(bun -e '
const r = JSON.parse(await Bun.file(process.argv[1]).text());
const names = new Set(["Noxis","Lazarus","Caliber","Static","Black Rose"]);
const rows = (r.widgets ?? []).filter((w) => w.kind === "button" && names.has(w.label));
const focused = rows.filter((w) => w.focused === true);
if (focused.length !== 1) { console.error(`expected one keyboard-focused agency, got ${focused.length}`); process.exit(1); }
const selected = rows.filter((w) => w.selected === true);
if (selected.length !== 1) { console.error(`expected one selected agency before hover, got ${selected.length}`); process.exit(1); }
const target = rows.find((w) => w.label !== focused[0].label && w.selected !== true && w.w > 0 && w.h > 0);
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

const kbSelected = rowsOf(kb).filter((w) => w.selected === true);
const hoverSelected = rowsOf(hover).filter((w) => w.selected === true);
if (kbSelected.length !== 1 || hoverSelected.length !== 1) {
  console.error(`expected exactly one selected agency before/after hover, got ${kbSelected.length}/${hoverSelected.length}`);
  process.exit(1);
}
if (hoverSelected[0].label !== kbSelected[0].label) {
  console.error(`hover committed agency selection: before "${kbSelected[0].label}", after "${hoverSelected[0].label}"`);
  process.exit(1);
}
' "$KB" "$HOVER" "$HX" "$HY"

cli --port "$CTRL_PORT" hover_at --x 5 --y 5 >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 2 >/dev/null
cli --port "$CTRL_PORT" inspect > "$HOVER_OUT"

bun -e '
const names = new Set(["Noxis","Lazarus","Caliber","Static","Black Rose"]);
const rowsOf = (j) => (j.widgets ?? []).filter((w) => w.kind === "button" && names.has(w.label));
const hover = JSON.parse(await Bun.file(process.argv[1]).text());
const hoverOut = JSON.parse(await Bun.file(process.argv[2]).text());
const hx = Number(process.argv[3]), hy = Number(process.argv[4]);

const hovered = rowsOf(hover).find((w) => hx >= w.x && hx < w.x + w.w && hy >= w.y && hy < w.y + w.h);
if (!hovered) { console.error("hovered agency missing before click assertion"); process.exit(1); }

const focusedAfterOut = rowsOf(hoverOut).filter((w) => w.focused === true);
if (focusedAfterOut.length !== 0) {
  console.error(`expected agency focus to clear after hover-out, got ${JSON.stringify(focusedAfterOut.map((w)=>w.label))}`);
  process.exit(1);
}
const selectedAfterOut = rowsOf(hoverOut).filter((w) => w.selected === true);
const selectedBeforeHover = rowsOf(hover).filter((w) => w.selected === true);
if (selectedBeforeHover.length !== 1 || selectedAfterOut.length !== 1 ||
    selectedAfterOut[0].label !== selectedBeforeHover[0].label) {
  console.error(`hover-out changed committed agency: before ${JSON.stringify(selectedBeforeHover)}, after ${JSON.stringify(selectedAfterOut)}`);
  process.exit(1);
}
' "$HOVER" "$HOVER_OUT" "$HX" "$HY"

cli --port "$CTRL_PORT" hover_at --x "$HX" --y "$HY" >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 2 >/dev/null
cli --port "$CTRL_PORT" click_at --x "$HX" --y "$HY" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBY --timeout-ms 15000 >/dev/null

echo "PASS 17_character_create_focus"
