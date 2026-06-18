# Lobby Connect — origin/main parity plan

Golden: `/tmp/goldens/lobby_connect.png` (640×480).
Screen source: `clients/silencer/src/client/ui/screens/lobby_connect.cppx`.
Spec section: `docs/plans/2026-06-06-cppx-visual-design-language.md` §3.13 + P21/P22.

---

## 0. Verdict: DROP the bank-40 sprite, use a plain bordered Panel frame

The §3.13 spec and P22 say "use the bank-40 idx2 `DialogPanel` (baked wells)".
**The golden does NOT match that sprite.** The golden is a **single thin
1px green rectangular frame** — flat black interior, no rounded wells, no inner
bevels, no ornate corners. The bank-40 idx2 sprite is the ornate
password-dialog chrome with baked rounded wells; rendering it would put rounded
sunken pill wells behind the fields, which the golden has none of. So for
visual parity we render a **plain bordered rectangle** (`tokens::panel_patch`
green-dim border on a black/transparent fill), NOT `chrome.dialog_pw`.

This means the current code's whole `panel_style = chrome.dialog_pw ? image_patch
: panel_patch` branch collapses to just the `panel_patch` arm. `use_chrome()`
is no longer needed by this screen.

The fields then need their OWN thin bordered rectangles (the golden shows each
field as a distinct thin green box), because there is no longer a baked well
under them. The current `bare`/`chromeless` field paint (which suppressed the
component chrome so the baked well showed through) is replaced by a thin green
field border drawn by the Input itself.

---

## 1. Golden layout breakdown (regions + flex grammar)

Root: full-bleed **pure black** (`#000000`), no starfield/Mars/slate. Centered
horizontally; the frame sits roughly centered, very slightly above-of-center.

Frame: a single ~**280×290** thin-green-bordered rectangle (1px `green-dim
#2E7D45`), transparent/black interior. Inside it, a **Column** that fills the
frame:

| Region | Vertical share | Content |
|---|---|---|
| **Log clip** (top) | ~grow, ~60% | empty until connecting; green status-log lines, top-left aligned, clipped |
| **Username row** | fixed ~22px | `Username` label (left) + thin field box (`AgentZero`) |
| **Password row** | fixed ~22px | `Password` label (left) + thin field box (`*******`) |
| **Action row** (bottom) | fixed ~24px | `Login/Create` rect button + `Cancel` rect button, centered |

Flex grammar to fill 640×480:
- root `Box`: `width/height 100%`, `align Center / justify Center`, black fill.
- panel `Box`: fixed `width≈280 height≈290`, `direction Column`, padding
  `{l10,r10,t10,b10}`, `gap≈6`, thin green border.
- log clip: `flex_grow 1`, `width 100%`, `overflow Hidden`, `align Start` →
  eats the empty top area and pushes the field rows + actions to the BOTTOM.
- each field row: `direction Row`, `align Center`, `gap≈8`. The **label sits
  LEFT of the field**; the field is a fixed-width thin box to its right.
- field rows + action row are after the grow region, so they bottom-anchor.

Key golden facts (vs current):
- Frame interior is empty/black at top — log region must GROW to fill it and
  push everything else down. Current already does this (`flex_grow 1` on
  `logclip`), so the anchoring is correct; the FRAME and FIELD look are wrong.
- Labels are **left of** thin **rectangular** fields (current already lays out
  row=label+field; but fields are chromeless-on-baked-well, golden has visible
  thin field borders and no well).
- Buttons are **rectangular** (P21 `RectChromeButton`), near-square corners —
  current uses `AppButtonVariant::Chrome` which resolves to the bank-7 metal
  nine-slice sprite. Golden buttons are simple thin green-bordered rects with a
  centered green label (`[ Login/Create ]` / `[ Cancel ]`). The bank-7 chrome
  sprite is a *metal* look, not the flat green rect of the golden — so for
  parity use a **plain bordered button**, not the Chrome sprite.

---

## 2. Concrete ordered edits to `lobby_connect.cppx`

### Edit A — drop sprite chrome; plain bordered panel frame
Remove the `ChromeTextures chrome = use_chrome();` line and the `dialog_pw`
sizing/branch. Hardcode the frame dims and use `panel_patch`:

```cpp
// no use_chrome(); no chrome.* anywhere in this screen now.
constexpr float kPanelW = 280.0f;
constexpr float kPanelH = 290.0f;
::ui::StylePatch panel_style =
    tokens::panel_patch(tokens::kSurfaceMenu, tokens::kBorderPanel);
```

`kSurfaceMenu` is `#000000` opaque — matches the golden's black interior.
`kBorderPanel` is `#2E7D45` green-dim 1px — matches the thin frame.
Remove `#include "client/ui/hooks/use_chrome.h"`.

### Edit B — visible thin green field boxes (replace `bare`/chromeless)
The golden's fields have their OWN thin green border on black. Replace the
`clear`/`bare` chromeless patch with a thin bordered field patch in every state
(so the focused field shows the same thin green box; brighten the border on
focus to read as the green caret/active state):

