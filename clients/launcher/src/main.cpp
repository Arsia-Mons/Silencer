#include "app.h"
#include "backgrounds.h"
#include "ui.h"

#include "client/ui/app_shell/ui_pipeline.h"
#include "updater/updaterstage2.h"
#include "render/cppx_ui/pipeline_host.h"
#include "render/cppx_ui/sdl_ui_input.h"
#include "ui/input.h"
#include "ui/runtime/react.h"

#define SDL_MAIN_HANDLED // we own main(); don't let SDL redefine it (repo convention)
#include <SDL3/SDL.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#ifndef LAUNCHER_FONT_DIR
#define LAUNCHER_FONT_DIR "."
#endif
#ifndef LAUNCHER_ASSETS_DIR
#define LAUNCHER_ASSETS_DIR "."
#endif

namespace {

// Resource lookup, in order:
//   1. the env override — wins outright, even if the dir turns out to be bad,
//      so a test can point at an empty dir and see the failure,
//   2. shipped beside the binary: SDL_GetBasePath is the bundle's
//      Contents/Resources on macOS and the executable's dir elsewhere,
//   3. the compile-time source-tree path, for running out of build/.
//
// (2) is probed with a sentinel file rather than assumed. An installed copy
// has no source tree to fall back on and a bare dev build has nothing staged
// beside it, so whichever of the two exists is the right answer. Without this
// the baked absolute path is all there is, and a launcher copied off the build
// machine dies in PipelineHost::ensure before it ever opens a window.
std::string resolve_resource_dir(const char *env_var, const char *subdir,
                                 const char *sentinel, const char *baked) {
  if (const char *env = getenv(env_var); env && env[0])
    return env;
  if (const char *base = SDL_GetBasePath(); base && base[0]) {
    std::string dir = std::string(base) + subdir; // GetBasePath ends in a separator
    if (FILE *probe = fopen((dir + "/" + sentinel).c_str(), "rb")) {
      fclose(probe);
      return dir;
    }
  }
  return baked;
}

const char *font_dir() {
  static const std::string dir = resolve_resource_dir(
      "SILENCER_LAUNCHER_FONT_DIR", "fonts", "silencer-135.otf", LAUNCHER_FONT_DIR);
  return dir.c_str();
}

const char *assets_dir() {
  static const std::string dir = resolve_resource_dir(
      "SILENCER_LAUNCHER_ASSETS_DIR", "assets", "PALETTE.BIN", LAUNCHER_ASSETS_DIR);
  return dir.c_str();
}

// Where this launcher is installed, for the read-only SETTINGS row.
// Report the .app itself on macOS — the only path a user recognizes, and the
// thing they would move. SDL_GetBasePath resolves inside the bundle: to
// Contents/Resources when the process is bundled (verified), and to the
// executable's own dir otherwise, so trim either tail. SDL owns the returned
// buffer; do not free it.
std::string launcher_install_dir() {
  const char *base = SDL_GetBasePath();
  if (!base || !base[0])
    return "";
  std::string d = base;
  while (d.size() > 1 && (d.back() == '/' || d.back() == '\\'))
    d.pop_back();
  for (const char *suffix : {"/Contents/Resources", "/Contents/MacOS"}) {
    const size_t n = strlen(suffix);
    if (d.size() > n && d.compare(d.size() - n, n, suffix) == 0) {
      d.resize(d.size() - n);
      break;
    }
  }
  return d;
}

// The BROWSE... folder pick. SDL may run the dialog callback on another
// thread, so the chosen path is marshaled back to the main loop as an SDL
// user event (type s_folder_event, data1 = SDL_strdup'd path).
Uint32 s_folder_event = 0;

void folder_chosen(void *, const char *const *filelist, int) {
  if (!filelist || !filelist[0] || !filelist[0][0])
    return; // canceled, or the dialog errored
  SDL_Event ev{};
  ev.type = s_folder_event;
  ev.user.data1 = SDL_strdup(filelist[0]);
  SDL_PushEvent(&ev);
}

// Shot-mode input script (SILENCER_LAUNCHER_SCRIPT): semicolon-separated
// "tab" / "backtab" / "click:x,y" (logical coords) / "dblclick:x,y" /
// "text:<utf8>" / "key:<name>[+shift|ctrl|alt|super]" steps, applied one at a
// time once the async states settle — offscreen self-verify for focus rings,
// pointer flows, and the text-editing model (#336) the UI_STATE seeds can't
// reach. A click presses one frame and releases the next (activation confirms
// on release over the same node).
struct ScriptStep {
  bool click = false;
  int clicks = 1;    // click streak for click steps (dblclick => 2)
  bool back = false; // backtab
  float x = 0, y = 0;
  std::string text; // text: step — typed into the focused field
  ::ui::UiKey key = ::ui::UiKey::Unknown; // key: step
  uint16_t mods = ::ui::UI_KEY_MOD_NONE;
};

::ui::UiKey script_key_by_name(const std::string &name) {
  if (name == "left")
    return ::ui::UiKey::Left;
  if (name == "right")
    return ::ui::UiKey::Right;
  if (name == "home")
    return ::ui::UiKey::Home;
  if (name == "end")
    return ::ui::UiKey::End;
  if (name == "backspace")
    return ::ui::UiKey::Backspace;
  if (name == "delete")
    return ::ui::UiKey::DeleteForward;
  if (name == "enter")
    return ::ui::UiKey::Enter;
  if (name == "a")
    return ::ui::UiKey::A;
  if (name == "c")
    return ::ui::UiKey::C;
  if (name == "v")
    return ::ui::UiKey::V;
  if (name == "x")
    return ::ui::UiKey::X;
  return ::ui::UiKey::Unknown;
}

std::vector<ScriptStep> parse_script(const char *env) {
  std::vector<ScriptStep> out;
  if (!env || !env[0])
    return out;
  const std::string s = env;
  size_t pos = 0;
  while (pos <= s.size()) {
    const size_t semi = s.find(';', pos);
    const std::string tok =
        s.substr(pos, semi == std::string::npos ? std::string::npos : semi - pos);
    ScriptStep step;
    if (tok == "tab" || tok == "backtab") {
      step.back = tok[0] == 'b';
      out.push_back(step);
    } else if (tok.rfind("click:", 0) == 0 &&
               sscanf(tok.c_str() + 6, "%f,%f", &step.x, &step.y) == 2) {
      step.click = true;
      out.push_back(step);
    } else if (tok.rfind("dblclick:", 0) == 0 &&
               sscanf(tok.c_str() + 9, "%f,%f", &step.x, &step.y) == 2) {
      step.click = true;
      step.clicks = 2;
      out.push_back(step);
    } else if (tok.rfind("text:", 0) == 0) {
      step.text = tok.substr(5);
      out.push_back(step);
    } else if (tok.rfind("key:", 0) == 0) {
      // key:left+shift+alt — a key name, then any modifiers.
      std::string rest = tok.substr(4);
      size_t plus = 0;
      bool first = true;
      while (plus <= rest.size()) {
        const size_t next = rest.find('+', plus);
        const std::string part = rest.substr(
            plus, next == std::string::npos ? std::string::npos : next - plus);
        if (first) {
          step.key = script_key_by_name(part);
          first = false;
        } else if (part == "shift") {
          step.mods |= ::ui::UI_KEY_MOD_SHIFT;
        } else if (part == "ctrl") {
          step.mods |= ::ui::UI_KEY_MOD_CTRL;
        } else if (part == "alt") {
          step.mods |= ::ui::UI_KEY_MOD_ALT;
        } else if (part == "super") {
          step.mods |= ::ui::UI_KEY_MOD_SUPER;
        }
        if (next == std::string::npos)
          break;
        plus = next + 1;
      }
      if (step.key != ::ui::UiKey::Unknown)
        out.push_back(step);
    }
    if (semi == std::string::npos)
      break;
    pos = semi + 1;
  }
  return out;
}

// `typing` = a text field is focused: editing keys go to the field (as UiKey
// events WITH modifiers, so selection/word-jump/clipboard chords work) instead
// of nav/confirm, and Space types rather than activates. Keycode + modifier
// translation is the shared bridge table (#336) — never a local copy.
void handle_key(SDL_Keycode key, ::ui::UiInputFrame &in, bool typing) {
  in.source = ::ui::UiFocusSource::Keyboard;
  const uint16_t mods = silencer::cppx_ui::ui_mods_from_sdl(SDL_GetModState());
  switch (key) {
  case SDLK_ESCAPE:
    in.cancel_pressed = true;
    return;
  case SDLK_RETURN:
  case SDLK_KP_ENTER:
    in.confirm_pressed = true;
    break;
  case SDLK_SPACE:
    if (!typing)
      in.confirm_pressed = true;
    return;
  case SDLK_TAB:
    if (mods & ::ui::UI_KEY_MOD_SHIFT)
      in.nav_previous = true;
    else
      in.nav_next = true;
    return;
  case SDLK_UP:
  case SDLK_DOWN:
    if (!typing)
      (key == SDLK_UP ? in.nav_up : in.nav_down) = true;
    return;
  case SDLK_LEFT:
  case SDLK_RIGHT:
    if (!typing) {
      (key == SDLK_LEFT ? in.nav_left : in.nav_right) = true;
      return;
    }
    break;
  default:
    break;
  }
  ::ui::UiKey k = silencer::cppx_ui::ui_key_from_sdl(key);
  if (k != ::ui::UiKey::Unknown)
    ::ui::ui_input_push_key(in, k, mods);
}

} // namespace

