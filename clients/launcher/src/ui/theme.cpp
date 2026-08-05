// The launcher's theme and the providers that carry a frame's ViewModel into
// the tree. Everything the views read comes through here.

#include "ui.h"

#include "ui/tokens.h"

namespace launcher {

ReactContext LauncherContext = {};

const ViewModel &use_launcher() {
  static const ViewModel empty = {};
  const ViewModel *vm = static_cast<const ViewModel *>(use_context(&LauncherContext));
  return vm ? *vm : empty;
}

const AppSnapshot &use_snapshot() {
  static const AppSnapshot empty = {};
  const ViewModel &vm = use_launcher();
  return vm.snap ? *vm.snap : empty;
}

const Intents &use_intents() {
  static const Intents empty = {};
  const ViewModel &vm = use_launcher();
  return vm.intents ? *vm.intents : empty;
}

const ::ui::Theme &launcher_theme() {
  static const ::ui::Theme t = [] {
    ::ui::Theme th{};
    th.text_default = kText;
    th.text_disabled = kTextFaint;
    th.focus_ring = kAccent;
    th.box.base = ::ui::VisualStyle{};
    th.text.base.text = tv(kText, 11, kFaceBody);
    // button/box/input roles are driven entirely by per-instance style patches.
    return th;
  }();
  return t;
}

::ui::UiElement launcher_providers(::ui::UiElement child, const ViewModel *vm) {
  ::ui::UiElement themed = ::ui::provider(
      "LauncherTheme", &::ui::ThemeContext,
      const_cast<::ui::Theme *>(&launcher_theme()), ::ui::children({child}));
  return ::ui::provider("LauncherProvider", &LauncherContext,
                        const_cast<ViewModel *>(vm), ::ui::children({themed}));
}

} // namespace launcher
