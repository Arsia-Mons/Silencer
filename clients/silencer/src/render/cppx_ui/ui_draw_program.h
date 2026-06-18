#pragma once

// The backend-neutral UI render program: a de-indexed vertex stream + a command
// stream (draw / scissor / layer) + a texture manifest, emitted by
// build_ui_draw_program and consumed directly by the SDL_GPU backend.
//
// Deliberately SDL-free (it crosses the cppx_ui -> renderdevice -> backend
// boundary; only sdl3gpubackend.cpp may include <SDL3/SDL_gpu.h>). All
// parity-critical math happens CPU-side in the emitter; the backend just draws.

#include <cstdint>
#include <vector>

namespace silencer::cppx_ui {

// One UI vertex (de-indexed). Position in clip space (transform baked CPU-side,
// so the vertex shader is pass-through); color premultiplied, normalized; uv
// [0,1]. Layout matches vert_ui (FLOAT2 pos @0, FLOAT2 uv @8, FLOAT4 color @16).
struct GpuUiVertex {
  float x = 0.f, y = 0.f;                     // clip space [-1,1]
  float u = 0.f, v = 0.f;                     // [0,1]
  float r = 0.f, g = 0.f, b = 0.f, a = 0.f;   // premultiplied [0,1]
};

// A premultiplied-RGBA texture a batch references. `key` is opaque + stable
// across frames (ui_texture_key.h); `rgba`/`w`/`h` are registry-owned source
// bytes (valid through this frame's Present); `version` bumps on change so the
// backend can skip re-upload.
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

// The full UI frame: a command stream over a shared de-indexed vertex array +
// the texture manifest the batches reference.
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
