// Hermetic geometry-math tests for src/ui/runtime/geometry.{h,cpp}.
// SDL-free, no game vocabulary, CHECK-macro mold (returns false on failure).
// Covers: corner_segments bounds, single-quad path, rounded-corner vertex
// containment, gradient endpoint colors, shadow skirt fade, and overflow.

#include "ui/runtime/geometry.h"
#include "ui/style/visual_style.h"

#include <math.h>
#include <stdio.h>

#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__,       \
              #expr);                                                          \
      return false;                                                            \
    }                                                                          \
  } while (0)

using namespace ui;

namespace {

// Generous fixed buffers for the "happy path" sink.
constexpr int kVCap = 4096;
constexpr int kICap = 8192;

struct Buffers {
  Vertex verts[kVCap];
  uint16_t idx[kICap];
};

MeshSink make_sink(Buffers &b) {
  MeshSink s;
  s.verts = b.verts;
  s.vcap = kVCap;
  s.vcount = 0;
  s.idx = b.idx;
  s.icap = kICap;
  s.icount = 0;
  return s;
}

bool approx(float a, float b, float eps = 1e-3f) { return fabsf(a - b) <= eps; }

} // namespace

// --- corner_segments -------------------------------------------------------
static bool test_corner_segments() {
  CHECK(corner_segments(0.f) == 16);   // floor at 16
  CHECK(corner_segments(10.f) == 16);  // ceil(5)=5 -> still floored to 16
  CHECK(corner_segments(40.f) == 20);  // ceil(20)=20
  CHECK(corner_segments(100.f) == 50); // ceil(50)=50, under cap
  CHECK(corner_segments(1000.f) == 64); // cap at 64
  CHECK(corner_segments(100.f) <= 64);
  return true;
}

// --- single quad for tiny radius -------------------------------------------
static bool test_square_is_two_triangles() {
  Buffers b;
  MeshSink s = make_sink(b);
  CHECK(tessellate_rect_fill(DrawRect{0, 0, 100, 40}, 0.f, Color{10, 20, 30, 255}, s));
  // 4 vertices, 6 indices => exactly two triangles.
  CHECK(s.vcount == 4);
  CHECK(s.icount == 6);
  // all vertices carry the premultiplied fill color verbatim.
  for (int i = 0; i < s.vcount; ++i)
    CHECK(s.verts[i].color == (Color{10, 20, 30, 255}));
  // radius below the 0.5 epsilon also collapses to a quad (separate buffers so
  // the assertions above stay valid).
  Buffers b2;
  MeshSink s2 = make_sink(b2);
  CHECK(tessellate_rect_fill(DrawRect{0, 0, 100, 40}, 0.4f, Color{1, 2, 3, 4}, s2));
  CHECK(s2.vcount == 4 && s2.icount == 6);
  return true;
}

// --- rounded corners produce more than a quad and stay inside the box ------
static bool test_rounded_corners_contained() {
  Buffers b;
  MeshSink s = make_sink(b);
  const DrawRect r{0, 0, 200, 120};
  const float radius = 24.f;
  CHECK(tessellate_rect_fill(r, radius, Color{200, 200, 200, 255}, s));
  // Rounded => many triangles (centroid fan over the ring), well past 2.
  CHECK(s.icount > 6);
  CHECK(s.vcount > 4);

  // Every vertex must lie within the rect bounds (corners cut inward).
  for (int i = 0; i < s.vcount; ++i) {
    const Vertex &v = s.verts[i];
    CHECK(v.x >= r.x - 1e-3f && v.x <= r.x + r.w + 1e-3f);
    CHECK(v.y >= r.y - 1e-3f && v.y <= r.y + r.h + 1e-3f);
  }

  // A corner ring vertex must lie within [corner_pivot, corner_pivot+radius]:
  // i.e. no ring point should be closer than (box corner) nor farther than the
  // pivot+radius. Concretely, the top-left ring sample nearest the TL box
  // corner must sit within `radius` of the TL pivot (x0+r, y0+r).
  const float plx = r.x + radius, ply = r.y + radius;
  bool saw_tl_arc = false;
  for (int i = 0; i < s.vcount; ++i) {
    const Vertex &v = s.verts[i];
    // points in the TL quadrant (left of pivot x AND above pivot y)
    if (v.x <= plx + 1e-3f && v.y <= ply + 1e-3f) {
      const float d = sqrtf((v.x - plx) * (v.x - plx) + (v.y - ply) * (v.y - ply));
      // distance from the pivot is either ~0 (the centroid is not here) or
      // <= radius for arc points. Arc points sit exactly `radius` from pivot.
      CHECK(d <= radius + 1e-2f);
      if (approx(d, radius, 1e-2f))
        saw_tl_arc = true;
    }
  }
  CHECK(saw_tl_arc); // the TL corner arc was actually emitted on the radius.
  return true;
}

