# clients/launcher/installer — Windows installer

`silencer-launcher.iss` — Inno Setup 6 script for
`silencer-launcher-windows-x64-setup-<version>.exe`. Built by
`release.yml`'s `build-launcher-windows` job, after
`.github/actions/build-launcher-windows` stages the payload.

```
ISCC.exe /DMyAppVersion=00042 [/DSourceDir=<staged>] silencer-launcher.iss
```

`SourceDir` defaults to `build-launcher\package\silencer-launcher` —
the layout the CI action stages. ISCC writes to `Output/` next to this
file; the release job moves the `.exe` out of there.

Modelled on `clients/silencer/installer/silencer.iss`. Keep the two in
step where it makes sense, but note the differences below — they are
deliberate.

## Gotchas

- **`AppId` is `{D661CF3A-...}`, not the game's `{F6A1252E-...}`.**
  Windows keys upgrade and uninstall off this GUID. Reusing the game's
  would make each product's installer clobber the other's registration;
  changing this one orphans every prior launcher install. Never
  regenerate it.
- **`DefaultDirName` is `{localappdata}\Programs\Silencer Launcher`,
  with the word Launcher.** `{localappdata}\Programs\Silencer` is the
  launcher's own default `base_dir` for **game** installs
  (`src/config.cpp`). Installing the launcher there drops it on top of
  the per-channel dirs it manages.
- **The root exe is the bootstrap stub** (issue #347, stub-first
  layout): shortcuts point at `{app}\silencer-launcher.exe` = the stub;
  the cppx launcher + its DLLs + resources live under `{app}\payload\`,
  and the stub's versioned store grows at `{app}\versions\`.
  `[InstallDelete]` clears the pre-stub root DLLs/fonts/assets on
  upgrade, and resets `versions\` — a manual (re)install makes the
  installed seed the current version.
- **`fonts\` and `assets\` must ship and must stay beside the payload
  exe.** `resolve_resource_dir()` (`src/main.cpp`) probes those two dir
  names next to the binary for `silencer-135.otf` and `PALETTE.BIN`.
  Missing fonts are fatal — the launcher exits before a window opens,
  so a dropped `[Files]` line reads as "the installer works, the app is
  broken". The CI action asserts both sentinels before ISCC runs.
- **`[UninstallDelete]` wipes `{app}`.** Safe, and checked: the config
  is at `%APPDATA%\Silencer Launcher\` and installed games are under
  `%LOCALAPPDATA%\Programs\Silencer\`. Uninstalling the launcher must
  not delete a user's games.
- **Not code-signed.** SmartScreen warns on first run. The release
  notes say so. Same position as the game's installer.
