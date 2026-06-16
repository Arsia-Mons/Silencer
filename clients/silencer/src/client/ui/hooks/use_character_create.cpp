#include "client/ui/hooks/use_character_create.h"

#include "client/ui/screens/character_create_data.h" // kAgencyCount
#include "ui/runtime/element.h"
#include "ui/runtime/react.h"

#include <string>

namespace client::ui {
namespace {

// Holds the staged-create UI state; setters mutate the use_state pointers
// directly from event callbacks (the established Silencer screen idiom — the
// callbacks fire in the focus/hit-test pass, after this frame's build).
struct CharacterCreateContextValue {
  int *step = nullptr;
  std::string *alias = nullptr;
  bool *submitting = nullptr;
  int *preview_agent = nullptr;
  int *preview_agency = nullptr;
};

ReactContext CharacterCreateContext = {};

} // namespace

// The screen-local provider: owns the wizard cursor cells and publishes them on
// CharacterCreateContext for use_character_create() (doc §5/§6).
::ui::UiElement CharacterCreateProvider(const CharacterCreateProviderProps &props) {
  int *step = use_state<int>(0);
  std::string *alias = use_state<std::string>(std::string());
  bool *submitting = use_state<bool>(false);
  int *preview_agent = use_state<int>(0);
  int *preview_agency = use_state<int>(0);
  if (!step || !alias || !submitting || !preview_agent || !preview_agency)
    return ::ui::empty();
  CharacterCreateContextValue value{step, alias, submitting, preview_agent,
                                    preview_agency};
  const CharacterCreateContextValue *stored = ::ui::copy_value(value);
  if (!stored)
    return ::ui::empty();
  return ::ui::provider("CharacterCreateProvider", &CharacterCreateContext,
                        const_cast<CharacterCreateContextValue *>(stored),
                        props.children, props.key);
}

CharacterWizard use_character_create() {
  CharacterCreateContextValue *v = static_cast<CharacterCreateContextValue *>(
      use_context(&CharacterCreateContext));
  if (!v) {
    react_report_error("client/ui: missing CharacterCreateProvider\n");
    return {};
  }
  CharacterWizard w;
  w.step = v->step ? *v->step : 0;
  w.alias = v->alias ? *v->alias : std::string();
  w.submitting = v->submitting ? *v->submitting : false;
  w.preview_agent = v->preview_agent ? *v->preview_agent : 0;
  w.preview_agency = v->preview_agency ? *v->preview_agency : 0;
  int *step = v->step;
  std::string *alias = v->alias;
  bool *submitting = v->submitting;
  int *preview_agent = v->preview_agent;
  int *preview_agency = v->preview_agency;
  // origin cap: alias[17] (character_create_screen.h) — 16 chars.
  w.set_alias = [alias](const std::string &s) {
    if (alias)
      *alias = s.substr(0, 16);
  };
  w.open_alias = [step]() {
    if (step)
      *step = 1;
  };
  w.confirm_alias = [step, alias]() {
    if (step && alias && !alias->empty())
      *step = 2;
  };
  w.reopen_alias = [step]() {
    if (step)
      *step = 1; // Rename Once: back to alias entry, keep the typed alias
  };
  w.cancel = [step, alias, submitting]() {
    if (step)
      *step = 0;
    if (alias)
      alias->clear();
    if (submitting)
      *submitting = false;
  };
  w.begin_submit = [submitting]() {
    if (submitting)
      *submitting = true;
  };
  w.set_preview_agency = [preview_agency](int idx) {
    if (preview_agency && idx >= 0 && idx < kAgencyCount)
      *preview_agency = idx;
  };
  w.set_preview_agent = [preview_agent](int idx) {
    if (preview_agent && idx >= 0)
      *preview_agent = idx;
  };
  return w;
}

} // namespace client::ui
