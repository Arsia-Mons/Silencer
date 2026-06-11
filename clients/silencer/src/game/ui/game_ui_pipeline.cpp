#include "ui/game_ui_pipeline.h"

#include "game.h"
#include "world.h"
#include "resources/resources.h" // spritebank (SIL-87 chrome bake)
#include "lobby.h"
#include "lobbygame.h"
#include "peer.h"
#include "platform/os.h"
#include "updater.h"
#include "camera.h"
#include "detonator.h"
#include "gasloader.h"
#include "objecttypes.h"
#include "player.h"
#include "buyableitem.h"
// SIL-14 golden cppx render path.
#include "render/cppx_ui/pipeline_host.h"
#include "client/ui/app_shell/app_root.h"
#include "client/ui/app_shell/client_ui.h"
#include "client/ui/app_theme.h"
#include "client/ui/components/tokens.h"
#include "client/ui/screens/gallery.h"
#include "client/ui/screens/message_modal.h"
#include "client/ui/screens/password_modal.h"
#include "client/ui/hooks/use_key_map.h"
#include "client/ui/hooks/use_keybind_capture.h"
#include "client/ui/hooks/use_session.h"
#include "client/ui/hooks/use_settings.h"
#include "client/ui/hooks/use_updater.h"
#include "client/ui/providers/app_provider.h"
#include "client/ui/providers/chrome_textures_provider.h"
#include "client/ui/providers/clock_provider.h"
#include "client/ui/providers/key_map_provider.h"
#include "client/ui/providers/keybind_capture_provider.h"
#include "client/ui/providers/lobby_provider.h"
#include "client/ui/providers/server_provider.h"
#include "client/ui/providers/session_provider.h"
#include "client/ui/providers/settings_provider.h"
#include "client/ui/providers/updater_provider.h"
#include "client/ui/providers/world_session_provider.h"
#include "ui/lobby_ui_model.h"
#include "ui/world_session_model.h"
#include "player.h"
#include "actor/user.h"
#include "session_phase.h"
#include "config.h"
#include "mapfetch.h"
#include "audio.h"
#include "renderdevice.h"
#include "ui/runtime/react.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

namespace {
// SIL-15 use_key_map: convert UI chips back to an input Binding, enforcing the
// chord cap (reject, never truncate). Returns false if the chord is empty or
// over CHORD_CAP.
bool ChipsToBinding(const std::vector<client::ui::KeyMapChip> &chips,
                    Binding &out) {
  if (chips.empty() || (int)chips.size() > CHORD_CAP)
    return false;
  for (const client::ui::KeyMapChip &c : chips) {
    BindingKey bk;
    bk.device = c.device;
    bk.code = c.code;
    bk.axisDir = c.axis_dir;
    out.keys.push_back(bk);
  }
  return true;
}

bool KeybindProfileIsBuiltin(const std::string &name) {
  return name == "default" || name == "wasd" || name == "gamepad";
}

// Label a BindingKey for the UI (the UI never re-derives labels): keyboard via
// GetKeyName, mouse to LMB/MMB/RMB, gamepad button/axis via GamepadShortLabel
// (type-aware). Shared by the use_key_map read view + the capture pending chord.
client::ui::KeyMapChip ChipFromBindingKey(const BindingKey &bk,
                                          SDL_GamepadType padType) {
  client::ui::KeyMapChip chip;
  chip.device = bk.device;
  chip.code = bk.code;
  chip.axis_dir = bk.axisDir;
  switch (bk.device) {
  case BindingDevice::Keyboard:
    chip.label = KeyMap::GetKeyName((SDL_Scancode)bk.code);
    break;
  case BindingDevice::Mouse:
    chip.label = (bk.code == 1)   ? "LMB"
                 : (bk.code == 2) ? "MMB"
                 : (bk.code == 3) ? "RMB"
                                  : ("M" + std::to_string(bk.code));
    break;
  default: { // GamepadButton / GamepadAxis
    std::string s = Stringify(bk);
    auto colon = s.find(':');
    std::string raw = (colon != std::string::npos) ? s.substr(colon + 1) : s;
    chip.label = GamepadShortLabel(raw, padType);
    break;
  }
  }
  return chip;
}
} // namespace

void GameUiPipeline::DrawInGameWorldInsets(Surface& surface, float frametime) {
Player * localplayer = game.world.GetPeerPlayer(game.world.GetLocalPeerId());
if(!localplayer) return;
Renderer::Rect dstrect;
for(int slot = 0; slot < 2; ++slot){
if(!game.world.IsSystemCameraActive(slot)) continue;
Surface systemscreen(135, 44, 1);
Camera camera(135 * 2, 44 * 2);
Object * followobject = game.world.GetObjectFromId(game.world.GetSystemCameraFollowId(slot));
int px = 0;
int py = 0;
if(followobject){
px = followobject->x + ((followobject->oldx - followobject->x) * frametime);
py = followobject->y + ((followobject->oldy - followobject->y) * frametime);
if(slot == 1 && followobject->type == ObjectTypes::DETONATOR){
Detonator * detonator = static_cast<Detonator*>(followobject);
if(detonator->HasDetonated() && py < detonator->lowestypos){
py = detonator->lowestypos;
}
}
}
camera.Follow(game.world,
              px + game.world.GetSystemCameraX(slot),
              py + game.world.GetSystemCameraY(slot),
              0, 0, 0, 0);
game.renderer.DrawWorldScaled(&systemscreen, camera, 3, frametime);
game.renderer.EffectRampColor(&systemscreen, 0, 190);
dstrect.x = (slot == 0) ? 5 : 500;
dstrect.y = (slot == 0) ? 349 : 348;
Renderer::BlitSurface(&systemscreen, 0, &surface, &dstrect);
}
dstrect.x = 235;
dstrect.y = 419;
Renderer::BlitSurface(&game.world.map.minimap.surface, 0, &surface, &dstrect);
}

void GameUiPipeline::RenderClientUiFrame(Surface& surface, float frametime) {
(void)frametime;
// SIL-14: the golden retained cppx path is the live UI. GameRenderer::Present
// uploads the RGBA we stash here. The Clay frame path is gone; the Clay
// runtime/objects themselves are retired in SIL-22.
RenderCppxClientUiFrame(surface);
}

GameUiPipeline::GameUiPipeline(Game & g) : game(g) {}

GameUiPipeline::~GameUiPipeline() {
// Tear the cppx host down before the global hook runtime it depends on.
cppxHost.reset();
if(cppxReactInitialized){
react_shutdown();
cppxReactInitialized = false;
}
}

client::ui::SessionPhase GameUiPipeline::CurrentSessionPhase() const {
return silencer::game_ui::project_session_phase(game.state, game.nextstate);
}

const uint8_t * GameUiPipeline::CppxUiFrame(int & outW, int & outH) const {
outW = cppxUiW;
outH = cppxUiH;
return cppxUiRgba;
}

