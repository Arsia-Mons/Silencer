#include "app.h"
#include "backgrounds.h"
#include "ui.h"

#include "client/ui/app_shell/ui_pipeline.h"
#include "render/cppx_ui/pipeline_host.h"
#include "ui/input.h"
#include "ui/runtime/react.h"

#define SDL_MAIN_HANDLED // we own main(); don't let SDL redefine it (repo convention)
#include <SDL3/SDL.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <cstdio>
#include <cstdlib>
#include <random>

#ifndef LAUNCHER_FONT_DIR
#define LAUNCHER_FONT_DIR "."
#endif
#ifndef LAUNCHER_ASSETS_DIR
#define LAUNCHER_ASSETS_DIR "."
#endif

namespace {

const char *font_dir() {
  const char *env = getenv("SILENCER_LAUNCHER_FONT_DIR");
  return (env && env[0]) ? env : LAUNCHER_FONT_DIR;
}

const char *assets_dir() {
  const char *env = getenv("SILENCER_LAUNCHER_ASSETS_DIR");
  return (env && env[0]) ? env : LAUNCHER_ASSETS_DIR;
}

void handle_key(SDL_Keycode key, ::ui::UiInputFrame &in, bool &running) {
  switch (key) {
  case SDLK_ESCAPE: running = false; break;
  case SDLK_RETURN:
  case SDLK_KP_ENTER:
  case SDLK_SPACE:
    in.confirm_pressed = true;
    in.source = ::ui::UiFocusSource::Keyboard;
    break;
  case SDLK_TAB:
    in.nav_next = true;
    in.source = ::ui::UiFocusSource::Keyboard;
    break;
  case SDLK_UP: in.nav_up = true; in.source = ::ui::UiFocusSource::Keyboard; break;
  case SDLK_DOWN: in.nav_down = true; in.source = ::ui::UiFocusSource::Keyboard; break;
  case SDLK_LEFT: in.nav_left = true; in.source = ::ui::UiFocusSource::Keyboard; break;
  case SDLK_RIGHT: in.nav_right = true; in.source = ::ui::UiFocusSource::Keyboard; break;
  default: break;
  }
}

} // namespace

