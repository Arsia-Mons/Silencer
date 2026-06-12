# Origin in-game HUD visual spec (640×480 indexed screenbuffer)

Extracted 2026-06-11 from .worktrees/origin-capture source (read-only Explore pass).
Companion to the in-game golden capture plan in PARITY.md. All coordinates in 640×480
space; colors are palette indices + brightness; sprites from resource banks.

## Composition root
`InGameHud.cpp:20-67` `BuildInGameHudUi()`, trigger `view.mapLoaded && view.localPlayer.valid`.
Build order: system camera frames → status sprites/gauges → readouts → team strip →
secret sprites + hack progress (if in team base) → trace timer → buy/tech overlay →
chat overlay. Overlays dispatcher `InGameOverlays.cpp:231-243` order: status lines →
top ticker → center message → player list (F1) → quit prompt.
Global animation source: `renderer.GetHudAnimationPhase()` → Uint8 phase, +1/frame.

## 1. Status sprites & gauges — hud_status_sprites.cpp:18-181
- Minimap frame: bank94 idx0 at SpriteX/Y(94,0), brightness 128.
- Low fuel warning: bank95 idx8 at SpriteX/Y(94,0) if fuelLow.
- Fuel bar: bank95 idx6, fill width = (fuel/maxFuel)*SpriteWidth(95,6) (int truncate),
  src slice (0,0,fuelW,H); fuel mask bank95 idx5 over it. Brightness 128.
- Health bar (vertical, bottom-up): bank95 idx0; healthY = H - (health/maxHealth)*H;
  draw at (SpriteX, SpriteY+healthY) src (0,healthY,W,H-healthY).
  Warning flash bank95 idx3 if health<=50% AND phase%8<=3.
- Shield bar: bank95 idx1, same vertical-fill arithmetic, shieldY clamped >=0.
  Overshield pulse (shield>maxShield): base 136, time=6, phase%(12): +=(phase%6)*2 up /
  (6-(phase%6))*2 down → range 136-148. Warning flash bank95 idx4 if <=50% AND phase%8<=3.
- Poison: bank97 idx5 at absolute (183,453) if poisonedBy!=0.
- Files bar: bank95 idx7, fill width = (files/maxFiles)*SpriteWidth(95,7).
- Weapon face: bank96 idx 1/2/3/4 (Blaster/Laser/Rocket/Flamer); glow idx 5/6/7/8 same pos;
  selector bracket bank96 idx0 at SpriteY(96,0) + currentWeapon*14.
- Inventory frame: bank94 idx2.
- Inventory slots: xoffsets {612,584,556,528}, yoffsets {13,13,11,7}; icon bank97
  idx=InvIdToResIndex(item), brightness selected?128:32; slot letter (InvIdToLetter)
  TextSize=Tiny at (xoffsets[i]-2, yoffsets[i]) LegacyPalette(0, slotBrightness).
- All draws via HudFloatingElement → AllocSpriteCustomData({bank,index,src...,brightness,rampColor,rampPlus}).

## 2. Numeric readouts — hud_readouts.cpp:14-103
- Current ammo: (117,457) 40×18, TextSize=HudCounter, leading space if <10,
  LegacyPalette(0,128,false,true) — drawAlpha=true.
- Per-weapon ammo column at x=0, TextSize=Tiny, centered, LegacyPalette(0,128):
  Blaster "99" always (0,414); Laser (0,428), Rocket (0,442), Flamer (0,456) only if >0.
- Credits: (572,456) 60×18 HudCounter, LegacyPalette(202).
- Health numeric: (145,463) 26×8 Tiny centered, LegacyPalette(161).
- Shield numeric: (468,463) 26×8 Tiny centered, LegacyPalette(202).
- Inventory counts: (xoffsets[i]+20, yoffsets[i]+20) 32×10 TinyCounter, LegacyPalette(0),
  only if count>1.
- Trace timer (:90-103): "Government Trace Time: N" at (20,350) 180×12 Body,
  LegacyPalette(0,136); shown if team.beamingTerminalTraceTime>0 OR player.tracetime>0.

