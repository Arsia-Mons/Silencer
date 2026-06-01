#!/usr/bin/env bash
# SIL-19: the Options cluster is now Tier-1 cppx navigation — an Overlay pushed
# over the menu (the session phase stays MAINMENU; no GoToState mounting). This
# drives the modern flow on the retained cppx path: Options → Audio/Display
# sub-screens, use_settings live-preview, dirty tracking, and Cancel = revert.
set -euo pipefail
. "$(dirname "$0")/lib.sh"

PORT=$(pick_port)
PID=$(start_silencer "$PORT")
trap "stop_silencer $PID $PORT" EXIT
wait_alive "$PORT"

cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

# MainMenu → Options (overlay; phase unchanged).
cli --port "$PORT" click --label Options >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
cli --port "$PORT" state | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
if (r.state !== "MAINMENU") { console.error(`Options should be an overlay, not a state change (got ${r.state})`); process.exit(1); }
'
cli --port "$PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const labels = new Set((r.nodes ?? []).filter((n) => n.role === "button").map((b) => b.label));
for (const x of ["Audio", "Display", "Done", "Cancel"]) {
  if (!labels.has(x)) { console.error(`Options missing button: ${x}`); process.exit(1); }
}
'

# Options → Display: toggle Fullscreen (live preview reflected in the tree).
cli --port "$PORT" click --label OptionsDisplay >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
before=$(cli --port "$PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const c = (r.nodes ?? []).find((n) => n.role === "checkbox" && n.value === "Fullscreen");
if (!c) { console.error("no Fullscreen checkbox"); process.exit(1); }
console.log(c.checked);
')
cli --port "$PORT" click --label FullscreenToggle >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null
after=$(cli --port "$PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const c = (r.nodes ?? []).find((n) => n.role === "checkbox" && n.value === "Fullscreen");
console.log(c.checked);
')
[ "$before" != "$after" ] || { echo "Fullscreen toggle did not live-preview ($before -> $after)"; exit 1; }
cli --port "$PORT" click --label OptionsBack >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null

# Options → Audio: bump the volume stepper (live preview).
cli --port "$PORT" click --label OptionsAudio >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
vol0=$(cli --port "$PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const t = (r.nodes ?? []).find((n) => (n.value ?? "").startsWith("Volume:"));
if (!t) { console.error("no volume readout"); process.exit(1); }
console.log(t.value);
')
cli --port "$PORT" click --label VolumeUp >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null
vol1=$(cli --port "$PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const t = (r.nodes ?? []).find((n) => (n.value ?? "").startsWith("Volume:"));
console.log(t.value);
')
[ "$vol0" != "$vol1" ] || { echo "volume stepper did not live-preview ($vol0 -> $vol1)"; exit 1; }
cli --port "$PORT" click --label OptionsBack >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null

# Options root reflects the unsaved edits, and Cancel reverts + pops to MainMenu.
cli --port "$PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
if (!(r.nodes ?? []).some((n) => n.value === "Unsaved changes")) {
  console.error("Options did not show a dirty indicator after edits"); process.exit(1);
}
'
cli --port "$PORT" click --label OptionsCancel >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
cli --port "$PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const onMenu = (r.nodes ?? []).some((n) => n.role === "button" && n.label === "Play Online");
const stillOptions = (r.nodes ?? []).some((n) => n.role === "button" && n.label === "Done");
if (!onMenu || stillOptions) { console.error("Cancel did not revert + pop to MainMenu"); process.exit(1); }
'

echo "PASS 10_navigate"
