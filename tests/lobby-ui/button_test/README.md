# button_test — P5 unit test for the Button primitive

Drives `clay_button_test --variant <oval|chrome|text|ghost> --size
<sm|md|lg|compact|auto>` across the public Button mappings, each
capturing a 640x480 PNG of a single `Button` rendered through the Clay
→ SDL3 bridge into a fresh `Surface`. Committed references are pixdiffed;
new mappings without references are still captured as render smoke probes.

Pass bar (render): **< 1.0% per variant** (prd P5).

Additionally drives `clay_button_check` — a non-PNG control op that
exercises Chrome+Compact against a 5-frame pointer-state timeline and
returns counters for hover brightness, click dispatch, stable bounds, and
responsive Oval+Auto sizing. It also verifies the retained legacy active
timeline for Oval pointer hover and keyboard focus. Expected output:

| Field                       | Expected | Why |
|-----------------------------|----------|---|
| `chrome_brightness_idle`    | 128      | Neutral effectbrightness (legacy default). |
| `chrome_brightness_hover`   | 136      | Legacy ACTIVE-state brightness 128 + (4 * 2). |
| `chrome_sprite_index_hover` | 24       | Chrome keeps its static sprite face. |
| `oval_hover_sprite_indices` | 7,8,9,10,11 | Legacy Oval activation frame sequence. |
| `oval_hover_brightness`     | 128,130,132,134,136 | Legacy activation brightness ramp. |
| `oval_unhover_sprite_indices` | 11,10,9,8,7 | Legacy Oval deactivation frame sequence. |
| `oval_unhover_brightness`   | 136,134,132,130,128 | Legacy deactivation brightness ramp. |
| `oval_focus_sprite_index`   | 11       | Keyboard focus reaches the same active Oval frame. |
| `oval_focus_brightness`     | 136      | Keyboard focus reaches the same active brightness. |
| `oval_wall_clock_partial_sprite_index` | 7 | A partial legacy tick does not jump the animation. |
| `oval_wall_clock_partial_brightness` | 128 | A partial legacy tick keeps neutral brightness. |
| `oval_wall_clock_next_sprite_index` | 8 | The next elapsed legacy tick advances one frame. |
| `oval_wall_clock_next_brightness` | 130 | The next elapsed legacy tick advances one brightness step. |
| `clicks_fired_on_press`     | 1        | One fire across press transition + dispatch. |
| `clicks_fired_when_held`    | 0        | No re-fire on held frames after first dispatch. |
| `compact_width`             | 156      | Chrome+Compact maps to the existing framed lobby button. |
| `compact_height`            | 21       | Chrome+Compact keeps the existing hit height. |

## Usage

```bash
bash tests/lobby-ui/button_test/run.sh
```

To regenerate baselines after a deliberate change to the test scene:

```bash
REGEN=1 bash tests/lobby-ui/button_test/run.sh
```

## Why three separate ops, not one combined frame

The prd P5 pass_check is explicit: "three small render tests — one per
variant". Each variant renders into its own 640x480 frame so a regression
in one (e.g. checkbox swapping idx 18 ↔ 19) doesn't accidentally cancel
out a regression in another. Single-variant baselines also keep regen
predictable when adding new variants in the future.

## Why the click fires across a multi-frame window

Clay 0.14's `Clay_OnHover` dispatches the registered proxy during
`Clay_SetPointerState` — using the *previous* frame's pointer state and
*previous* frame's element hashmap. So a press input edge takes effect
on the next layout pass after the input arrives. This is the documented
"one-frame hit-test lag". The legacy lobby has the same lag (one engine
Tick between mouse-down and `clicked = true`), so this matches behavior.

The check op verifies the contract "exactly one click per press" by
counting clicks across the entire press window (transition + dispatch)
and the subsequent held frame; the first must equal 1 and the second 0.
