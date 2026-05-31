#pragma once

#include <stdint.h>

#include "../../../../ui/runtime/element.h"

namespace client::ui {

using UiScreenEntryId = uint32_t;

enum class ScreenKind {
  Normal,
  Overlay,
};

class UiScreen {
public:
  virtual ~UiScreen() = default;

  UiScreenEntryId entry_id() const { return entry_id_; }
  void set_entry_id(UiScreenEntryId id) { entry_id_ = id; }

  ScreenKind kind() const { return kind_; }

  virtual const char *debug_name() const = 0;
  virtual bool build_element(::ui::UiElementFrame &frame, ::ui::UiElement *out);
  virtual void build_ui() = 0;

protected:
  UiScreen() = default;
  explicit UiScreen(ScreenKind kind) : kind_(kind) {}

private:
  UiScreenEntryId entry_id_ = 0;
  ScreenKind kind_ = ScreenKind::Normal;
};

class OverlayScreen : public UiScreen {
protected:
  OverlayScreen() : UiScreen(ScreenKind::Overlay) {}
};

} // namespace client::ui
