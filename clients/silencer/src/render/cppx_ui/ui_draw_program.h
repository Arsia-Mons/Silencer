#pragma once

// ui_draw_program.h — the backend-neutral UI render program (SIL-240).
//
// The cppx UI used to be CPU-rasterized to a window-sized RGBA buffer and
// uploaded whole to the GPU every changed frame. This type is the replacement
// hand-off: the cppx-side emitter (build_ui_draw_program, ui_draw_program.cpp)
// walks the same ::ui::DrawCommandList IR the software executor does and lowers
// it to a flat GPU draw program — a de-indexed vertex stream + a small command
// stream (draw / scissor / layer) + a manifest of the textures the batches
// reference. The SDL_GPU backend (sdl3gpubackend.cpp) consumes it directly.
//
// This header is deliberately SDL-free (only <cstdint>/<vector>): it crosses the
// cppx_ui -> renderdevice -> backend boundary, and only sdl3gpubackend.cpp is
// allowed to include <SDL3/SDL_gpu.h>. All parity-critical geometry/UV/snap math
// happens CPU-side in the emitter; the backend just binds + draws.

#include <cstdint>
#include <vector>

namespace silencer::cppx_ui {

// One UI vertex in the de-indexed geometry stream. Positions are in CLIP SPACE
// (the points -> device-px -> clip transform is baked CPU-side in the emitter,
// so the vertex shader is a pass-through and needs no uniform); color is
// PREMULTIPLIED and normalized [0,1]; uv in [0,1]. Eight contiguous floats
// (32 bytes) matching the vert_ui vertex-input layout (FLOAT2 pos @0, FLOAT2 uv
// @8, FLOAT4 color @16).
struct GpuUiVertex {
  float x = 0.f, y = 0.f;                     // clip space [-1,1]
  float u = 0.f, v = 0.f;                     // [0,1]
  float r = 0.f, g = 0.f, b = 0.f, a = 0.f;   // premultiplied [0,1]
};

// A premultiplied-RGBA texture a textured batch references. `key` is opaque +
// stable across frames (see ui_texture_key.h); `rgba`/`w`/`h` are the source
// bytes (owned by a cppx_ui registry, valid through this frame's Present);
// `version` bumps when the bytes change so the backend can skip re-upload.
struct GpuUiTexture {
  uint64_t key = 0;
  const uint8_t *rgba = nullptr;
  int w = 0, h = 0;
  uint64_t version = 0;
};

enum class GpuUiOp : uint8_t {
  DrawBatch,  // draw [first_vertex, first_vertex+vertex_count) with texture_key
  SetClip,    // set scissor to (clip x,y,w,h) in device pixels (top-left origin)
  ClearClip,  // disable scissor
  PushLayer,  // begin a group-opacity offscreen layer (layer_opacity)
  PopLayer,   // composite the layer back over its parent at its opacity
};

struct GpuUiCommand {
  GpuUiOp op = GpuUiOp::DrawBatch;
  uint32_t first_vertex = 0;
  uint32_t vertex_count = 0;
  uint64_t texture_key = 0;   // 0 => solid (backend binds a 1x1 white texture)
  int clip_x = 0, clip_y = 0, clip_w = 0, clip_h = 0;
  float layer_opacity = 1.f;
};

// The full backend-neutral UI frame: a flat command stream over a shared
// de-indexed vertex array, plus the manifest of textures the batches reference.
struct GpuUiProgram {
  std::vector<GpuUiVertex> verts;
  std::vector<GpuUiCommand> commands;
  std::vector<GpuUiTexture> textures;
  int target_w = 0, target_h = 0;     // device resolution the geometry is in
  uint64_t texture_generation = 0;    // bumped on a registry reset; backend
                                      // flushes its GPU texture cache on change

  void clear() {
    verts.clear();
    commands.clear();
    textures.clear();
  }
};

} // namespace silencer::cppx_ui
