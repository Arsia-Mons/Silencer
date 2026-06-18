# Configure Controls — origin/main visual-parity plan (cppx)

Golden: `/tmp/goldens/options_controls.png`
Target file: `clients/silencer/src/client/ui/screens/options_controls.cppx`
Spec section: `docs/plans/2026-06-06-cppx-visual-design-language.md` §3.5 (P4 ChromePanel,
P5 TitleTab, P9 BindingRow, P10 BindingWell, P11 ScrollBar, P12 ActionRow).

---

## 1. Golden layout breakdown (regions + flex grammar)

The whole 640×480 is filled by ONE sculpted chrome panel sprite over the starfield.
There are no floating screen-titles and no buttons outside the panel.

```
Root (Starfield, full-bleed 640×480)            ScreenLayout variant=Overlay (already paints starfield)
└─ ChromePanel (P4 — bank 7 idx 5, WHOLE-SPRITE native ~628×441, centered)
   ├─ TitleTab "Configure Controls" (P5)         pill seated ON the panel's top edge (~y=22)
   └─ content column (the panel's interior, padded inside the baked frame)
      ├─ ScrollView (viewport ~520×300, scrollbar on right)
      │   ├─ Preset row    (degenerate BindingRow: label + single value oval)
      │   ├─ Move Up        BindingRow: label | value-oval | "OR" | BindingWell
      │   ├─ Move Down      "
      │   ├─ Move Left      "
      │   ├─ Move Right     "
      │   └─ … (every action, virtualized)
      ├─ ScrollBar (P11) at far right of the viewport (~x555..575, top+bottom nubs)
      └─ ActionRow (P12): [Save] [Cancel] ovals, centered, ~y420
```

Region grammar (filling the viewport):
- **Root** = `ScreenLayout` `Overlay` variant. It already paints the baked
  `starfield` and centers its children (`align_items/justify_content = Center`).
  Good as-is — do NOT add a panel border on the root.
- **ChromePanel** = `Panel variant={Chrome}`. Already sizes the Box to the native
  baked sprite (`chrome_panel_w/h`, ~628×441) and paints `image_patch(chrome_panel)`.
  Its current padding is `{40,40,40,40}` — keep ~`{44 top, 36 L/R, 44 bottom}` so
  the content clears the baked double-border + corner brackets. `direction=Column`,
  `gap≈10`, `align_items=Stretch` (override from Center so rows fill the inner width).
- **TitleTab** sits on the top edge. In the golden the pill is part of the look,
  but bank 7 idx 5 only bakes the panel body; the "Configure Controls" pill is a
  SEPARATE element. Cheapest parity: keep a centered title TEXT (current
  `ScreenTitle`) positioned to overlap the top border. The pill *chrome* behind it
  is a net-new sprite (see §3, P5) — defer; text-on-top reads acceptably for v1.
- **BindingRow grid** (P9). One Yoga Row per action, fixed columns:
  - col1 label: right-aligned text, fixed width ~150px (`Move Up:` etc.)
  - col2 value: `OvalButton` (Oval Md/Sm), shows the primary combo's display
  - col3 "OR": dim-green text, fixed ~36px (omit on the Preset row)
  - col4 alt: `BindingWell` — an empty oval (the secondary combo, blank if none)
  - row height ~52 (`row-height-binding`), `align_items=Center`, `gap≈10`.
  Preset row is degenerate: label `Preset:` + single value oval, no OR/well.
- **ScrollView** wraps the rows, virtualized (`row_height=52`). `viewport_height`
  ≈ 300 to fill the panel interior between title and footer. It already draws a
  thin right thumb when overflowing; the sculpted P11 nub/track is a net-new
  sprite (defer — see §3/§5 risk).
- **ActionRow** footer: two ovals `Save` + `Cancel`, centered, gap 12.

---

## 2. Concrete ordered edits to `options_controls.cppx`