int main(int, char **) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  // Fixed 900x600 (the layout uses definite widths for text wrapping).
  SDL_Window *window =
      SDL_CreateWindow("Silencer Launcher", 900, 600, SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (!window) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
  if (!renderer) {
    fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  SDL_SetRenderVSync(renderer, 1);
  fprintf(stderr, "[launcher] video driver: %s\n",
          SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "(none)");

  // Offscreen self-verification: when SILENCER_LAUNCHER_SHOT is a path, render
  // until the async manifest/news states settle, dump the frame to a PNG, quit.
  const char *shot_path = getenv("SILENCER_LAUNCHER_SHOT");
  int frame_no = 0;

  react_init_runtime();

  launcher::App app;

  launcher::Intents intents;
  intents.set_channel = [&app](const std::string &c) { app.set_channel(c); };
  intents.refresh = [&app]() { app.refresh(); };
  intents.start_update = [&app]() { app.start_update(); };
  intents.select_server = [&app](int i) { app.select_server(i); };
  intents.play = [&app]() { app.play(); };

  launcher::AppSnapshot snap;
  launcher::ViewModel vm;
  vm.snap = &snap;
  vm.intents = &intents;

  silencer::cppx_ui::PipelineHost host;
  {
    // ensure() lazily creates the UiPipeline; it must run before pipeline() is
    // touched. Seed it with the initial backing-store size.
    int ipw = 0, iph = 0;
    SDL_GetWindowSizeInPixels(window, &ipw, &iph);
    if (!host.ensure(ipw > 0 ? ipw : 900, iph > 0 ? iph : 600, font_dir())) {
      fprintf(stderr, "PipelineHost::ensure failed (font dir: %s)\n", font_dir());
      react_shutdown();
      SDL_DestroyRenderer(renderer);
      SDL_DestroyWindow(window);
      SDL_Quit();
      return 1;
    }
  }
  host.pipeline().set_frame_provider(
      [&vm](::ui::UiElement child) { return launcher::launcher_providers(child, &vm); });
  host.pipeline().client_ui().push_screen(launcher::make_launcher_screen());

  // Backdrop: decode the curated sprite backdrops, keep one picked at random
  // for the whole session. The texture is bound to the host's renderer, so it
  // re-bakes whenever ensure() resets it.
  std::vector<launcher::Background> bgs = launcher::load_backgrounds(assets_dir());
  fprintf(stderr, "[launcher] backgrounds: %d loaded from %s\n", (int)bgs.size(),
          assets_dir());
  launcher::Background bg;
  if (!bgs.empty()) {
    std::mt19937 rng((unsigned)std::random_device{}());
    bg = std::move(
        bgs[std::uniform_int_distribution<size_t>(0, bgs.size() - 1)(rng)]);
    bgs.clear();
  }
  uint32_t bg_id = 0;

  SDL_Texture *tex = nullptr;
  int tex_w = 0, tex_h = 0;

  // Frame-cost instrumentation: raster = host.render (layout + CPU raster),
  // upload = SDL_UpdateTexture, present includes the vsync wait.
  const double perf_ms = 1000.0 / (double)SDL_GetPerformanceFrequency();
  double sum_repaint = 0, sum_idle = 0, sum_upload = 0, sum_present = 0, max_repaint = 0;
  int perf_nc = 0, perf_nu = 0, perf_n = 0;
  int perf_skip = 3; // skip warmup frames (font atlas, first layout)
  // Perf-run overrides: PERF_SCALE rasters at scale× the logical size
  // (synthetic HiDPI), PERF_FRAMES quits after that many measured frames.
  const char *ps = getenv("SILENCER_LAUNCHER_PERF_SCALE");
  const float perf_scale = ps ? (float)atof(ps) : 0.f;
  const char *pf = getenv("SILENCER_LAUNCHER_PERF_FRAMES");
  const int perf_frame_limit = pf ? atoi(pf) : 0;

  bool running = true;
  while (running) {
    ::ui::UiInputFrame in = {};
    in.source = ::ui::UiFocusSource::Mouse;
    static bool lmb_down = false;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
      case SDL_EVENT_QUIT:
        fprintf(stderr, "[launcher] SDL_EVENT_QUIT received\n");
        running = false;
        break;
      case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (e.button.button == SDL_BUTTON_LEFT) {
          in.pointer_pressed = true;
          lmb_down = true;
        }
        break;
      case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e.button.button == SDL_BUTTON_LEFT) {
          in.pointer_released = true;
          lmb_down = false;
        }
        break;
      case SDL_EVENT_MOUSE_WHEEL:
        in.wheel_y += e.wheel.y;
        break;
      case SDL_EVENT_KEY_DOWN:
        handle_key(e.key.key, in, running);
        break;
      default:
        break;
      }
    }

    float mx = 0, my = 0;
    SDL_GetMouseState(&mx, &my);
    in.pointer_down = lmb_down;

    int lw = 0, lh = 0, pw = 0, ph = 0;
    SDL_GetWindowSize(window, &lw, &lh);
    SDL_GetWindowSizeInPixels(window, &pw, &ph);
    if (pw < 1 || ph < 1 || lw < 1 || lh < 1)
      continue;
    if (perf_scale > 0.f) {
      pw = (int)(lw * perf_scale);
      ph = (int)(lh * perf_scale);
    }

    snap = app.snapshot();

    if (!host.ensure(pw, ph, font_dir())) {
      fprintf(stderr, "PipelineHost::ensure failed (font dir: %s)\n", font_dir());
      running = false;
      break;
    }

    if (!bg.indices.empty()) {
      if (host.chrome_needs_bake()) {
        bg_id = host.bake_chrome_sprite(bg.indices.data(), bg.w, bg.h, bg.palette);
        if (!bg_id)
          fprintf(stderr, "[launcher] backgrounds: bake failed (%dx%d)\n", bg.w,
                  bg.h);
        host.mark_chrome_baked();
      }
      vm.bg_texture = bg_id;
    }

    client::ui::UiPipelineFrame frame = {};
    frame.input = in;
    frame.layout = {(float)lw, (float)lh};
    frame.pointer = {mx, my};

    // In perf mode, force a focus-nav repaint periodically so repaint cost is
    // sampled even when all async states have settled.
    if (perf_scale > 0.f && perf_n % 30 == 7) {
      frame.input.nav_next = true;
      frame.input.source = ::ui::UiFocusSource::Keyboard;
    }

    int ow = 0, oh = 0;
    bool unchanged = false;
    silencer::cppx_ui::UiDamage dmg;
    Uint64 t0 = SDL_GetPerformanceCounter();
    const uint8_t *rgba = host.render(frame, &ow, &oh, &unchanged, &dmg);
    Uint64 t1 = SDL_GetPerformanceCounter();
    if (!rgba)
      continue;

    bool fresh_tex = false;
    if (!tex || tex_w != ow || tex_h != oh) {
      if (tex)
        SDL_DestroyTexture(tex);
      tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                              SDL_TEXTUREACCESS_STREAMING, ow, oh);
      SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
      tex_w = ow;
      tex_h = oh;
      fresh_tex = true;
    }
    // Upload only what changed: nothing on an unchanged frame (the persistent
    // texture already holds it), the damage rects on a partial repaint, the
    // full frame otherwise. A fresh texture always needs the full frame.
    if (fresh_tex || (!unchanged && (dmg.full || dmg.count == 0))) {
      SDL_UpdateTexture(tex, nullptr, rgba, ow * 4);
    } else if (!unchanged) {
      for (int i = 0; i < dmg.count; ++i) {
        const SDL_Rect &r = dmg.rects[i];
        SDL_UpdateTexture(tex, &r, rgba + ((size_t)r.y * ow + r.x) * 4u,
                          ow * 4);
      }
    }
    Uint64 t2 = SDL_GetPerformanceCounter();

    SDL_SetRenderDrawColor(renderer, 3, 6, 3, 255);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, tex, nullptr, nullptr);
    SDL_RenderPresent(renderer);
    Uint64 t3 = SDL_GetPerformanceCounter();

    if (perf_skip > 0) {
      --perf_skip;
    } else {
      double r = (t1 - t0) * perf_ms;
      if (unchanged) {
        sum_idle += r;
        ++perf_nu;
      } else {
        sum_repaint += r;
        if (r > max_repaint)
          max_repaint = r;
        ++perf_nc;
      }
      sum_upload += (t2 - t1) * perf_ms;
      sum_present += (t3 - t2) * perf_ms;
      ++perf_n;
      if (perf_frame_limit > 0 && perf_n >= perf_frame_limit)
        running = false;
    }

    if (shot_path) {
      ++frame_no;
      bool settled = snap.manifest_status != launcher::ManifestStatus::Idle &&
                     snap.manifest_status != launcher::ManifestStatus::Loading &&
                     snap.news_status != launcher::NewsStatus::Idle &&
                     snap.news_status != launcher::NewsStatus::Loading;
      for (const auto &sv : snap.servers)
        if (sv.ping == launcher::PingStatus::Unknown || sv.ping == launcher::PingStatus::Probing)
          settled = false;
      if ((settled && frame_no > 20) || frame_no >= 360) {
        if (stbi_write_png(shot_path, ow, oh, 4, rgba, ow * 4))
          fprintf(stderr, "[launcher] wrote screenshot %s (%dx%d, frame %d)\n",
                  shot_path, ow, oh, frame_no);
        else
          fprintf(stderr, "[launcher] stbi_write_png failed: %s\n", shot_path);
        running = false;
      }
      SDL_Delay(16);
    }
  }

  if (perf_n > 0)
    fprintf(stderr,
            "[perf] final: %dx%d | repaint avg %.2f max %.2f ms (n=%d) | "
            "idle avg %.2f ms (n=%d) | upload avg %.2f ms | present avg %.2f ms "
            "(vsync incl.)\n",
            tex_w, tex_h, perf_nc ? sum_repaint / perf_nc : 0.0, max_repaint, perf_nc,
            perf_nu ? sum_idle / perf_nu : 0.0, perf_nu, sum_upload / perf_n,
            sum_present / perf_n);

  if (tex)
    SDL_DestroyTexture(tex);
  react_shutdown();
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
