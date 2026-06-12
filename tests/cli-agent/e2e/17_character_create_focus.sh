#!/usr/bin/env bash
# Character-create SELECT AGENCY step (step2 of the 3-step wizard): verify the
# golden interaction model on the agency rows. Hovering a row previews that
# agency's details and sets its hovered pseudo-class without moving focus,
# committing a selection, or leaving CREATECHARACTER; hovering empty space
# clears the hover state. Then verify the double-submit guard: clicking an
# agency twice in quick succession creates exactly ONE character, not two.
#
# Driven end-to-end through a local Go lobby (harness lifted from
# 30_lobby_login.sh): MainMenu -> Connect To Lobby -> login alice/secret ->
# CREATECHARACTER -> Create New Character -> Alias=Alice -> enter -> agency.
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

# Occasionally the headless binary wedges before opening the control port
# (empty log, wait_alive timeout) — retry the launch once on a fresh port.
for attempt in 1 2; do
  HOME="$SILENCER_HOME" "$SILENCER_BIN" \
    --headless \
    --control-port "$CTRL_PORT" \
    --lobby-host 127.0.0.1 \
    --lobby-port "$LOBBY_PORT" \
    >"/tmp/silencer-e2e-$CTRL_PORT.log" 2>&1 &
  SILENCER_PID=$!
  if wait_alive "$CTRL_PORT"; then break; fi
  if [ "$attempt" = 2 ]; then
    echo "silencer never came up after 2 attempts" >&2
    exit 1
  fi
  stop_silencer "$SILENCER_PID" "$CTRL_PORT" || true
  SILENCER_PID=""
  CTRL_PORT=$(pick_port)
done

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

# MainMenu -> LobbyConnect -> login (auto-creates account) -> CREATECHARACTER.
cli --port "$CTRL_PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
wait_for_widget "Connect To Lobby"
cli --port "$CTRL_PORT" click --label "Connect To Lobby" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000 >/dev/null
wait_for_widget "Username"
for ch in a l i c e; do cli --port "$CTRL_PORT" key --key "$ch" >/dev/null; done
cli --port "$CTRL_PORT" key --key tab >/dev/null
for ch in s e c r e t; do cli --port "$CTRL_PORT" key --key "$ch" >/dev/null; done
wait_for_lobby_state AUTHENTICATING
cli --port "$CTRL_PORT" click --label "Login/Create" >/dev/null

# Walk the 3-step create wizard to the SELECT AGENCY step:
#   step0 roster -> Create New Character
#   step1 alias  -> type "Alice" into the focused "Alias" input -> enter submits
#                   (this build has no separate Continue button on the alias step)
#   step2 agency -> 5 rows incl. "Black Rose"
cli --port "$CTRL_PORT" wait_for_state --state CREATECHARACTER --timeout-ms 15000 >/dev/null
wait_for_widget "Create New Character"
cli --port "$CTRL_PORT" click --label "Create New Character" >/dev/null
wait_for_widget "Alias"
cli --port "$CTRL_PORT" set_text --label "Alias" --text "Alice" >/dev/null
cli --port "$CTRL_PORT" key --key enter >/dev/null
wait_for_widget "Black Rose"
cli --port "$CTRL_PORT" wait_frames --n 2 >/dev/null

OUT_DIR="$(mktemp -d)"
BASE="$OUT_DIR/inspect-base.json"
HOVER="$OUT_DIR/inspect-hover.json"
HOVER_OUT="$OUT_DIR/inspect-hover-out.json"

