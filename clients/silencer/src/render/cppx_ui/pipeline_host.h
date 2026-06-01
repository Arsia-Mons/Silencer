#pragma once

#include <SDL3/SDL.h>

#include "client/ui/app_shell/ui_pipeline.h"
#include "font_registry.h"
#include "texture_registry.h"
#include "ui_surface.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace silencer::cppx_ui {

// The production render seam SIL-13 deferred: drives the golden
// client::ui::UiPipeline and turns one frame into a tightly-packed RGBA buffer
// that the GPU backend uploads via RenderDevice::UploadUiFrame (the same
// software-raster -> upload -> GPU-composite path SIL-11 proved with the demo
// overlay, now fed by the real pipeline instead of a hand-built command list).
//
// Each frame: begin the UiSurface, run render_client_ui_frame() — which builds
// the retained tree, lays it out, focuses, and builds the tagged-union IR, then
// calls our render lambda that executes that IR via execute_draw_commands —
// resolve, and copy the surface to a packed buffer. SDL-side; lives in
// renderer/ alongside the executor it drives.
//
// react_init_runtime() must have been called once before use (the host does not
// own the global hook runtime). Caller sets frame.layout to ensure()'s w x h.
class PipelineHost {
public:
  PipelineHost();
  ~PipelineHost();

  PipelineHost(const PipelineHost &) = delete;
  PipelineHost &operator=(const PipelineHost &) = delete;

  // (Re)create the w x h software surface + renderer + pipeline. Loads the four
  // UI faces from `font_dir` on first call (the UiSurface requires a default
  // font) and installs the text measurer. Idempotent for an unchanged size.
  // Returns false if SDL surface/renderer creation or font load fails.
  bool ensure(int w, int h, const char *font_dir);

  client::ui::UiPipeline &pipeline() { return *pipeline_; }

  // Run one pipeline frame for `frame` and return tightly-packed RGBA (w*h*4),
  // or null if not initialized. The buffer is owned by the host and valid until
  // the next render()/ensure().
  const uint8_t *render(const client::ui::UiPipelineFrame &frame, int *out_w,
                        int *out_h);

private:
  SDL_Surface *surf_ = nullptr;
  SDL_Renderer *r_ = nullptr;
  FontRegistry fonts_;
  TextureRegistry textures_;
  UiSurface ui_;
  std::unique_ptr<client::ui::UiPipeline> pipeline_;
  std::vector<uint8_t> packed_;
  int w_ = 0;
  int h_ = 0;
};

} // namespace silencer::cppx_ui
