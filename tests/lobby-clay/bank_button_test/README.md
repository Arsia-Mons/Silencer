# bank_button_test — P5 unit test for the BankButton primitive

Drives `clay_bank_button_test --variant <chrome|inline|checkbox>` three
times, each capturing a 640x480 PNG of a single `BankButton` rendered
through the Clay → SDL3 bridge into a fresh `Surface`, and pixdiffs each
against the committed `reference_<variant>.png`.

Pass bar (render): **< 1.0% per variant** (prd P5).

Additionally drives `clay_bank_button_check` — a non-PNG control op that
exercises the Chrome variant against a 5-frame pointer-state timeline and
returns counters for hover brightness + click dispatch. Expected output:

| Field                       | Expected | Why |
|-----------------------------|----------|---|
| `chrome_brightness_idle`    | 128      | Neutral effectbrightness (legacy default). |
| `chrome_brightness_hover`   | 136      | Legacy ACTIVE-state brightness 128 + (4 * 2). |
| `clicks_fired_on_press`     | 1        | One fire across press transition + dispatch. |
| `clicks_fired_when_held`    | 0        | No re-fire on held frames after first dispatch. |

## Usage

```bash
bash tests/lobby-clay/bank_button_test/run.sh
```

To regenerate baselines after a deliberate change to the test scene:

```bash
REGEN=1 bash tests/lobby-clay/bank_button_test/run.sh
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
