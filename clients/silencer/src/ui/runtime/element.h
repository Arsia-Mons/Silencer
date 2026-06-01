#pragma once

#include "react.h"
#include "tree.h"

#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <new>
#include <stdint.h>
#include <type_traits>

namespace ui {

constexpr int UI_RETAINED_MAX_ELEMENTS = 768;
constexpr int UI_RETAINED_MAX_CHILD_ELEMENTS = 768;
// Per-frame arena for copied component-prop records. Each props struct now
// embeds a StyleStatePatch `style` (seven StylePatch slots), so a single record
// is ~2.3 KB; a complex screen (the loadout) instantiates dozens, so the arena
// is sized to hold them with headroom rather than the former 64 KB.
constexpr int UI_RETAINED_ELEMENT_ARENA_BYTES = 524288;
constexpr int UI_RETAINED_MAX_ELEMENT_DESTRUCTORS = 1024;
constexpr int UI_RETAINED_STRING_ARENA_BYTES = 16384;

struct UiElement;
class UiElementFrame;
struct UiChild;

struct UiChildren {
  const UiElement *items = nullptr;
  int count = 0;
};

enum class UiElementKind : uint8_t {
  Empty,
  Fragment,
  Host,
  Component,
  Provider,
};

enum class HostKind : uint8_t {
  Box,
  Text,
  Button,
  Input,
  Checkbox,
  Dialog,
};

struct HostTextProps {
  const char *value = "";
};

struct AccessibilityProps {
  SemanticRole role = SemanticRole::Auto;
  const char *label = "";
  const char *description = "";
};

struct HostCallbacks {
  std::function<void(const FocusEvent &)> on_focus = {};
  std::function<void(const BlurEvent &)> on_blur = {};
  std::function<void(const ActivationEvent &)> on_activate = {};
  std::function<void(const KeyEvent &)> on_key = {};
  std::function<void(const TextInputEvent &)> on_text_input = {};
  std::function<void(const TextChangeEvent &)> on_text_change = {};
  std::function<void(const TextSubmitEvent &)> on_text_submit = {};
  std::function<void(const ScrollEvent &)> on_scroll = {};
  std::function<void(const TextEditingEvent &)> on_text_editing = {};
};

struct HostProps {
  const char *key = nullptr;
  const char *id = nullptr;
  int id_offset = 0;
  LayoutStyle style = {};
  VisualStyle visual = {}; // dense resolved paint (dual-path; see styling design)
  HostTextProps text = {};
  NodeInteraction interaction = {};
  TextEditMetadata text_edit = {};
  AccessibilityProps accessibility = {};
  HostCallbacks callbacks = {};
  UiChildren children = {};
};

using UiComponentRenderFn = UiElement (*)(const void *props,
                                          UiElementFrame &frame);

struct ComponentElement {
  const char *name = "";
  const char *key = nullptr;
  const void *props = nullptr;
  UiComponentRenderFn render = nullptr;
};

struct ProviderElement {
  const char *name = "";
  const char *key = nullptr;
  ReactContext *context = nullptr;
  void *value = nullptr;
  UiChildren children = {};
};

struct HostElement {
  HostKind kind = HostKind::Box;
  HostProps props = {};
};

struct UiElement {
  UiElementKind kind = UiElementKind::Empty;
  UiChildren children = {};
  HostElement host = {};
  ComponentElement component = {};
  ProviderElement provider = {};
};

struct UiChild {
  UiElement element = {};
  UiChildren children = {};
  bool flatten_children = false;

  UiChild(UiElement value) : element(value) {}
  UiChild(const char *value);
  UiChild(UiChildren value) : children(value), flatten_children(true) {}
};

namespace detail {

template <typename Props>
auto component_key_from_props(const Props &props, int)
    -> decltype((void)props.key, static_cast<const char *>(nullptr)) {
  return props.key && props.key[0] != '\0' ? props.key : nullptr;
}

template <typename Props>
const char *component_key_from_props(const Props &, long) {
  return nullptr;
}

} // namespace detail

class UiElementFrame {
public:
  UiElementFrame();
  ~UiElementFrame();

  UiElementFrame(const UiElementFrame &) = delete;
  UiElementFrame &operator=(const UiElementFrame &) = delete;

  void reset();

  UiChildren children(std::initializer_list<UiElement> items);
  UiChildren children(std::initializer_list<UiChild> items);
  UiChildren children(const UiElement *items, int count);
  UiElement empty();
  UiElement fragment(UiChildren children);
  UiElement host(HostKind kind, const HostProps &props);
  UiElement box(const HostProps &props);
  UiElement text(const char *value, const char *key = nullptr,
                 const LayoutStyle &style = {});
  UiElement provider(const char *name, ReactContext *context, void *value,
                     UiChildren children, const char *key = nullptr);

  template <typename T> const T *copy_value(const T &value) {
    return store(value);
  }