```cpp
auto field_box = [](::ui::Color border) {
  return ::ui::patch()
      .chromeless(true)                      // kill the theme input slab/ring
      .background(::ui::Color{0, 0, 0, 255}) // black interior
      .gradient(::ui::Gradient{})
      .corner_radius(0.f)                    // square-ish, like the golden
      .border(::ui::Border{{1, 1, 1, 1},
                           {border, border, border, border}})
      .outline(::ui::Outline{});             // no extra focus ring
};
::ui::StyleStatePatch field{};
field.base = field_box(tokens::kBorderPanel);          // #2E7D45 idle
field.hover = field_box(tokens::kBorderPanel);
field.focus_visible = field_box(tokens::kAccentBorder);// #3CFF3C brighter focus
field.pressed = field_box(tokens::kAccentBorder);
```

Field text is the bright value over black — keep the Input's resolved text
paint (green via app_theme), or set a `.text(...)` in the patch to
`tokens::kTextBody`. Field layout: shorten to fit the panel (the golden field
is ~150px wide):

```cpp
auto field_layout = ::ui::LayoutStyle{
    .align_items = ::ui::AlignItems::Center,
    .justify_content = ::ui::JustifyContent::Start,
    .width = ::ui::Length::points(150.0f),
    .height = ::ui::Length::points(20.0f),
    .padding = {6.0f, 6.0f, 1.0f, 1.0f}};
```

Apply `style={field}` to both `<Input>`s (replacing `style={bare}`).

### Edit C — labels left of fields, brighter than log
Labels stay `BodyText` but should read brighter than the dim log lines. The
golden labels are mid/bright green; the log is dimmer. Current uses
`BodyTextVariant::Body` for labels (`kTextBody #4FB867`) and
`BodyTextVariant::Message` for log (`kTextBodyMuted #2E7D45`) — that contrast is
already correct. Keep labels `Body`, log `Message`. (Optional: bump labels to
`BodyTextVariant::Strong` if the golden reads brighter.) Give labels a fixed
width so the two field boxes left-align in a column:

```cpp
<BodyText key="ulbl" variant={BodyTextVariant::Body} value="Username" />
```
Add `layout` width ~64px via a wrapping Box if column alignment drifts; the
golden's `Username`/`Password` are right-padded so fields align — wrap each
label in a fixed-width Box (`width 70`, `align Start`) if needed.

### Edit D — rectangular plain-bordered buttons (drop Chrome sprite)
Replace the two `AppButtonVariant::Chrome` buttons. The cleanest parity is a
new tiny **rect button** look. Two options:

1. **Minimal (preferred): reuse `AppButton` with a new flat-rect treatment.**
   `AppButtonVariant::Chrome` falls back to a rounded slate button only when
   the sprite is missing; with the bank-7 sprite baked it renders metal. We do
   NOT want metal. Add a `Ghost`-style bordered look by passing a per-button
   style, OR introduce `AppButtonVariant::Rect` (flat green border, square
   corners, centered green Large-face label) in `app_button_variant.h` +
   `app_button.cppx`. The Rect patch:

```cpp
// app_button_variant.h — new variant paint
inline ::ui::StyleStatePatch app_button_rect_patch() {
  auto rect = [](::ui::Color border) {
    return ::ui::patch()
        .background(::ui::Color{0, 0, 0, 255})
        .gradient(::ui::Gradient{})
        .corner_radius(2.0f)
        .border(::ui::Border{{1,1,1,1},{border,border,border,border}});
  };
  ::ui::StyleStatePatch ov{};
  ov.base = rect(tokens::kBorderPanel);
  ov.hover = rect(tokens::kAccentBorder);
  ov.focus_visible = rect(tokens::kAccentBorder);
  ov.pressed = rect(tokens::kAccent);
  return ov;
}
```
   Wire `AppButtonVariant::Rect` through `app_button.cppx`'s switch (mirror the
   `Chrome` arm but use `app_button_rect_patch()` for paint and
   `app_button_chrome_layout()`-ish geometry sized to label). Label stays the
   Large-face green Text child (already how AppButton paints labels).

2. **Even more minimal:** keep `variant={Chrome}` but accept the metal sprite.
   REJECTED — golden is flat green, not metal.

Go with option 1 (`Rect` variant). Buttons:
```cpp
<AppButton key="login"  controlId="Login"  variant={AppButtonVariant::Rect} label="Login/Create" onPress={submit} />
<AppButton key="cancel" controlId="Cancel" variant={AppButtonVariant::Rect} label="Cancel"       onPress={lobby.cancel} />
```
Keep the centered action row (`gap≈10`). The action row stays last so it
bottom-anchors under the field rows.

### Edit E — panel sizing + gaps
Set the panel `Box` to fixed `width=kPanelW height=kPanelH`, `gap≈6`,
`padding {l10,r10,t10,b10}` (golden frame is tight). Keep `align Center` so the
two field rows + action row center within the panel; or `align Stretch` +
left-pad labels if you want the fields to start at a fixed x. Golden shows the
label/field group slightly indented and centered — `align Center` with the
existing row layout is fine.

---

## 3. Data / hooks / sprites: exist vs net-new