void GameUiPipeline::BakeChromeTextures(int rw, int rh, float uiScale) {
// Bake the curated legacy chrome sprites into texture_ids. Runs once per host
// renderer lifetime (guarded by PipelineHost::chrome_needs_bake). This is the
// ONLY place that reads the indexed spritebank + active palette; the resulting
// opaque ids travel to screens via the ChromeTexturesProvider.
cppxChrome = {};
hudRampVariants_.clear();
hudEmblems_.clear();
if(!cppxHost) return;
// Use the BASE palette, not GetPaletteColors(): the latter is the display cache
// that is faded to black on menu/transition screens, which would bake the chrome
// sprites pure black. The base palette carries the authored sprite colors (the
// green oval lives at indices 210/213-224).
const SDL_Color *palette = game.renderer.palette.GetColors();
if(!palette) return;
const auto &banks = game.world.resources.spritebank;

// origin TextInput caret = legacy palette idx 140 resolved against the screen's
// presentation palette page (menus/cc ResetPresentation(1), lobby cluster (2),
// in-game (0)); screens read these off use_chrome().
auto page_color = [&](int pageIdx, int colorIdx){
const SDL_Color &c = game.renderer.palette.colors[pageIdx][colorIdx];
return ::ui::Color{c.r, c.g, c.b, 255};
};
cppxChrome.caret_menu = page_color(1, 140);
cppxChrome.caret_lobby = page_color(2, 140);
cppxChrome.caret_game = page_color(0, 140);

// Each sprite bank's pixel indices are authored against a SPECIFIC palette page
// (resources.cpp Load: bank 0->5, 1->6, 2->7, 3->8, 6->1, 7->2; others base).
// Baking every bank with the single current page mapped bank 6's planet/oval and
// bank 7's chrome indices onto the wrong colors — the rainbow-speckle Mars bug.
// Select each bank's authored page so the bake matches the in-game palettization.
auto page_for_bank = [&](size_t bank) -> const SDL_Color* {
switch(bank){
case 0: return game.renderer.palette.colors[5];
case 1: return game.renderer.palette.colors[6];
case 2: return game.renderer.palette.colors[7];
case 3: return game.renderer.palette.colors[8];
case 6: return game.renderer.palette.colors[1];
case 7: return game.renderer.palette.colors[2];
default: return palette;
}
};

auto bake = [&](size_t bank, size_t index, uint32_t &id_out,
                uint16_t *w_out = nullptr, uint16_t *h_out = nullptr){
if(bank >= banks.size() || index >= banks[bank].size()) return;
const std::shared_ptr<Surface> &sp = banks[bank][index];
if(!sp || sp->w < 1 || sp->h < 1 || sp->pixels.empty()) return;
uint32_t id = cppxHost->bake_chrome_sprite(sp->pixels.data(), sp->w, sp->h,
                                           page_for_bank(bank));
if(id){ id_out = id; if(w_out) *w_out = (uint16_t)sp->w; if(h_out) *h_out = (uint16_t)sp->h; }
};

// The ovals (and the toggle cells below) are drawn 1:1 in origin's virtual
// canvas: their device striping phase comes from the whole-frame magnify at
// their absolute position. Register the indexed source so the executor swaps
// each draw for a per-phase device-cell variant (resolve_legacy).
using silencer::cppx_ui::LegacyFit;
auto register_legacy = [&](size_t bank, size_t index, uint32_t id,
                           LegacyFit fit = LegacyFit::Cell, int cl = 0,
                           int cr = 0, int ct = 0, int cb = 0){
if(!id || bank >= banks.size() || index >= banks[bank].size()) return;
const std::shared_ptr<Surface> &sp = banks[bank][index];
if(!sp || sp->pixels.empty()) return;
cppxHost->register_legacy(id, sp->pixels.data(), sp->w, sp->h,
                          page_for_bank(bank), kLegacyRenderWidth,
                          kLegacyRenderHeight, fit, cl, cr, ct, cb);
};
// bank 6 — the green oval menu button (idx7 Md / idx28 Sm / idx23 Lg) and
// the LegacyRow list-row plate (idx2; cc roster + agency rows), each as
// origin's 5-frame hover/focus ramp: frame p = sprite (base index + p) at
// brightness 128 + p*2 (button.cpp SpriteIndexForFrame + FrameForPhase).
// Frame 0 is the rest sprite every rest-state golden shows; every frame
// registers so the per-phase device-cell variant swap covers hover states.
auto bake_ramp = [&](size_t bank, size_t base, uint32_t *ids, LegacyFit fit,
                     uint16_t *w_out = nullptr, uint16_t *h_out = nullptr){
for(int p = 0; p < client::ui::ChromeTextures::kOvalPhases; ++p){
const size_t index = base + (size_t)p;
if(bank >= banks.size() || index >= banks[bank].size()) continue;
const std::shared_ptr<Surface> &sp = banks[bank][index];
if(!sp || sp->w < 1 || sp->h < 1 || sp->pixels.empty()) continue;
const Uint8 brightness = (Uint8)(128 + p * 2);
const uint8_t *indices = sp->pixels.data();
std::unique_ptr<Surface> copy;
if(brightness != 128){
copy.reset(game.renderer.CreateSurfaceCopy(sp.get()));
if(!copy) continue;
game.renderer.EffectBrightness(copy.get(), nullptr, brightness);
indices = copy->pixels.data();
}
uint32_t id = cppxHost->bake_chrome_sprite(indices, sp->w, sp->h,
                                           page_for_bank(bank));
if(!id) continue;
cppxHost->register_legacy(id, indices, sp->w, sp->h, page_for_bank(bank),
                          kLegacyRenderWidth, kLegacyRenderHeight, fit);
ids[p] = id;
if(p == 0){
if(w_out) *w_out = (uint16_t)sp->w;
if(h_out) *h_out = (uint16_t)sp->h;
}
}
};
bake_ramp(6, 7, cppxChrome.oval_md, LegacyFit::Cell);
bake_ramp(6, 28, cppxChrome.oval_sm, LegacyFit::Cell);
bake_ramp(6, 23, cppxChrome.oval_lg, LegacyFit::Cell);
bake_ramp(6, 2, cppxChrome.row_plate, LegacyFit::Stretch,
          &cppxChrome.row_plate_w, &cppxChrome.row_plate_h);
// bank 7 — the metal-chrome nine-slice button (idx24; origin never swaps art on focus).
bake(7, 24, cppxChrome.chrome_btn_idle);
// origin draws it nine-sliced in VIRTUAL space (Button Chrome: caps L/R 12,
// T/B 4) before the whole-frame magnify — the executor swaps each nine-slice
// draw for a per-phase, per-size device-cell variant (tiled-band arithmetic).
register_legacy(7, 24, cppxChrome.chrome_btn_idle, LegacyFit::NineSlice,
                12, 12, 4, 4);
// Frame sprites (plain, native size): bank-7 chrome panel + bank-40 dialog
// frames. All drawn 1:1 in virtual space — register for the per-phase swap.
bake(7, 5, cppxChrome.chrome_panel, &cppxChrome.chrome_panel_w, &cppxChrome.chrome_panel_h);
bake(40, 4, cppxChrome.dialog_msg, &cppxChrome.dialog_msg_w, &cppxChrome.dialog_msg_h);
bake(40, 2, cppxChrome.dialog_pw, &cppxChrome.dialog_pw_w, &cppxChrome.dialog_pw_h);
register_legacy(7, 5, cppxChrome.chrome_panel);
register_legacy(40, 4, cppxChrome.dialog_msg);
register_legacy(40, 2, cppxChrome.dialog_pw);
// bank 7 idx 2 — the lobby-connect dialog (origin PackImage(7,2)): frame, soft
// glow, log well, form sub-panel + field/button wells all baked in. Drawn 1:1
// in virtual space — register for the per-phase variant swap.
bake(7, 2, cppxChrome.dialog_connect, &cppxChrome.dialog_connect_w, &cppxChrome.dialog_connect_h);
register_legacy(7, 2, cppxChrome.dialog_connect);
// The 116x24 stipple strip that covers the dialog's baked 52px button wells
// (origin lobby_connect EnsureButtonPatch: panel-coord parity, idx 210/146).
{
constexpr int kPatchW = 116, kPatchH = 24;
constexpr int kPatchX = 84, kPatchY = 246; // panel-relative parity anchors
static uint8_t patch[kPatchW * kPatchH];
for(int y = 0; y < kPatchH; ++y)
for(int x = 0; x < kPatchW; ++x)
patch[y * kPatchW + x] =
    (((kPatchY + y) & 1) == 0 || ((kPatchX + x) & 1) == 0) ? 210 : 146;
uint32_t id = cppxHost->bake_chrome_sprite(patch, kPatchW, kPatchH,
                                           page_for_bank(7));
if(id){
cppxChrome.dialog_btn_patch = id;
cppxHost->register_legacy(id, patch, kPatchW, kPatchH, page_for_bank(7),
                          kLegacyRenderWidth, kLegacyRenderHeight,
                          silencer::cppx_ui::LegacyFit::Cell);
}
}
// Full-bleed backdrops bake at DEVICE resolution through origin's two-hop
// menu compositing (cover/stretch into the virtual canvas, then magnify) so
// the uneven scanline striping matches the golden pixel-for-pixel. Fits per
// origin: menus PackImage(6,0)=cover, Options·Controls PackImageStretch(6,0),
// lobby PackImageStretch(7,1).
auto bake_backdrop = [&](size_t bank, size_t index, bool stretch,
                         uint32_t &id_out){
if(bank >= banks.size() || index >= banks[bank].size()) return;
const std::shared_ptr<Surface> &sp = banks[bank][index];
if(!sp || sp->w < 1 || sp->h < 1 || sp->pixels.empty()) return;
uint32_t id = cppxHost->bake_backdrop_sprite(sp->pixels.data(), sp->w, sp->h,
                                             page_for_bank(bank), stretch,
                                             kLegacyRenderWidth,
                                             kLegacyRenderHeight);
if(id) id_out = id;
};
bake_backdrop(6, 0, false, cppxChrome.starfield);
bake_backdrop(6, 0, true, cppxChrome.starfield_stretched);
bake_backdrop(7, 1, true, cppxChrome.lobby_backdrop);
// Options·Controls frame (bank 7 idx 7, origin PackImageStretch(7,7)): a
// STRETCHED element, so its dither phase only matches the golden through the
// same two-hop chain, evaluated at the element's absolute device footprint.
// Recreate origin's virtual element box (options_controls_screen.cpp: root
// padding = legacy margins L5/R7/T6/B20 scaled into the virtual canvas, panel
// GROWs to fill, height min 420), then bake at a device rect snapped outward
// to logical points that land on integer device px — Yoga rounds layout to
// whole logical px, so only those draw 1:1 (e.g. even points at scale 1.5).
// The Panel absolutely positions a box of exactly this logical rect.
if(7 < banks.size() && 7 < banks[7].size()){
const std::shared_ptr<Surface> &sp = banks[7][7];
if(sp && sp->w > 0 && sp->h > 0 && !sp->pixels.empty()){
float s = std::min(rw / (float)kLegacyRenderWidth, rh / (float)kLegacyRenderHeight);
if(s < 1.0f) s = 1.0f;
const int vw = std::max(1, (int)(rw / s));
const int vh = std::max(1, (int)(rh / s));
auto scale_legacy = [](int v, int cur, int legacy){
return std::max(0, (v * cur + legacy / 2) / legacy);
};
const int bx = scale_legacy(5, vw, kLegacyRenderWidth);
const int by = scale_legacy(6, vh, kLegacyRenderHeight);
const int bw = vw - bx - scale_legacy(7, vw, kLegacyRenderWidth);
const int bh = std::max(420, vh - by - scale_legacy(20, vh, kLegacyRenderHeight));
// Device footprint of the element under the centered whole-frame magnify.
const int scaledW = (int)(vw * s + 0.5f);
const int scaledH = (int)(vh * s + 0.5f);
const int offX = scaledW < rw ? (rw - scaledW) / 2 : 0;
const int offY = scaledH < rh ? (rh - scaledH) / 2 : 0;
const int fx0 = offX + (int)std::ceil(bx * s);
const int fy0 = offY + (int)std::ceil(by * s);
const int fx1 = std::min(offX + (int)std::ceil((bx + bw) * s), rw);
const int fy1 = std::min(offY + (int)std::ceil((by + bh) * s), rh);
// Snap a logical point onto the integer-device grid (bounded search; falls
// back to the nearest logical px when uiScale admits no exact hit).
auto device_integral = [&](int l){
const float d = l * uiScale;
return std::fabs(d - std::floor(d + 0.5f)) < 0.01f;
};
auto snap_lo = [&](int dev){
int l = (int)std::floor(dev / uiScale);
for(int k = 0; k < 8 && l > 0 && !device_integral(l); ++k) --l;
return l;
};
auto snap_hi = [&](int dev){
int l = (int)std::ceil(dev / uiScale);
for(int k = 0; k < 8 && !device_integral(l); ++k) ++l;
return l;
};
const int lx0 = snap_lo(fx0), ly0 = snap_lo(fy0);
const int lx1 = snap_hi(fx1), ly1 = snap_hi(fy1);
const int devX = (int)std::floor(lx0 * uiScale + 0.5f);
const int devY = (int)std::floor(ly0 * uiScale + 0.5f);
const int texW = (int)std::floor(lx1 * uiScale + 0.5f) - devX;
const int texH = (int)std::floor(ly1 * uiScale + 0.5f) - devY;
uint32_t id = cppxHost->bake_element_sprite(
    sp->pixels.data(), sp->w, sp->h, page_for_bank(7), bx, by, bw, bh,
    kLegacyRenderWidth, kLegacyRenderHeight, devX, devY, texW, texH);
if(id){
cppxChrome.chrome_controls = id;
cppxChrome.chrome_controls_x = (float)lx0;
cppxChrome.chrome_controls_y = (float)ly0;
cppxChrome.chrome_controls_w = (float)(lx1 - lx0);
cppxChrome.chrome_controls_h = (float)(ly1 - ly0);
}
}
}
// bank 208 frame 60 — the static SILENCER logo (final reveal frame). Drawn at
// native x1.5 logical = 1:1 in origin's virtual canvas, so it striped like the
// ovals — register every logo frame for the per-phase variant swap too.
bake(208, 60, cppxChrome.logo, &cppxChrome.logo_w, &cppxChrome.logo_h);
register_legacy(208, 60, cppxChrome.logo);
// SIL-94/107: the logo reveal frames (individual textures). [0] is the full
// logo (frame 60); later entries step back through the legacy reveal (the
// latter, mostly-formed half so the wordmark stays legible through the loop).
// main_menu plays a reveal/hold/retract loop over them on the wall clock.
{
const size_t kLogoIdx[client::ui::ChromeTextures::kLogoFrames] =
    {60, 58, 56, 54, 52, 50, 48, 46};
int n = 0;
for(int i = 0; i < client::ui::ChromeTextures::kLogoFrames; ++i){
bake(208, kLogoIdx[i], cppxChrome.logo_frame[i]);
register_legacy(208, kLogoIdx[i], cppxChrome.logo_frame[i]);
if(cppxChrome.logo_frame[i]) n = i + 1; // count contiguous baked frames
}
cppxChrome.logo_frame_count = n;
}
// bank 6 idx12-15 — boolean toggle indicator cells (origin: left = 12 on /
// 13 off, right = 15 on / 14 off).
bake(6, 12, cppxChrome.toggle_l_on, &cppxChrome.toggle_w, &cppxChrome.toggle_h);
bake(6, 13, cppxChrome.toggle_l_off);
bake(6, 14, cppxChrome.toggle_r_off);
bake(6, 15, cppxChrome.toggle_r_on);
register_legacy(6, 12, cppxChrome.toggle_l_on);
register_legacy(6, 13, cppxChrome.toggle_l_off);
register_legacy(6, 14, cppxChrome.toggle_r_off);
register_legacy(6, 15, cppxChrome.toggle_r_on);
// bank 134 '['/']' — the advantage-metadata bracket glyphs (origin borrows the
// bank-134 art because bank 133's bracket cells are dash-shaped). origin's
// AdvantageBracket draws a 4x11 crop (srcX 0 left / 1 right) through
// EffectColor(224) at 1:1 virtual — bake the cropped+tinted cells and
// register them so the per-phase variant swap applies (src-cropped draws are
// not bake-eligible).
{
auto bake_bracket = [&](char ch, int src_x, uint32_t &id_out){
if(134 >= banks.size()) return;
size_t gi = (size_t)(ch - 33);
if(gi >= banks[134].size()) return;
const std::shared_ptr<Surface> &sp = banks[134][gi];
if(!sp || sp->pixels.empty()) return;
Surface * copy = game.renderer.CreateSurfaceCopy(sp.get());
if(!copy) return;
game.renderer.EffectColor(copy, nullptr, 224);
static uint8_t crop[2][4 * 11];
uint8_t *dst = crop[src_x ? 1 : 0];
for(int y = 0; y < 11; ++y)
for(int x = 0; x < 4; ++x){
const int sx = src_x + x, sy = y;
dst[y * 4 + x] = (sx < copy->w && sy < copy->h)
                     ? copy->pixels[(size_t)sy * copy->w + sx] : 0;
}
delete copy;
uint32_t id = cppxHost->bake_chrome_sprite(dst, 4, 11, palette);
if(id){
id_out = id;
cppxHost->register_legacy(id, dst, 4, 11, palette, kLegacyRenderWidth,
                          kLegacyRenderHeight,
                          silencer::cppx_ui::LegacyFit::Cell);
}
};
bake_bracket('[', 0, cppxChrome.bracket_l);
bake_bracket(']', 1, cppxChrome.bracket_r);
}
// bank 181 idx0-4 — the five agency emblems (SIL-102 Character Create detail).
// origin draws them PackImageContain into their element box, so register the
// contain flavor: the executor swaps each draw for a per-phase/per-size
// variant baked through origin's letterbox + magnify arithmetic.
const auto &res = game.world.resources;
for(int i = 0; i < 5; ++i){
bake(181, (size_t)i, cppxChrome.agency_emblem[i],
     i == 0 ? &cppxChrome.agency_emblem_w : nullptr,
     i == 0 ? &cppxChrome.agency_emblem_h : nullptr);
if(cppxChrome.agency_emblem[i] && 181 < banks.size() && (size_t)i < banks[181].size()){
const std::shared_ptr<Surface> &esp = banks[181][(size_t)i];
if(esp && !esp->pixels.empty()){
cppxHost->register_legacy(cppxChrome.agency_emblem[i], esp->pixels.data(),
                          esp->w, esp->h, page_for_bank(181),
                          kLegacyRenderWidth, kLegacyRenderHeight,
                          silencer::cppx_ui::LegacyFit::Contain);
cppxChrome.agency_emblem_ws[i] = (uint16_t)esp->w;
cppxChrome.agency_emblem_hs[i] = (uint16_t)esp->h;
}
if(181 < res.spriteoffsetx.size() && (size_t)i < res.spriteoffsetx[181].size())
cppxChrome.agency_emblem_ox[i] = (int16_t)res.spriteoffsetx[181][i];
if(181 < res.spriteoffsety.size() && (size_t)i < res.spriteoffsety[181].size())
cppxChrome.agency_emblem_oy[i] = (int16_t)res.spriteoffsety[181][i];
}
}
// Staging roster ready checks (bank 7 idx18/19), drawn 1:1 in virtual space —
// register as plain legacy sprites for the per-phase variant swap.
bake(7, 18, cppxChrome.ready_on, &cppxChrome.ready_w, &cppxChrome.ready_h);
bake(7, 19, cppxChrome.ready_off);
register_legacy(7, 18, cppxChrome.ready_on);
register_legacy(7, 19, cppxChrome.ready_off);
// Brightness-64 copies (origin tech_tree_grid non-interactable toggles). The
// tech grid presents on palette page 2 — the brightness index table must be
// page 2's, like the dim text variant.
auto bake_dim = [&](size_t bank, size_t index, uint32_t &id_out){
if(bank >= banks.size() || index >= banks[bank].size()) return;
const std::shared_ptr<Surface> &sp = banks[bank][index];
if(!sp || sp->pixels.empty()) return;
Surface * copy = game.renderer.CreateSurfaceCopy(sp.get());
if(!copy) return;
game.renderer.EffectBrightness(copy, nullptr, 64);
uint32_t id = cppxHost->bake_chrome_sprite(copy->pixels.data(), copy->w,
                                           copy->h, page_for_bank(bank));
if(id){
id_out = id;
cppxHost->register_legacy(id, copy->pixels.data(), copy->w, copy->h,
                          page_for_bank(bank), kLegacyRenderWidth,
                          kLegacyRenderHeight,
                          silencer::cppx_ui::LegacyFit::Cell);
}
delete copy;
};
{
const Uint8 prevPage = game.renderer.palette.CurrentPalette();
if(prevPage != 2) game.renderer.palette.SetPalette(2);
bake_dim(7, 18, cppxChrome.ready_on_dim);
bake_dim(7, 19, cppxChrome.ready_off_dim);
if(prevPage != 2) game.renderer.palette.SetPalette(prevPage);
}
if(7 < res.spriteoffsetx.size() && 18 < res.spriteoffsetx[7].size())
cppxChrome.ready_ox = (int16_t)res.spriteoffsetx[7][18];
if(7 < res.spriteoffsety.size() && 18 < res.spriteoffsety[7].size())
cppxChrome.ready_oy = (int16_t)res.spriteoffsety[7][18];
// In-game HUD sprites (origin ui/hud/*). Authored against the base palette
// page (resources.cpp leaves banks 94-103 at paletteoffset 0). Each entry
// carries native size + sheet offsets so the screen reproduces origin's
// SpriteX/Y(bank,idx) = anchor - offset placement from data.
{
auto hud_bake = [&](size_t bank, size_t index,
                    client::ui::ChromeTextures::Sprite &out,
                    uint8_t effect_color = 0, uint8_t brightness = 128){
if(bank >= banks.size() || index >= banks[bank].size()) return;
const std::shared_ptr<Surface> &sp = banks[bank][index];
if(!sp || sp->w < 1 || sp->h < 1 || sp->pixels.empty()) return;
uint32_t id = 0;
if(effect_color == 0 && brightness == 128){
id = cppxHost->bake_chrome_sprite(sp->pixels.data(), sp->w, sp->h, palette);
}else{
Surface * copy = game.renderer.CreateSurfaceCopy(sp.get());
if(!copy) return;
if(effect_color) game.renderer.EffectColor(copy, nullptr, effect_color);
if(brightness != 128) game.renderer.EffectBrightness(copy, nullptr, brightness);
id = cppxHost->bake_chrome_sprite(copy->pixels.data(), copy->w, copy->h, palette);
delete copy;
}
if(!id) return;
out.id = id;
out.w = (uint16_t)sp->w;
out.h = (uint16_t)sp->h;
if(bank < res.spriteoffsetx.size() && index < res.spriteoffsetx[bank].size())
out.x = (int16_t)res.spriteoffsetx[bank][index];
if(bank < res.spriteoffsety.size() && index < res.spriteoffsety[bank].size())
out.y = (int16_t)res.spriteoffsety[bank][index];
};
auto &hud = cppxChrome.hud;
hud_bake(94, 0, hud.minimap_frame);
hud_bake(94, 1, hud.team_frame);
hud_bake(94, 2, hud.inv_frame);
hud_bake(95, 0, hud.health_bar);
hud_bake(95, 1, hud.shield_bar);
hud_bake(95, 3, hud.health_warn);
hud_bake(95, 4, hud.shield_warn);
hud_bake(95, 5, hud.fuel_mask);
hud_bake(95, 6, hud.fuel_bar);
hud_bake(95, 7, hud.files_bar);
hud_bake(95, 8, hud.fuel_low);
hud_bake(97, 5, hud.poison);
hud_bake(96, 0, hud.weapon_bracket);
for(int i = 0; i < 4; ++i){
hud_bake(96, (size_t)(1 + i), hud.weapon_face[i]);
hud_bake(96, (size_t)(5 + i), hud.weapon_glow[i]);
}
hud_bake(103, 0, hud.team_cap);
hud_bake(103, 1, hud.team_body);
hud_bake(103, 2, hud.secret_full);
hud_bake(103, 3, hud.secret_empty);
hud_bake(103, 3, hud.secret_beaming, /*effect_color=*/224);
for(int i = 0; i < 4; ++i){
hud_bake(103, (size_t)(4 + i), hud.peer_alive[i]);
hud_bake(103, (size_t)(8 + i), hud.peer_dead[i]);
}
for(int i = 0; i < client::ui::ChromeTextures::HudChrome::kInvIcons; ++i){
hud_bake(97, (size_t)i, hud.inv_icon[i]);
hud_bake(97, (size_t)i, hud.inv_icon_dim[i], 0, /*brightness=*/32);
}
hud_bake(102, 0, hud.buy_bg);
hud_bake(102, 1, hud.buy_highlight);
for(int i = 0; i < 9; ++i)
hud_bake(188, (size_t)i, hud.chat_edge[i]);
}

// ---- Bitmap glyph fonts (origin/main text parity) -----------------------
// origin/main renders ALL UI text from the legacy bitmap font banks 132..136
// (renderer.cpp::DrawText), monospace, glyph index = char - ioffset (34 for
// bank 132, else 33). Bake one atlas per cppx face; FaceId -> {bank, advance,
// lineHeight} mirrors origin text.cpp's TextRenderStyle table (640-space native
// metrics; the cppx token font sizes scale them up to the 960 window).
{
using GF = silencer::cppx_ui::GlyphFonts;
struct FaceBake { int face; int bank; float advance; float line_height; };
// advance/lineHeight mirror origin text.cpp ResolveTextRenderStyle EXACTLY.
// Origin tracks the same bank at different advances per style (ScreenTitle/
// Footer/BodySm), so each tracking is its own baked face.
static const FaceBake kFaceBakes[] = {
    {0, 133, 6.f, 11.f},  // Body        — bank 133 advance 6
    {1, 134, 8.f, 15.f},  // Heading     — bank 134 advance 8
    {2, 136, 16.f, 23.f}, // Prompt      — bank 136 advance 16
    {3, 132, 4.f, 7.f},   // Tiny        — bank 132 advance 4
    {4, 135, 11.f, 19.f}, // Title       — bank 135 advance 11
    {5, 135, 12.f, 19.f}, // ScreenTitle — bank 135 tracked wider
    {6, 133, 11.f, 11.f}, // Footer      — bank 133 tracked wide (version line)
    {7, 133, 7.f, 11.f},  // BodySm      — bank 133 tracked +1
    {8, 132, 6.f, 7.f},   // TinyCounter — bank 132 tracked wide (HUD inv counts)
    {9, 134, 10.f, 15.f}, // MessageHeading — bank 134 tracked wide
    {10, 136, 25.f, 23.f},// MessageTitle   — bank 136 tracked wide
    {11, 135, 13.f, 19.f},// MessageSubtitle— bank 135 tracked wide
};
for(const FaceBake & fb : kFaceBakes){
if((size_t)fb.bank >= banks.size()) continue;
const auto & glyphbank = banks[fb.bank];
const int ioffset = (fb.bank == 132) ? 34 : 33; // legacy GlyphOffsetForBank
GF::GlyphSrc src[GF::kGlyphCount] = {};
for(int i = 0; i < GF::kGlyphCount; ++i){
const int ch = GF::kFirstChar + i; // 32..126
const int gi = ch - ioffset;
if(ch == ' ' || gi < 0 || (size_t)gi >= glyphbank.size()) continue; // blank cell
const std::shared_ptr<Surface> & sp = glyphbank[gi];
if(!sp || sp->w < 1 || sp->h < 1 || sp->pixels.empty()) continue;
src[i].indices = sp->pixels.data();
src[i].w = sp->w;
src[i].h = sp->h;
}
// Font banks 132-136 are authored against the base palette page (page_for_bank's
// default arm), so the base `palette` resolves their glyph ramp colors.
cppxHost->build_glyph_face(fb.face, src, GF::kGlyphCount, palette, fb.advance, fb.line_height);
}

// Exact-color variants: bake origin's RENDERED text pixels per (face, token
// color) by running the legacy Effect* pipeline on indexed copies of the
// glyph art — the same transform origin's DrawText applies — and registering
// the result under the cppx token color screens author with. Text drawn in a
// registered color reproduces origin's palette-ramp pixels verbatim (opaque,
// no alpha AA); other colors keep the coverage-tint path.
{
using GF = silencer::cppx_ui::GlyphFonts;
enum class Fx { Raw, Color, Ramp };
struct VariantBake {
int face; int bank;
float advance; float line_height;
Fx fx; Uint8 fx_color; Uint8 brightness;
Uint8 page; // presentation palette page: the Effect* index tables AND the
            // final RGB resolve through it (lobby cluster presents on page 2;
            // page 0's tables map e.g. brightness-160 green and EffectColor-189
            // amber to measurably different ramps than the lobby golden)
::ui::Color key;
Uint8 alpha = 255; // <255: premultiplied translucent bake (origin DrawAlphaed)
};
static const VariantBake kVariantBakes[] = {
    // The authored art IS the standard green (24,124,20 core + dark ramp).
    {0, 133, 6.f, 11.f, Fx::Raw, 0, 128, 0, silencer::tokens::kTextBody},
    {1, 134, 8.f, 15.f, Fx::Raw, 0, 128, 0, silencer::tokens::kTextBody},
    {4, 135, 11.f, 19.f, Fx::Raw, 0, 128, 0, silencer::tokens::kTextBody},
    {5, 135, 12.f, 19.f, Fx::Raw, 0, 128, 0, silencer::tokens::kTextBody},
    {6, 133, 11.f, 11.f, Fx::Raw, 0, 128, 0, silencer::tokens::kTextBody},
    {7, 133, 7.f, 11.f, Fx::Raw, 0, 128, 0, silencer::tokens::kTextBody},
    // mission_summary "+ N XP": origin TextSize::Prompt (bank 136) default green.
    {2, 136, 16.f, 23.f, Fx::Raw, 0, 128, 0, silencer::tokens::kTextBody},
    // cc detail prose: origin LegacyPalette(129, 160, ramp). The Title-face
    // entry covers the staging title-bar map name (lobby_chrome mapText).
    {0, 133, 6.f, 11.f, Fx::Ramp, 129, 160, 0, silencer::tokens::kTextProse},
    {1, 134, 8.f, 15.f, Fx::Ramp, 129, 160, 0, silencer::tokens::kTextProse},
    {7, 133, 7.f, 11.f, Fx::Ramp, 129, 160, 0, silencer::tokens::kTextProse},
    {4, 135, 11.f, 19.f, Fx::Ramp, 129, 160, 0, silencer::tokens::kTextProse},
    // lobby header brand + version: LegacyPalette(152) / LegacyPalette(189).
    {4, 135, 11.f, 19.f, Fx::Color, 152, 128, 0, silencer::tokens::kTextBrand},
    {0, 133, 6.f, 11.f, Fx::Color, 189, 128, 2, silencer::tokens::kTextVersion},
    // agent display names: LegacyPalette(200).
    {1, 134, 8.f, 15.f, Fx::Color, 200, 128, 0, silencer::tokens::kTextAgentName},
    // lobby presence group headers: LegacyPalette(0, 160).
    {0, 133, 6.f, 11.f, Fx::Raw, 0, 160, 2, silencer::tokens::kTextPresenceHeader},
    // staging roster level badges: Tiny LegacyPalette(170).
    {3, 132, 4.f, 7.f, Fx::Color, 170, 128, 0, silencer::tokens::kTextRosterLevel},
    // tech grid: non-interactable rows (brightness 64) + slots-left readout
    // (LegacyPalette(129, 144, ramp)).
    {0, 133, 6.f, 11.f, Fx::Raw, 0, 64, 2, silencer::tokens::kTextTechDim},
    {0, 133, 6.f, 11.f, Fx::Ramp, 129, 144, 2, silencer::tokens::kTextTechSlots},
    // ---- in-game HUD (presentation palette page 0, INGAME_SPECS.md) ----
    {3, 132, 4.f, 7.f, Fx::Raw, 0, 128, 0, silencer::tokens::kTextHudDefault},
    {3, 132, 4.f, 7.f, Fx::Color, 202, 128, 0, silencer::tokens::kTextHudCredits},
    {3, 132, 4.f, 7.f, Fx::Color, 161, 128, 0, silencer::tokens::kTextHudHealth},
    {3, 132, 4.f, 7.f, Fx::Raw, 0, 32, 0, silencer::tokens::kTextHudDim32},
    {8, 132, 6.f, 7.f, Fx::Raw, 0, 128, 0, silencer::tokens::kTextHudDefault},
    {0, 133, 6.f, 11.f, Fx::Raw, 0, 128, 0, silencer::tokens::kTextHudDefault},
    {0, 133, 6.f, 11.f, Fx::Raw, 0, 136, 0, silencer::tokens::kTextHudBright},
    {1, 134, 8.f, 15.f, Fx::Raw, 0, 128, 0, silencer::tokens::kTextHudDefault},
    {1, 134, 8.f, 15.f, Fx::Raw, 0, 64, 0, silencer::tokens::kTextHudDim64},
    {5, 135, 12.f, 19.f, Fx::Color, 202, 128, 0, silencer::tokens::kTextHudCredits},
    {2, 136, 16.f, 23.f, Fx::Color, 202, 128, 0, silencer::tokens::kTextHudCredits},
};
const Uint8 prevPage = game.renderer.palette.CurrentPalette();
for(const VariantBake & vb : kVariantBakes){
if((size_t)vb.bank >= banks.size()) continue;
const auto & glyphbank = banks[vb.bank];
const int ioffset = (vb.bank == 132) ? 34 : 33;
const SDL_Color * vpal = game.renderer.palette.colors[vb.page];
if(game.renderer.palette.CurrentPalette() != vb.page)
game.renderer.palette.SetPalette(vb.page);
std::vector<std::unique_ptr<Surface>> fxcopies;
GF::GlyphSrc src[GF::kGlyphCount] = {};
for(int i = 0; i < GF::kGlyphCount; ++i){
const int ch = GF::kFirstChar + i;
const int gi = ch - ioffset;
if(ch == ' ' || gi < 0 || (size_t)gi >= glyphbank.size()) continue;
const std::shared_ptr<Surface> & sp = glyphbank[gi];
if(!sp || sp->w < 1 || sp->h < 1 || sp->pixels.empty()) continue;
if(vb.fx == Fx::Raw && vb.brightness == 128){
src[i].indices = sp->pixels.data();
}else{
Surface * copy = game.renderer.CreateSurfaceCopy(sp.get());
if(!copy) continue;
if(vb.fx == Fx::Color) game.renderer.EffectColor(copy, nullptr, vb.fx_color);
else if(vb.fx == Fx::Ramp) game.renderer.EffectRampColor(copy, nullptr, vb.fx_color);
if(vb.brightness != 128) game.renderer.EffectBrightness(copy, nullptr, vb.brightness);
fxcopies.emplace_back(copy);
src[i].indices = copy->pixels.data();
}
src[i].w = sp->w;
src[i].h = sp->h;
}
cppxHost->build_glyph_color_face(vb.face, vb.key.r, vb.key.g, vb.key.b, src,
                                 GF::kGlyphCount, vpal, vb.advance,
                                 vb.line_height, kLegacyRenderWidth,
                                 kLegacyRenderHeight, vb.alpha);
}
// Pulse-driven HUD text (page 0): center-message reveal (Title face, color
// 208, brightness 64..160 — text + shadow ramps) and buy-row selection
// (Heading face, color 0, brightness 128..136). Baked under the generic
// tokens::hud_text_key(color, brightness) keys.
{
const Uint8 prevPage = game.renderer.palette.CurrentPalette();
if(prevPage != 0) game.renderer.palette.SetPalette(0);
const SDL_Color * vpal = game.renderer.palette.colors[0];
auto bake_pulse = [&](int face, int bank, float advance, float line_height,
                      Uint8 color, Uint8 brightness){
if((size_t)bank >= banks.size()) return;
const auto & glyphbank = banks[bank];
const int ioffset = (bank == 132) ? 34 : 33;
std::vector<std::unique_ptr<Surface>> fxcopies;
GF::GlyphSrc src[GF::kGlyphCount] = {};
for(int i = 0; i < GF::kGlyphCount; ++i){
const int ch = GF::kFirstChar + i;
const int gi = ch - ioffset;
if(ch == ' ' || gi < 0 || (size_t)gi >= glyphbank.size()) continue;
const std::shared_ptr<Surface> & sp = glyphbank[gi];
if(!sp || sp->w < 1 || sp->h < 1 || sp->pixels.empty()) continue;
Surface * copy = game.renderer.CreateSurfaceCopy(sp.get());
if(!copy) continue;
if(color) game.renderer.EffectColor(copy, nullptr, color);
if(brightness != 128) game.renderer.EffectBrightness(copy, nullptr, brightness);
fxcopies.emplace_back(copy);
src[i].indices = copy->pixels.data();
src[i].w = sp->w;
src[i].h = sp->h;
}
const ::ui::Color key = silencer::tokens::hud_text_key(color, brightness);
cppxHost->build_glyph_color_face(face, key.r, key.g, key.b, src,
                                 GF::kGlyphCount, vpal, advance, line_height,
                                 kLegacyRenderWidth, kLegacyRenderHeight);
};
for(int b = 64; b <= 160; b += 2)
bake_pulse(4, 135, 11.f, 19.f, 208, (Uint8)b);
// Buy/tech selected-row pulse (Heading face, color 0, 129..136).
for(int b = 129; b <= 136; ++b)
bake_pulse(1, 134, 8.f, 15.f, 0, (Uint8)b);
// Oval-button hover/focus label ramp (Title face — the bank-135 oval label —
// color 0, brightness 130/132/134/136 = origin FrameForPhase 128 + p*2; the
// bank-134 List-row labels reuse the buy/tech entries above).
for(int b = 130; b <= 136; b += 2)
bake_pulse(4, 135, 11.f, 19.f, 0, (Uint8)b);
// Ammo counter: origin draws it DrawAlphaed — each glyph pixel is the
// palette ALPHA-TABLE mix of the glyph index over the pixel under it. The
// counter sits on the dash well's flat interior, so baking the face through
// Alpha(glyph, well_index) reproduces origin's exact quantized mix.
{
Uint8 well = 0;
if(94 < banks.size() && 0 < banks[94].size() && banks[94][0] &&
   !banks[94][0]->pixels.empty()){
const Surface * dash = banks[94][0].get();
int ox = 0, oy = 0;
if(94 < game.world.resources.spriteoffsetx.size() &&
   !game.world.resources.spriteoffsetx[94].empty()){
ox = game.world.resources.spriteoffsetx[94][0];
oy = game.world.resources.spriteoffsety[94][0];
}
const int sx = 124 + ox, sy = 462 + oy; // inside the counter well
if(sx >= 0 && sy >= 0 && sx < dash->w && sy < dash->h)
well = dash->pixels[(size_t)sy * dash->w + sx];
}
const auto & glyphbank = banks[135];
GF::GlyphSrc src[GF::kGlyphCount] = {};
std::vector<std::vector<uint8_t>> mixed;
for(int i = 0; i < GF::kGlyphCount; ++i){
const int ch = GF::kFirstChar + i;
const int gi = ch - 33;
if(ch == ' ' || gi < 0 || (size_t)gi >= glyphbank.size()) continue;
const std::shared_ptr<Surface> & sp = glyphbank[gi];
if(!sp || sp->w < 1 || sp->h < 1 || sp->pixels.empty()) continue;
mixed.emplace_back(sp->pixels);
for(uint8_t & px : mixed.back())
if(px) px = game.renderer.palette.Alpha(px, well);
src[i].indices = mixed.back().data();
src[i].w = sp->w;
src[i].h = sp->h;
}
const ::ui::Color key = silencer::tokens::kTextHudAmmo;
cppxHost->build_glyph_color_face(5, key.r, key.g, key.b, src, GF::kGlyphCount,
                                 vpal, 12.f, 19.f, kLegacyRenderWidth,
                                 kLegacyRenderHeight);
}
if(prevPage != 0) game.renderer.palette.SetPalette(prevPage);
}
if(game.renderer.palette.CurrentPalette() != prevPage)
game.renderer.palette.SetPalette(prevPage);
}
}
}

