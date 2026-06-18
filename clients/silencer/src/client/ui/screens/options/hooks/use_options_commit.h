#pragma once

#include "client/ui/hooks/use_navigation.h"

#include <functional>

namespace client::ui {

// The Save / Cancel pair shared by every options sub-screen: persist (Save) or
// revert (Cancel) the live edits, then pop the overlay. The commit/revert
// callbacks differ per screen — use_settings for Audio/Display, use_key_map for
// Controls — so they're passed in; the "pop the overlay afterwards" part is the
// shared boilerplate this hook removes from each view.
struct OptionsCommit {
  std::function<void()> save = {};
  std::function<void()> cancel = {};
};

inline OptionsCommit use_options_commit(std::function<void()> commit,
                                        std::function<void()> revert) {
  Navigation nav = use_navigation(); // context read — no hook state slot
  OptionsCommit c;
  c.save = [nav, commit]() {
    if (commit)
      commit();
    if (nav.pop_current)
      nav.pop_current();
  };
  c.cancel = [nav, revert]() {
    if (revert)
      revert();
    if (nav.pop_current)
      nav.pop_current();
  };
  return c;
}

} // namespace client::ui
