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

# Options → Display: toggle Fullscreen (SIL-100: the boolean is now a sprite
# BooleanSettingRow — an Oval label button + a two-cell toggle indicator, not a
# checkbox node — so verify the toggle control exists and click it; its live-
# preview EFFECT is asserted below when Options shows the unsaved edits).
cli --port "$PORT" click --label OptionsDisplay >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
cli --port "$PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const fs = (r.nodes ?? []).find((n) => n.role === "button" && n.label === "Fullscreen");
if (!fs) { console.error("no Fullscreen toggle"); process.exit(1); }
'
cli --port "$PORT" click --label FullscreenToggle >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null
cli --port "$PORT" click --label OptionsBack >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null

# Options → Audio: SIL-108 removed the invented -/+ volume slider for origin/main
# parity — the Audio screen is the Music on/off toggle (a sprite BooleanSettingRow
# button), no volume readout. Verify the slider is gone and the Music toggle exists.
cli --port "$PORT" click --label OptionsAudio >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
cli --port "$PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const nodes = r.nodes ?? [];
if (nodes.some((n) => (n.value ?? "").startsWith("Volume:"))) {
  console.error("Audio still shows a Volume readout (invented slider not removed)"); process.exit(1);
}
const labels = new Set(nodes.filter((n) => n.role === "button").map((b) => b.label));
if (labels.has("VolumeUp") || labels.has("+") || labels.has("-")) {
  console.error("Audio still has volume stepper buttons"); process.exit(1);
}
if (!nodes.some((n) => n.role === "button" && n.label === "Music")) {
  console.error("Audio missing the Music toggle"); process.exit(1);
}
'
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
const onMenu = (r.nodes ?? []).some((n) => n.role === "button" && n.label === "Connect To Lobby");
const stillOptions = (r.nodes ?? []).some((n) => n.role === "button" && n.label === "Done");
if (!onMenu || stillOptions) { console.error("Cancel did not revert + pop to MainMenu"); process.exit(1); }
'

echo "PASS 10_navigate"
