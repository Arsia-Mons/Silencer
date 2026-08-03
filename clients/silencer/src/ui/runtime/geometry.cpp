// geometry.cpp — SDL-FREE tessellation math. See geometry.h for the contract
// and docs/retained-ui/styling-render-system-design.md §9.3–§9.8 for topology.

#include "geometry.h"

#include <math.h>
#include <stdint.h>

namespace ui {

namespace {

constexpr float kPi = 3.14159265358979323846f;

// Below this radius the corners are not worth tessellating: the fill collapses
// to a single quad (design §9.3 "corner_radius <= 0.5f => single quad").
constexpr float kRadiusEps = 0.5f;

inline Vertex vtx(float x, float y, Color c) {
  Vertex v;
  v.x = x;
  v.y = y;
  v.color = c;
  v.u = 0.f;
  v.v = 0.f;
  return v;
}

// Clamp the authored radius so two opposite corners never overlap: a radius
// larger than half the shorter side would invert the inner rect.
float clamp_radius(const DrawRect &r, float radius) {
  if (radius < 0.f)
    radius = 0.f;
  const float half_min = 0.5f * (r.w < r.h ? r.w : r.h);
  if (radius > half_min)
    radius = half_min;
  return radius;
}

// Grow (d>0) or inset (d<0) a rect by d on every side. Degenerate sizes clamp
// to 0 (build_ring_seg + clamp_radius then collapse the corners safely). Used to
// build the concentric core/fringe rings of a feathered silhouette.
inline DrawRect grow_rect(const DrawRect &r, float d) {
  DrawRect o;
  o.x = r.x - d;
  o.y = r.y - d;
  o.w = r.w + 2.f * d;
  o.h = r.h + 2.f * d;
  if (o.w < 0.f)
    o.w = 0.f;
  if (o.h < 0.f)
    o.h = 0.f;
  return o;
}

// Half-feather, clamped so insetting a small shape by it cannot invert the core.
// `room` is the largest safe per-side inset (half the limiting dimension/gap);
// we keep ~10% of it as solid core in the worst case.
inline float clamp_half_feather(float feather, float room) {
  float half = feather * 0.5f;
  const float cap = 0.45f * room;
  if (half > cap)
    half = cap;
  if (half < 0.f)
    half = 0.f;
  return half;
}

// Premultiplied-transparent: the only fully-transparent color. Fringe vertices
// fade to this so the GPU blends a coverage ramp across the 1px band.
constexpr Color kClear{0, 0, 0, 0};

// Lerp two premultiplied colors at parameter t in [0,1]. Premultiplied space:
// every channel (including alpha) interpolates linearly and independently. We
// do NOT divide out / re-multiply alpha — the inputs are already premultiplied.
Color lerp_premul(Color a, Color b, float t) {
  if (t < 0.f)
    t = 0.f;
  if (t > 1.f)
    t = 1.f;
  auto mix = [t](uint8_t lo, uint8_t hi) -> uint8_t {
    const float v = (float)lo + ((float)hi - (float)lo) * t;
    // round to nearest; v is within [0,255] so no clamp needed.
    return (uint8_t)(v + 0.5f);
  };
  return Color{mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
}

// Append a rounded-rect outline as a closed CCW polyline of points, all sharing
// `color`, into `out` (a fixed array of size `cap`). Returns the point count, or
// -1 on overflow. Used as the silhouette generator for fills and gradients; the
// caller fans it from the centroid. With radius<=eps this is exactly 4 corners.
//
// Point order, CCW in screen space (+y down so we sweep clockwise in math terms
// but it is consistent across the module): TL corner arc, TR, BR, BL.
struct RingPt {
  float x, y;
};

// Build a ring with an EXPLICIT per-corner segment count. This is the key to
// paired rings (band strips / shadow skirts): both rings MUST share one `seg`
// so their point counts match regardless of each ring's own radius. seg<=0 =>
// 4-corner rectangle; seg>0 => 4 arcs of (seg+1) points each. A radius of 0 with
// seg>0 is fine: the arc points collapse onto the pivot (a sharp corner) while
// still emitting seg+1 points, so a tiny/zero inner radius keeps matching
// topology with a rounded outer ring.
int build_ring_seg(const DrawRect &r, float radius, int seg, RingPt *out,
                   int cap) {
  radius = clamp_radius(r, radius);
  const float x0 = r.x, y0 = r.y;
  const float x1 = r.x + r.w, y1 = r.y + r.h;
  int n = 0;
  auto push = [&](float x, float y) -> bool {
    if (n >= cap)
      return false;
    out[n++] = {x, y};
    return true;
  };

  if (seg <= 0) {
    // Plain rectangle: 4 corners, CCW with +y down (TL, TR, BR, BL).
    if (!push(x0, y0) || !push(x1, y0) || !push(x1, y1) || !push(x0, y1))
      return -1;
    return n;
  }

  // Pivots are the inner-rect corners.
  const float pl = x0 + radius, pr = x1 - radius; // pivot x left/right
  const float pt = y0 + radius, pb = y1 - radius; // pivot y top/bottom

  // Each corner sweeps a quarter turn. We march the perimeter so the resulting
  // polygon is a single ring (TL -> TR along the top, etc.). Angles use the
  // math convention (0 = +x, CCW), pivots placed so the arc bulges outward.
  // Top-left corner: angle 180° -> 270° (pointing up-left).
  for (int i = 0; i <= seg; ++i) {
    const float a = kPi + (kPi * 0.5f) * ((float)i / (float)seg);
    if (!push(pl + cosf(a) * radius, pt + sinf(a) * radius))
      return -1;
  }
  // Top-right corner: 270° -> 360°.
  for (int i = 0; i <= seg; ++i) {
    const float a = (kPi * 1.5f) + (kPi * 0.5f) * ((float)i / (float)seg);
    if (!push(pr + cosf(a) * radius, pt + sinf(a) * radius))
      return -1;
  }
  // Bottom-right corner: 0° -> 90°.
  for (int i = 0; i <= seg; ++i) {
    const float a = (kPi * 0.5f) * ((float)i / (float)seg);
    if (!push(pr + cosf(a) * radius, pb + sinf(a) * radius))
      return -1;
  }
  // Bottom-left corner: 90° -> 180°.
  for (int i = 0; i <= seg; ++i) {
    const float a = (kPi * 0.5f) + (kPi * 0.5f) * ((float)i / (float)seg);
    if (!push(pl + cosf(a) * radius, pb + sinf(a) * radius))
      return -1;
  }
  return n;
}

// Pick a segment count from the radius for a STANDALONE ring (fills). For paired
// rings use build_ring_seg directly with a shared seg.
int seg_for_radius(float radius) {
  return radius <= kRadiusEps ? 0 : corner_segments(radius);
}

// Max ring points: 4 arcs * (max_seg + 1). corner_segments caps at 64.
constexpr int kMaxRingPts = 4 * (64 + 1);

// Project point p onto the gradient axis and normalize to t in [0,1] across the
// rect's full projected extent. Axis d=(cos,sin) of angle_deg. The extent is the
// span of the four rect corners projected onto d; the min corner maps to t=0,
// max to t=1. Degenerate extent => t=0.
float gradient_t(const DrawRect &r, float dx, float dy, float px, float py) {
  const float c0 = r.x * dx + r.y * dy;
  const float c1 = (r.x + r.w) * dx + r.y * dy;
  const float c2 = r.x * dx + (r.y + r.h) * dy;
  const float c3 = (r.x + r.w) * dx + (r.y + r.h) * dy;
  float lo = c0, hi = c0;
  const float cs[3] = {c1, c2, c3};
  for (int i = 0; i < 3; ++i) {
    if (cs[i] < lo)
      lo = cs[i];
    if (cs[i] > hi)
      hi = cs[i];
  }
  const float span = hi - lo;
  if (span <= 1e-6f)
    return 0.f;
  return ((px * dx + py * dy) - lo) / span;
}

// Sample the gradient's pre-sorted stops at parameter t in [0,1], lerping the
// bracketing stops in premultiplied space. Stops are assumed sorted by .t (the
// authoring path keeps them sorted); we clamp to the first/last outside range.
Color gradient_color(const Gradient &g, float t) {
  const int n = (int)g.stop_count;
  if (n <= 0)
    return Color{0, 0, 0, 0};
  if (n == 1)
    return g.stops[0].color;
  if (t <= g.stops[0].t)
    return g.stops[0].color;
  if (t >= g.stops[n - 1].t)
    return g.stops[n - 1].color;
  for (int i = 0; i + 1 < n; ++i) {
    const GradientStop &lo = g.stops[i];
    const GradientStop &hi = g.stops[i + 1];
    if (t >= lo.t && t <= hi.t) {
      const float denom = hi.t - lo.t;
      const float local = denom <= 1e-6f ? 0.f : (t - lo.t) / denom;
      return lerp_premul(lo.color, hi.color, local);
    }
  }
  return g.stops[n - 1].color;
}

// Shared fill emitter for solid fills and gradients. `shade` maps a position to
// its premultiplied color (constant for solids, per-vertex for gradients). The
// topology is design §9.3: a fan from the rect centroid out to the rounded
// ring. (A centroid fan is equivalent in coverage to "center quad + 4 corner
// fans + 4 edge quads" — same triangles, just hubbed at the centroid so the
// edge bands and corner fans share the one apex.)
// Fan the interior of a ring from the rect centroid, each vertex shaded.
template <class Shade>
bool fan_interior(const RingPt *ring, int count, float cx, float cy,
                  MeshSink &sink, Shade shade) {
  const Vertex center = vtx(cx, cy, shade(cx, cy));
  for (int i = 0; i < count; ++i) {
    const RingPt &a = ring[i];
    const RingPt &b = ring[(i + 1) % count];
    if (!sink.tri(center, vtx(a.x, a.y, shade(a.x, a.y)),
                  vtx(b.x, b.y, shade(b.x, b.y))))
      return false;
  }
  return true;
}

template <class Shade>
bool emit_fill(const DrawRect &r, float corner_radius, MeshSink &sink,
               Shade shade, float feather) {
  const float radius = clamp_radius(r, corner_radius);

  if (radius <= kRadiusEps) {
    // Axis-aligned: hard quad, two triangles (NO feather — header rule). CCW
    // with +y down: TL, TR, BR, BL.
    const float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h;
    return sink.quad(vtx(x0, y0, shade(x0, y0)), vtx(x1, y0, shade(x1, y0)),
                     vtx(x1, y1, shade(x1, y1)), vtx(x0, y1, shade(x0, y1)));
  }

  const int seg = seg_for_radius(radius); // shared by every concentric ring
  const float cx = r.x + r.w * 0.5f;
  const float cy = r.y + r.h * 0.5f;

  if (feather <= 0.f) {
    // Legacy path: single ring, centroid fan. Byte-for-byte unchanged.
    RingPt ring[kMaxRingPts];
    const int count = build_ring_seg(r, radius, seg, ring, kMaxRingPts);
    if (count < 3)
      return false;
    return fan_interior(ring, count, cx, cy, sink, shade);
  }

  // Feathered path: a solid core inset by half-feather + a 1px transparent
  // fringe extending half-feather outward, so the coverage ramp straddles the
  // nominal edge. half is clamped so a small shape's core never inverts.
  const float half_min = 0.5f * (r.w < r.h ? r.w : r.h);
  const float half = clamp_half_feather(feather, half_min);

  RingPt core[kMaxRingPts];
  RingPt edge[kMaxRingPts];
  const int nc =
      build_ring_seg(grow_rect(r, -half), radius - half, seg, core, kMaxRingPts);
  const int ne =
      build_ring_seg(grow_rect(r, half), radius + half, seg, edge, kMaxRingPts);
  if (nc < 3 || ne != nc)
    return false;

  if (!fan_interior(core, nc, cx, cy, sink, shade))
    return false;
  // Fringe: core (full shade) -> edge (premultiplied transparent).
  for (int i = 0; i < nc; ++i) {
    const int j = (i + 1) % nc;
    if (!sink.quad(vtx(core[i].x, core[i].y, shade(core[i].x, core[i].y)),
                   vtx(core[j].x, core[j].y, shade(core[j].x, core[j].y)),
                   vtx(edge[j].x, edge[j].y, kClear),
                   vtx(edge[i].x, edge[i].y, kClear)))
      return false;
  }
  return true;
}

// Pick the border-band color for a ring vertex by the 45° radial split (design
// §9.4): the angle from the box center selects top/right/bottom/left. With +y
// down, atan2(dy,dx) gives: right side around 0, bottom around +90°, left
// around ±180°, top around -90°. The four 90° wedges centered on the axes are
// split at the 45° bisectors.
Color side_color_for(const SideColors &col, float cx, float cy, float px,
                     float py) {
  const float ang = atan2f(py - cy, px - cx); // [-pi, pi]
  const float q = kPi * 0.25f;                // 45°
  if (ang >= -q && ang < q)
    return col.right;
  if (ang >= q && ang < kPi - q)
    return col.bottom;
  if (ang >= kPi - q || ang < -(kPi - q))
    return col.left;
  return col.top; // [-(pi-q), -q)
}

} // namespace

// ---------------------------------------------------------------------------
// MeshSink
// ---------------------------------------------------------------------------

bool MeshSink::tri(const Vertex &a, const Vertex &b, const Vertex &c) {
  // Guard vertex capacity, index capacity, AND uint16 index space (indices are
  // uint16_t, so vcount must stay <= 65536) — never a silent wrap.
  if (vcount + 3 > vcap || icount + 3 > icap || vcount + 3 > 65536)
    return false;
  const uint16_t base = (uint16_t)vcount;
  verts[vcount++] = a;
  verts[vcount++] = b;
  verts[vcount++] = c;
  idx[icount++] = base;
  idx[icount++] = (uint16_t)(base + 1);
  idx[icount++] = (uint16_t)(base + 2);
  return true;
}

bool MeshSink::quad(const Vertex &a, const Vertex &b, const Vertex &c,
                    const Vertex &d) {
  if (vcount + 4 > vcap || icount + 6 > icap || vcount + 4 > 65536)
    return false;
  const uint16_t base = (uint16_t)vcount;
  verts[vcount++] = a;
  verts[vcount++] = b;
  verts[vcount++] = c;
  verts[vcount++] = d;
  idx[icount++] = base;
  idx[icount++] = (uint16_t)(base + 1);
  idx[icount++] = (uint16_t)(base + 2);
  idx[icount++] = base;
  idx[icount++] = (uint16_t)(base + 2);
  idx[icount++] = (uint16_t)(base + 3);
  return true;
}

// ---------------------------------------------------------------------------
// corner_segments
// ---------------------------------------------------------------------------

int corner_segments(float radius) {
  int seg = (int)ceilf(radius * 0.5f);
  if (seg < 16)
    seg = 16;
  if (seg > 64)
    seg = 64;
  return seg;
}

// ---------------------------------------------------------------------------
// tessellate_rect_fill
// ---------------------------------------------------------------------------

bool tessellate_rect_fill(const DrawRect &rect, float corner_radius, Color fill,
                          MeshSink &sink, float feather) {
  return emit_fill(rect, corner_radius, sink,
                   [fill](float, float) { return fill; }, feather);
}

bool fill_is_hard_quad(const DrawRect &rect, float corner_radius) {
  return clamp_radius(rect, corner_radius) <= kRadiusEps;
}

// ---------------------------------------------------------------------------
// gradient_fill_colors
// ---------------------------------------------------------------------------

bool gradient_fill_colors(const DrawRect &rect, float corner_radius,
                          const Gradient &gradient, MeshSink &sink,
                          float feather) {
  if (gradient.stop_count == 0)
    return true; // no gradient => emit nothing (not an error).

  const float rad = gradient.angle_deg * (kPi / 180.f);
  const float dx = cosf(rad);
  const float dy = sinf(rad);
  return emit_fill(rect, corner_radius, sink,
                   [&](float px, float py) {
                     const float t = gradient_t(rect, dx, dy, px, py);
                     return gradient_color(gradient, t);
                   },
                   feather);
}

// ---------------------------------------------------------------------------
// tessellate_frame
// ---------------------------------------------------------------------------

// One border band: the region between the border-box ring and the ring inset by
// the per-side widths. We build the outer and inner rings (both with the band's
// corner radius) and bridge them with quads, coloring each pair by the 45°
// radial split. The inner radius shrinks with the (uniform-ish) inset so corner
// curvature is preserved.
// Bridge two equal-length rings into a quad strip, each vertex colored by a
// position->color functor (the 45° side split for solid bands, kClear for a
// transparent fringe edge). Ring `a` and ring `b` must share a point count.
template <class CA, class CB>
bool bridge_rings(const RingPt *a, const RingPt *b, int n, MeshSink &sink,
                  CA color_a, CB color_b) {
  for (int i = 0; i < n; ++i) {
    const int j = (i + 1) % n;
    if (!sink.quad(vtx(a[i].x, a[i].y, color_a(a[i].x, a[i].y)),
                   vtx(a[j].x, a[j].y, color_a(a[j].x, a[j].y)),
                   vtx(b[j].x, b[j].y, color_b(b[j].x, b[j].y)),
                   vtx(b[i].x, b[i].y, color_b(b[i].x, b[i].y))))
      return false;
  }
  return true;
}

bool tessellate_rect_fill_ring(const DrawRect &rect, float corner_radius,
                               Color fill, MeshSink &sink, float feather,
                               const DrawRect &hole) {
  const float radius = clamp_radius(rect, corner_radius);
  if (radius <= kRadiusEps)
    return false; // hard quad: nothing rounded to keep — span the whole rect
  auto shade = [fill](float, float) { return fill; };

  const int seg = seg_for_radius(radius);

  // Mirror emit_fill's ring setup exactly so the emitted arcs/fringe are the
  // ones the plain fill would draw.
  DrawRect core_rect = rect;
  float core_radius = radius;
  float half = 0.f;
  if (feather > 0.f) {
    const float half_min = 0.5f * (rect.w < rect.h ? rect.w : rect.h);
    half = clamp_half_feather(feather, half_min);
    core_rect = grow_rect(rect, -half);
    core_radius = radius - half;
  }

  // The hole must sit inside the rounded core: the core rect inset by the
  // core radius is entirely within the rounded silhouette, so bound it there.
  const float inset = core_radius > 0.f ? core_radius : 0.f;
  if (hole.w <= 0.f || hole.h <= 0.f || hole.x < core_rect.x + inset ||
      hole.y < core_rect.y + inset ||
      hole.x + hole.w > core_rect.x + core_rect.w - inset ||
      hole.y + hole.h > core_rect.y + core_rect.h - inset)
    return false;

  RingPt core[kMaxRingPts];
  RingPt hole_ring[kMaxRingPts];
  const int nc = build_ring_seg(core_rect, core_radius, seg, core, kMaxRingPts);
  // Same seg on a radius-0 ring: matching topology, corners collapsed (the
  // emit_band trick), so the bridge pairs cleanly.
  const int nh = build_ring_seg(hole, 0.f, seg, hole_ring, kMaxRingPts);
  if (nc < 3 || nh != nc)
    return false;
  if (!bridge_rings(core, hole_ring, nc, sink, shade, shade))
    return false;

  if (feather > 0.f) {
    RingPt edge[kMaxRingPts];
    const int ne = build_ring_seg(grow_rect(rect, half), radius + half, seg,
                                  edge, kMaxRingPts);
    if (ne != nc)
      return false;
    // Fringe: core (full shade) -> edge (premultiplied transparent), exactly
    // as emit_fill's feathered path.
    for (int i = 0; i < nc; ++i) {
      const int j = (i + 1) % nc;
      if (!sink.quad(vtx(core[i].x, core[i].y, shade(core[i].x, core[i].y)),
                     vtx(core[j].x, core[j].y, shade(core[j].x, core[j].y)),
                     vtx(edge[j].x, edge[j].y, kClear),
                     vtx(edge[i].x, edge[i].y, kClear)))
        return false;
    }
  }
  return true;
}

static bool emit_band(const DrawRect &outer_rect, float outer_radius,
                      const DrawRect &inner_rect, float inner_radius,
                      const SideColors &colors, MeshSink &sink, float feather) {
  RingPt outer[kMaxRingPts];
  RingPt inner[kMaxRingPts];
  // ONE shared seg (from the outer radius) so every ring has matching topology,
  // even when the inner radius collapses (border width >= corner radius) or the
  // box is square. Without this, paired rings diverge and the strip is rejected.
  const int seg = seg_for_radius(clamp_radius(outer_rect, outer_radius));
  const int no = build_ring_seg(outer_rect, outer_radius, seg, outer, kMaxRingPts);
  const int ni = build_ring_seg(inner_rect, inner_radius, seg, inner, kMaxRingPts);
  if (no < 3 || ni != no)
    return false; // ring topology must match for a clean strip.

  const float cx = outer_rect.x + outer_rect.w * 0.5f;
  const float cy = outer_rect.y + outer_rect.h * 0.5f;
  auto side = [&](float px, float py) {
    return side_color_for(colors, cx, cy, px, py);
  };

  // Per-side band thickness (outer edge -> inner edge). The min gates feathering:
  // a zero/near-zero gap means an unpainted side, so fall back to a hard band.
  const float gl = inner_rect.x - outer_rect.x;
  const float gt = inner_rect.y - outer_rect.y;
  const float gr = (outer_rect.x + outer_rect.w) - (inner_rect.x + inner_rect.w);
  const float gb = (outer_rect.y + outer_rect.h) - (inner_rect.y + inner_rect.h);
  float min_gap = gl;
  if (gt < min_gap) min_gap = gt;
  if (gr < min_gap) min_gap = gr;
  if (gb < min_gap) min_gap = gb;

  const bool curved = clamp_radius(outer_rect, outer_radius) > kRadiusEps;
  if (feather <= 0.f || !curved || min_gap <= 0.05f) {
    // Hard band (legacy): outer ring -> inner ring, 45° side colors.
    return bridge_rings(outer, inner, no, sink, side, side);
  }

  // Feathered band: a solid core inset by half-feather on both edges, plus a
  // 1px transparent fringe straddling each nominal edge. half is clamped to the
  // thinnest side so the core never inverts.
  const float half = clamp_half_feather(feather, min_gap);
  RingPt co[kMaxRingPts], ci[kMaxRingPts], oe[kMaxRingPts], ie[kMaxRingPts];
  const int nco = build_ring_seg(grow_rect(outer_rect, -half), outer_radius - half, seg, co, kMaxRingPts);
  const int nci = build_ring_seg(grow_rect(inner_rect, half), inner_radius + half, seg, ci, kMaxRingPts);
  const int noe = build_ring_seg(grow_rect(outer_rect, half), outer_radius + half, seg, oe, kMaxRingPts);
  const int nie = build_ring_seg(grow_rect(inner_rect, -half), inner_radius - half, seg, ie, kMaxRingPts);
  if (nco != no || nci != no || noe != no || nie != no)
    return false;

  auto clear = [](float, float) { return kClear; };
  // Solid core, then outer fringe (transparent -> core) and inner fringe
  // (core -> transparent). Regions tile (oe..co | co..ci | ci..ie); no overlap.
  if (!bridge_rings(co, ci, no, sink, side, side))
    return false;
  if (!bridge_rings(oe, co, no, sink, clear, side))
    return false;
  if (!bridge_rings(ci, ie, no, sink, side, clear))
    return false;
  return true;
}

bool tessellate_frame(const DrawRect &rect, float corner_radius,
                      const Border &border, const Outline &outline,
                      MeshSink &sink, float feather) {
  const float radius = clamp_radius(rect, corner_radius);

  // --- Border bands -------------------------------------------------------
  // A side is painted only when its width>0 and color.a>0. v1 builds a single
  // band from the border-box edge inset by the per-side widths, colored by the
  // 45° radial split (so each side reads its own color, corners blend at the
  // bisector). If no side qualifies, no band geometry is emitted.
  const bool any_border =
      (border.width.top > 0.f && border.color.top.a > 0) ||
      (border.width.right > 0.f && border.color.right.a > 0) ||
      (border.width.bottom > 0.f && border.color.bottom.a > 0) ||
      (border.width.left > 0.f && border.color.left.a > 0);

  if (any_border) {
    DrawRect inner;
    inner.x = rect.x + border.width.left;
    inner.y = rect.y + border.width.top;
    inner.w = rect.w - border.width.left - border.width.right;
    inner.h = rect.h - border.width.top - border.width.bottom;
    if (inner.w < 0.f)
      inner.w = 0.f;
    if (inner.h < 0.f)
      inner.h = 0.f;
    // Shrink the corner radius by the inset so curvature tracks the box; the
    // largest side width dominates the visible corner.
    float max_w = border.width.left;
    if (border.width.right > max_w)
      max_w = border.width.right;
    if (border.width.top > max_w)
      max_w = border.width.top;
    if (border.width.bottom > max_w)
      max_w = border.width.bottom;
    float inner_radius = radius - max_w;
    if (inner_radius < 0.f)
      inner_radius = 0.f;
    if (!emit_band(rect, radius, inner, inner_radius, border.color, sink,
                   feather))
      return false;
  }

  // --- Signed-offset outline ring (design §9.11) --------------------------
  // width<=0 or color.a==0 => no ring. Outer rect = border-box grown by offset
  // on every side (negative grows inward); the ring is a width-thick band on
  // that rect, corner radius = corner_radius + offset clamped at 0.
  if (outline.width > 0.f && outline.color.a > 0) {
    DrawRect ring_outer;
    ring_outer.x = rect.x - outline.offset;
    ring_outer.y = rect.y - outline.offset;
    ring_outer.w = rect.w + 2.f * outline.offset;
    ring_outer.h = rect.h + 2.f * outline.offset;
    if (ring_outer.w < 0.f)
      ring_outer.w = 0.f;
    if (ring_outer.h < 0.f)
      ring_outer.h = 0.f;

    float ring_outer_radius = radius + outline.offset;
    if (ring_outer_radius < 0.f)
      ring_outer_radius = 0.f;
    ring_outer_radius = clamp_radius(ring_outer, ring_outer_radius);

    DrawRect ring_inner;
    ring_inner.x = ring_outer.x + outline.width;
    ring_inner.y = ring_outer.y + outline.width;
    ring_inner.w = ring_outer.w - 2.f * outline.width;
    ring_inner.h = ring_outer.h - 2.f * outline.width;
    if (ring_inner.w < 0.f)
      ring_inner.w = 0.f;
    if (ring_inner.h < 0.f)
      ring_inner.h = 0.f;

    float ring_inner_radius = ring_outer_radius - outline.width;
    if (ring_inner_radius < 0.f)
      ring_inner_radius = 0.f;

    // Uniform color on all four sides of the ring.
    const SideColors ring_colors{outline.color, outline.color, outline.color,
                                 outline.color};
    if (!emit_band(ring_outer, ring_outer_radius, ring_inner, ring_inner_radius,
                   ring_colors, sink, feather))
      return false;
  }

  return true;
}

// ---------------------------------------------------------------------------
// tessellate_shadow
// ---------------------------------------------------------------------------

bool tessellate_shadow(const DrawRect &rect, float corner_radius,
                       const Shadow &shadow, MeshSink &sink) {
  if (shadow.color.a == 0)
    return true; // no shadow => emit nothing (not an error).

  // Inner (full-color) box: border-box translated by offset, expanded by spread.
  DrawRect inner;
  inner.x = rect.x + shadow.offset.x - shadow.spread;
  inner.y = rect.y + shadow.offset.y - shadow.spread;
  inner.w = rect.w + 2.f * shadow.spread;
  inner.h = rect.h + 2.f * shadow.spread;
  if (inner.w < 0.f)
    inner.w = 0.f;
  if (inner.h < 0.f)
    inner.h = 0.f;

  const float inner_radius = clamp_radius(inner, corner_radius + shadow.spread);

  // Solid inner fill at full color.
  if (!tessellate_rect_fill(inner, inner_radius, shadow.color, sink))
    return false;

  const float blur = shadow.blur;
  if (blur <= 0.f)
    return true; // no feather requested.

  // Skirt: a blur-wide band around the inner box, inner edge at full color and
  // outer edge fading to premultiplied-transparent (0,0,0,0). Outer box is the
  // inner box grown by blur on every side; outer corner radius grows with it.
  DrawRect outer;
  outer.x = inner.x - blur;
  outer.y = inner.y - blur;
  outer.w = inner.w + 2.f * blur;
  outer.h = inner.h + 2.f * blur;

  float outer_radius = inner_radius + blur;
  if (outer_radius < 0.f)
    outer_radius = 0.f;
  outer_radius = clamp_radius(outer, outer_radius);

  RingPt inner_ring[kMaxRingPts];
  RingPt outer_ring[kMaxRingPts];
  // Shared seg (from the outer radius) so a plain box (inner_radius 0) with
  // blur>0 still produces matching ring topology — the masked-blocker case.
  const int seg = seg_for_radius(clamp_radius(outer, outer_radius));
  const int ni = build_ring_seg(inner, inner_radius, seg, inner_ring, kMaxRingPts);
  const int no = build_ring_seg(outer, outer_radius, seg, outer_ring, kMaxRingPts);
  if (ni < 3 || no != ni)
    return false;

  const Color edge = shadow.color;
  const Color fade{0, 0, 0, 0}; // premultiplied transparent

  for (int i = 0; i < ni; ++i) {
    const int j = (i + 1) % ni;
    const RingPt &i0 = inner_ring[i];
    const RingPt &i1 = inner_ring[j];
    const RingPt &o0 = outer_ring[i];
    const RingPt &o1 = outer_ring[j];
    // Quad inner_i -> inner_j -> outer_j -> outer_i: full color on the inner
    // pair, transparent on the outer pair => a linear feather across the skirt.
    if (!sink.quad(vtx(i0.x, i0.y, edge), vtx(i1.x, i1.y, edge),
                   vtx(o1.x, o1.y, fade), vtx(o0.x, o0.y, fade)))
      return false;
  }
  return true;
}

} // namespace ui
