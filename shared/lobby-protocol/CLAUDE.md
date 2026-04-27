# shared/lobby-protocol — wire protocol spec + golden vectors

Cross-component spec for the lobby's binary TCP protocol.
[`protocol.md`](./protocol.md) is the human-readable reference;
[`vectors.json`](./vectors.json) is the machine-readable test corpus.

## Consumers

- [`clients/lobby-sdk/cpp/`](../../clients/lobby-sdk/cpp/) — C++ SDK
  test suite loads `vectors.json` for round-trip verification.
- [`clients/lobby-sdk/ts/`](../../clients/lobby-sdk/ts/) — TS SDK
  test suite, same.
- [`services/lobby/`](../../services/lobby/) — authoritative
  implementation in `protocol.go`. Not currently driven from the
  vectors here, but should be: a future `protocol_test.go` can read
  this file to verify Go encode/decode against the same bytes.

## Authority

[`services/lobby/protocol.go`](../../services/lobby/protocol.go) is
the source of truth for the wire format. This directory is a
**spec mirror plus tests**, not an implementation.

## Editing rules

1. Adding/changing an opcode is a **breaking wire change**. Bump the
   version handshake on both client and server (see
   `services/lobby/CLAUDE.md`).
2. When you change `protocol.go`:
   - Update [`protocol.md`](./protocol.md).
   - Update or add an entry in [`vectors.json`](./vectors.json).
   - Re-run both SDK test suites (`bun test` under
     `clients/lobby-sdk/ts/`, `ctest` under
     `clients/lobby-sdk/cpp/build/`).
3. Hex strings in `vectors.json` are full frames including the
   leading length byte.
