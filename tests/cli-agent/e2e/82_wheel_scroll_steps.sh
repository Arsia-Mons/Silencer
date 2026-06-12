#!/usr/bin/env bash
# Wheel-scroll step arithmetic vs origin source (the PARITY "scrolling
# behaviors" close-out — no scrolled-state golden needed because the step
# arithmetic AND the scrolled row content are asserted here):
#
# A. Controls keybind list. Origin: one wheel notch = ONE row
#    (UiInputRouter.cpp:70-77 — amount = -int(wheelY)), accumulated then
#    clamped to [0, Action::Count - visibleRows] with no wrap/overscroll
#    (options_controls_screen.cpp:105-107 + MaxScroll :71-74). Ours: the
#    ScrollView steps offset by kRowH per notch (step = row pitch ⇒ 1 row/
#    notch) and clamps offset to [0, content - viewport]. Same 30-action
#    table order (input/keybinds.h Action enum = origin's), so row CONTENT at
#    any scroll position matches; both stop with the LAST action row flush at
#    the list end. Asserted below via the virtualized rows actually
#    intersecting the ControlsList viewport after each notch.
#
# B. In-game buy/tech overlay window. Origin has NO wheel scroll in-game
#    (InGameUiController::ApplyActions handles no Scroll action); the 5-row
#    window only FOLLOWS the selection (ClampBuyTechSelection /
#    UpdateOverlayState, InGameUiController.cpp:34-39 + :100-106:
#    selected >= scrolled+5 ⇒ scrolled = selected-4; selected < scrolled ⇒
#    scrolled = selected; floor 0) — and the selection itself cannot leave the
#    window via input (origin registers interactables for the VISIBLE rows
#    only, hud_buy_tech_overlay.cpp:90-103, and spatial nav finds no candidate
#    below the bottom ghost row). Ours ports that exact arithmetic
#    (world_session_model.cpp); asserted via tech_scrolled_index /
#    tech_selected_index through the control socket. The tutorial tech list
#    has 2 rows (peer techchoices = LASER|ROCKET) — sufficient: even on an
#    overflowing list the window cannot move via input in origin (or here),
#    so the assertable behavior is selection clamp + window pinned at 0.
#
# C. Lobby Select-Map list: origin NEVER scrolls it — mapScrollPos is declared
#    (game_create_panel.h:52), read once (game_create_panel_map_form.cpp:304),
#    and written NOWHERE; create-mode Scroll actions route exclusively to the
#    Game Options box (game_create_panel.cpp:217-222). Our map list renders the
#    bundled maps (≤14 rows, all visible) with no scroll surface — equivalent.
#    Nothing to drive; recorded here + in PARITY.md.
set -euo pipefail
. "$(dirname "$0")/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT
wait_alive "$PORT"