## 3. Team strip — hud_teams.cpp:16-127
- Frame: 1 team → bank94 idx1; 2+ teams → bank103 idx0 cap at SpriteY(103,0,-133+(teams-1)*20)
  + idx1 body.
- Rows: teamyoffset starts 5, +20/team. Peers: bank103, alive idx4+i / dead idx8+i,
  x=25+17*i. Brightness 128.
  In-base pulse: rampColor=210, rampPlus triangle (phase>>2)%8 → 0-4.
  Has-secret pulse: rampColor=114, rampPlus triangle phase%16 → 0-8.
- Emblem: (5, yoffset+1), AllocTeamEmblemCustomData({181, agency, color, 17, true}).
- Secret slots ×3: filled idx2 / empty idx3, x=-(9*(3-i))+11; beaming (secrets==i) →
  idx3 color=224; uncertain flicker (playerswithsecret > i-secrets AND tick%12<6) → idx2.

## 4. Secret hack overlay — hud_secret_overlays.cpp
- Background (:18-71, if baseDoorId!=0): bank187 idx1 (beaming) / idx0 at offset yoffset,
  brightness 128.
- Highlight flashes (if highlightSecrets / highlightMinimap): bank86 idx2 / idx1,
  pulse base 120, phase%32 triangle ±16 → 120-136.
- Hack progress (:73-115, if baseDoorId && !beaming): 9 lines at (10, 54+i*13+yoffset)
  110×12 Body: "Guv Net","OS","Protocol","Cypher Lock 1..3","Header","Schedule","Location".
  Threshold: secretprogress -= 20 per line; hacking flicker (state==Hacking && state_i==16
  && phase%16<8) shifts threshold to -20. Below threshold → color=114 brightness 96,
  else color=0 brightness 136.

## 5. System camera frames — hud_system_camera.cpp:13-40
Two calls when active: (bank95 idx2, offsetBank92, logicalY=381) and (bank95 idx11,
offsetBank92, logicalY=318). Pos: x=-SpriteOffsetX(bank,idx),
y=-SpriteOffsetY(92,idx)+logicalY. Brightness 128.

## 6. Chat overlay — hud_chat_overlay.cpp:142-236
Constants: kChatX=400 kChatY=280 kChatW=231 kChatBackgroundInteriorH=30 kChatChromeH=70
bank=188 kTextX=10 kTextStartY=10 kLineStepY=10 kMaxHistoryChars=36
kChatInputVisibleChars=28 kRightCapX=36.
- Panel: top edge idx0 cap / idx1 tile / idx2 right cap at kChatW-36; bottom edge idx6/7/8
  at y=30. Height = max(70, 10 + contentLines*10 + 12).
- History ≤5 lines (drop first if input active at 5): (10, 10+i*10) Body,
  LegacyPalette(0,136), truncate 36 chars.
- Input: prefix "(TEAM):" / "(ALL):" LegacyPalette(0,136); input box at (10+prefixW, y),
  28 visible chars × 6px, Body, LegacyPalette(0,128); caret color INDEX 140,
  blink (SDL_GetTicks()/50)%32<16.
- Toggle interactable "ingame.chat.channel", selected=chatWithTeam.

## 7. Buy/tech overlay — hud_buy_tech_overlay.cpp:58-162
- Background bank102 idx0 at its sprite pos. Highlight bank102 idx1 at baseY + i*25 when
  row selected.
- Rows (5 visible, 25px pitch): icon at SpriteX(bank,idx,169), SpriteY(...,139+yoffset),
  brightness 128 selected / 64 dim; name at (222,145+yoffset) Heading
  LegacyPalette(0,brightness); price centered at x=440 same line.
- Footer ("CREDITS: N"/"VIRUSES: N") centered at x=320, y=275, Heading.

## 8. Player list overlay (F1) — hud_player_list_overlay.cpp:16-98
Root 640×480, padding 50; panel centered top, bg rgba(0,0,0,128), height 10+teams*58.
Per team row (H58): emblem slot 40px (AllocTeamEmblemCustomData {181,agency,color,17,true});
peers column padded top ((4-peerCount)*12)/2; per peer row H12: name (Body) + stats
"L:%d    E:%d  S:%d  J:%d  H:%d  C:%d" (level, endurance, shield, jetpack, hacking, contacts).

