# Task Notes: Restore bright green active-games frame in lobby UI

Created: 2026-05-17T11:22:07.129750
Completed: 2026-05-17T11:40:07.960793

## Summary
Committed lobby UI fixes that make the right tall panes use container-based flex sizing and restore the bright inset borders for Game Options, Select Map, and the Active Games server browser. Verified with client build plus the game_select_panel_test and game_create_panel_test harnesses.

## Files Changed
- clients/silencer/src/client/ui/screens/lobby/game_create_panel.h
- clients/silencer/src/client/ui/screens/lobby/game_create_panel_map_form.cpp
- clients/silencer/src/client/ui/screens/lobby/game_create_panel_options.cpp
- clients/silencer/src/client/ui/screens/lobby/game_select_panel.h
- clients/silencer/src/client/ui/screens/lobby/game_select_panel_layout.cpp
- clients/silencer/src/client/ui/screens/lobby/lobby_main_area.cpp
- clients/silencer/src/ui/primitives/box.h

## TODO
- [ ] Review diff.patch
- [ ] Fill out evaluation rubric
- [ ] Implement tests
- [ ] Validate trace data
