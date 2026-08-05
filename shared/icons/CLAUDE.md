# shared/icons/ — application icons

Icon assets bundled into the `clients/silencer` and `clients/launcher`
builds for each platform. Not consumed at runtime; baked into
installer artifacts.

- `icon.ico` — Windows. Referenced by `resources.rc` (`IDI_ICON1`)
  and embedded into the `.exe`.
- `icon.icns` — macOS. Referenced by both clients' `CMakeLists.txt`
  (`MACOSX_BUNDLE_ICON_FILE`) and copied into each `.app`'s
  `Contents/Resources/`. The launcher shares the game's icon on
  purpose — one product, one mark in the Dock. Change it and both
  bundles change.
- `icon_{16,32,64,128}.png` — Linux. Installed by
  `CMakeLists.txt` to `${DATAROOTDIR}/icons/hicolor/<size>x<size>/apps/silencer.png`
  for the desktop entry (`silencer.desktop`).