wait_for_node() {
  local label="$1"
  for i in $(seq 1 100); do
    found=$(cli --port "$PORT" inspect | LABEL="$label" bun -e \
      'const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
       const l = process.env.LABEL;
       console.log((r.nodes||[]).some((w)=>w.label===l||w.control_id===l) ? "yes" : "no");' 2>/dev/null || echo no)
    if [ "$found" = "yes" ]; then return 0; fi
    sleep 0.05
  done
  echo "node '$label' never appeared" >&2
  return 1
}

# --- Part A: controls list wheel steps ---
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
cli --port "$PORT" click --label "Options" >/dev/null
wait_for_node "OptionsControls"
cli --port "$PORT" click --label "OptionsControls" >/dev/null
wait_for_node "ControlsList"
cli --port "$PORT" wait_frames --n 3 >/dev/null

# Center of the scroll viewport — the wheel routes to the hovered node and
# bubbles to the ScrollView (SIL-111).
read -r LX LY <<<"$(cli --port "$PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const l = (r.nodes||[]).find((n)=>n.control_id==="ControlsList");
if (!l) { console.error("no ControlsList"); process.exit(1); }
console.log(`${Math.round(l.x + l.w/2)} ${Math.round(l.y + l.h/2)}`);')"

# Prints "<min> <max> <firstLabel>" for BindP rows intersecting the viewport.
view_rows() {
  cli --port "$PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const n = r.nodes||[];
const l = n.find((x)=>x.control_id==="ControlsList");
const hit = (a,b)=> a.x < b.x+b.w && b.x < a.x+a.w && a.y < b.y+b.h && b.y < a.y+a.h;
const rows = n.filter((x)=>/^BindP\d+$/.test(x.control_id??"") && hit(x,l))
  .map((x)=>Number(x.control_id.slice(5))).sort((a,b)=>a-b);
if (!rows.length) { console.error("no keybind rows in view"); process.exit(1); }
// Row content: the action label text vertically aligned with the first row.
const first = n.find((x)=>x.control_id===`BindP${rows[0]}`);
const label = n.find((x)=>x.role==="text" && /:$/.test(x.value??"") &&
  Math.abs((x.y??0) - first.y) < first.h && hit(x,l));
console.log(`${rows[0]} ${rows[rows.length-1]} ${label ? label.value : "?"}`);'
}

wheel() { # wheel <dy>
  cli --port "$PORT" scroll --x "$LX" --y "$LY" --dy "$1" >/dev/null
  cli --port "$PORT" wait_frames --n 2 >/dev/null
}
expect() {
  if [ "$2" != "$3" ]; then echo "FAIL 82: $1 = [$2], want [$3]" >&2; exit 1; fi
}

# Rest: rows 0..3 in view, first row is Move Up (ACTION_TABLE order).
expect "rest window"        "$(view_rows)" "0 3 Move Up:"
wheel -1   # one notch down = one row
expect "one notch down"     "$(view_rows)" "1 4 Move Down:"
wheel 1    # one notch up = back to rest
expect "one notch back up"  "$(view_rows)" "0 3 Move Up:"
wheel 1    # overscroll at the top clamps
expect "top clamp"          "$(view_rows)" "0 3 Move Up:"
wheel -100 # clamp at the end: last action row (UI Cancel, index 29) lands flush
expect "bottom clamp"       "$(view_rows)" "26 29 UI Left:"
wheel -1   # still clamped
expect "bottom clamp holds" "$(view_rows)" "26 29 UI Left:"
wheel 1    # one notch back up from the clamped end
expect "one notch off end"  "$(view_rows)" "25 28 UI Down:"

cli --port "$PORT" click --label "ControlsBack" >/dev/null
wait_for_node "OptionsControls"

# --- Part B: in-game tech overlay 5-row window (selection-follow, no wheel) ---
cli --port "$PORT" key --key escape >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null
cli --port "$PORT" click --label Tutorial >/dev/null
cli --port "$PORT" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 15000 >/dev/null
cli --port "$PORT" pause >/dev/null
for _ in $(seq 1 100); do
  if cli --port "$PORT" world_state | bun -e '
    const s = JSON.parse(await new Response(Bun.stdin.stream()).text());
    const r = s.result ?? s;
    process.exit((r.players?.length ?? 0) > 0 ? 0 : 1);'; then break; fi
  cli --port "$PORT" step --frames 4 >/dev/null
done

field() {
  cli --port "$PORT" ingame_ui_mode --mode status | bun -e \
    'const s=JSON.parse(await new Response(Bun.stdin.stream()).text());const r=s.result??s;console.log(r[process.argv[1]])' "$1"
}
press() {
  cli --port "$PORT" key --key "$1" >/dev/null
  cli --port "$PORT" wait_frames --n 2 >/dev/null
}

cli --port "$PORT" ingame_ui_mode --mode tech >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null
expect "tech overlay open" "$(field tech_active)" true
COUNT="$(field tech_item_count)"
expect "tutorial tech list" "$COUNT" 2
expect "tech window rest" "$(field tech_scrolled_index)" 0

# Ghost rows mounted = min(5, items) — the visible window only (origin
# registers only the visible rows, hud_buy_tech_overlay.cpp:90).
ROWS="$(cli --port "$PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
console.log((r.nodes||[]).filter((n)=>n.control_id==="BuyTechRow").length);')"
expect "visible ghost rows" "$ROWS" "$COUNT"

press down
expect "Down moves the selection" "$(field tech_selected_index)" 1
expect "window holds (selected < scrolled+5)" "$(field tech_scrolled_index)" 0
press down  # origin-exact: no interactable below ⇒ selection clamps, no wrap
expect "selection clamps at the list end" "$(field tech_selected_index)" 1
expect "window still at rest" "$(field tech_scrolled_index)" 0
press up
expect "selection back at the top" "$(field tech_selected_index)" 0
expect "window floor 0" "$(field tech_scrolled_index)" 0

press escape
expect "Esc closes the overlay" "$(field tech_active)" false

echo "PASS 82_wheel_scroll_steps"