uint32_t GameUiPipeline::EnsureHudRampVariant(uint8_t bank, uint16_t index,
                                              uint8_t rampColor,
                                              uint8_t rampPlus,
                                              uint8_t brightness) {
if(!cppxHost) return 0;
const uint64_t key = ((uint64_t)brightness << 32) | ((uint64_t)bank << 24) |
                     ((uint64_t)index << 16) | ((uint64_t)rampColor << 8) |
                     rampPlus;
auto it = hudRampVariants_.find(key);
if(it != hudRampVariants_.end()) return it->second;
const auto &banks = game.world.resources.spritebank;
if(bank >= banks.size() || index >= banks[bank].size()) return 0;
const std::shared_ptr<Surface> &sp = banks[bank][index];
if(!sp || sp->pixels.empty()) return 0;
const SDL_Color *palette = game.renderer.palette.GetColors();
if(!palette) return 0;
uint32_t id = 0;
if(rampColor == 0 && brightness == 128){
id = cppxHost->bake_chrome_sprite(sp->pixels.data(), sp->w, sp->h, palette);
}else{
Surface * copy = game.renderer.CreateSurfaceCopy(sp.get());
if(!copy) return 0;
if(rampColor){
if(rampPlus) game.renderer.EffectRampColorPlus(copy, nullptr, rampColor, rampPlus);
else game.renderer.EffectRampColor(copy, nullptr, rampColor);
}
if(brightness != 128) game.renderer.EffectBrightness(copy, nullptr, brightness);
id = cppxHost->bake_chrome_sprite(copy->pixels.data(), copy->w, copy->h, palette);
delete copy;
}
hudRampVariants_[key] = id;
return id;
}