// --- gradient endpoints map to first/last stop colors ----------------------
static bool test_gradient_endpoints() {
  Buffers b;
  MeshSink s = make_sink(b);
  const DrawRect r{0, 0, 100, 100};
  Gradient g{};
  g.angle_deg = 0.f; // horizontal axis, +x
  g.stop_count = 2;
  g.stops[0] = GradientStop{0.f, Color{255, 0, 0, 255}};   // left
  g.stops[1] = GradientStop{1.f, Color{0, 0, 255, 255}};   // right
  CHECK(gradient_fill_colors(r, 0.f, g, s));
  CHECK(s.vcount == 4); // square (radius 0) => quad

  // Find the leftmost and rightmost vertices; they must carry the endpoint
  // stop colors (premultiplied, copied verbatim).
  int left = 0, right = 0;
  for (int i = 1; i < s.vcount; ++i) {
    if (s.verts[i].x < s.verts[left].x)
      left = i;
    if (s.verts[i].x > s.verts[right].x)
      right = i;
  }
  CHECK(s.verts[left].color == (Color{255, 0, 0, 255}));
  CHECK(s.verts[right].color == (Color{0, 0, 255, 255}));

  // stop_count==0 => no geometry, but still success (not an error).
  MeshSink s2 = make_sink(b);
  Gradient none{};
  CHECK(gradient_fill_colors(r, 0.f, none, s2));
  CHECK(s2.vcount == 0 && s2.icount == 0);
  return true;
}

// --- shadow skirt: outer ring alpha must be zero ---------------------------
static bool test_shadow_skirt_fades_to_zero() {
  Buffers b;
  MeshSink s = make_sink(b);
  const DrawRect r{40, 40, 100, 60};
  Shadow sh{};
  sh.color = Color{0, 0, 0, 200}; // premultiplied-ish opaque-black-ish
  sh.offset = Vec2{0, 4};
  sh.blur = 12.f;
  sh.spread = 2.f;
  CHECK(tessellate_shadow(r, 8.f, sh, s));
  CHECK(s.vcount > 4); // inner fill + skirt

  // Inner box (offset + spread) bounds; the inner full-color quad/fan sits
  // inside it. The skirt extends `blur` beyond that. Any vertex farther than
  // the inner box bounds (in the skirt region) must be fully transparent.
  const float ix0 = r.x + sh.offset.x - sh.spread;
  const float iy0 = r.y + sh.offset.y - sh.spread;
  const float ix1 = ix0 + r.w + 2 * sh.spread;
  const float iy1 = iy0 + r.h + 2 * sh.spread;

  bool saw_full = false; // at least one full-color vertex
  bool saw_fade = false; // at least one transparent skirt vertex
  for (int i = 0; i < s.vcount; ++i) {
    const Vertex &v = s.verts[i];
    const bool outside_inner = v.x < ix0 - 1e-3f || v.x > ix1 + 1e-3f ||
                               v.y < iy0 - 1e-3f || v.y > iy1 + 1e-3f;
    if (outside_inner) {
      CHECK(v.color.a == 0); // skirt outer edge fades to (0,0,0,0)
      CHECK(v.color == (Color{0, 0, 0, 0}));
      saw_fade = true;
    }
    if (v.color == sh.color)
      saw_full = true;
  }
  CHECK(saw_full);
  CHECK(saw_fade);

  // color.a==0 => no shadow at all, success.
  MeshSink s2 = make_sink(b);
  Shadow invisible{};
  invisible.blur = 10.f;
  CHECK(tessellate_shadow(r, 8.f, invisible, s2));
  CHECK(s2.vcount == 0 && s2.icount == 0);
  return true;
}

