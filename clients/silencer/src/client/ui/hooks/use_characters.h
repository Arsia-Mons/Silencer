#pragma once

#include <functional>
#include <string>
#include <vector>

namespace client::ui {

// The character roster model (doc §6): the agents the account owns + the create
// / select intents the CharacterCreate wizard drives. `received` is false while
// the roster is still in flight from the lobby (the wizard waits on it before
// reconciling a create). Intents route over the public lobby seam
// (CreateCharacter / SelectCharacter), queued + drained after render; a
// successful create routes to the lobby via the CREATECHARACTER tick reconcile,
// a select routes immediately. (rename / set_agency join with the lobby
// CharacterPanel in a later SIL-21 sub-slice.)
struct Characters {
  std::vector<std::string> roster = {};
  bool received = false;
  // The selected agent's name + a compact stat summary, for the lobby
  // CharacterPanel (empty until the roster + user record arrive).
  std::string selected_summary = {};

  std::function<void(const std::string &, int)> create = {}; // (alias, agency 0..4)
  std::function<void(int)> select = {};                       // existing, by roster index
};

Characters use_characters();

} // namespace client::ui
