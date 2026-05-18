# tools/pixdiff

Tiny C++ PNG pixel-diff utility used by screenshot-based CLI/E2E and
visual-regression harnesses.

Build from this directory with CMake:

```
cmake -B build
cmake --build build
```

The expected binary path for existing scripts is `tools/pixdiff/build/pixdiff`.
Keep this tool standalone; do not make it depend on the game client or Bun
workspace packages.

`stb_image.h` is vendored stb_image v2.30 from nothings/stb. Its header states
it is public domain / MIT dual-licensed; keep that header intact when updating
the vendored copy.