- `use_lobby_session()` — **EXISTS** (`hooks/use_lobby_session.h`): `status_log`,
  `connect`, `cancel`. No change. Log text via `use_text_storage` — EXISTS.
- `use_chrome()` / `ChromeTextures` — **EXISTS but NO LONGER USED** by this
  screen after Edit A. Remove the include.
- `tokens::panel_patch`, `kSurfaceMenu`, `kBorderPanel`, `kAccentBorder`,
  `kAccent`, `kTextBody`/`kTextBodyMuted` — **ALL EXIST** (`tokens.h`). No new
  tokens.
- `Panel` component — could use `PanelVariant::Bordered`, but it forces
  `direction Column`, `width 260`, padding 12, and a near-black GREEN-GLASS fill
  (`kSurfacePanel #061008 a235`), not pure black. The golden interior is pure
  black. Simpler to keep the screen's own `<Box>` + `panel_patch(kSurfaceMenu,
  kBorderPanel)` (already the fallback path in current code). **No new
  component.**
- `BodyText` (Body/Message variants), `Input`, `Box` — **EXIST**.
- `AppButton` — EXISTS; **net-new `AppButtonVariant::Rect`** (Edit D) is the one
  additive change: enum value in `app_button_variant.h`, paint helper
  `app_button_rect_patch()`, and a switch arm in `app_button.cppx`. ~25 lines.
  This is reusable for other flat-rect buttons; spec P21 (`RectChromeButton`) is
  exactly this primitive, so it belongs in the design system, not the screen.
- **Password masking** — NOT supported by `Input` (no `secret`/mask prop in
  `input.hx`; value is rendered raw). The golden shows `*******`. This is a
  DEFERRED parity gap: either (a) accept plaintext echo for now (functional,
  visually off), or (b) add a `bool secret` prop to `InputProps` +
  mask-on-render in the runtime input draw path (`input.cppx` / runtime). Out of
  scope for visual-frame parity; flag separately. The golden's masked dots are
  the only true blocker to pixel parity on the password row.

---

## 4. Capture / verify recipe (headless)

Lobby Connect needs a running lobby (the screen advances on the connect flow).
Use the same harness as `71_visual_regression_lobby.sh` (full recipe there):

```bash
. tests/cli-agent/e2e/lib.sh
# start lobby (lobby_bin) on $LOBBY_PORT, then:
HOME=$TMP/home "$SILENCER_BIN" --headless --control-port $CTRL_PORT \
  --lobby-host 127.0.0.1 --lobby-port $LOBBY_PORT &
wait_alive "$CTRL_PORT"
cli --port $CTRL_PORT wait_for_state --state MAINMENU --timeout-ms 15000
cli --port $CTRL_PORT resize --w 640 --h 480          # capture at logical 640x480
cli --port $CTRL_PORT click --label "Connect To Lobby"
cli --port $CTRL_PORT wait_for_state --state LOBBYCONNECT --timeout-ms 5000
# wait for the Username widget, then screenshot:
cli --port $CTRL_PORT screenshot --out /tmp/lobby_connect.png
cli --port $CTRL_PORT inspect > /tmp/lobby_connect.json
```

Then pixdiff vs the golden:
```bash
tools/pixdiff/build/pixdiff /tmp/lobby_connect.png /tmp/goldens/lobby_connect.png
```
(The e2e suite captures at 960×720 — `71_*` uses `W=960 H=720`. For 1:1 with
the 640×480 golden, resize to 640×480 or rescale before diff. The existing
`tests/cli-agent/e2e/golden/lobby_connect.png` is a STALE pre-restore capture
showing the slate/blue regression — re-bless it with `BLESS=1` after the edits.)

Quick visual check: `cli inspect` and confirm nodes `Username`, `Password`
(role `input`), `Login`, `Cancel` (role `button`) exist with sane bounds, then
eyeball the screenshot vs golden.

---

## 5. Risks / unknowns

1. **Password masking** — no runtime support; pixel parity on the password row
   (`*******`) is blocked until a `secret` Input prop lands. Biggest true gap.
2. **`AppButtonVariant::Rect` plumbing** — adding an enum value touches the
   AppButton switch in `app_button.cppx` (must handle layout + paint arms;
   ensure label still renders as the Large-face green Text child, not the bare
   grey fallback — see app_button_variant.h note). Low risk, but it's the only
   new code path.
3. **Spec vs golden conflict** — the design spec (§3.13/P22) prescribes the
   bank-40 sprite; this plan deliberately overrides it because the GOLDEN is a
   plain rect. If a later re-bless restores the sprite look, revisit. Flag for
   reviewer.
4. **Field/label column alignment** — `align Center` rows may leave the two
   fields slightly misaligned if labels differ in width (`Username` vs
   `Password` are equal length, so likely fine). Wrap labels in a fixed-width
   Box if they drift.
5. **Frame dims** — `280×290` is eyeballed from the 640×480 golden; tune via
   pixdiff. The log region `flex_grow 1` handles the tall empty top regardless.
6. **Stale e2e golden** — `tests/cli-agent/e2e/golden/lobby_connect.png` is the
   old slate render; don't diff against it. Re-bless after the change.
