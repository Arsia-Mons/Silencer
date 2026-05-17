# Task Notes: Fix lobby chat scaling math

Created: 2026-05-17T00:34:25.429013
Completed: 2026-05-17T00:38:59.412834

## Summary
Restored the Create Game map hover preview in the lobby Clay UI, including hovered-row tracking, minimap preview rendering, and the floating root-level preview overlay. Verified with a successful client build.

## Files Changed
- clients/silencer/src/client/ui/screens/lobby/game_create_panel.h
- clients/silencer/src/client/ui/screens/lobby/game_create_panel_map_form.cpp
- clients/silencer/src/client/ui/screens/lobby/lobby_main_area.cpp
- clients/silencer/src/client/ui/screens/lobby/lobby_main_area.h
- clients/silencer/src/client/ui/screens/lobby/lobby_screen.cpp
- clients/silencer/src/game/game.h
- clients/silencer/src/render/clay_ui_compositor.cpp
- clients/silencer/src/render/clay_ui_payloads.h
- clients/silencer/src/ui/primitives/scroll_list.cpp
- clients/silencer/src/ui/primitives/scroll_list.h

## TODO
- [ ] Review diff.patch
- [ ] Fill out evaluation rubric
- [ ] Implement tests
- [ ] Validate trace data
