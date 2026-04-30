# clients/lobby-sdk/cpp — C++ lobby SDK

Standalone C++14 static library. **No SDL3 dependency** — that's the
defining constraint vs. the in-game client at `clients/silencer/`.

## Build & test

```sh
cmake -B build -S .
cmake --build build
./build/codec_test           # golden vectors + sha1 + roundtrip
ctest --test-dir build       # same, via ctest
```

The example binary (`build/chat_listener`) connects to a real lobby
and prints chat — useful for end-to-end smoke checks against a local
lobby (`go run ./services/lobby` in another terminal).

## Layout

- `include/silencer/lobby/types.h` — wire-format structs
  (`LobbyGame`, `UserInfo`, `MatchStats`, etc) + opcode/platform
  enums.
- `include/silencer/lobby/codec.h` — pure encoders/decoders. No
  socket I/O. Throws `CodecError` on malformed input.
- `include/silencer/lobby/client.h` — `Client` class: non-blocking
  TCP, `select`-driven, single-threaded. Drive it from your main loop
  by calling `poll(timeout)` repeatedly; callbacks fire from inside
  `poll()` on the calling thread.
- `src/sha1.cpp` — vendored RFC 3174 SHA-1 (public domain). Match
  the reference C++ client's `sha1::calc()` byte-for-byte.

## Invariants

- **No SDL** in any source under this directory. The library must
  build without SDL on the include path.
- **Opcodes & wire format** must match `services/lobby/protocol.go`
  byte-for-byte. The codec tests load
  `../../shared/lobby-protocol/vectors.json` to enforce this.
- **`Client::poll()` must not block longer than `max_wait`.** It
  uses `select(2)` with the supplied timeout and dispatches
  callbacks synchronously after the read.
- **Don't block in callbacks.** They run on the same thread as
  `poll()`; long work delays heartbeat and can trip the read
  timeout.

## Gotchas

- Connecting to the production lobby on port 517 needs root on
  macOS/Linux. For local dev, `services/lobby/CLAUDE.md` calls out
  using `:15170` — the SDK takes a configurable port, so just set
  `cfg.port = 15170`.
- POSIX-only today (`sys/socket.h`, `select`). Windows port is a
  small `#ifdef _WIN32` block (`<winsock2.h>`, `closesocket`,
  `WSAStartup`); add when needed.
- `MAX_FRAME_PAYLOAD == 255`. The codec enforces this on encode;
  exceeding it throws.