client::ui::ChromeTextures::Sprite
GameUiPipeline::EnsureHudTeamEmblem(uint8_t agency, uint8_t color) {
client::ui::ChromeTextures::Sprite out;
if(!cppxHost) return out;
const uint16_t key = ((uint16_t)agency << 8) | color;
auto it = hudEmblems_.find(key);
if(it != hudEmblems_.end()) return it->second;
const auto &banks = game.world.resources.spritebank;
if(181 >= banks.size() || agency >= banks[181].size()) return out;
const std::shared_ptr<Surface> &sp = banks[181][agency];
if(!sp || sp->pixels.empty()) return out;
const SDL_Color *palette = game.renderer.palette.GetColors();
if(!palette) return out;
// origin TeamEmblem custom draw: team-colorize + outline the VISIBLE-pixel
// border (color 17), then DrawScaled factor 2 — a pixel-SKIPPING downsample
// to half size. Bake the skipped pixels so the texture draws 1:1.
Surface * copy = game.renderer.CreateSurfaceCopy(sp.get());
if(!copy) return out;
game.renderer.EffectTeamColor(copy, nullptr, color, false, true);
for(int py = 0; py < sp->h; ++py)
for(int px = 0; px < sp->w; ++px){
if(sp->pixels[(size_t)py * sp->w + px]) continue;
const bool edge =
    (px > 0 && sp->pixels[(size_t)py * sp->w + px - 1]) ||
    (px < sp->w - 1 && sp->pixels[(size_t)py * sp->w + px + 1]) ||
    (py > 0 && sp->pixels[(size_t)(py - 1) * sp->w + px]) ||
    (py < sp->h - 1 && sp->pixels[(size_t)(py + 1) * sp->w + px]);
if(edge) copy->pixels[(size_t)py * copy->w + px] = 17;
}
const int hw = (sp->w + 1) / 2, hh = (sp->h + 1) / 2;
std::vector<uint8_t> half((size_t)hw * hh);
for(int py = 0, y2 = 0; py < sp->h; py += 2, ++y2)
for(int px = 0, x2 = 0; px < sp->w; px += 2, ++x2)
half[(size_t)y2 * hw + x2] = copy->pixels[(size_t)py * copy->w + px];
delete copy;
uint32_t id = cppxHost->bake_chrome_sprite(half.data(), hw, hh, palette);
if(id){
out.id = id;
out.w = (uint16_t)hw;
out.h = (uint16_t)hh;
}
hudEmblems_[key] = out;
return out;
}