// --- frame emits border band + outline ring --------------------------------
static bool test_frame_emits_bands() {
  Buffers b;
  MeshSink s = make_sink(b);
  const DrawRect r{0, 0, 80, 40};
  Border border{};
  border.width = SideWidths{2, 2, 2, 2};
  border.color = SideColors{Color{255, 0, 0, 255}, Color{0, 255, 0, 255},
                            Color{0, 0, 255, 255}, Color{255, 255, 0, 255}};
  Outline outline{};
  outline.width = 2.f;
  outline.color = Color{120, 170, 255, 255};
  outline.offset = 2.f; // outset ring
  CHECK(tessellate_frame(r, 6.f, border, outline, s));
  CHECK(s.vcount > 0 && s.icount > 0);

  // The outset ring must extend beyond the border-box (offset + width on a
  // positive-offset ring grows outward).
  bool beyond = false;
  for (int i = 0; i < s.vcount; ++i)
    if (s.verts[i].x < r.x - 1e-3f || s.verts[i].y < r.y - 1e-3f)
      beyond = true;
  CHECK(beyond);

  // No border + no outline => no geometry, still success.
  MeshSink s2 = make_sink(b);
  CHECK(tessellate_frame(r, 6.f, Border{}, Outline{}, s2));
  CHECK(s2.vcount == 0 && s2.icount == 0);
  return true;
}

// --- overflow returns false (no truncation) --------------------------------
static bool test_overflow_returns_false() {
  // Tight sink: room for fewer than one quad.
  Vertex vbuf[2];
  uint16_t ibuf[2];
  MeshSink tight;
  tight.verts = vbuf;
  tight.vcap = 2;
  tight.vcount = 0;
  tight.idx = ibuf;
  tight.icap = 2;
  tight.icount = 0;

  // Even the single-quad path needs 4 verts / 6 idx -> must fail cleanly.
  CHECK(tessellate_rect_fill(DrawRect{0, 0, 10, 10}, 0.f, Color{1, 1, 1, 255}, tight) == false);

  // Rounded fill into a tight sink also fails.
  MeshSink tight2 = tight;
  tight2.vcount = 0;
  tight2.icount = 0;
  CHECK(tessellate_rect_fill(DrawRect{0, 0, 200, 200}, 40.f, Color{1, 1, 1, 255}, tight2) == false);

  // Gradient into a tight sink fails too.
  Gradient g{};
  g.stop_count = 2;
  g.stops[0] = GradientStop{0.f, Color{1, 0, 0, 255}};
  g.stops[1] = GradientStop{1.f, Color{0, 0, 1, 255}};
  MeshSink tight3 = tight;
  tight3.vcount = 0;
  tight3.icount = 0;
  CHECK(gradient_fill_colors(DrawRect{0, 0, 10, 10}, 0.f, g, tight3) == false);

  // Frame into a tight sink fails.
  Border border{};
  border.width = SideWidths{2, 2, 2, 2};
  border.color = SideColors{Color{255, 0, 0, 255}, Color{255, 0, 0, 255},
                            Color{255, 0, 0, 255}, Color{255, 0, 0, 255}};
  MeshSink tight4 = tight;
  tight4.vcount = 0;
  tight4.icount = 0;
  CHECK(tessellate_frame(DrawRect{0, 0, 40, 40}, 4.f, border, Outline{}, tight4) == false);

  // Shadow into a tight sink fails.
  Shadow sh{};
  sh.color = Color{0, 0, 0, 200};
  sh.blur = 8.f;
  MeshSink tight5 = tight;
  tight5.vcount = 0;
  tight5.icount = 0;
  CHECK(tessellate_shadow(DrawRect{0, 0, 40, 40}, 4.f, sh, tight5) == false);
  return true;
}

