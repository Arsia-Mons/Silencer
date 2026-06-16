#!/usr/bin/env bash
# Visual regression: hover/focus states.
#
# 1) Oval ramp steady state: hover the mainmenu's first oval (Tutorial) and
#    gate the settled frame against the origin golden hover_mainmenu_oval.png
#    (origin button.cpp: hover targets phase 4 — sprite base+4, brightness
#    136, label at 136).
# 2) lobby_connect Chrome negative: hovering the Login button must change
#    NOTHING inside the button's cell. That dialog's baked button wells own the
#    visual affordance; real LOBBY action buttons are covered separately by
#    83_lobby_hover_visual.sh.
set -euo pipefail
. "$(dirname "$0")/lib.sh"

GOLDEN="$REPO_ROOT/tests/cli-agent/e2e/golden/hover_mainmenu_oval.png"
RENDER="/tmp/cppx_renders/hover_mainmenu_oval_e2e.png"
[ -f "$GOLDEN" ] || { echo "golden missing: $GOLDEN" >&2; exit 1; }

# --- 1. oval hover ramp vs golden -------------------------------------------
bash "$REPO_ROOT/tools/cap/cap_hover_cppx.sh" "$RENDER"
out="$(python3 "$REPO_ROOT/tools/cap/pixdiff_tolerant.py" "$RENDER" "$GOLDEN")"
echo "hover_mainmenu_oval: $out"
echo "$out" | head -1 | grep -q "PASS" || { echo "hover state diverged from golden" >&2; exit 1; }

# --- 2. chrome hover negative ------------------------------------------------
PORT=$(pick_port); PID=$(start_silencer "$PORT")
trap 'stop_silencer "$PID" "$PORT"' EXIT
wait_alive "$PORT"
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
cli --port "$PORT" resize --w 1920 --h 1080 >/dev/null
cli --port "$PORT" click --label "Connect To Lobby" >/dev/null
cli --port "$PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000 >/dev/null
cli --port "$PORT" wait_frames --n 5 >/dev/null
NODE=$(cli --port "$PORT" inspect | bun -e '
  const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
  const n = (r.nodes || []).find((w) => (w.label || "").startsWith("Login"));
  if (!n) { console.error("no Login button"); process.exit(1); }
  console.log(`${n.x} ${n.y} ${n.w} ${n.h}`)')
read -r NX NY NW NH <<<"$NODE"
cli --port "$PORT" screenshot --out /tmp/hover_chrome_before.png >/dev/null
cli --port "$PORT" hover_at --x "$(bun -e "console.log(Math.round($NX + $NW / 2))")" \
  --y "$(bun -e "console.log(Math.round($NY + $NH / 2))")" >/dev/null
sleep 0.6
cli --port "$PORT" screenshot --out /tmp/hover_chrome_after.png >/dev/null
python3 - "$NX" "$NY" "$NW" "$NH" <<'PY'
import sys
from PIL import Image
import numpy as np
x, y, w, h = (float(a) for a in sys.argv[1:5])
a = np.array(Image.open("/tmp/hover_chrome_before.png").convert("RGB"), dtype=int)
b = np.array(Image.open("/tmp/hover_chrome_after.png").convert("RGB"), dtype=int)
x0, x1 = int(x * 1.5) - 2, int((x + w) * 1.5) + 2
y0, y1 = int(y * 1.5) - 2, int((y + h) * 1.5) + 2
d = (np.abs(a - b).sum(axis=2) > 0)[y0:y1, x0:x1].sum()
print(f"lobby_connect chrome button region diff px under hover: {d}")
if d:
    raise SystemExit("lobby_connect chrome button changed on hover")
PY

echo "PASS 75_visual_regression_hover"