void GameUiPipeline::RenderCppxClientUiFrame(Surface& surface) {
cppxUiRgba = nullptr;
#ifdef SILENCER_CPPX_FONT_DIR
// SIL-94: snapshot the per-frame wall clock ONCE per render frame. The
// frame-provider lambda below runs once per visible SCREEN LAYER (base +
// overlays), so computing the delta there collapsed it to 0 on every layer
// after the first — animation hooks never advanced.
{
const uint32_t nowMs = SDL_GetTicks();
cppxClock_ = {nowMs, cppxLastUiTicks_ ? (nowMs - cppxLastUiTicks_) : 0u};
cppxLastUiTicks_ = nowMs;
}
// Native window-pixel resolution so the UI composite maps 1:1 over the
// upscaled world frame (matches the SIL-11 demo). Headless / no window falls
// back to the surface size so the path still runs (UploadUiFrame is a no-op
// on devices without a UI composite pass).
int rw = surface.w;
int rh = surface.h;
SDL_Window * win = game.gameRenderer.GetWindow();
if(win){
int pw = 0;
int ph = 0;
if(SDL_GetWindowSizeInPixels(win, &pw, &ph) && pw > 0 && ph > 0){
rw = pw;
rh = ph;
}
}
if(rw < 1 || rh < 1) return;
// Responsive logical canvas: screens are authored against a height-pinned 720
// space (width = aspect*720), and PipelineHost::render scales that to the
// physical rw x rh surface (fonts re-rasterized crisply, sprites scaled) so
// the UI tracks the window like origin/main. Scale may drop BELOW 1: the
// 720-space metrics are origin's 480-virtual metrics x1.5, so a 480-high
// window at scale 2/3 reproduces origin's native 640x480 layout exactly —
// clamping at 1 rendered the UI 1.5x oversized there (e2e 21/53 overflow).
// Floor 480/720 mirrors origin (its virtual canvas never exceeds the window).
const float kLogicalH = 720.0f;
float cppxScale = static_cast<float>(rh) / kLogicalH;
const float kMinScale = 480.0f / 720.0f;
if(cppxScale < kMinScale) cppxScale = kMinScale;
const float cppxLogicalW = static_cast<float>(rw) / cppxScale;
const float cppxLogicalH = static_cast<float>(rh) / cppxScale;
cppxCanvasW_ = cppxLogicalW;
cppxCanvasH_ = cppxLogicalH;

if(!cppxReactInitialized){
react_init_runtime();
cppxReactInitialized = true;
}
if(!cppxHost){
cppxHost = std::make_unique<silencer::cppx_ui::PipelineHost>();
}
if(!cppxHost->ensure(rw, rh, SILENCER_CPPX_FONT_DIR)) return;

// SIL-87: bake the curated legacy chrome sprites into texture_ids once per
// renderer lifetime (re-baked after a resize reset, never per frame). The
// composition root is the only place that may read the indexed spritebank +
// palette; the ids flow to screens through the ChromeTexturesProvider below.
if(cppxHost->chrome_needs_bake()){
BakeChromeTextures(rw, rh, cppxScale);
cppxHost->mark_chrome_baked();
}

if(!cppxAppRootPushed){
// The global FrameProvider chain (doc §5), outermost-first. ServerProvider
// carries the live Game handle; SessionProvider publishes the projected phase
// to AppRoot's reconciler. Theme/App/Settings/KeyMap/Updater join as their
// hooks land.
cppxHost->pipeline().set_frame_provider([this](::ui::UiElement child){
// Assemble the session model fresh each frame: read projection + intent
// closures over the public Game command seam (no friend, no handle leak).
client::ui::Session session = {};
session.phase = CurrentSessionPhase();
session.authenticated = (game.world.lobby.state == Lobby::AUTHENTICATED);
session.paused = game.paused;
session.is_live_multiplayer = game.IsLiveMultiplayer();
session.current_game_id = game.currentlobbygameid;
session.play_online = [this]{ game.GoToState(GameState::LOBBYCONNECT); };
session.start_tutorial = [this]{ game.GoToState(GameState::SINGLEPLAYERGAME); };
session.open_character_create = [this]{ game.GoToState(GameState::CREATECHARACTER); };
session.leave_match = [this]{ game.LeaveJoinedGame(); };
session.leave_to_menu = [this]{ game.GoToState(GameState::MAINMENU); };
session.set_paused = [this](bool p){ game.paused = p; };

// Updater model: poll the self-updater's main-thread-safe atomics + map its
// state to the UI phase; intents route to the public ::Updater methods.
client::ui::UpdaterModel updater = {};
switch(game.updater.GetState()){
case ::Updater::IDLE: updater.phase = client::ui::UpdaterPhase::Idle; break;
case ::Updater::PROMPTING: updater.phase = client::ui::UpdaterPhase::Prompting; break;
case ::Updater::DOWNLOADING: updater.phase = client::ui::UpdaterPhase::Downloading; break;
case ::Updater::VERIFYING: updater.phase = client::ui::UpdaterPhase::Verifying; break;
case ::Updater::STAGING: updater.phase = client::ui::UpdaterPhase::Staging; break;
case ::Updater::FAILED: updater.phase = client::ui::UpdaterPhase::Failed; break;
case ::Updater::DONE: updater.phase = client::ui::UpdaterPhase::Done; break;
}
updater.progress = game.updater.GetProgress();
updater.error = game.updater.GetErrorMessage();
updater.retry_count = game.updater.GetRetryCount();
updater.download_url = game.updater.GetDownloadURL();
updater.can_retry = (game.updater.GetState() == ::Updater::FAILED);
updater.consent = [this]{ game.updater.Consent(); };
updater.cancel = [this]{ game.updater.Cancel(); };
updater.retry = [this]{ game.updater.Retry(); };
updater.open_download_page = [this]{
SDL_OpenURL(game.updater.GetDownloadURL().c_str());
};

// Settings model (doc §6): read the live Config + install live-apply preview
// closures over the public subsystems (SIL-6 LOCKED: live-apply preview). dirty
// = live Config diverges from the last-committed snapshot.
client::ui::Settings settings = {};
{
Config & cfg = Config::GetInstance();
if(!committedSettingsInit_){
committedSettings_ = {cfg.music, cfg.musicvolume, cfg.fullscreen, cfg.scalefilter};
committedSettingsInit_ = true;
}
settings.music = cfg.music;
settings.music_volume = cfg.musicvolume;
settings.fullscreen = cfg.fullscreen;
settings.smooth_scaling = cfg.scalefilter;
settings.dirty = (cfg.music != committedSettings_.music
|| cfg.musicvolume != committedSettings_.musicvolume
|| cfg.fullscreen != committedSettings_.fullscreen
|| cfg.scalefilter != committedSettings_.scalefilter);
settings.set_music = [this](bool on){
Config::GetInstance().music = on;
if(on) Audio::GetInstance().ResumeMusic(); else Audio::GetInstance().PauseMusic();
};
settings.set_music_volume = [this](uint8_t v){
Config::GetInstance().musicvolume = v;
Audio::GetInstance().SetMusicVolume(v);
};
settings.set_fullscreen = [this](bool on){
Config::GetInstance().fullscreen = on;
if(SDL_Window * w = game.gameRenderer.GetWindow()) SDL_SetWindowFullscreen(w, on);
};
settings.set_smooth_scaling = [this](bool on){
Config::GetInstance().scalefilter = on;
if(RenderDevice * d = game.gameRenderer.GetRenderDevice()) d->SetScaleFilter(on);
};
settings.commit = [this]{
Config & c = Config::GetInstance();
c.Save();
committedSettings_ = {c.music, c.musicvolume, c.fullscreen, c.scalefilter};
};
settings.revert = [this]{
Config & c = Config::GetInstance();
c.Load();
if(SDL_Window * w = game.gameRenderer.GetWindow()) SDL_SetWindowFullscreen(w, c.fullscreen);
if(RenderDevice * d = game.gameRenderer.GetRenderDevice()) d->SetScaleFilter(c.scalefilter);
Audio::GetInstance().SetMusicVolume(c.musicvolume);
if(c.music) Audio::GetInstance().ResumeMusic(); else Audio::GetInstance().PauseMusic();
committedSettings_ = {c.music, c.musicvolume, c.fullscreen, c.scalefilter};
};
}

// KeyMap model (doc §6/§7b): rows-of-combos read view (labels rendered here,
// since only the comp root has the gamepad type) + the six mutation intents.
// Binding mutations fork-if-builtin + enforce caps, all drained after render.
client::ui::KeyMap key_map = {};
{
const KeyMap & km = game.GetKeyMap();
SDL_Gamepad * pad = game.GetGamepad();
SDL_GamepadType padType = pad ? SDL_GetGamepadType(pad) : SDL_GAMEPAD_TYPE_UNKNOWN;
key_map.actions.reserve((int)Action::Count);
for(int i = 0; i < (int)Action::Count; ++i){
Action a = ACTION_TABLE[i].action;
client::ui::KeyMapAction outA;
outA.action = a;
outA.label = GetActionInfo(a).label;
const ActionBindings & ab = km.Get(a);
for(const Binding & b : ab.bindings){
client::ui::KeyMapCombo combo;
for(const BindingKey & bk : b.keys){
combo.chips.push_back(ChipFromBindingKey(bk, padType));
}
outA.combos.push_back(std::move(combo));
}
key_map.actions.push_back(std::move(outA));
}
const std::string activeProfile = Config::GetInstance().active_keybind_profile;
key_map.preset_label = !km.label.empty() ? km.label
: (!km.name.empty() ? km.name : activeProfile);
key_map.preset_is_builtin = KeybindProfileIsBuiltin(activeProfile);
key_map.dirty = keymapDirty_;
key_map.add_combo = [this](Action a, std::vector<client::ui::KeyMapChip> chips){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, a, chips = std::move(chips)]() mutable {
Binding b;
if(!ChipsToBinding(chips, b)) return;
ForkActiveProfileIfBuiltin(game.GetKeyMap());
ActionBindings & ab = game.GetKeyMap().Get(a);
if((int)ab.bindings.size() >= COMBO_CAP) return;
ab.bindings.push_back(std::move(b));
keymapDirty_ = true;
});
};
key_map.replace_combo = [this](Action a, int idx, std::vector<client::ui::KeyMapChip> chips){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, a, idx, chips = std::move(chips)]() mutable {
Binding b;
if(!ChipsToBinding(chips, b)) return;
ForkActiveProfileIfBuiltin(game.GetKeyMap());
ActionBindings & ab = game.GetKeyMap().Get(a);
if(idx < 0 || idx >= (int)ab.bindings.size()) return;
ab.bindings[idx] = std::move(b);
keymapDirty_ = true;
});
};
key_map.remove_combo = [this](Action a, int idx){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, a, idx](){
ForkActiveProfileIfBuiltin(game.GetKeyMap());
ActionBindings & ab = game.GetKeyMap().Get(a);
if(idx < 0 || idx >= (int)ab.bindings.size()) return;
ab.bindings.erase(ab.bindings.begin() + idx);
keymapDirty_ = true;
});
};
key_map.clear_action = [this](Action a){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, a](){
ForkActiveProfileIfBuiltin(game.GetKeyMap());
game.GetKeyMap().Get(a).bindings.clear();
keymapDirty_ = true;
});
};
key_map.cycle_preset = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
CycleKeybindPreset(game.GetKeyMap());
keymapDirty_ = false;
});
};
key_map.commit = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
const std::string active = Config::GetInstance().active_keybind_profile;
if(!KeybindProfileIsBuiltin(active)) game.GetKeyMap().SaveFile(WritableProfilePath(active));
Config::GetInstance().Save();
keymapDirty_ = false;
});
};
key_map.revert = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
LoadActiveKeymap(game.GetKeyMap());
Config::GetInstance().Load();
keymapDirty_ = false;
});
};
}

