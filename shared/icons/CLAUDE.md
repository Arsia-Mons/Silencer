# shared/icons/ — application icons

Icon assets bundled into the `clients/silencer` build for each
platform. Not consumed at runtime; baked into installer artifacts.

- `icon.ico` — Windows. Referenced by `resources.rc` (`IDI_ICON1`)
  and embedded into the `.exe`.
- `icon.icns` — macOS. Referenced by `CMakeLists.txt`
  (`MACOSX_BUNDLE_ICON_FILE`) and copied into the `.app`'s
  `Contents/Resources/`.
- `icon_{16,32,64,128}.png` — Linux. Installed by
  `CMakeLists.txt` to `${DATAROOTDIR}/icons/hicolor/<size>x<size>/apps/zsilencer.png`
  for the desktop entry (`zsilencer.desktop`).