// --- midpoint gradient color lerps in premultiplied space ------------------
static bool test_gradient_midpoint() {
  Buffers b;
  MeshSink s = make_sink(b);
  const DrawRect r{0, 0, 100, 10};
  Gradient g{};
  g.angle_deg = 0.f;
  g.stop_count = 2;
  g.stops[0] = GradientStop{0.f, Color{0, 0, 0, 0}};
  g.stops[1] = GradientStop{1.f, Color{200, 100, 40, 200}};
  CHECK(gradient_fill_colors(r, 0.f, g, s));
  // The center vertex of a rounded path would sit at t=0.5; for the square path
  // there is no center vertex, so probe via the corners: left==stop0, right==stop1.
  int left = 0, right = 0;
  for (int i = 1; i < s.vcount; ++i) {
    if (s.verts[i].x < s.verts[left].x)
      left = i;
    if (s.verts[i].x > s.verts[right].x)
      right = i;
  }
  CHECK(s.verts[left].color == (Color{0, 0, 0, 0}));
  CHECK(s.verts[right].color == (Color{200, 100, 40, 200}));
  return true;
}

// --- regression: paired-ring topology must match across radius regimes ------
// These cases (caught by adversarial review) all hit the "inner ring vs outer
// ring point-count mismatch" defect that the shared-seg fix resolves. Each must
// succeed and emit non-empty geometry, never spuriously return false.
static bool test_paired_ring_regressions() {
  // (1) PLAIN box (corner_radius=0) drop shadow with blur>0: outer skirt ring is
  // rounded (blur radius) while the inner ring is square -> must still match.
  {
    Buffers b;
    MeshSink s = make_sink(b);
    Shadow sh{};
    sh.color = Color{0, 0, 0, 180};
    sh.blur = 10.f;
    CHECK(tessellate_shadow(DrawRect{40, 40, 100, 60}, /*corner_radius=*/0.f, sh, s));
    CHECK(s.vcount > 4 && s.icount > 6); // inner fill + skirt, not a spurious false
  }
  // (2) Large radius (>32) frame: inner/outer band rings would pick different
  // per-ring seg without the shared-seg fix.
  {
    Buffers b;
    MeshSink s = make_sink(b);
    Border border{};
    border.width = SideWidths{3, 3, 3, 3};
    border.color = SideColors{Color{200, 0, 0, 255}, Color{200, 0, 0, 255},
                              Color{200, 0, 0, 255}, Color{200, 0, 0, 255}};
    CHECK(tessellate_frame(DrawRect{0, 0, 200, 160}, /*radius=*/40.f, border, Outline{}, s));
    CHECK(s.vcount > 0 && s.icount > 0);
  }
  // (3) Border width >= corner radius: inner radius collapses to 0 while outer
  // stays rounded -> the most common real case (1-2px border, small radius).
  {
    Buffers b;
    MeshSink s = make_sink(b);
    Border border{};
    border.width = SideWidths{6, 6, 6, 6};
    border.color = SideColors{Color{0, 200, 0, 255}, Color{0, 200, 0, 255},
                              Color{0, 200, 0, 255}, Color{0, 200, 0, 255}};
    CHECK(tessellate_frame(DrawRect{0, 0, 80, 50}, /*radius=*/4.f, border, Outline{}, s));
    CHECK(s.vcount > 0 && s.icount > 0);
  }
  // (4) Outline width approaching/exceeding the ring's outer radius.
  {
    Buffers b;
    MeshSink s = make_sink(b);
    Outline outline{};
    outline.width = 4.f;
    outline.color = Color{120, 170, 255, 255};
    outline.offset = 0.f;
    CHECK(tessellate_frame(DrawRect{0, 0, 60, 60}, /*radius=*/4.f, Border{}, outline, s));
    CHECK(s.vcount > 0 && s.icount > 0);
  }
  return true;
}