// Keybind capture model (doc §7b): the comp-root-owned capture state, with the
// pending chord labeled, + the begin/cancel/confirm closures. Published
// globally (the raw multi-device edge source is global) though only
// OptionsControls consumes it.
client::ui::KeybindCapture capture = {};
{
SDL_Gamepad * pad = game.GetGamepad();
SDL_GamepadType padType = pad ? SDL_GetGamepadType(pad) : SDL_GAMEPAD_TYPE_UNKNOWN;
capture.capturing = keybindCapture_.active;
capture.target_action = keybindCapture_.targetAction;
capture.target_action_label = GetActionInfo(keybindCapture_.targetAction).label;
capture.target_combo_index = keybindCapture_.targetComboIndex;
for(const BindingKey & bk : keybindCapture_.pending){
capture.pending_chips.push_back(ChipFromBindingKey(bk, padType));
}
capture.begin_capture = [this](Action a, int idx){ BeginKeybindCapture(a, idx); };
capture.cancel_capture = [this](){ CancelKeybindCapture(); };
capture.confirm_chord = [this](){ ConfirmKeybindChord(); };
}

// Lobby model (doc §5/§6): the per-tick snapshot — captured under LockMutex
// before this build, so no build-time lock — plus the queued intents over the
// public lobby seam. Consumed by LobbyConnect (connect/auth) and MissionSummary
// (progression). The connect *orchestration* lives on the game tick
// (LobbyConnectFlow); this carries only the credential-submit/cancel + upgrade/
// finish intents. SIL-20.
client::ui::LobbyProviderValue lobby = {};
lobby.snapshot = lobbySnapshot_;
lobby.connect = [this](const std::string & user, const std::string & pass){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, user, pass](){
Lobby & lb = game.world.lobby;
lb.LockMutex();
if(lb.state == Lobby::AUTHENTICATING){
lb.SetLocalUsername(user.c_str());
lb.SendCredentials(user.c_str(), pass.c_str());
lb.state = Lobby::AUTHSENT;
}
lb.UnlockMutex();
});
};
lobby.cancel = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
Lobby & lb = game.world.lobby;
lb.LockMutex();
lb.Disconnect();
lb.UnlockMutex();
game.GoToState(GameState::MAINMENU);
});
};
lobby.upgrade = [this](int index){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, index](){
if(index < 0 || index >= 6) return;
static const Lobby::StatID kIds[6] = {Lobby::STAT_ENDURANCE, Lobby::STAT_SHIELD,
Lobby::STAT_JETPACK, Lobby::STAT_TECHSLOTS, Lobby::STAT_HACKING, Lobby::STAT_CONTACTS};
Lobby & lb = game.world.lobby;
lb.LockMutex();
User * user = lb.GetUserInfo(lb.accountid);
if(user) lb.UpgradeStat(user->selectedcharid, user->statsagency, kIds[index]);
lb.UnlockMutex();
});
};
lobby.finish = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
Lobby & lb = game.world.lobby;
lb.LockMutex();
bool authed = (lb.state == Lobby::AUTHENTICATED);
if(authed) lb.JoinChannel(lb.lastchannel);
lb.UnlockMutex();
game.GoToState(authed ? GameState::LOBBY : GameState::MAINMENU);
});
};
lobby.create_character = [this](const std::string & alias, int agency){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, alias, agency](){
if(agency < 0 || agency > 4) return;
Lobby & lb = game.world.lobby;
lb.LockMutex();
lb.charactersreceived = false;
lb.CreateCharacter(alias.c_str(), (Uint8)agency);
lb.UnlockMutex();
// Routing waits for the roster to grow (CREATECHARACTER tick reconcile).
});
};
lobby.select_character = [this](int index){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, index](){
Lobby & lb = game.world.lobby;
bool selected = false;
lb.LockMutex();
if(index >= 0 && index < (int)lb.characters.size()){
lb.SelectCharacter(lb.characters[index].id);
selected = true;
}
lb.UnlockMutex();
if(selected) game.GoToState(GameState::LOBBY);
});
};
lobby.send_chat = [this](const std::string & message){
if(message.empty()) return;
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, message](){
Lobby & lb = game.world.lobby;
lb.LockMutex();
lb.SendChat(lb.channel, message.c_str());
lb.UnlockMutex();
});
};
// Games browser intents (doc §6): id-based join/spectate/create over the public
// seam, queued. The LOBBY-tick game-join pump drives connect → staging and the
// create finalization (auto-join on creategamestatus==1 && creategameclicked).
lobby.join_game = [this](uint32_t id, const std::string & password){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, id, password](){
Lobby & lb = game.world.lobby;
lb.LockMutex();
LobbyGame * lg = lb.GetGameById(id);
lb.UnlockMutex();
if(!lg) return;
game.currentlobbygameid = id;
char * pw = password.empty() ? nullptr : const_cast<char*>(password.c_str());
game.JoinGame(*lg, pw);
});
};
lobby.spectate_game = [this](uint32_t id){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, id](){
Lobby & lb = game.world.lobby;
lb.LockMutex();
LobbyGame * lg = lb.GetGameById(id);
lb.UnlockMutex();
if(!lg) return;
game.currentlobbygameid = id;
game.SpectateGame(*lg, nullptr);
});
};
lobby.create_game = [this](const client::ui::CreateGameRequest & req){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, req](){
if(req.map.empty()) return;
MapDownloader & md = game.gameSession.MapDownloaderRef();
std::string path = md.FindMap(req.map.c_str());
if(path.empty()) return;
unsigned char maphash[20] = {0};
md.CalculateMapHash(path.c_str(), &maphash);
Lobby & lb = game.world.lobby;
lb.LockMutex();
lb.CreateGame(req.name.c_str(), req.map.c_str(), maphash,
req.password.empty() ? nullptr : req.password.c_str(),
req.security, req.min_level, req.max_level,
req.max_players, req.max_teams, req.spectatable);
lb.UnlockMutex();
// The pump auto-joins our own created game once the lobby confirms it.
game.creategameclicked = true;
});
};
// Staging room intents (doc §6/§7a): ready/change-team/leave over the public
// World seam. Host-ready is guarded hook-side (ishost && !AllPeersDownloadedMap).
lobby.send_ready = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
Peer * lp = game.world.GetPeer(game.world.GetLocalPeerId());
bool host = lp && lp->ishost;
if(!host || game.world.AllPeersDownloadedMap()) game.world.SendReady();
});
};
lobby.change_team = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
game.world.ChangeTeam();
});
};
lobby.set_tech = [this](uint32_t choices){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, choices](){
game.world.SetTech(choices);
});
};
lobby.leave_game = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
game.LeaveJoinedGame();
});
};

