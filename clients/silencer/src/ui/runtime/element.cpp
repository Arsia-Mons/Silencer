#include "element.h"

#include <string.h>

namespace ui {

namespace {

thread_local UiElementFrame *g_current_element_frame = nullptr;

UiElementFrame *require_current_element_frame(const char *operation) {
  UiElementFrame *frame = current_element_frame();
  if (!frame) {
    react_report_error("ui/retained: missing current UiElementFrame for %s\n",
                       operation ? operation : "operation");
  }
  return frame;
}

const char *host_kind_name(HostKind kind) {
  switch (kind) {
  case HostKind::Box:
    return "Box";
  case HostKind::Text:
    return "Text";
  case HostKind::Button:
    return "Button";
  case HostKind::Input:
    return "Input";
  case HostKind::Checkbox:
    return "Checkbox";
  case HostKind::Dialog:
    return "Dialog";
  }
  return "Box";
}

NodeRole node_role_for_host_kind(HostKind kind) {
  switch (kind) {
  case HostKind::Box:
    return NodeRole::Box;
  case HostKind::Text:
    return NodeRole::Text;
  case HostKind::Button:
    return NodeRole::Button;
  case HostKind::Input:
    return NodeRole::Input;
  case HostKind::Checkbox:
    return NodeRole::Checkbox;
  case HostKind::Dialog:
    return NodeRole::Dialog;
  }
  return NodeRole::Generic;
}

ReactFiberId make_reconciler_fiber_id(const char *name, const char *key) {
  uint32_t sibling_index = react_next_child_index();
  if (key && key[0] != '\0') {
    return react_make_instance_fiber_key_id(name, key);
  }
  return react_make_instance_fiber_id(name, sibling_index, false);
}

class Reconciler {
public:
  Reconciler(UiTree &tree, UiElementFrame &frame)
      : tree_(tree), frame_(frame) {}

  bool commit(const UiElement &element) {
    switch (element.kind) {
    case UiElementKind::Empty:
      return true;
    case UiElementKind::Fragment:
      return commit_children(element.children);
    case UiElementKind::Host:
      return commit_host(element.host);
    case UiElementKind::Component:
      return commit_component(element.component);
    case UiElementKind::Provider:
      return commit_provider(element.provider);
    }
    ++error_count_;
    return false;
  }

  int error_count() const { return error_count_; }

private:
  bool commit_children(UiChildren children) {
    for (int i = 0; i < children.count; ++i) {
      if (!commit(children.items[i]))
        return false;
    }
    return true;
  }

  bool commit_host(const HostElement &host) {
    const HostProps &props = host.props;
    const char *type = host_kind_name(host.kind);
    NodeId id = props.key && props.key[0] != '\0'
                    ? tree_.begin_keyed_node(type, props.key, props.style)
                    : tree_.begin_node(type, props.style);
    if (id == 0) {
      ++error_count_;
      return false;
    }

    NodeMetadata metadata = {
        .role = node_role_for_host_kind(host.kind),
        .semantic_role = props.accessibility.role,
        .visual = props.visual,
        .fiber_id = react_current_fiber_id(),
        .control_id = props.id,
        .control_offset = props.id_offset,
        .accessibility_label = props.accessibility.label,
        .accessibility_description = props.accessibility.description,
        .value = props.text.value,
        .interaction = props.interaction,
        .text_edit = props.text_edit,
        .on_focus = props.callbacks.on_focus,
        .on_blur = props.callbacks.on_blur,
        .on_activate = props.callbacks.on_activate,
        .on_key = props.callbacks.on_key,
        .on_text_input = props.callbacks.on_text_input,
        .on_text_change = props.callbacks.on_text_change,
        .on_scroll = props.callbacks.on_scroll,
        .on_text_editing = props.callbacks.on_text_editing,
    };
    bool metadata_ok = tree_.set_metadata(id, metadata);
    if (!metadata_ok) {
      ++error_count_;
    }
    bool measure_ok = true;
    if (host.kind == HostKind::Text) {
      // Route layout through the shared injected measurer (design §10.1): the
      // node's value + resolved TextVisual drive the query, so layout and paint
      // measure identically. metadata is committed above, so the node already
      // carries value + visual.text.
      measure_ok = tree_.set_text_measure(id);
      if (!measure_ok) {
        ++error_count_;
      }
    }

    bool children_ok =
        metadata_ok && measure_ok ? commit_children(props.children) : false;
    bool end_ok = tree_.end_node();
    if (!end_ok) {
      ++error_count_;
    }
    return metadata_ok && measure_ok && children_ok && end_ok;
  }

