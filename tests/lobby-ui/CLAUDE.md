# tests/lobby-ui

Manual visual harnesses for Clay migration screenshots and primitive parity.
These scripts are not part of the auto-discovered CLI E2E suite; the automatic
runner only executes `tests/cli-agent/e2e/[0-9]*_*.sh`.

Prerequisites:

```
cmake --build build --target silencer
cmake --build tools/pixdiff/build
cd services/lobby && go build
```

Run individual harnesses from this directory, for example:

```
tests/lobby-ui/game_create_panel_test/run.sh
tests/lobby-ui/text_input_test/run.sh
```

The panel harnesses write fresh `legacy.png` / `screenshot.png` captures into
their own directories. Treat those as generated output and do not commit them
unless a script explicitly documents the file as a stable reference baseline.