// In-match model (doc §5/§6): the per-tick world-session snapshot + the queued
// intents over the public Game/World seam. Consumed by InGameScreen
// (use_player_status / use_match). Snapshot is empty outside the in-match phases.
client::ui::WorldSessionValue world_session = {};
world_session.snapshot = worldSessionSnapshot_;
world_session.select_inventory_slot = [this](int slot){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, slot](){
if(slot < 0 || slot > 3) return;
Player * p = game.world.GetPeerPlayer(game.world.GetLocalPeerId());
if(p) p->currentinventoryitem = (Uint8)slot;
});
};
world_session.confirm_quit = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
// Quit to the menu (the full pause/leave→summary flow lands with PauseScreen).
game.GoToState(GameState::MAINMENU);
});
};
// SIL-21 (5/n) in-match overlay intents (doc §6/§7a): buy/tech station, chat
// compose, and the scoreboard toggle — queued over the public World/Player seam
// (drained after render, FADEOUT-gated). The viewed agent matches the snapshot.
world_session.buytech_select = [this](int index){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, index](){
Player * p = game.world.GetPeerPlayer(game.world.viewedpeerid);
if(!p || !(p->isbuying || p->techstationactive)) return;
std::vector<BuyableItem *> items;
p->CollectBuyMenuItems(game.world, p->techstationactive, items);
if(items.empty()) return;
int i = index < 0 ? 0 : (index >= (int)items.size() ? (int)items.size() - 1 : index);
if(p->techstationactive) p->techifacelastitem = i; else p->buyifacelastitem = i;
});
};
world_session.buytech_purchase = [this](int index){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, index](){
Player * p = game.world.GetPeerPlayer(game.world.viewedpeerid);
if(!p || !(p->isbuying || p->techstationactive)) return;
std::vector<BuyableItem *> items;
p->CollectBuyMenuItems(game.world, p->techstationactive, items);
if(items.empty()) return;
int i = index < 0 ? 0 : (index >= (int)items.size() ? (int)items.size() - 1 : index);
Uint8 id = items[i]->id;
if(p->isbuying) p->BuyItem(game.world, id);
else if(p->InOwnBase(game.world)) p->RepairItem(game.world, id);
else p->VirusItem(game.world, id);
});
};
world_session.buytech_close = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
Player * p = game.world.GetPeerPlayer(game.world.viewedpeerid);
if(p){ p->isbuying = false; p->techstationactive = false; }
});
};
world_session.chat_set_text = [this](const std::string & text){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, text](){
Player * p = game.world.GetPeerPlayer(game.world.viewedpeerid);
if(!p) return;
std::strncpy(p->chatText, text.c_str(), sizeof(p->chatText) - 1);
p->chatText[sizeof(p->chatText) - 1] = '\0';
});
};
world_session.chat_send = [this](const std::string & text){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, text](){
Player * p = game.world.GetPeerPlayer(game.world.viewedpeerid);
if(!p) return;
if(!text.empty()){
char buf[101];
std::strncpy(buf, text.c_str(), sizeof(buf) - 1);
buf[sizeof(buf) - 1] = '\0';
game.world.SendChat(p->chatwithteam, buf);
}
p->chatActive = false;
p->chatText[0] = '\0';
});
};
world_session.chat_cancel = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
Player * p = game.world.GetPeerPlayer(game.world.viewedpeerid);
if(p){ p->chatActive = false; p->chatText[0] = '\0'; }
});
};
world_session.chat_toggle_channel = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
Player * p = game.world.GetPeerPlayer(game.world.viewedpeerid);
if(p) p->chatwithteam = !p->chatwithteam;
});
};
world_session.set_show_player_list = [this](bool show){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this, show](){
game.world.SetShowingPlayerList(show);
});
};
world_session.request_peer_list = [this](){
cppxHost->pipeline().client_ui().queue_deferred_mutation([this](){
game.world.RequestPeerList();
});
};