# Center of the "Lazarus" agency row (UI-space), computed from inspect geometry.
read -r HX HY < <(cli --port "$CTRL_PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const names = new Set(["Noxis","Lazarus","Caliber","Static","Black Rose"]);
const rows = (r.nodes ?? []).filter((w) => w.role === "button" && names.has(w.label));
if (rows.length < 5) { console.error(`expected 5 agency rows, got ${rows.length}`); process.exit(1); }
const target = rows.find((w) => w.label === "Lazarus" && w.w > 0 && w.h > 0);
if (!target) { console.error("Lazarus agency row missing"); process.exit(1); }
console.log(`${Math.round(target.x + target.w/2)} ${Math.round(target.y + target.h/2)}`);
')

cli --port "$CTRL_PORT" inspect > "$BASE"
bun -e '
const base = JSON.parse(await Bun.file(process.argv[1]).text());
const text = (base.nodes ?? []).map((n) => `${n.value || ""} ${n.label || ""}`).join("\n");
if (!text.includes("The Noxis corporation")) {
  console.error("baseline agency details did not show Noxis");
  process.exit(1);
}
if (text.includes("Like the mythical phoenix")) {
  console.error("baseline agency details already showed Lazarus");
  process.exit(1);
}
' "$BASE"

# (A) Hover the Lazarus row -> its node.hovered must become true and the right
# details column must preview Lazarus without moving focus.
cli --port "$CTRL_PORT" hover_at --x "$HX" --y "$HY" >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 2 >/dev/null
cli --port "$CTRL_PORT" inspect > "$HOVER"

bun -e '
const names = new Set(["Noxis","Lazarus","Caliber","Static","Black Rose"]);
const rowsOf = (j) => (j.nodes ?? []).filter((w) => w.role === "button" && names.has(w.label));
const hover = JSON.parse(await Bun.file(process.argv[1]).text());
const hx = Number(process.argv[2]), hy = Number(process.argv[3]);

const under = rowsOf(hover).find((w) => hx >= w.x && hx < w.x + w.w && hy >= w.y && hy < w.y + w.h);
if (!under || under.label !== "Lazarus") {
  console.error(`hover point did not land on Lazarus row: ${JSON.stringify(under)}`);
  process.exit(1);
}
const hovered = rowsOf(hover).filter((w) => w.hovered === true);
if (hovered.length !== 1 || hovered[0].label !== "Lazarus") {
  console.error(`expected only Lazarus hovered, got ${JSON.stringify(hovered.map((w)=>w.label))}`);
  process.exit(1);
}
const text = (hover.nodes ?? []).map((n) => `${n.value || ""} ${n.label || ""}`).join("\n");
if (!text.includes("Resurrection Ability") ||
    !text.includes("Like the mythical phoenix")) {
  console.error("hovering Lazarus did not update agency details");
  process.exit(1);
}
if (text.includes("The Noxis corporation")) {
  console.error("hovering Lazarus left Noxis details visible");
  process.exit(1);
}
' "$HOVER" "$HX" "$HY"

# Hover empty space -> no agency row should be hovered any more.
cli --port "$CTRL_PORT" hover_at --x 5 --y 5 >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 2 >/dev/null
cli --port "$CTRL_PORT" inspect > "$HOVER_OUT"

bun -e '
const names = new Set(["Noxis","Lazarus","Caliber","Static","Black Rose"]);
const rowsOf = (j) => (j.nodes ?? []).filter((w) => w.role === "button" && names.has(w.label));
const hoverOut = JSON.parse(await Bun.file(process.argv[1]).text());
const hovered = rowsOf(hoverOut).filter((w) => w.hovered === true);
if (hovered.length !== 0) {
  console.error(`expected no agency hovered after hover-out, got ${JSON.stringify(hovered.map((w)=>w.label))}`);
  process.exit(1);
}
' "$HOVER_OUT"

# (B) Hovering must NOT create a character or leave CREATECHARACTER.
CURRENT_STATE=$(cli --port "$CTRL_PORT" state | bun -e '
console.log(JSON.parse(await new Response(Bun.stdin.stream()).text()).state || "");
')
if [ "$CURRENT_STATE" != "CREATECHARACTER" ]; then
  echo "hover escaped CREATECHARACTER into state $CURRENT_STATE" >&2
  exit 1
fi

# (C) Double-submit guard: click the "Noxis" row twice in quick succession.
# Only one character must be created.
read -r NX NY < <(cli --port "$CTRL_PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const target = (r.nodes ?? []).find((w) => w.role === "button" && w.label === "Noxis" && w.w > 0 && w.h > 0);
if (!target) { console.error("Noxis agency row missing"); process.exit(1); }
console.log(`${Math.round(target.x + target.w/2)} ${Math.round(target.y + target.h/2)}`);
')
cli --port "$CTRL_PORT" click_at --x "$NX" --y "$NY" >/dev/null
cli --port "$CTRL_PORT" click_at --x "$NX" --y "$NY" >/dev/null 2>&1 || true
cli --port "$CTRL_PORT" wait_for_state --state LOBBY --timeout-ms 15000 >/dev/null

# The lobby persists the user db asynchronously; poll briefly until the created
# character lands, then assert EXACTLY one "Alice" exists (no double-create).
CREATED_COUNT=0
for i in $(seq 1 50); do
  CREATED_COUNT=$(bun -e '
  const db = JSON.parse(await Bun.file(process.argv[1]).text());
  const chars = db.users?.alice?.chars ?? [];
  console.log(chars.filter((c) => c.name === "Alice").length);
  ' "$LOBBY_DB" 2>/dev/null || echo 0)
  if [ "$CREATED_COUNT" != "0" ]; then break; fi
  sleep 0.1
done
if [ "$CREATED_COUNT" != "1" ]; then
  echo "rapid agency submit created $CREATED_COUNT Alice characters (want exactly 1)" >&2
  cat "$LOBBY_DB" >&2
  exit 1
fi

echo "PASS 17_character_create_focus"
