#pragma once

#include "ui/runtime/element.h"

#include <functional>
#include <string>

namespace client::ui {

// The CharacterCreate wizard cursor (doc §6, golden `use_loadout` idiom),
// published by the screen-local CharacterCreateProvider. Pure screen-local UI
// state driving the staged create flow over the roster:
//   step 0 — roster (existing agents + a "Create New Character" entry)
//   step 1 — alias entry (a modal over the roster)
//   step 2 — agency selection (a modal of agency rows)
// The actual create routes through use_characters().create on an agency pick;
// `submitting` guards a double-submit (the CREATECHARACTER tick routes to the
// lobby once the new agent round-trips, which unmounts the screen).
struct CharacterWizard {
  int step = 0;
  std::string alias = {};
  bool submitting = false;
  // SIL-101: the agency row currently focused/previewed in step 2, feeding the
  // right detail column. Clamped to 0 by the screen so the column is always
  // populated (defaults to Noxis).
  int preview_agent = 0;
  int preview_agency = 0;

  std::function<void(const std::string &)> set_alias = {};
  std::function<void()> open_alias = {};    // step 0 -> 1
  std::function<void()> confirm_alias = {}; // step 1 -> 2 (no-op if alias empty)
  std::function<void()> reopen_alias = {};  // step 2 -> 1 (Rename Once)
  std::function<void()> cancel = {};        // -> step 0, clears alias + submit guard
  std::function<void()> begin_submit = {};  // latch the double-submit guard
  std::function<void(int)> set_preview_agent = {}; // step-0 detail preview
  std::function<void(int)> set_preview_agency = {}; // step-2 detail preview
};

CharacterWizard use_character_create();

// The screen-local provider that owns the wizard cursor's use_state cells and
// publishes them on CharacterCreateContext (read back by use_character_create()).
// Declared here so character_create.cppx can mount it via JSX; defined in
// use_character_create.cpp alongside the reducer.
struct CharacterCreateProviderProps {
  const char *key = nullptr;
  ::ui::UiChildren children = {};
};
::ui::UiElement CharacterCreateProvider(const CharacterCreateProviderProps &props);

} // namespace client::ui
