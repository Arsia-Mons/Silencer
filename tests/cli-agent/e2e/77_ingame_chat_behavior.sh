#!/usr/bin/env bash
# In-game chat entry/log behavior vs origin (InGameUiController::ApplyActions +
# WorldMessaging):
#   - Enter on the compose input SENDS: the buffer clears and the input
#     closes (origin SubmitText branch). NOTE: in single-player the sent line
#     does NOT land in the history — origin's WorldMessaging::SendChat fires
#     a MSG_CHAT packet at GetAuthorityPeer() (yourself) whose socket is
#     never bound in the tutorial (tick_singleplayer.cpp comments out
#     world.Listen), so the packet drops. Verified against the origin binary
#     2026-06-11: after Enter, chat_active=false and no history line renders.
#     History appends are covered by the lobby-match path; here we assert the
#     origin-exact single-player behavior.
#   - Esc closes WITHOUT sending and drops the buffer (origin Cancel branch).
#   - Activating the channel prefix toggles TEAM/ALL (origin
#     "ingame.chat.channel" interactable -> chatwithteam flip).
#   - The compose buffer caps at 100 chars (origin Player::chatText[101]).
# Introspection: the ingame_ui_mode status result (chat_active/chat_with_team/
# chat_line_count/chat_text_len).
set -euo pipefail
. "$(dirname "$0")/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT
wait_alive "$PORT"

cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
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

status() { cli --port "$PORT" ingame_ui_mode --mode status; }
field() { # field <json> <name>
  bun -e 'const s=JSON.parse(process.argv[1]);const r=s.result??s;console.log(r[process.argv[2]])' "$1" "$2"
}
expect() { # expect <label> <actual> <want>
  if [ "$2" != "$3" ]; then
    echo "FAIL 77: $1 = $2, want $3" >&2
    exit 1
  fi
}

# --- Esc closes without sending --------------------------------------------
st="$(status)"
expect "baseline chat_line_count" "$(field "$st" chat_line_count)" 0
cli --port "$PORT" ingame_ui_mode --mode chat --chat-text "drop me" >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null
cli --port "$PORT" key --key escape >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
st="$(status)"
expect "esc closes input" "$(field "$st" chat_active)" false
expect "esc drops the buffer" "$(field "$st" chat_text_len)" 0
expect "esc sends nothing" "$(field "$st" chat_line_count)" 0

# --- Enter sends + closes ----------------------------------------------------
cli --port "$PORT" ingame_ui_mode --mode chat --chat-text "hello parity" >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null
cli --port "$PORT" key --key enter >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
cli --port "$PORT" step --frames 2 >/dev/null
st="$(status)"
expect "enter closes input" "$(field "$st" chat_active)" false
expect "enter clears the buffer" "$(field "$st" chat_text_len)" 0
# origin-exact: single-player MSG_CHAT drops at the unbound self socket
expect "no history append in single-player" "$(field "$st" chat_line_count)" 0

# --- Channel toggle (activate the prefix ghost target) ----------------------
cli --port "$PORT" ingame_ui_mode --mode chat --chat-text "x" >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null
st="$(status)"
expect "default channel is ALL" "$(field "$st" chat_with_team)" false
# locate the channel target from the retained tree and click its center
read -r CX CY <<<"$(cli --port "$PORT" inspect | bun -e '
  const s = JSON.parse(await new Response(Bun.stdin.stream()).text());
  const r = s.result ?? s;
  const n = r.nodes.find(n => n.control_id === "IngameChatChannel");
  if (!n) { console.error("channel target not found"); process.exit(1); }
  console.log(Math.round(n.x + n.w / 2), Math.round(n.y + n.h / 2));')"
cli --port "$PORT" click_at --x "$CX" --y "$CY" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
st="$(status)"
expect "toggle flips to TEAM" "$(field "$st" chat_with_team)" true
expect "toggle keeps the input open" "$(field "$st" chat_active)" true

# --- Compose cap = 100 (origin chatText[101]) --------------------------------
LONG="$(printf 'a%.0s' $(seq 1 150))"
cli --port "$PORT" ingame_ui_mode --mode chat --chat-text "$LONG" >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null
st="$(status)"
expect "compose buffer caps at 100" "$(field "$st" chat_text_len)" 100
cli --port "$PORT" key --key escape >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null

echo "PASS 77_ingame_chat_behavior"