  bool commit_component(const ComponentElement &component) {
    if (!component.name || !component.name[0] || !component.render) {
      ++error_count_;
      return false;
    }

    react_enter(make_reconciler_fiber_id(component.name, component.key));
    UiElement rendered = component.render(component.props, frame_);
    bool ok = commit(rendered);
    react_leave();
    return ok;
  }

  bool commit_provider(const ProviderElement &provider) {
    if (!provider.name || !provider.name[0] || !provider.context) {
      ++error_count_;
      return false;
    }

    react_enter(make_reconciler_fiber_id(provider.name, provider.key));
    react_provider_push(provider.context, provider.value);
    bool ok = commit_children(provider.children);
    react_provider_pop(provider.context);
    react_leave();
    return ok;
  }

  UiTree &tree_;
  UiElementFrame &frame_;
  int error_count_ = 0;
};

} // namespace

UiElementFrame::UiElementFrame() = default;

UiElementFrame::~UiElementFrame() { reset(); }

UiElementFrameScope::UiElementFrameScope(UiElementFrame &frame)
    : previous_(g_current_element_frame) {
  g_current_element_frame = &frame;
}

UiElementFrameScope::~UiElementFrameScope() {
  g_current_element_frame = previous_;
}

UiElementFrame *current_element_frame() { return g_current_element_frame; }

void UiElementFrame::reset() {
  for (int i = destructor_count_ - 1; i >= 0; --i) {
    if (destructors_[i].destroy) {
      destructors_[i].destroy(destructors_[i].storage);
    }
  }
  for (int i = 0; i < element_count_; ++i) {
    elements_[i] = {};
  }
  for (int i = 0; i < child_element_count_; ++i) {
    child_elements_[i] = {};
  }

  destructors_ = {};
  strings_ = {};
  element_count_ = 0;
  child_element_count_ = 0;
  arena_offset_ = 0;
  destructor_count_ = 0;
  string_count_ = 0;
  error_count_ = 0;
}

UiChildren UiElementFrame::children(std::initializer_list<UiElement> items) {
  return children(items.begin(), static_cast<int>(items.size()));
}

UiChildren UiElementFrame::children(const UiElement *items, int count) {
  if (count <= 0)
    return {};
  if (!items) {
    ++error_count_;
    return {};
  }
  if (child_element_count_ + count > UI_RETAINED_MAX_CHILD_ELEMENTS) {
    ++error_count_;
    return {};
  }

  UiElement *start = &child_elements_[child_element_count_];
  int index = 0;
  for (int i = 0; i < count; ++i) {
    const UiElement &item = items[i];
    start[index++] = item;
  }
  child_element_count_ += index;
  return {
      .items = start,
      .count = index,
  };
}

UiChildren UiElementFrame::children(std::initializer_list<UiChild> items) {
  int child_count = 0;
  for (const UiChild &item : items) {
    if (!item.flatten_children) {
      ++child_count;
      continue;
    }
    if (item.children.count < 0 ||
        (item.children.count > 0 && !item.children.items)) {
      ++error_count_;
      return {};
    }
    child_count += item.children.count;
  }
  if (child_count == 0)
    return {};
  if (child_element_count_ + child_count > UI_RETAINED_MAX_CHILD_ELEMENTS) {
    ++error_count_;
    return {};
  }

  UiElement *start = &child_elements_[child_element_count_];
  int index = 0;
  for (const UiChild &item : items) {
    if (!item.flatten_children) {
      start[index++] = item.element;
      continue;
    }
    for (int child_index = 0; child_index < item.children.count;
         ++child_index) {
      start[index++] = item.children.items[child_index];
    }
  }
  child_element_count_ += index;
  return {
      .items = start,
      .count = index,
  };
}

UiElement UiElementFrame::empty() { return {}; }

UiElement UiElementFrame::fragment(UiChildren children) {
  if (element_count_ >= UI_RETAINED_MAX_ELEMENTS) {
    ++error_count_;
    return empty();
  }
  UiElement &element = elements_[element_count_++];
  element = {
      .kind = UiElementKind::Fragment,
      .children = children,
  };
  return element;
}

UiElement UiElementFrame::host(HostKind kind, const HostProps &props) {
  if (element_count_ >= UI_RETAINED_MAX_ELEMENTS) {
    ++error_count_;
    return empty();
  }
  UiElement &element = elements_[element_count_++];
  element = {
      .kind = UiElementKind::Host,
      .host =
          {
              .kind = kind,
              .props = copy_host_props(props),
          },
  };
  return element;
}

UiElement UiElementFrame::box(const HostProps &props) {
  return host(HostKind::Box, props);
}

UiElement UiElementFrame::text(const char *value, const char *key,
                               const LayoutStyle &style) {
  return host(HostKind::Text, {
                                  .key = key,
                                  .style = style,
                                  .text = {.value = value},
                              });
}

UiElement UiElementFrame::provider(const char *name, ReactContext *context,
                                   void *value, UiChildren children,
                                   const char *key) {
  if (element_count_ >= UI_RETAINED_MAX_ELEMENTS) {
    ++error_count_;
    return empty();
  }
  UiElement &element = elements_[element_count_++];
  element = {
      .kind = UiElementKind::Provider,
      .provider =
          {
              .name = copy_string(name),
              .key = copy_string(key),
              .context = context,
              .value = value,
              .children = children,
          },
  };
  return element;
}

const char *UiElementFrame::copy_string(const char *value) {
  if (!value)
    return nullptr;
  size_t length = strlen(value);
  if (string_count_ + static_cast<int>(length) + 1 >
      UI_RETAINED_STRING_ARENA_BYTES) {
    ++error_count_;
    return "";
  }
  char *dest = &strings_[string_count_];
  memcpy(dest, value, length + 1);
  string_count_ += static_cast<int>(length) + 1;
  return dest;
}

UiElement UiElementFrame::component_raw(const char *name, const char *key,
                                        const void *props,
                                        UiComponentRenderFn render) {
  if (element_count_ >= UI_RETAINED_MAX_ELEMENTS) {
    ++error_count_;
    return empty();
  }
  UiElement &element = elements_[element_count_++];
  element = {
      .kind = UiElementKind::Component,
      .component =
          {
              .name = copy_string(name),
              .key = copy_string(key),
              .props = props,
              .render = render,
          },
  };
  return element;
}

HostProps UiElementFrame::copy_host_props(const HostProps &props) {
  HostProps copied = props;
  copied.key = copy_string(props.key);
  copied.id = copy_string(props.id);
  copied.text.value = copy_string(props.text.value);
  copied.text_edit.input_value = copy_string(props.text_edit.input_value);
  copied.text_edit.composition = copy_string(props.text_edit.composition);
  copied.accessibility.label = copy_string(props.accessibility.label);
  copied.accessibility.description =
      copy_string(props.accessibility.description);
  return copied;
}

UiChildren children(std::initializer_list<UiElement> items) {
  UiElementFrame *frame = require_current_element_frame("children");
  return frame ? frame->children(items) : UiChildren{};
}

UiChildren children(const UiElement *items, int count) {
  UiElementFrame *frame = require_current_element_frame("children");
  return frame ? frame->children(items, count) : UiChildren{};
}

UiChildren children(std::initializer_list<UiChild> items) {
  UiElementFrame *frame = require_current_element_frame("children");
  return frame ? frame->children(items) : UiChildren{};
}

UiElement empty() {
  UiElementFrame *frame = require_current_element_frame("empty");
  return frame ? frame->empty() : UiElement{};
}

UiElement fragment(UiChildren children) {
  UiElementFrame *frame = require_current_element_frame("fragment");
  return frame ? frame->fragment(children) : UiElement{};
}

UiElement host(HostKind kind, const HostProps &props) {
  UiElementFrame *frame = require_current_element_frame("host");
  return frame ? frame->host(kind, props) : UiElement{};
}

UiElement box(const HostProps &props) {
  UiElementFrame *frame = require_current_element_frame("box");
  return frame ? frame->box(props) : UiElement{};
}

UiElement text(const char *value, const char *key, const LayoutStyle &style) {
  UiElementFrame *frame = require_current_element_frame("text");
  return frame ? frame->text(value, key, style) : UiElement{};
}

UiElement provider(const char *name, ReactContext *context, void *value,
                   UiChildren children, const char *key) {
  UiElementFrame *frame = require_current_element_frame("provider");
  return frame ? frame->provider(name, context, value, children, key)
               : UiElement{};
}

const char *copy_string(const char *value) {
  UiElementFrame *frame = require_current_element_frame("copy_string");
  return frame ? frame->copy_string(value) : nullptr;
}

ReconcileResult reconcile_retained_tree(UiTree &tree, UiElementFrame &frame,
                                        const UiElement &root, float width,
                                        float height) {
  tree.begin_frame(width, height);
  react_begin_frame();

  ReconcileResult commit = commit_retained_elements(tree, frame, root);

  bool tree_ok = tree.end_frame();
  react_end_frame();

  int errors = commit.error_count + tree.error_count() + react_error_count();
  return {
      .ok = commit.ok && tree_ok && errors == 0,
      .error_count = errors,
  };
}

ReconcileResult commit_retained_elements(UiTree &tree, UiElementFrame &frame,
                                         const UiElement &root) {
  UiElementFrameScope frame_scope(frame);
  Reconciler reconciler(tree, frame);
  bool ok = reconciler.commit(root);
  int errors = frame.error_count() + reconciler.error_count();
  return {
      .ok = ok && errors == 0,
      .error_count = errors,
  };
}

} // namespace ui
