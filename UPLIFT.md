# UPLIFT.md — design-intent audit ledger

Parity (PARITY.md) reproduced origin/main exactly, defects included. This
ledger catalogs where the golden was a faulty target: origin implementation
accidents we recreated that should instead be elevated to the inferred
intent. Findings are PROPOSED until the user signs off per-item.

State: IN PROGRESS

## Coverage

| Unit | State | Findings |
|---|---|---|
| [systemic] whole-frame magnify: glyph striping | EXAMINED 2026-06-12 | U-1 |
| [systemic] whole-frame magnify: sprite phase striping | UNEXAMINED | |
| [systemic] background image double-scaling / banding | UNEXAMINED | |
| [systemic] float-rect flooring: 1px seams & jitter | UNEXAMINED | |
| [systemic] palette quantization & dim formulas | UNEXAMINED | |
| [systemic] spacing/alignment consistency across siblings | UNEXAMINED | |
| mainmenu | UNEXAMINED | |
| options | UNEXAMINED | |
| options_audio | UNEXAMINED | |
| options_display | UNEXAMINED | |
| options_controls | UNEXAMINED | |
| lobby_connect | UNEXAMINED | |
| character_create | UNEXAMINED | |
| cc_alias | UNEXAMINED | |
| cc_select_agency | UNEXAMINED | |
| lobby_screen | UNEXAMINED | |
| create_game | UNEXAMINED | |
| game_staging | UNEXAMINED | |
| tech_select | UNEXAMINED | |
| mission_summary | UNEXAMINED | |
| message_modal / password_modal | UNEXAMINED | |
| ingame: hud_base / top_ticker / status_lines | UNEXAMINED | |
| ingame: chat (open + history) / messages | UNEXAMINED | |
| ingame: player_list / system_camera | UNEXAMINED | |
| ingame: tech_overlay / buy_tech / secret_overlay | UNEXAMINED | |
| ingame: quit_prompt | UNEXAMINED | |

## Findings

### U-1: position-dependent glyph striping from origin's whole-frame magnify
- **Unit:** [systemic] whole-frame magnify: glyph striping
- **Class:** ARTIFACT
- **Severity:** med
- **Golden shows:** Same letter renders with different pixel patterns — and
  different overall size — depending on absolute screen position. Measured in
  `tests/cli-agent/e2e/golden/mainmenu.png`: 'o' in "Tutorial" (x1224,y376) is
  21×23 px with column-dup runs [3,2,2,7,2,2,3]; 'o' in "Options" (x1263,y678)
  is 20×22 px with runs [2,2,2,7,2,3,2]. The two ADJACENT 'b's in "Lobby"
  (x1463 vs x1488) carry shifted stripe phase [2,2,3,2,2,2,3,2] vs
  [2,2,2,3,2,2,2,3]. The three 'o's inside the single string "Connect To
  Lobby" each stripe differently (float pen drifts x-phase within a string).
  Evidence: `docs/plans/uplift-evidence/systemic-glyph-striping/`
  (o_tutorial_vs_options.png, t_tutorial_vs_options.png,
  bb_lobby_adjacent.png, lobby_word_4x.png).
- **Origin cause:** `origin/main:clients/silencer/src/render/clay_ui_compositor.cpp`
  Render() ~L1185-1231 — menus composite at virtual res (853×480 @1080p), then
  whole-frame nearest magnify by s=2.25 with sx=int(dx/s), sy=int(dy/s); each
  virtual px becomes 2 or 3 device px in a {3,2,2,2} cycle phased by absolute
  frame position. Nothing per-glyph is authored.
- **Inferred intent:** "Draw this bitmap glyph at UI scale s" — every instance
  of a glyph identical, evenly distributed pixels. (Seed candidate, confirmed
  by user in the audit brief.)
- **Elevated target:** Canonical-phase glyph rendering: each glyph baked once
  at fixed resample phase (src=int(lx/s), lx from 0), drawn at integer device
  pen positions with uniform advance (round(11·2.25)=25 device px). Keeps the
  2.25× nearest retro chunkiness; removes positional unevenness. Replaces
  `glyph_fonts.{h,cpp}` string_variant (memo (face,color,string,X%18,Y%18)) +
  the render_text_glyphs executor intercept; delete the per-position variant
  machinery and the PARITY.md pen nudges it obsoletes.
- **Parity blast radius:** Systemic — every golden containing menu text (all
  menu/lobby screens, modals, mission_summary, in-game overlays using
  exact-color faces). All affected goldens superseded per protocol
  (ORIGIN_GOLDENS.md + PARITY.md UPLIFTED rows).
- **Effort:** M
- **Ticket:** SIL-190
- **Status:** TICKETED
