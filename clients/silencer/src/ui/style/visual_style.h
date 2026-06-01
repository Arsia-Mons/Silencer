#pragma once

// VisualStyle: the dense, resolved paint description the renderer reads.
// From-first-principles styling system (see docs/retained-ui/styling-render-system-design.md §2).
// All Color values are authored STRAIGHT alpha; premultiplication happens once,
// at the IR emit boundary (draw_list §8.4). Resolved-output presence is read via
// value cues here (background.a==0 => no fill, etc.) — NOT an authoring sentinel.

#include <stdint.h>

#include <type_traits>

namespace ui {

struct Color {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 0;
};

constexpr bool operator==(Color a, Color b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}
constexpr bool operator!=(Color a, Color b) { return !(a == b); }

struct Vec2 {
  float x = 0.f;
  float y = 0.f;
};

// IR-local paint rectangle (deliberately lighter than the layout Rect in
// tree.h, which drags edge metrics). Shared by the DrawCommand IR and the
// SDL-free geometry/tessellation module.
struct DrawRect {
  float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
};

struct SideWidths {
  float top = 0.f;
  float right = 0.f;
  float bottom = 0.f;
  float left = 0.f;
};

struct SideColors {
  Color top{};
  Color right{};
  Color bottom{};
  Color left{};
};

struct Border {
  SideWidths width{};
  SideColors color{};
};

// Single-color stroke at a signed offset from the border-box:
// offset < 0 insets, offset > 0 outsets. Used for the focus ring.
struct Outline {
  float width = 0.f;
  Color color{};
  float offset = 0.f;
};

struct GradientStop {
  float t = 0.f; // in [0,1]
  Color color{};
};

constexpr int UI_MAX_GRADIENT_STOPS = 8;

struct Gradient {
  float angle_deg = 0.f;
  uint8_t stop_count = 0; // 0 => no gradient
  GradientStop stops[UI_MAX_GRADIENT_STOPS]{};
};

struct BackgroundImage {
  uint32_t texture_id = 0; // 0 => no image
  Color tint{255, 255, 255, 255};
  SideWidths nine_slice{}; // all-zero => plain stretch; nonzero => 9-patch
};

struct RawBitmapImage {
  const uint8_t *pixels = nullptr; // host-owned, consumed during the same frame
  uint16_t width = 0;
  uint16_t height = 0;
};

struct Shadow {
  Color color{}; // color.a==0 => no shadow
  Vec2 offset{};
  float blur = 0.f;
  float spread = 0.f;
};

enum class TextAlign : uint8_t { Left = 0, Center, Right };
enum class TextWrap : uint8_t { None = 0, Words };

struct TextVisual {
  Color color{};
  uint16_t font_id = 0;
  uint16_t font_size = 0;
  TextAlign align = TextAlign::Left;
  TextWrap wrap = TextWrap::None;
  float line_height = 0.f; // 0 => face natural line skip
};

struct VisualStyle {
  Color background{};        // a==0 => no fill
  float corner_radius = 0.f; // uniform in v1
  Border border{};
  Outline outline{};         // focus ring etc.
  Gradient gradient{};       // stop_count==0 => no gradient
  BackgroundImage image{};   // texture_id==0 => no image
  RawBitmapImage bitmap{};   // pixels==nullptr => no raw bitmap
  Shadow shadow{};           // color.a==0 => no shadow
  float opacity = 1.f;       // <1 => group-opacity layer
  bool hidden = false;       // skip paint, keep layout
  TextVisual text{};
};

static_assert(std::is_aggregate_v<VisualStyle>);
static_assert(std::is_trivially_copyable_v<VisualStyle>);

// One wrapped line of text, alignment pre-baked; produced by the MeasureTextFn
// seam (text_measure.h) and consumed by the draw-list transcriber.
struct LineRun {
  uint32_t slice_offset = 0; // byte offset into the query's utf8
  uint32_t slice_len = 0;    // byte length of this line's visible glyphs
  float x = 0.f, y = 0.f;    // top-left pen origin, alignment applied
  float w = 0.f, h = 0.f;    // measured advance width, line box height
};

} // namespace ui
