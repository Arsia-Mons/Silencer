# P4 BankText primitive unit test

Pixdiff test for the `BankText` Clay primitive
(`clients/silencer/src/ui/primitives/bank_text.{h,cpp}`).

## What it does

`run.sh` boots the headless silencer binary, fires the
`clay_bank_text_test` control op, and pixdiffs the resulting PNG against
the committed `reference.png`.

The scene is a 640x480 root container with `padding = { 15, 0, 32, 0 }`
hosting a single `BankText` call:

```
BankText("Silencer", BankTextVariant::Title, { .effectColor = 152 })
```

Title variant → bank 135, cell width 11. EffectColor 152 is the lobby
title's palette index. The rendered glyphs land at (15, 32) and match
the legacy `Overlay` text path byte-for-byte (same `Renderer::DrawText`
call shape: `bank=135 width=11 tint=152 brightness=128 ramp=false`).

## Pass bar

`< 0.5%` byte diff. Current diff:
`0.0000%` (byte-stable across runs).

## Run

```bash
bash tests/lobby-ui/bank_text_test/run.sh
```

Override `SILENCER_BIN` if the worktree binary isn't at
`clients/silencer/build/Silencer.app/...`.

## Regenerating the reference

Only when the primitive's render contract changes (e.g., a variant's
bank/width preset is intentionally tweaked):

```bash
REGEN=1 bash tests/lobby-ui/bank_text_test/run.sh
```

Do NOT use `REGEN=1` to silence a failing pixdiff — fix the rendering
divergence first.