int main(int argc, char **argv) {
  // Stage-2 self-replace. This same binary is re-executed by the nested helper
  // in Contents/Helpers to swap the bundle out from under the launcher that
  // spawned it, so the check must come before SDL touches anything — stage-2
  // opens no window and must not init a video driver it will never use.
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--self-update-stage2")
      return UpdaterStage2::Run(argc, argv);
  }

  // Clear leftovers from a previous self-update (`<install>.old`, and on
  // Windows the sidelined `<file>.old-<ticks>` copies stage-2 leaves when a
  // target was locked). Safe when no update ever ran.
  UpdaterStage2::CleanupPreviousUpdate();

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

  // Shot-mode e2e triggers (no click injection offscreen):
  // SILENCER_LAUNCHER_TEST_SIGNIN=user:pass runs the real TCP opAuth flow;
  // SILENCER_LAUNCHER_TEST_INSTALL=<channel> runs the download→verify→extract
  // flow. The shot waits for both to settle.
  if (shot_path) {
    if (const char *si = getenv("SILENCER_LAUNCHER_TEST_SIGNIN")) {
      std::string cred = si;
      size_t colon = cred.find(':');
      if (colon != std::string::npos)
        app.sign_in(cred.substr(0, colon), cred.substr(colon + 1));
    }
    if (const char *ti = getenv("SILENCER_LAUNCHER_TEST_INSTALL"))
      app.install(ti);
  }

  launcher::Intents intents;
  intents.set_channel = [&app](const std::string &c) { app.set_channel(c); };
  intents.refresh = [&app]() { app.refresh(); };
  intents.install = [&app](const std::string &c) { app.install(c); };
  intents.uninstall = [&app](const std::string &c) { app.uninstall(c); };
  intents.set_base_dir = [&app](const std::string &d) { app.set_base_dir(d); };
  s_folder_event = SDL_RegisterEvents(1);
  intents.browse_base_dir = [window, &app]() {
    const std::string start = app.snapshot().base_dir;
    SDL_ShowOpenFolderDialog(folder_chosen, nullptr, window, start.c_str(), false);
  };
  intents.play = [&app]() { app.play(); };
  intents.sign_in = [&app](const std::string &u, const std::string &p) { app.sign_in(u, p); };
  intents.sign_out = [&app]() { app.sign_out(); };
  intents.open_url = [](const std::string &url) { SDL_OpenURL(url.c_str()); };

  launcher::AppSnapshot snap;
  launcher::ViewModel vm;
  vm.snap = &snap;
  vm.intents = &intents;
  vm.install_dir = launcher_install_dir();

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

  // The canonical logo (bank 208 frame 60), baked alongside the backdrop.
  launcher::Background logo;
  const bool have_logo = launcher::load_logo(assets_dir(), &logo);
  if (!have_logo)
    fprintf(stderr, "[launcher] logo: unavailable, falling back to text\n");
  uint32_t logo_id = 0;

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
  // PERF_WHEEL=1: hover the content panel and wheel-scroll every frame — the
  // scroll benchmark (a moving content track repaints the whole viewport).
  const char *pwh = getenv("SILENCER_LAUNCHER_PERF_WHEEL");
  const bool perf_wheel = pwh && pwh[0] == '1';

  // Idle-frame skip: the UI can only change through input, app-state deltas,
  // or its own multi-frame settling (interaction reads lag one frame by
  // design). Once the pipeline has produced two consecutive byte-identical
  // frames and none of those sources fire, the loop re-presents the retained
  // texture without rebuilding anything.
  int settled_renders = 0;
  float prev_mx = -1.f, prev_my = -1.f;

  // Shot-mode input script state.
  std::vector<ScriptStep> script =
      shot_path ? parse_script(getenv("SILENCER_LAUNCHER_SCRIPT")) : std::vector<ScriptStep>();
  size_t script_i = 0;
  int script_wait = 0;        // frames to idle between steps
  bool script_release = false; // a press was injected last frame
  float script_mx = -1.f, script_my = -1.f; // scripted pointer, sticky

  bool running = true;
  bool text_input_on = false;
  while (running) {
    ::ui::UiInputFrame in = {};
    in.source = ::ui::UiFocusSource::Mouse;
    static bool lmb_down = false;
    bool input_event = false;

    const bool typing = host.pipeline().client_ui().wants_text_input();
    if (typing != text_input_on) {
      // SDL3 gates SDL_EVENT_TEXT_INPUT behind an explicit start per window.
      if (typing)
        SDL_StartTextInput(window);
      else
        SDL_StopTextInput(window);
      text_input_on = typing;
    }

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
          // Click streak + modifiers feed text selection (double-click = word,
          // shift+click = extend).
          in.pointer_clicks = (int)e.button.clicks;
          in.pointer_mods =
              silencer::cppx_ui::ui_mods_from_sdl(SDL_GetModState());
          lmb_down = true;
          input_event = true;
        }
        break;
      case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e.button.button == SDL_BUTTON_LEFT) {
          in.pointer_released = true;
          lmb_down = false;
          input_event = true;
        }
        break;
      case SDL_EVENT_MOUSE_WHEEL:
        in.wheel_y += e.wheel.y;
        input_event = true;
        break;
      case SDL_EVENT_KEY_DOWN:
        handle_key(e.key.key, in, typing);
        input_event = true;
        break;
      case SDL_EVENT_TEXT_INPUT:
        ::ui::ui_input_push_text(in, e.text.text);
        in.source = ::ui::UiFocusSource::Keyboard;
        input_event = true;
        break;
      default:
        if (s_folder_event && e.type == s_folder_event && e.user.data1) {
          app.set_base_dir(static_cast<char *>(e.user.data1));
          SDL_free(e.user.data1);
          input_event = true;
        }
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

    launcher::AppSnapshot next_snap = app.snapshot();
    const bool snap_changed = !(next_snap == snap);
    snap = std::move(next_snap);

    // Every async source the shot (and its input script) waits out.
    auto manifest_settled = [](launcher::ManifestStatus m) {
      return m != launcher::ManifestStatus::Idle && m != launcher::ManifestStatus::Loading;
    };
    auto fetch_settled = [](launcher::FetchStatus f) {
      return f != launcher::FetchStatus::Idle && f != launcher::FetchStatus::Loading;
    };
    const bool async_settled =
        manifest_settled(snap.stable.manifest) && manifest_settled(snap.nightly.manifest) &&
        fetch_settled(snap.news_status) && fetch_settled(snap.releases_status) &&
        snap.ping != launcher::PingStatus::Unknown &&
        snap.ping != launcher::PingStatus::Probing &&
        snap.auth != launcher::AuthStatus::Connecting &&
        snap.update_status != launcher::UpdateStatus::Downloading &&
        snap.update_status != launcher::UpdateStatus::Verifying &&
        snap.update_status != launcher::UpdateStatus::Extracting;

    // Replay the shot script one step at a time once the app is quiet.
    if (shot_path && async_settled) {
      if (script_release) {
        in.pointer_released = true;
        in.pointer_down = false;
        script_release = false;
        input_event = true;
      } else if (script_wait > 0) {
        --script_wait;
      } else if (script_i < script.size()) {
        const ScriptStep &step = script[script_i++];
        if (step.click) {
          script_mx = step.x;
          script_my = step.y;
          in.pointer_pressed = true;
          in.pointer_down = true;
          in.pointer_clicks = step.clicks;
          in.pointer_mods = step.mods;
          script_release = true;
        } else if (!step.text.empty()) {
          ::ui::ui_input_push_text(in, step.text.c_str());
          in.source = ::ui::UiFocusSource::Keyboard;
        } else if (step.key != ::ui::UiKey::Unknown) {
          ::ui::ui_input_push_key(in, step.key, step.mods);
          in.source = ::ui::UiFocusSource::Keyboard;
        } else {
          (step.back ? in.nav_previous : in.nav_next) = true;
          in.source = ::ui::UiFocusSource::Keyboard;
        }
        input_event = true;
        script_wait = 3;
      }
    }
    const bool script_done = script_i >= script.size() && !script_release && script_wait == 0;

    // Perf injections count as input (they exist to force repaints).
    const bool perf_inject =
        perf_scale > 0.f && (perf_n % 30 == 7 || perf_wheel);
    const bool pointer_moved = mx != prev_mx || my != prev_my;
    prev_mx = mx;
    prev_my = my;
    const bool size_changed = tex && (pw != tex_w || ph != tex_h);

    if (!shot_path && tex && settled_renders >= 2 && !input_event &&
        !pointer_moved && !snap_changed && !size_changed && !perf_inject) {
      // Nothing can have changed: re-present the retained frame and move on.
      SDL_SetRenderDrawColor(renderer, 3, 6, 3, 255);
      SDL_RenderClear(renderer);
      SDL_RenderTexture(renderer, tex, nullptr, nullptr);
      SDL_RenderPresent(renderer);
      if (perf_skip > 0) {
        --perf_skip;
      } else {
        ++perf_nu; // skipped frame: an idle frame that cost ~0
        ++perf_n;
        if (perf_frame_limit > 0 && perf_n >= perf_frame_limit)
          running = false;
      }
      continue;
    }

    if (!host.ensure(pw, ph, font_dir())) {
      fprintf(stderr, "PipelineHost::ensure failed (font dir: %s)\n", font_dir());
      running = false;
      break;
    }

    // ensure() drops renderer-bound textures (glyph atlases included) on a
    // reset; re-bake both the origin glyph fonts and the backdrop then.
    if (host.chrome_needs_bake()) {
      if (!launcher::bake_glyph_faces(host, assets_dir()))
        fprintf(stderr, "[launcher] glyph fonts: bake incomplete, TTF fallback stays\n");
      if (!bg.indices.empty()) {
        bg_id = host.bake_chrome_sprite(bg.indices.data(), bg.w, bg.h, bg.palette);
        if (!bg_id)
          fprintf(stderr, "[launcher] backgrounds: bake failed (%dx%d)\n", bg.w,
                  bg.h);
      }
      if (have_logo) {
        logo_id =
            host.bake_chrome_sprite(logo.indices.data(), logo.w, logo.h, logo.palette);
        if (!logo_id)
          fprintf(stderr, "[launcher] logo: bake failed (%dx%d)\n", logo.w, logo.h);
      }
      host.mark_chrome_baked();
    }
    if (!bg.indices.empty())
      vm.bg_texture = bg_id;
    if (have_logo) {
      vm.logo_texture = logo_id;
      vm.logo_w = logo.w;
      vm.logo_h = logo.h;
    }

    client::ui::UiPipelineFrame frame = {};
    frame.input = in;
    frame.layout = {(float)lw, (float)lh};
    frame.pointer = {mx, my};
    if (script_mx >= 0.f)
      frame.pointer = {script_mx, script_my}; // scripted pointer stays put

    // In perf mode, force a focus-nav repaint periodically so repaint cost is
    // sampled even when all async states have settled.
    if (perf_scale > 0.f && perf_n % 30 == 7) {
      frame.input.nav_next = true;
      frame.input.source = ::ui::UiFocusSource::Keyboard;
    }
    if (perf_scale > 0.f && perf_wheel) {
      frame.input.wheel_y = (perf_n & 1) ? 1.f : -1.f;
      frame.pointer = {450.f, 350.f}; // over the content panel
    }

    int ow = 0, oh = 0;
    bool unchanged = false;
    silencer::cppx_ui::UiDamage dmg;
    Uint64 t0 = SDL_GetPerformanceCounter();
    const uint8_t *rgba = host.render(frame, &ow, &oh, &unchanged, &dmg);
    Uint64 t1 = SDL_GetPerformanceCounter();
    if (!rgba)
      continue;
    // An input frame never counts toward settling even when byte-identical:
    // activations dispatch after the tree is built (update_retained_runtime),
    // so their state change renders one frame later — which the skip gate
    // would swallow whenever the input frame itself drew nothing (activating
    // an already-focused control leaves no visual delta).
    settled_renders = (unchanged && !input_event) ? settled_renders + 1 : 0;

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
      // Wait for the script's last activation to render and stabilize
      // (settled_renders needs two identical input-free frames) before the
      // write, so the PNG shows the post-interaction state.
      const bool settled =
          async_settled && script_done && (script.empty() || settled_renders >= 2);
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
