#!/usr/bin/env bash
# SIL-12: rows-of-combos binding caps. An action holds at most COMBO_CAP (4)
# OR-combos; each combo is an AND-chord of at most CHORD_CAP (3) keys. Over-cap
# rows are REJECTED (never silently truncated) at the CLI put funnel, and a
# valid combo set persists to disk and reads back unchanged.
#
# The caps are device-agnostic, so this smoke uses keyboard bindings: gamepad
# button strings only parse when the gamepad subsystem is initialized, which
# `--headless` does not start.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'cli --port "$PORT" keybind delete sil12caps >/dev/null 2>&1 || true; stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

# Fresh profile forked from default so the active profile is never disturbed.
cli --port "$PORT" keybind delete sil12caps >/dev/null 2>&1 || true
cli --port "$PORT" keybind new --profile sil12caps --from default >/dev/null

# Valid: one single-key combo + one 3-key chord (both within caps).
cli --port "$PORT" keybind put --profile sil12caps --action fire \
  --bindings KEY:F KEY:A,KEY:B,KEY:C >/dev/null

# Reads back exactly as written (persisted to disk + CLI automation lookup).
cli --port "$PORT" keybind get --profile sil12caps --action fire | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const b = r.bindings;
if (!Array.isArray(b) || b.length !== 2) {
  console.error("expected 2 combos, got " + JSON.stringify(b)); process.exit(1);
}
if (b[0] !== "KEY:F") {
  console.error("combo 0 != KEY:F: " + JSON.stringify(b)); process.exit(1);
}
if (!Array.isArray(b[1]) || b[1].length !== 3) {
  console.error("combo 1 not a 3-key chord: " + JSON.stringify(b)); process.exit(1);
}
'

# Over CHORD_CAP: a 4-key chord is rejected (not truncated to 3).
if cli --port "$PORT" keybind put --profile sil12caps --action fire \
   --bindings KEY:A,KEY:B,KEY:C,KEY:D >/dev/null 2>&1; then
  echo "FAIL: 4-key chord accepted (CHORD_CAP=3 not enforced)" >&2
  exit 1
fi

# Over COMBO_CAP: five OR-combos are rejected.
if cli --port "$PORT" keybind put --profile sil12caps --action fire \
   --bindings KEY:A KEY:B KEY:C KEY:D KEY:E >/dev/null 2>&1; then
  echo "FAIL: 5 combos accepted (COMBO_CAP=4 not enforced)" >&2
  exit 1
fi

# The rejected puts must not have partially applied: fire still has 2 combos.
cli --port "$PORT" keybind get --profile sil12caps --action fire | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
if ((r.bindings ?? []).length !== 2) {
  console.error("over-cap put mutated state: " + JSON.stringify(r.bindings));
  process.exit(1);
}
'

echo "18_keybind_caps: OK"
