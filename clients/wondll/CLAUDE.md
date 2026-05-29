# clients/wondll

Drop-in `WONDLL.dll` replacement that lets the original Silencer Beta 0110
binary connect to the self-hosted Silencer lobby instead of WON.net.

## What this is

The original `Silencer.exe` calls into `WONDLL.dll` for all authentication and
profile operations. By placing a replacement DLL alongside `Silencer.exe`,
Windows loads ours instead — no exe patching required.

The DLL implements all 16 original WON API exports as thin HTTP stubs that
talk to the Silencer lobby's WON-compat endpoints (`/won/*` on the player-auth
server, default port `:15171`).

## Source

| File | Purpose |
|------|---------|
| `wondll.c` | All 16 export stubs + WinInet HTTP + minimal JSON parser |
| `CMakeLists.txt` | CMake build (32-bit Windows DLL) |
| `cmake/mingw-i686.cmake` | MinGW cross-compile toolchain file |

## Build

### macOS / Linux (cross-compile with MinGW)

```sh
brew install mingw-w64          # macOS
# apt install gcc-mingw-w64-i686  # Ubuntu

cd clients/wondll
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build
# Output: build/WONDLL.dll
```

### Windows (MSVC x86)

Open a **"x86 Native Tools Command Prompt"** (not x64):

```cmd
cd clients\wondll
cmake -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Usage

1. Build `WONDLL.dll` (above).
2. Copy it into the same folder as `Silencer.exe`.
3. Set the lobby URL if not on localhost:
   ```
   set SILENCER_LOBBY_URL=http://192.168.1.10:15171
   ```
4. Launch `Silencer.exe` — it will connect to your lobby instead of WON.net.

## Configuration

| Env var | Default | Description |
|---------|---------|-------------|
| `SILENCER_LOBBY_URL` | `http://127.0.0.1:15171` | Player-auth server base URL |

## Lobby-side endpoints

The DLL calls these endpoints on the lobby's player-auth server
(`services/lobby/won_compat.go`):

| Method | Path | Description |
|--------|------|-------------|
| `POST` | `/won/login` | Authenticate existing account |
| `POST` | `/won/create-account` | Register new account |
| `GET`  | `/won/profile/:name` | Fetch player profile |
| `POST` | `/won/profile/:name` | Update profile field |

## Exported functions (all 16)

```
WONAuthCloseHandle          WONAuthGetCertificate
WONAuthGetNicknameA         WONAuthGetPrivateKey
WONAuthLoadVerifierKeyFromFileA  WONAuthLoginA
WONAuthLoginNewAccountA     WONIPAddressSetFromString
WONProfileCloseHandle       WONProfileCreate
WONProfileCreateAccount     WONProfileGet
WONProfileGetAccount        WONProfileRemove
WONProfileSet               WONProfileUpdateAccount
```

All use `__stdcall` (WINAPI) calling convention to match the original DLL.

## Notes

- Must be 32-bit (i686) — `Silencer.exe` is a 32-bit Win32 binary.
- No external runtime deps: links only `wininet.dll` and `ws2_32.dll`
  (standard on all Windows versions since 98).
- The original WON.net Auth1 challenge/response crypto is bypassed entirely —
  the stubs return success immediately.