  template <typename Props>
  UiElement component(const char *name, const Props &props,
                      UiElement (*render)(const Props &),
                      const char *key = nullptr) {
    ComponentRecord<Props> record = {
        .render = render,
        .props = props,
    };
    const ComponentRecord<Props> *stored = store(record);
    if (!stored)
      return empty();
    const char *resolved_key =
        key ? key : detail::component_key_from_props(props, 0);
    return component_raw(name, resolved_key, stored,
                         &render_component_record<Props>);
  }

  const char *copy_string(const char *value);

  int error_count() const { return error_count_; }

private:
  template <typename Props> struct ComponentRecord {
    UiElement (*render)(const Props &);
    Props props;
  };

  struct DestructorEntry {
    void *storage = nullptr;
    void (*destroy)(void *storage) = nullptr;
  };

  template <typename T> static void destroy_value(void *storage) {
    static_cast<T *>(storage)->~T();
  }

  template <typename T>
  static UiElement render_component_record(const void *raw,
                                           UiElementFrame &frame) {
    const ComponentRecord<T> *record =
        static_cast<const ComponentRecord<T> *>(raw);
    if (!record || !record->render)
      return frame.empty();
    (void)frame;
    return record->render(record->props);
  }

  template <typename T> const T *store(const T &value) {
    static_assert(!std::is_reference_v<T>);
    uintptr_t base = reinterpret_cast<uintptr_t>(arena_.data()) + arena_offset_;
    uintptr_t aligned =
        (base + alignof(T) - 1u) & ~(uintptr_t)(alignof(T) - 1u);
    size_t next_offset =
        (aligned - reinterpret_cast<uintptr_t>(arena_.data())) + sizeof(T);
    if (next_offset > arena_.size() ||
        destructor_count_ >= UI_RETAINED_MAX_ELEMENT_DESTRUCTORS) {
      ++error_count_;
      return nullptr;
    }

    void *storage = reinterpret_cast<void *>(aligned);
    new (storage) T(value);
    destructors_[destructor_count_++] = {
        .storage = storage,
        .destroy = &destroy_value<T>,
    };
    arena_offset_ = next_offset;
    return static_cast<const T *>(storage);
  }

  UiElement component_raw(const char *name, const char *key, const void *props,
                          UiComponentRenderFn render);
  HostProps copy_host_props(const HostProps &props);

  std::array<UiElement, UI_RETAINED_MAX_ELEMENTS> elements_ = {};
  std::array<UiElement, UI_RETAINED_MAX_CHILD_ELEMENTS> child_elements_ = {};
  std::array<std::byte, UI_RETAINED_ELEMENT_ARENA_BYTES> arena_ = {};
  std::array<DestructorEntry, UI_RETAINED_MAX_ELEMENT_DESTRUCTORS>
      destructors_ = {};
  std::array<char, UI_RETAINED_STRING_ARENA_BYTES> strings_ = {};
  int element_count_ = 0;
  int child_element_count_ = 0;
  size_t arena_offset_ = 0;
  int destructor_count_ = 0;
  int string_count_ = 0;
  int error_count_ = 0;
};

class UiElementFrameScope {
public:
  explicit UiElementFrameScope(UiElementFrame &frame);
  ~UiElementFrameScope();

  UiElementFrameScope(const UiElementFrameScope &) = delete;
  UiElementFrameScope &operator=(const UiElementFrameScope &) = delete;

private:
  UiElementFrame *previous_ = nullptr;
};

UiElementFrame *current_element_frame();
UiChildren children(std::initializer_list<UiElement> items);
UiChildren children(std::initializer_list<UiChild> items);
UiChildren children(const UiElement *items, int count);
UiElement empty();
UiElement fragment(UiChildren children);
UiElement host(HostKind kind, const HostProps &props);
UiElement box(const HostProps &props);
UiElement text(const char *value, const char *key = nullptr,
               const LayoutStyle &style = {});
UiElement provider(const char *name, ReactContext *context, void *value,
                   UiChildren children, const char *key = nullptr);
const char *copy_string(const char *value);

inline UiChild::UiChild(const char *value)
    : element(value ? text(value) : empty()) {}

template <typename T> const T *copy_value(const T &value) {
  UiElementFrame *frame = current_element_frame();
  if (!frame) {
    react_report_error(
        "ui/retained: missing current UiElementFrame for copy_value\n");
    return nullptr;
  }
  return frame->copy_value(value);
}

template <typename Props>
UiElement component(const char *name, const Props &props,
                    UiElement (*render)(const Props &),
                    const char *key = nullptr) {
  UiElementFrame *frame = current_element_frame();
  if (!frame) {
    react_report_error(
        "ui/retained: missing current UiElementFrame for component %s\n",
        name ? name : "");
    return {};
  }
  return frame->component(name, props, render, key);
}

struct ReconcileResult {
  bool ok = false;
  int error_count = 0;
};

ReconcileResult reconcile_retained_tree(UiTree &tree, UiElementFrame &frame,
                                        const UiElement &root, float width,
                                        float height);
ReconcileResult commit_retained_elements(UiTree &tree, UiElementFrame &frame,
                                         const UiElement &root);

} // namespace ui