// Global FrameProvider chain (doc §5), outermost (Theme) → innermost
// (WorldSession): Theme ▸ Server ▸ App ▸ Session ▸ Settings ▸ KeyMap ▸ Updater ▸
// KeybindCapture ▸ Lobby ▸ WorldSession ▸ <screen>.
::ui::UiElement tree = client::ui::WorldSessionProvider(
client::ui::WorldSessionValue{std::move(world_session)}, ::ui::children({child}));
tree = client::ui::LobbyProvider(
client::ui::LobbyProviderValue{std::move(lobby)}, ::ui::children({tree}));
tree = client::ui::KeybindCaptureProvider(
client::ui::KeybindCaptureProviderValue{std::move(capture)}, ::ui::children({tree}));
tree = client::ui::UpdaterProvider(
client::ui::UpdaterProviderValue{updater}, ::ui::children({tree}));
tree = client::ui::KeyMapProvider(
client::ui::KeyMapProviderValue{std::move(key_map)}, ::ui::children({tree}));
tree = client::ui::SettingsProvider(
client::ui::SettingsProviderValue{settings}, ::ui::children({tree}));
tree = client::ui::SessionProvider(
client::ui::SessionProviderValue{session}, ::ui::children({tree}));
tree = client::ui::AppProvider(
client::ui::AppProviderValue{.quit = [this]{ game.quitRequested = true; },
                            .version = SILENCER_VERSION,
                            .canvas_w = cppxCanvasW_,
                            .canvas_h = cppxCanvasH_},
::ui::children({tree}));
// SIL-94: per-frame wall-clock for component animation (use_clock) —
// snapshotted once per render frame in RenderCppxClientUiFrame (this lambda
// runs once per visible screen layer).
tree = client::ui::ClockProvider(cppxClock_, ::ui::children({tree}));
// SIL-87: baked legacy-sprite chrome ids (read by use_chrome()).
tree = client::ui::ChromeTexturesProvider(cppxChrome, ::ui::children({tree}));
tree = silencer::game_ui::ServerProvider(
silencer::game_ui::ServerProviderValue{&game},
::ui::children({tree}));
tree = client::ui::ThemeProvider(::ui::children({tree}));
return tree;
});
cppxHost->pipeline().client_ui().push_screen(
std::make_unique<client::ui::AppRoot>());
cppxAppRootPushed = true;
}

// SIL-18 input: the accumulated per-frame edges (events.cpp windowed +
// control-socket injection) plus the derived pointer. An injected click is a
// single-frame press+release at a UI-space point (so the control socket can
// activate a node by location); otherwise the real mouse drives hover/press.
client::ui::UiPipelineFrame frame = {};
frame.layout = {cppxLogicalW, cppxLogicalH};
frame.input = uiInput_;
float mx = -1000.0f;
float my = -1000.0f;
if(injectedPointer_){
mx = injectedPointerX_;
my = injectedPointerY_;
frame.input.pointer_pressed = true;
frame.input.pointer_released = true;
frame.input.pointer_down = false;
frame.input.source = ::ui::UiFocusSource::Mouse;
}else if(hasInjectedHover_){
// Sticky headless hover: park the pointer at the injected point (no press) so
// focus-follows-hover tracks it. Persists until the next InjectPointerMove.
mx = injectedHoverX_;
my = injectedHoverY_;
frame.input.source = ::ui::UiFocusSource::Mouse;
}else if(win){
float wx = 0.0f;
float wy = 0.0f;
Uint32 buttons = SDL_GetMouseState(&wx, &wy);
int ww = 0;
int wh = 0;
SDL_GetWindowSize(win, &ww, &wh);
// Map window pixels into the logical layout space (the tree lays out logical,
// not physical — see frame.layout above).
mx = (ww > 0) ? (wx / static_cast<float>(ww)) * cppxLogicalW : wx;
my = (wh > 0) ? (wy / static_cast<float>(wh)) * cppxLogicalH : wy;
bool down = (buttons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0;
frame.input.pointer_down = down;
frame.input.pointer_pressed = down && !prevPointerDown_;
frame.input.pointer_released = !down && prevPointerDown_;
if(down || frame.input.pointer_pressed || frame.input.pointer_released){
frame.input.source = ::ui::UiFocusSource::Mouse;
}
prevPointerDown_ = down;
}
frame.pointer = {mx, my};

// SIL-20: capture the lobby read-state once per tick, under LockMutex, *before*
// the build below reads it (the frame provider copies lobbySnapshot_ with no
// build-time lock). Default/empty outside lobby phases.
lobbySnapshot_ = silencer::game_ui::CaptureLobbySnapshot(game, CurrentSessionPhase());
// SIL-21 (3/n): fold in the bundled-map choices for the GameCreatePanel. Maps
// don't change at runtime, so list them once (disk read on the game thread).
if(!bundledMapsListed_){
// origin BuildMapList: bundled res-dir maps + the player's downloaded maps
// (data-dir level/download), deduped and sorted. On macOS GetResDir() is
// empty and "level" resolves against the CWD, so pin it like origin does
// (CDResDir before the res listing, CDDataDir after).
CDResDir();
bundledMaps_ = game.gameSession.MapDownloaderRef().ListFiles((GetResDir() + "level").c_str());
CDDataDir();
std::vector<std::string> downloaded =
    game.gameSession.MapDownloaderRef().ListFiles((GetDataDir() + "level/download").c_str());
for(const std::string &f : downloaded){
if(std::find(bundledMaps_.begin(), bundledMaps_.end(), f) == bundledMaps_.end())
bundledMaps_.push_back(f);
}
std::sort(bundledMaps_.begin(), bundledMaps_.end());
// origin appends the map-api server's list after the sorted local maps (the
// "[DL] " tag is stripped at render, so they show as plain names in server
// order). Names already local are skipped.
for(auto & entry : FetchServerMapList(Config::GetInstance().mapapiurl)){
if(std::find(bundledMaps_.begin(), bundledMaps_.end(), entry.first) == bundledMaps_.end())
bundledMaps_.push_back(entry.first);
}
bundledMapsListed_ = true;
}
lobbySnapshot_.bundled_maps = bundledMaps_;
// SIL-21 (4/n): capture the in-match read-state (viewed agent + replicated match
// state) on the game thread before the build. Empty outside the match phases.
worldSessionSnapshot_ = silencer::game_ui::CaptureWorldSessionSnapshot(game, CurrentSessionPhase());

int ow = 0;
int oh = 0;
const uint8_t * rgba = cppxHost->render(frame, &ow, &oh);
if(rgba){
cppxUiRgba = rgba;
cppxUiW = ow;
cppxUiH = oh;
}

// UI interaction sounds (origin ClientUi.cpp:95-111 PlayMenuButtonSound):
// hover-ENTER edge on an audible button (dedupe via the remembered id) or an
// activate/keyboard-navigate landing on one -> GAS soundUIClick via
// Audio::PlayUI. The edges come from the UI side as data; only this
// composition root touches Audio. Count edges even when audio is disabled
// (headless) so e2e can assert the triggers.
{
const client::ui::ClientUi::UiAudioEvents & ev =
    cppxHost->pipeline().client_ui().audio_events();
bool play = false;
if(ev.hovered_button && (uint64_t)ev.hovered_button != lastHoveredAudible_)
play = true;
lastHoveredAudible_ = (uint64_t)ev.hovered_button;
if(ev.activated_button || ev.nav_focused_button)
play = true;
if(play){
++uiClickCount_;
Audio & audio = Audio::GetInstance();
if(audio.enabled){
const std::string & sound = GASLoader::Get().player.soundUIClick;
auto it = game.world.resources.soundbank.find(sound);
if(it != game.world.resources.soundbank.end() && it->second)
audio.PlayUI(it->second);
}
}
}

// Text-input platform gating (windowed only): the cppx pipeline reports whether
// the focused node is a text field; toggle SDL text input to match. This is the
// only owner of SDL_StartTextInput/StopTextInput (see src/game/CLAUDE.md).
if(win){
bool wants = cppxHost->pipeline().client_ui().wants_text_input();
if(wants != textInputActive_){
if(wants) SDL_StartTextInput(win); else SDL_StopTextInput(win);
textInputActive_ = wants;
}
}

// One-frame edges are consumed; reset for the next accumulation window.
uiInput_ = {};
injectedPointer_ = false;
#else
(void)surface;
#endif
}

void GameUiPipeline::InjectPointerClick(float x, float y) {
injectedPointer_ = true;
injectedPointerX_ = x;
injectedPointerY_ = y;
}

void GameUiPipeline::InjectPointerMove(float x, float y) {
hasInjectedHover_ = true;
injectedHoverX_ = x;
injectedHoverY_ = y;
}

client::ui::ClientUi * GameUiPipeline::TryClientUi() {
return cppxHost ? &cppxHost->pipeline().client_ui() : nullptr;
}

void GameUiPipeline::ShowPasswordModal(const char * title) {
client::ui::ClientUi * ui = TryClientUi();
if(!ui) return;
passwordModal_ = {true, false, {}};
ui->push_screen(std::make_unique<client::ui::PasswordModalScreen>(
title ? title : "Password",
[this](const std::string & value){
passwordModal_.submitted = true;
passwordModal_.value = value;
}));
}

void GameUiPipeline::ShowMessageModal(const char * title, const char * message) {
client::ui::ClientUi * ui = TryClientUi();
if(!ui) return;
ui->push_screen(std::make_unique<client::ui::MessageModalScreen>(
title ? title : "", message ? message : ""));
}

void GameUiPipeline::ShowGallery() {
client::ui::ClientUi * ui = TryClientUi();
if(!ui) return;
ui->push_screen(std::make_unique<client::ui::GalleryScreen>());
}

void GameUiPipeline::BeginKeybindCapture(Action action, int comboIndex) {
keybindCapture_.active = true;
keybindCapture_.targetAction = action;
keybindCapture_.targetComboIndex = comboIndex;
keybindCapture_.pending.clear();
}

bool GameUiPipeline::FeedKeybindEdge(const BindingKey & key) {
if(!keybindCapture_.active) return false;
if((int)keybindCapture_.pending.size() >= CHORD_CAP) return false;
for(const BindingKey & k : keybindCapture_.pending){
if(k.device == key.device && k.code == key.code && k.axisDir == key.axisDir){
return false; // dedup: a held edge only adds its chip once
}
}
keybindCapture_.pending.push_back(key);
return true;
}

void GameUiPipeline::CancelKeybindCapture() {
keybindCapture_.active = false;
keybindCapture_.targetComboIndex = -1;
keybindCapture_.pending.clear();
}

void GameUiPipeline::ConfirmKeybindChord() {
if(!keybindCapture_.active || keybindCapture_.pending.empty()){
CancelKeybindCapture();
return;
}
// Apply directly (not via the after-render mutation queue): this also runs
// from the control socket, which fires before begin_frame clears that queue.
// A keymap edit is local config — not a structural tree mutation or a wire
// message — so a direct, immediate write is safe and FADEOUT-gating-exempt.
if((int)keybindCapture_.pending.size() <= CHORD_CAP){
ForkActiveProfileIfBuiltin(game.GetKeyMap());
ActionBindings & ab = game.GetKeyMap().Get(keybindCapture_.targetAction);
Binding b;
b.keys = keybindCapture_.pending;
int idx = keybindCapture_.targetComboIndex;
if(idx >= 0 && idx < (int)ab.bindings.size()){
ab.bindings[idx] = std::move(b);
keymapDirty_ = true;
}else if((int)ab.bindings.size() < COMBO_CAP){
ab.bindings.push_back(std::move(b));
keymapDirty_ = true;
}
}
CancelKeybindCapture();
}

