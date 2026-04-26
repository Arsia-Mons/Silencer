# shared/design/sdl3

C++17 + SDL3 hydration of the Silencer main menu, built only from
`docs/design/` plus the binary assets in `shared/assets/`.

Build:

```
cmake -B build
cmake --build build
```

Dump-mode capture (no window opens):

```
SILENCER_DUMP_DIR=/tmp/sdl3_dump \
  ./build/silencer_design /Users/hv/repos/Silencer/shared/assets
```

Writes `${SILENCER_DUMP_DIR}/screen_00.ppm` (640x480 P6).
