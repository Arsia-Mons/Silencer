# tests/lobby-ui

Manual visual harnesses for Clay migration screenshots and primitive parity.
These scripts are not part of the auto-discovered CLI E2E suite; the automatic
runner only executes `tests/cli-agent/e2e/[0-9]*_*.sh`.

Prerequisites:

```
clients/silencer/build.sh win-ninja
cmake --build tools/pixdiff/build
(cd services/lobby && go build)
```

Run individual harnesses from this directory, for example:

```
tests/lobby-ui/lobby_stepped_pane_test/run.sh
tests/lobby-ui/text_input_test/run.sh
```

The stepped-pane harness writes a fresh `screenshot.png` beside its committed
`reference.png`. Treat `screenshot.png` as generated output and only update
`reference.png` intentionally via the script's documented `REGEN=1` flow.