## 9. Center messages — InGameOverlays.cpp:58-145
Type table (color idx, liney, size): 0→208/60/Title (shadowed); 1→128/190/MessageHeading;
2→128/60/Title; 3→192/60/Title; 4→153/60/Title; 10→224/60/MessageTitle(line0)+
MessageSubtitle(rest); 11→153/60/Title; 20→153/200/Title.
Per-char reveal (i < message_i): type<10 → brightness = 128 + max(0,-(message_i-messagetime+8)*8)
+ max(0,(msg%16-8)*2); type>=10 → 128. Tail: if message_i-i<=5, += (5-(message_i-i))*8.
Shadow at (x+1,y+1) brightness max(8,b-64). X centers per line:
(w - linelength*advance)/2 + advance*(linelength-nextline). Type>=10: line0 y+40, rest y+20.

## 10. Status lines — InGameOverlays.cpp:147-183
Bottom-center stack from y=370, -10/line upward. brightness = 128 - max(0,(16-time)*8);
shadow (x+1,y+1) max(8,b-64). BodySm, centered x=(w-len*adv)/2, color from line view.

## 11. Top ticker — InGameOverlays.cpp:185-209
(200,10) 245×12 BodySm. 35-char window, start=max(0, progress/2 - 24) → scrolls 2 chars/frame.

## 12. Quit prompt — InGameOverlays.cpp:211-227
"Hit Enter To Quit", TextSize=Prompt, centered x, y=200, LegacyPalette(202).
Shown when quitState 1 or 2.

## Palette indices used by the HUD
0 default text · 114 hacked/secret-pulse · 128 message gray · 140 caret · 153 danger ·
161 health numeric · 192 type-3 message · 202 credits/shield/quit (bright) · 208 default
message · 210 in-base ramp · 224 beaming/type-10. Active palette is the level palette
(Renderer::palette) — indices resolve per level, same caveat as the menu caret discovery.

## Brightness conventions
128 full · 136 bright text · 96 hacked rows · 64 buy-tech dim · 32 inventory dim.
Pulses: shield 136-148 (period 12), highlights 120-136 (period 32), warning flashes
phase%8<=3, caret 50ms*32 ticks, secret flicker tick%12<6.

## TextSize line heights (advance values are empirical per font — query TextAdvance())
Title 19 · Heading 15 · Body 11 · BodySm 11 · Tiny 7 · HudCounter 19 · TinyCounter 7 ·
MessageHeading 15 · MessageTitle 23 · MessageSubtitle 19 · Prompt 23.

## Sprite banks
94 minimap/team/inventory frames · 95 gauges/camera frames · 96 weapon faces/glows/bracket ·
97 inventory icons/poison · 102 buy-tech · 103 team strip · 86 highlights · 181 emblems ·
187 secret bg · 188 chat chrome.
# Part II — mission_summary screen, match-end drive, hover/focus states

Extracted 2026-06-11 (read-only Explore pass + follow-up).
agent releases the worktree.

## A) Mission summary screen (post-game XP/upgrade) — mission_summary_screen.cpp

Canvas 640×480 menu virtual, presentation page ResetPresentation(1) (:241).
- Backdrop PackImage(6,0) (:310); inner panel 628×441 PackImageStretch(7,5) at stage
  padding (5,7,19,20) (:311-325).
- Title "Mission Summary": ScreenTitle face, centered at (192,44) (:332-334).
- "+ N XP": TextSize::Prompt, LegacyPalette(0), centered X 467 Y 45 (:370-372);
  experience = stats.CalculateExperience() = kills*35 − deaths*10 + secretsreturned*125 + …
  (stats.cpp:83-106).
- Summary stats scrollbox: Body, lineHeight 11, viewport 300, column 180×308 (:335-349).
- Six stat rows (Progression order): label "Current <Stat> Level:" Body at X=390,
  Y=97+i*46 (:204-206); value right-aligned to X=556 (:209-215).
- Upgrade buttons (one per stat when upgradesAvailable[i]): Oval Md (bank 6 idx 7 base,
  phases 7-11), 196×33, at (372, 108+i*46) (:229-234).