The current file is a select-then-rebind table inside a plain `Overlay`
(no chrome frame, no per-row ovals, a 7-button Rebind/Clear/Confirm/Cancel/
Save/Revert/Back stack + Cycle-Preset). Parity rewrite = chrome frame + two-column
oval binding rows + single Save/Cancel + visible scrollbar. The capture machinery
(use_keybind_capture, 72-tick timeout) STAYS but moves behind a per-row oval press
(pressing a value oval arms capture for that action's combo slot) instead of a
global Rebind button + Selected readout.

### Edit 0 — keep hooks, drop the select/Selected model
Keep `use_navigation`, `use_key_map`, `use_keybind_capture`, the idle/timeout
`use_state` block (verbatim — §lines 133-147). DELETE `selected`/`select`/
`sel_label`/`rebind_selected`/`clear_selected` and the `Selected:`/banner readouts.
Rebind is now driven per-row (Edit 2).

### Edit 1 — per-combo display helper
Replace `binding_display` (whole-action AND/OR string) with a per-combo accessor:
```cpp
// chips of one combo, AND-joined; "" if the combo is absent.
std::string combo_display(const KeyMapAction &a, size_t combo_index) {
  if (combo_index >= a.combos.size()) return "";
  const KeyMapCombo &c = a.combos[combo_index];
  std::string out;
  for (size_t k = 0; k < c.chips.size(); ++k) { if (k) out += " + "; out += c.chips[k].label; }
  return out;
}
```
Primary = combo 0, alternate = combo 1.

### Edit 2 — new `make_binding_row` helper (replaces `make_control_row`)
A plain helper whose body is JSX (transpiles cleanly; building inline in a
push_back arg does NOT — keep it a named function). Columns are a Yoga Row.
Pressing the value oval arms capture for that combo slot.
```cpp
::ui::UiElement make_binding_row(
    const char *key, int index, const char *label,
    const char *primary, const char *secondary, bool has_or,
    std::function<void()> rebind_primary,
    std::function<void()> rebind_secondary) {
  // col widths are fixed so every row's ovals line up (the golden grid).
  ::ui::UiElement label_el = <BodyText key="lbl" variant={BodyTextVariant::Strong} value={label} />;
  // value oval (primary). Empty string -> BindingWell look (oval w/ no label).
  ::ui::UiElement primary_el = <AppButton key="p" variant={AppButtonVariant::Oval}
      size={AppButtonSize::Sm} label={primary} onPress={rebind_primary} />;
  // ... (assemble label col fixed-width via a wrapping Box; see col widths below)
  return <Box key={key} layout={row_layout}> ... </Box>;
}
```
- col1 label Box: `width=points(150)`, `justify_content=FlexEnd` (right-align),
  `align_items=Center`.
- col2 value: `AppButton Oval Sm` (sizes to label via nine-slice; min_w 104).
  `controlId` = `copy_string("BindP"+i)` so e2e/inspect can find it.
- col3 "OR": `BodyText Detail` tone Muted, fixed `width=points(36)`, centered;
  rendered only when `has_or` (skip on Preset row).
- col4 alt: another `AppButton Oval Sm` with `label=secondary` (`""` →
  BindingWell — see Edit 4); `controlId` = `"BindS"+i`.
- row_layout: `direction=Row, align_items=Center, width=percent(100),
  height=points(52), gap=points(10), padding axes {10, 0}`.

### Edit 3 — view body: build rows, wrap in ScrollView, wrap in Chrome Panel
Replace the whole `OptionsControlsView` return tree. Keep the dynamic-children
list pattern (it already virtualizes and respects the fiber budget):
```cpp
std::vector<::ui::UiElement> rows;
rows.reserve(km.actions.size() + 1);
// Preset row first (degenerate): label "Preset:" + single value oval "Default".
{
  const char *plabel = use_text_storage("Preset:");
  const char *pval = ::ui::copy_string(km.preset_label.c_str());
  rows.push_back(make_preset_row("preset", plabel, pval, km.cycle_preset));
}
const float kRowH = 52.0f;
for (int i = 0; i < action_count; ++i) {
  const KeyMapAction &a = km.actions[i];
  const char *rkey = ::ui::copy_string(("row" + std::to_string(i)).c_str());
  const char *label = use_text_storage("%s:", a.label.c_str());     // "Move Up:"
  const char *prim = ::ui::copy_string(combo_display(a, 0).c_str());
  const char *sec  = ::ui::copy_string(combo_display(a, 1).c_str());
  Action act = a.action;
  std::function<void()> rb0 = [cap, km, act]() {
    if (cap.begin_capture) cap.begin_capture(act, rebind_combo_index(km, act)); };
  std::function<void()> rb1 = [cap, act]() {
    if (cap.begin_capture) cap.begin_capture(act, 1); };  // alt slot
  rows.push_back(make_binding_row(rkey, i, label, prim, sec, /*has_or=*/true, rb0, rb1));
}
```
Then the ScrollView (mirror current call, retune sizes):
```cpp
::ui::UiElement table = ::ui::component("ScrollView",
  ScrollViewProps{
    .key = "table", .id = "ControlsList",
    .viewport_height = 300.0f,
    .content_height = kRowH * (float)(action_count + 1),
    .step = kRowH, .row_height = kRowH,
    .layout = ::ui::LayoutStyle{.width = ::ui::Length::points(540.0f)},
    .children = ::ui::children(rows)},
  ScrollView);
```
Return tree (note: `table` is a precomputed local var — a multi-line JSX child
must be a UiElement var first, per cppx gotchas):
```cpp
return <ScreenLayout key="root" variant={ScreenLayoutVariant::Overlay}>
  <Panel key="frame" variant={PanelVariant::Chrome}>
    <ScreenTitle key="title" variant={ScreenTitleVariant::Screen} value="Configure Controls" />
    {table}
    <ActionRow key="actions">
      <AppButton key="save"   controlId="SaveBinds"     variant={AppButtonVariant::Oval} label="Save"   onPress={km.commit} />
      <AppButton key="cancel" controlId="ControlsBack"  variant={AppButtonVariant::Oval} label="Cancel" onPress={cancel} />
    </ActionRow>
  </Panel>
</ScreenLayout>
```
`cancel` = revert-then-pop:
```cpp
std::function<void()> cancel = [nav, revert = km.revert]() {
  if (revert) revert();
  if (nav.pop_current) nav.pop_current();
};
```
Save = `km.commit` then pop (wrap likewise if the golden's Save also leaves).

NOTE: the in-progress capture (cap.capturing) needs a confirm/cancel affordance.
origin/main captures inline (press key → it commits, Esc cancels) with the 72-tick
timeout already wired. Keep `cap.confirm_chord`/`cap.cancel_capture` reachable: the
simplest parity-safe path is to leave the timeout auto-commit/cancel and let the
game-layer edge feed drive it (events.cpp owns the raw edges). No on-screen
Confirm/Cancel buttons (they were a migration invention; golden has none).

### Edit 4 — BindingWell (empty oval) — P10
A `BindingWell` is just an `AppButton Oval` with an empty label and the faint
fill. `app_button_oval_patch` already paints the oval sprite with no text when
`label==nullptr`. So col4 with `label=""`/`nullptr` IS the well — no new
component required for v1. (If a distinct dim "empty" tint is wanted later, add
an `OvalButton` `as-well` variant; defer.)

### Edit 5 — includes / using
Add `using ::ui::components::Box/BoxProps` (already present). Add
`#include "client/ui/components/surfaces/panel.h"` and the `Panel`/`PanelProps`
`using` so `<Panel variant={PanelVariant::Chrome}>` resolves. Keep the rest.

---

## 3. Data / hooks / sprites: exist vs net-new

ALREADY EXIST (cite):
- **Hooks** `use_key_map` (`hooks/use_key_map.h` — `km.actions` w/ combos, labels
  pre-resolved, `cycle_preset`/`commit`/`revert`/`clear_action`), `use_keybind_capture`
  (`hooks/use_keybind_capture.h` — `begin_capture(action, combo_index)`,
  `confirm_chord`, `cancel_capture`, 72-tick timeout owned by the screen),
  `use_navigation`. No data changes needed.
- **ChromePanel** = `Panel variant={Chrome}` (`components/surfaces/panel.cppx:93`)
  → `use_chrome().chrome_panel` (bank 7 idx 5, baked, `chrome_panel_w/h`). EXISTS.
- **OvalButton** = `AppButton variant={Oval}` (`app_button.cppx:60`,
  `app_button_variant.h` oval patch + nine-slice caps). Sizes to label. EXISTS.
- **ScrollView** (virtualized, `row_height`) — `ui/components/scroll_view.cppx`. EXISTS.
- **ActionRow** (`components/actions/action_row.cppx`) Row, gap 12. EXISTS.
- **ScreenTitle / BodyText** text roles (`components/text/`). EXIST.
- **ScreenLayout Overlay** paints the baked starfield. EXISTS.

NET-NEW (minimal):
- **BindingRow / preset row** = two file-local helper functions in
  `options_controls.cppx` (NOT shared components). No new files.
- **BindingWell** = empty-label `AppButton Oval` (no new component for v1).
- **TitleTab pill (P5)** — the rounded lozenge chrome behind "Configure Controls".
  No baked sprite for it today (use_chrome bakes panel/oval/chrome-btn/dialog/
  toggle/logo/emblem only). DEFER: render the title as TEXT overlapping the panel
  top edge. To add the real pill later: bake a pill sprite (likely a bank-7 idx)
  into `ChromeTextures` + a `TitleTab` component nine-sliced horizontally. Not
  required for first-pass parity.
- **Sculpted ScrollBar (P11)** — top/bottom nubs + double-line track + grip thumb.
  Net-new sprite assembly; the ScrollView's existing thin green thumb is the v1
  stand-in. DEFER the sculpted version.

---

## 4. Capture / verify recipe (headless)

Reuse the visual-regression path (`tests/cli-agent/e2e/70_visual_regression.sh:127`):
```bash
. tests/cli-agent/e2e/lib.sh
PORT=$(pick_port); PID=$(start_silencer "$PORT"); wait_alive "$PORT"
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000
cli --port "$PORT" resize --w 960 --h 720          # 640×480 logical → 1.5x
cli --port "$PORT" click --label "Options"
cli --port "$PORT" click --label "OptionsControls"
cli --port "$PORT" wait_frames --n 3
cli --port "$PORT" screenshot --out /tmp/controls_now.png
cli --port "$PORT" inspect > /tmp/controls_nodes.json
stop_silencer "$PID" "$PORT"
# diff:
tools/pixdiff/build/pixdiff /tmp/controls_now.png /tmp/goldens/options_controls.png
```
- `inspect` must show: `ControlsList` (the ScrollView), several `BindP*`/`BindS*`
  oval control ids, `SaveBinds`, `ControlsBack`, a `text` node `"Configure Controls"`.
- Wheel-scroll proof (virtualization): `cli scroll --x 320 --y 250 --dy -8`, then
  re-inspect; the visible `BindP*` window must change.
- **Update `tests/cli-agent/e2e/12_controls_scroll.sh`** and the
  `crop_check options_controls …` ids in `70_visual_regression.sh:129-132`
  (they reference the OLD ids `RebindFire`/`CyclePreset`/`RevertBinds`) to the new
  ids (`BindP0`, `SaveBinds`, `ControlsBack`, `ControlsList`). The title assertion
  `value === "Controls"` → `"Configure Controls"`. Re-bless the golden with `BLESS=1`.

---

## 5. Risks / unknowns

1. **Node/fiber budget (REACT_MAX_FIBERS=256).** Each binding row is now
   ~6 nodes (Box + label-Box + label-Text + 2 oval-Buttons w/ Text children + OR
   Text) vs the old ~3. Virtualization (`row_height=52`, ~6 visible rows) keeps it
   to ~40 row nodes + chrome — safe. MUST keep `row_height` set and a flat,
   uniform-height row list (the ScrollView only virtualizes uniform lists).
2. **ScrollView width vs panel interior.** The Chrome panel inner width (~628−2×36)
   must exceed the ScrollView width (540) + room for the thumb (right edge). Tune
   panel padding + ScrollView width together; verify ovals don't clip the baked
   double-border.
3. **TitleTab pill absent** — v1 ships text-only title; the golden shows a pill.
   Acceptable miss; flag for a follow-up bake. Pixdiff threshold (0.40) should
   tolerate it; if not, raise crop scope or add the bake.
4. **Per-combo rebind UX.** Pressing a value oval arms `begin_capture`; commit/
   cancel rely on the game-layer edge feed + 72-tick timeout (no on-screen
   Confirm/Cancel). Verify a real rebind round-trips through the control socket
   (the raw edges live in events.cpp) before claiming parity — the alt-slot
   (combo_index=1) path is the least-exercised.
5. **Oval interior bug (c)** is fixed (`image_patch` chromeless), but confirm the
   empty BindingWell oval renders hollow (transparent interior) and not a dark slab.
6. **Save/Cancel leaving behavior.** Golden footer is Save+Cancel; confirm whether
   Save should pop the screen or stay. Mirror origin/main (likely pop on both).