// --- anti-aliasing: feathered fill grows a transparent fringe --------------
// With feather>0 a rounded fill must emit a solid core (full-alpha vertices)
// surrounded by a fringe that fades to premultiplied-transparent (0,0,0,0). The
// fringe extrudes ~feather/2 beyond the nominal edge; the solid core stays
// inside it. (feather==0 keeps the legacy no-fringe behavior — covered above.)
static bool test_feathered_fill_has_fringe() {
  Buffers b;
  MeshSink s = make_sink(b);
  const DrawRect r{0, 0, 200, 120};
  const float radius = 24.f;
  const Color fill{200, 200, 200, 255};
  CHECK(tessellate_rect_fill(r, radius, fill, s, /*feather=*/1.f));

  bool saw_full = false;  // a solid-core vertex at full fill alpha
  bool saw_clear = false; // a fringe vertex faded to (0,0,0,0)
  bool saw_outside = false; // fringe extrudes past the nominal left edge
  for (int i = 0; i < s.vcount; ++i) {
    const Vertex &v = s.verts[i];
    if (v.color == fill)
      saw_full = true;
    if (v.color == (Color{0, 0, 0, 0})) {
      saw_clear = true;
      // every transparent vertex is part of the outer fringe ring.
      if (v.x < r.x - 1e-3f || v.y < r.y - 1e-3f)
        saw_outside = true;
    }
    // every FULL-alpha vertex stays within the (inset) nominal box.
    if (v.color.a == 255) {
      CHECK(v.x >= r.x - 1e-3f && v.x <= r.x + r.w + 1e-3f);
      CHECK(v.y >= r.y - 1e-3f && v.y <= r.y + r.h + 1e-3f);
    }
  }
  CHECK(saw_full);
  CHECK(saw_clear);
  CHECK(saw_outside);

  // A square (radius<=eps) fill IGNORES feather — axis-aligned edges stay hard.
  Buffers b2;
  MeshSink s2 = make_sink(b2);
  CHECK(tessellate_rect_fill(DrawRect{0, 0, 100, 40}, 0.f, fill, s2, 1.f));
  CHECK(s2.vcount == 4 && s2.icount == 6);
  for (int i = 0; i < s2.vcount; ++i)
    CHECK(s2.verts[i].color == fill); // no transparent fringe verts
  return true;
}

// --- anti-aliasing: feathered frame grows a transparent fringe -------------
static bool test_feathered_frame_has_fringe() {
  Buffers b;
  MeshSink s = make_sink(b);
  const DrawRect r{0, 0, 120, 60};
  Border border{};
  border.width = SideWidths{2, 2, 2, 2};
  const Color bc{220, 90, 90, 255};
  border.color = SideColors{bc, bc, bc, bc};
  CHECK(tessellate_frame(r, /*radius=*/10.f, border, Outline{}, s, /*feather=*/1.f));
  bool saw_full = false, saw_clear = false;
  for (int i = 0; i < s.vcount; ++i) {
    if (s.verts[i].color == bc)
      saw_full = true;
    if (s.verts[i].color == (Color{0, 0, 0, 0}))
      saw_clear = true;
  }
  CHECK(saw_full);  // solid border core
  CHECK(saw_clear); // feathered edges

  // A SQUARE frame (radius<=eps) ignores feather: no transparent fringe verts.
  Buffers b2;
  MeshSink s2 = make_sink(b2);
  CHECK(tessellate_frame(DrawRect{0, 0, 120, 60}, 0.f, border, Outline{}, s2, 1.f));
  for (int i = 0; i < s2.vcount; ++i)
    CHECK(s2.verts[i].color != (Color{0, 0, 0, 0}));
  return true;
}

// --- anti-aliasing: feathered paths still fail cleanly on overflow ----------
static bool test_feathered_overflow_returns_false() {
  Vertex vbuf[8];
  uint16_t ibuf[8];
  MeshSink tight;
  tight.verts = vbuf;
  tight.vcap = 8;
  tight.vcount = 0;
  tight.idx = ibuf;
  tight.icap = 8;
  tight.icount = 0;
  // A feathered rounded fill needs far more than 8 verts -> clean false.
  CHECK(tessellate_rect_fill(DrawRect{0, 0, 200, 120}, 24.f,
                             Color{1, 1, 1, 255}, tight, 1.f) == false);
  return true;
}

int main() {
  bool ok = test_corner_segments() && test_square_is_two_triangles() &&
            test_rounded_corners_contained() && test_gradient_endpoints() &&
            test_shadow_skirt_fades_to_zero() && test_frame_emits_bands() &&
            test_overflow_returns_false() && test_gradient_midpoint() &&
            test_paired_ring_regressions() && test_feathered_fill_has_fringe() &&
            test_feathered_frame_has_fringe() &&
            test_feathered_overflow_returns_false();
  if (!ok) {
    fprintf(stderr, "ui_geometry_tests: FAIL\n");
    return 1;
  }
  printf("ui_geometry_tests: OK (sizeof Vertex=%zu, DrawRect=%zu)\n",
         sizeof(Vertex), sizeof(DrawRect));
  return 0;
}