- Done button: Oval Md at (372,388) (:357-362).
- "*NEW UPGRADE AVAILABLE*" banner: Body, LegacyPalette(129, 160, colorRamp=true), Y=77
  (:386-391); shown when totalbonusupgrades − ag.defaultbonuses < maxupgrades (:509).
- upgradesAvailable[i] from current<max per stat (e.g. ag.endurance < ag.maxendurance)
  (:511-516).
- Flow: upgradeClicked index from action.id suffix (:420-424) → Tick dispatches
  world.lobby.UpgradeStat(selectedcharid, statsagency, kUpgradeStatIds[i]) (:267-274);
  statupgraded network flag → Refresh() reloads user->statscopy (:260-265);
  doneClicked → leaves via Tick (:276-284).
- ENTRY: TickInGame → GoToState(MISSIONSUMMARY) iff gameSession.CheckForEndOfGame() AND
  world.lobby.state == AUTHENTICATED (tick_ingame.cpp:303-311; else MAINMENU). Stats
  copied at match end: user->statscopy = peer->stats, user->statsagency = team->agency
  (game_session.cpp:181-202).

## B) Match-end drive (for golden capture)

- CheckForEndOfGame: world.winningteamid set AND messaging.message_i >= tps*3 (3s delay)
  (game_session.cpp:174-211).
- QUIT PATH SKIPS THE SUMMARY (straight to LOBBY/MAINMENU, tick_ingame.cpp:291-301);
  connection-loss path also skips.
- **DETERMINISTIC HEADLESS PATH (verified, world.cpp:100-114):** authority polls
  IsMatchOver + TIME LIMIT each tick:
  `cfg->timeLimitSecs > 0 && (int)(tickcount/tps) >= cfg->timeLimitSecs → over;
  winningteamid = mode->WinningTeamId() or 0xFFFF (draw)`.
  cfg = GASLoader::Get().GetGameModeConfig((int)gameMode->Id()) — GAS data the harness
  controls. Serve a GameModeConfig with a tiny timeLimitSecs (e.g. 5), launch a 1-player
  match AUTHENTICATED, step ~ (timeLimit + 3)*tps ticks → MISSIONSUMMARY mounts.
  GAS is hot-fetched per map load from adminapiurl (defaults in gasloader when absent) —
  either point adminapiurl at a stub server or check gasloader local-override path.

## C) Hover/focus visual states per widget family (origin primitives)

ONE shared focus state: pointer hover focuses (keyboard-focused element unhighlights —
UiInputRouter.cpp:50-54). NO separate focus-cursor sprite anywhere.

| Family | Hover/focus visual | Values |
|---|---|---|
| Oval buttons (+LegacyRow) | 5-phase sprite + brightness ANIMATION | bank6: Md base idx7, phases idx 7→11; LegacyRow idx 2→6; brightness 128 + phase*2 → 128-136 (button.cpp:199-209, 294-295); hover/focus/selected all target phase 4; disabled pins phase 0 |
| Chrome rect buttons | NOTHING changes | bank7 idx24, brightness always 128 (button.cpp:148-149, 516, 300) — confirmed, focus art swap correctly absent |
| List rows | selection bar only | full-width highlightColor (default palette 180) for SELECTED rows (scroll_list.cpp:151-155); hover/focus: no visual change |
| Text inputs | caret only on focus | caret when focused && !inactive; inactive = brightness 64 (text_input.cpp:119-123); no hover visual |
| Toggles | none on hover/focus | brightness selected/unselected (default 128/128) only (toggle.cpp:65-90) |
| Scrollbars | none | caller sprites, no states (scroll_list.cpp:36-47) |
| Ghost/Text buttons | brightness only | 128→136 same phase ramp; Text disabled = 64 |

Hover-album capture implication: settle phase-4 deterministically (hover_at then step
≥4 ticks — phase advances 1/frame via Activating mode) before screenshot; capture per
family: oval phase-4, list selected-bar, input caret (already golden-covered), Chrome
(should be byte-identical to rest — a NEGATIVE check).

CPPX-side implication: our AppButton must implement the 5-phase oval ramp (sprite
variants idx 7-11 + brightness) — verify whether it currently swaps art on focus at all;
the rest-state goldens never exercised this.
