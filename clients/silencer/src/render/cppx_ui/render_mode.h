#pragma once

// RenderMode — the anti-aliasing *strategy* the renderer uses to put crisp edges
// on rounded vector primitives (fills, borders, gradients). It is the single
// source of truth for "how do we get smooth corners", consumed by exactly two
// pipeline stages, each through one pure mapping function below:
//
//   1. the FRAME stage (UiSurface::begin_frame, driven by GameLoop) reads
//      supersample_for(mode, density) to decide whether to render the whole
//      scene into an N× offscreen and box-downsample it;
//   2. the PRIMITIVE stage (execute_draw_commands) switches on the mode to pick
//      how each rounded shape is rasterized.
//
// The three strategies are the three textbook ways a UI library gets an
// "infinitely smooth" border radius — kept side by side so they can be compared:
//
//   Ssaa     — brute force. Hard-edged geometry rendered at 2× then averaged
//              down. The edge AA comes entirely from the resolve. Costs 4× fill.
//   FringeAa — analytic edge coverage, geometric. The silhouette is extruded
//              into a 1-device-pixel band that fades to transparent (the Dear
//              ImGui AntiAliasedFill technique). The core stays a tessellated
//              polygon. No supersampling. This is the project's prior default.
//   Sdf      — analytic edge coverage, per pixel. A signed-distance field of the
//              rounded rect is evaluated per pixel into a coverage mask; the
//              curve is exact at any size, no facets. The "infinitely smooth"
//              answer. No supersampling.
//
// This is a renderer strategy descriptor: pure data, NO SDL, NO game vocabulary.
// It deliberately is NOT exposed to ui/ or game/ — components never choose how
// they are rasterized; app/ toggles the mode and renderer/ honours it.

#include <stdint.h>

#include <string.h>

namespace silencer::cppx_ui {

enum class RenderMode : uint8_t {
  Ssaa = 0, // full-scene supersample; primitives hard-edged, AA from downsample
  FringeAa, // per-primitive 1px analytic feather band (Dear ImGui); no SSAA
  Sdf,      // per-primitive signed-distance-field coverage mask; no SSAA
};

inline constexpr int kRenderModeCount = 3;

// Human-facing label (HUD / window title / logs).
inline const char *render_mode_name(RenderMode m) {
  switch (m) {
  case RenderMode::Ssaa:
    return "SSAA";
  case RenderMode::FringeAa:
    return "Fringe AA";
  case RenderMode::Sdf:
    return "SDF";
  }
  return "?";
}

// Machine-facing token (CLI / env var / screenshot filenames). Stable.
inline const char *render_mode_slug(RenderMode m) {
  switch (m) {
  case RenderMode::Ssaa:
    return "ssaa";
  case RenderMode::FringeAa:
    return "fringe";
  case RenderMode::Sdf:
    return "sdf";
  }
  return "ssaa";
}

// Parse a slug (case-insensitive, accepts a few aliases). Returns false and
// leaves *out untouched on no match, so callers keep their default.
inline bool render_mode_from_slug(const char *s, RenderMode *out) {
  if (!s || !out)
    return false;
  auto eq = [](const char *a, const char *b) {
#if defined(_WIN32)
    return _stricmp(a, b) == 0;
#else
    return strcasecmp(a, b) == 0;
#endif
  };
  if (eq(s, "ssaa") || eq(s, "supersample") || eq(s, "ss")) {
    *out = RenderMode::Ssaa;
    return true;
  }
  if (eq(s, "fringe") || eq(s, "fringeaa") || eq(s, "fringe_aa") ||
      eq(s, "feather")) {
    *out = RenderMode::FringeAa;
    return true;
  }
  if (eq(s, "sdf") || eq(s, "distance")) {
    *out = RenderMode::Sdf;
    return true;
  }
  return false;
}

// Cycle order for the runtime toggle key.
inline RenderMode next_render_mode(RenderMode m) {
  return static_cast<RenderMode>((static_cast<int>(m) + 1) % kRenderModeCount);
}

// FRAME-stage policy: device-pixel supersample factor for `mode` at display
// `density`. Only SSAA supersamples — and only on a standard-density display,
// since a HiDPI panel already renders at native resolution. The analytic modes
// (Fringe/SDF) never supersample; their AA is per-primitive. This is the one
// place the old `density >= 1.5 ? 1 : 2` heuristic lives now.
inline int supersample_for(RenderMode mode, float density) {
  if (mode != RenderMode::Ssaa)
    return 1;
  return density >= 1.5f ? 1 : 2;
}

} // namespace silencer::cppx_ui
