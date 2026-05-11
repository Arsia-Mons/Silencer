#include "options_controls.h"

#include "context.h"
#include "layout.h"
#include "render_commands.h"
#include "theme.h"

#include "config.h"
#include "game.h"
#include "game_state.h"
#include "input.h"
#include "keybinds.h"
#include "resources.h"
#include "screen_context.h"
#include "world.h"
#include "renderer.h"
#include "surface.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>

namespace ui {
namespace v2 {

namespace {

// Frame-local string arena. Same shape as main_menu / options_audio —
// reset at the top of RenderOptionsControls; buffer survives through
// Clay_EndLayout. Sized larger because this screen materialises ~30
// strings per frame (5 rows × {keyname, c1, op, c2} plus chrome).
constexpr size_t kFrameStringBytes = 1024;
thread_local char   g_frame_strings[kFrameStringBytes];
thread_local size_t g_frame_off = 0;

Clay_String FrameStr(const char * s) {
	size_t n = std::strlen(s);
	if(g_frame_off + n + 1 > kFrameStringBytes) g_frame_off = 0;
	char * p = &g_frame_strings[g_frame_off];
	std::memcpy(p, s, n);
	p[n] = '\0';
	g_frame_off += n + 1;
	Clay_String out{};
	out.length = (int32_t)n;
	out.chars  = p;
	return out;
}

void OnButtonClick(Clay_ElementId, Clay_PointerData p, intptr_t user) {
	if(p.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
	auto * h = reinterpret_cast<const std::function<void()> *>(user);
	if(h && *h) (*h)();
}

// Per-button rebind closures, kept thread_local so their addresses
// stay valid across the Runtime::DispatchMouseDown path that fires
// Clay_SetPointerState *after* Render returns. Rebound at the top of
// every RenderOptionsControls call so they always see the latest
// handlers struct.
thread_local std::function<void()> g_rebind_handlers[5][2];

void TextLabel(Clay_String s, Clay_TextElementConfig cfg) {
	CLAY({ .layout = { .sizing = { CLAY_SIZING_FIT(), CLAY_SIZING_FIT() } } }) {
		CLAY_TEXT(s, CLAY_TEXT_CONFIG(cfg));
	}
}

void Button(const char * key, const char * label, const std::function<void()> & handler,
            float width, float height) {
	Clay_String key_s   = FrameStr(key);
	Clay_String label_s = FrameStr(label);
	CLAY({
		.id = Clay_GetElementId(key_s),
		.layout = {
			.sizing = { CLAY_SIZING_FIXED(width), CLAY_SIZING_FIXED(height) },
			.childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
		},
		.backgroundColor = Clay_Hovered() ? ::ui::kColorSelectHi : ::ui::kColorScrollbarFill,
		.cornerRadius    = ::ui::kCornerSmall,
	}) {
		Clay_OnHover(OnButtonClick, (intptr_t)&handler);
		CLAY_TEXT(label_s, CLAY_TEXT_CONFIG(::ui::kFontHeading));
	}
}

}  // namespace

void RenderOptionsControls(const Context & ctx,
                           const OptionsControlsHandlers & h,
                           const OptionsControlsState & state) {
	(void)ctx;
	g_frame_off = 0;

	// Bind per-button rebind closures (10 of them: 5 rows × {primary, secondary}).
	for(int i = 0; i < 5; i++){
		for(int s = 0; s < 2; s++){
			auto fn  = h.on_rebind_key;
			int  row = i, slot = s;
			g_rebind_handlers[i][s] = [fn, row, slot](){ if(fn) fn(row, slot); };
		}
	}

	CLAY({
		.id = CLAY_ID("OptionsControlsRoot"),
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
			.padding         = CLAY_PADDING_ALL(12),
			.childGap        = 8,
			.childAlignment  = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = ::ui::kColorPanelBg,
	}) {
		TextLabel(FrameStr("Configure Controls"), ::ui::kFontTitle);

		// Preset row: "Preset:" label + cycle button.
		CLAY({
			.id = CLAY_ID("OptionsControlsPresetRow"),
			.layout = {
				.sizing = { CLAY_SIZING_FIT(), CLAY_SIZING_FIT() },
				.childGap = 8,
				.childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
				.layoutDirection = CLAY_LEFT_TO_RIGHT,
			},
		}) {
			TextLabel(FrameStr("Preset:"), ::ui::kFontHeading);
			Button("ctrl_preset",
			       state.preset_text.empty() ? "(none)" : state.preset_text.c_str(),
			       h.on_preset, 220, 33);
		}

		// Five binding rows. Each row: keyname | c1 | OR/AND | c2.
		for(int i = 0; i < 5; i++){
			char row_key[24];
			std::snprintf(row_key, sizeof(row_key), "ctrl_row_%d", i);
			char c1_key[24];
			std::snprintf(c1_key, sizeof(c1_key), "ctrl_c1_%d", i);
			char c2_key[24];
			std::snprintf(c2_key, sizeof(c2_key), "ctrl_c2_%d", i);

			CLAY({
				.id = Clay_GetElementId(FrameStr(row_key)),
				.layout = {
					.sizing = { CLAY_SIZING_FIT(), CLAY_SIZING_FIT() },
					.childGap = 8,
					.childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
					.layoutDirection = CLAY_LEFT_TO_RIGHT,
				},
			}) {
				// Fixed-width keyname column so c1 aligns across rows.
				CLAY({
					.layout = {
						.sizing = { CLAY_SIZING_FIXED(120), CLAY_SIZING_FIXED(33) },
						.childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
					},
				}) {
					if(!state.rows[i].keyname.empty()){
						CLAY_TEXT(FrameStr(state.rows[i].keyname.c_str()),
						          CLAY_TEXT_CONFIG(::ui::kFontHeading));
					}
				}
				Button(c1_key, state.rows[i].c1_text.empty() ? "-" : state.rows[i].c1_text.c_str(),
				       g_rebind_handlers[i][0], 112, 33);
				// OR/AND column — fixed width keeps c2 column aligned.
				CLAY({
					.layout = {
						.sizing = { CLAY_SIZING_FIXED(40), CLAY_SIZING_FIXED(33) },
						.childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
					},
				}) {
					if(!state.rows[i].op_text.empty()){
						CLAY_TEXT(FrameStr(state.rows[i].op_text.c_str()),
						          CLAY_TEXT_CONFIG(::ui::kFontHeading));
					}
				}
				Button(c2_key, state.rows[i].c2_text.empty() ? "-" : state.rows[i].c2_text.c_str(),
				       g_rebind_handlers[i][1], 112, 33);
			}
		}

		// Save / Cancel.
		CLAY({
			.id = CLAY_ID("OptionsControlsActions"),
			.layout = {
				.sizing = { CLAY_SIZING_FIT(), CLAY_SIZING_FIT() },
				.padding = { 0, 0, 8, 0 },
				.childGap = 8,
				.layoutDirection = CLAY_LEFT_TO_RIGHT,
			},
		}) {
			Button("ctrl_save",   "Save",   h.on_save,   120, 33);
			Button("ctrl_cancel", "Cancel", h.on_cancel, 120, 33);
		}
	}
}

// -----------------------------------------------------------------------------
// OptionsControlsRuntime — engine wire-in for GameState::OPTIONSCONTROLS.
// -----------------------------------------------------------------------------

namespace {

constexpr int      CONTROLS_VISIBLE_ROWS        = 5;
constexpr uint32_t CONTROLS_REBIND_TIMEOUT      = 72;
constexpr int      CONTROLS_SECONDARY_SLOT_BASE = 100;

bool IsBuiltinKeybindProfile(const std::string & name){
	return name == "default" || name == "wasd" || name == "gamepad";
}

bool BindingsAreAnded(const ActionBindings & ab){
	if(ab.bindings.empty()) return false;
	const auto & b0 = ab.bindings[0];
	return b0.keys.size() >= 2 &&
	       b0.keys[0].device == BindingDevice::Keyboard &&
	       b0.keys[1].device == BindingDevice::Keyboard;
}

std::string ControlsBindingLabel(const KeyMap & km, SDL_Gamepad * pad, Action a, int slot){
	const auto & ab = km.Get(a);
	int found = 0;
	for(const auto & b : ab.bindings){
		if(b.keys.empty()) continue;
		if(found == slot){
			const auto & k = b.keys[0];
			if(k.device == BindingDevice::Keyboard){
				return KeyMap::GetKeyName((SDL_Scancode)k.code);
			}
			std::string s = Stringify(k);
			auto colon = s.find(':');
			std::string raw = (colon != std::string::npos) ? s.substr(colon + 1) : s;
			return GamepadShortLabel(raw, pad ? SDL_GetGamepadType(pad) : SDL_GAMEPAD_TYPE_UNKNOWN);
		}
		found++;
	}
	return KeyMap::GetKeyName(SDL_SCANCODE_UNKNOWN);
}

void WriteControlsLegacy(KeyMap & km, Action a, SDL_Scancode key1, SDL_Scancode key2, bool and_){
	auto & ab = km.Get(a);
	ab.bindings.clear();
	auto mk = [](SDL_Scancode sc){
		BindingKey k;
		k.device  = BindingDevice::Keyboard;
		k.code    = (int)sc;
		k.axisDir = 0;
		return k;
	};
	if(key1 == SDL_SCANCODE_UNKNOWN && key2 == SDL_SCANCODE_UNKNOWN) return;
	if(and_ && key1 != SDL_SCANCODE_UNKNOWN && key2 != SDL_SCANCODE_UNKNOWN){
		Binding b; b.keys.push_back(mk(key1)); b.keys.push_back(mk(key2));
		ab.bindings.push_back(std::move(b));
		return;
	}
	if(key1 != SDL_SCANCODE_UNKNOWN){
		Binding b; b.keys.push_back(mk(key1));
		ab.bindings.push_back(std::move(b));
	}
	if(key2 != SDL_SCANCODE_UNKNOWN){
		Binding b; b.keys.push_back(mk(key2));
		ab.bindings.push_back(std::move(b));
	}
}

void ControlsCurrentKeys(const KeyMap & km, Action a, SDL_Scancode & key1, SDL_Scancode & key2){
	key1 = key2 = SDL_SCANCODE_UNKNOWN;
	const auto & ab = km.Get(a);
	if(ab.bindings.empty()) return;
	const auto & b0 = ab.bindings[0];
	if(b0.keys.size() >= 2 &&
	   b0.keys[0].device == BindingDevice::Keyboard &&
	   b0.keys[1].device == BindingDevice::Keyboard){
		key1 = (SDL_Scancode)b0.keys[0].code;
		key2 = (SDL_Scancode)b0.keys[1].code;
		return;
	}
	if(!b0.keys.empty() && b0.keys[0].device == BindingDevice::Keyboard){
		key1 = (SDL_Scancode)b0.keys[0].code;
	}
	if(ab.bindings.size() >= 2){
		const auto & b1 = ab.bindings[1];
		if(!b1.keys.empty() && b1.keys[0].device == BindingDevice::Keyboard){
			key2 = (SDL_Scancode)b1.keys[0].code;
		}
	}
}

OptionsControlsState ComputeOptionsControlsLive(ScreenContext & sctx, int active_slot_uid){
	OptionsControlsState s;
	const KeyMap & km = sctx.keymap;
	SDL_Gamepad * pad = sctx.game.GetGamepad();
	s.preset_text = !km.label.empty() ? km.label
	              : !km.name.empty()  ? km.name
	              : std::string(Config::GetInstance().active_keybind_profile);
	for(int i = 0; i < CONTROLS_VISIBLE_ROWS; i++){
		int row = i;  // scroll_position currently fixed at 0
		if(row >= (int)Action::Count) break;
		Action a = ACTION_TABLE[row].action;
		s.rows[i].keyname = std::string(GetActionInfo(a).label) + ":";
		s.rows[i].c1_text = ControlsBindingLabel(km, pad, a, 0);
		s.rows[i].c2_text = ControlsBindingLabel(km, pad, a, 1);
		s.rows[i].op_text = BindingsAreAnded(km.Get(a)) ? "AND" : "OR";
		if(active_slot_uid >= 0){
			if(active_slot_uid < CONTROLS_SECONDARY_SLOT_BASE && active_slot_uid == row){
				s.rows[i].c1_text = "-";
			}else if(active_slot_uid >= CONTROLS_SECONDARY_SLOT_BASE &&
			         (active_slot_uid - CONTROLS_SECONDARY_SLOT_BASE) == row){
				s.rows[i].c2_text = "-";
			}
		}
	}
	return s;
}

OptionsControlsHandlers BuildOptionsControlsHandlers(OptionsControlsRuntime * self, ScreenContext & sctx){
	OptionsControlsHandlers h;
	h.on_preset = [&sctx](){ CycleKeybindPreset(sctx.keymap); };
	h.on_save = [&sctx](){
		const std::string active = Config::GetInstance().active_keybind_profile;
		if(!IsBuiltinKeybindProfile(active)){
			sctx.keymap.SaveFile(WritableProfilePath(active));
		}
		Config::GetInstance().Save();
		sctx.GoToState(GameState::OPTIONS);
	};
	h.on_cancel = [&sctx](){
		LoadActiveKeymap(sctx.keymap);
		Config::GetInstance().Load();
		sctx.GoToState(GameState::OPTIONS);
	};
	h.on_rebind_key = [self](int row, int slot){ self->StartRebind(row, slot); };
	return h;
}

}  // namespace

OptionsControlsRuntime::OptionsControlsRuntime(World & world, ScreenContext & sctx)
	: world_(world), sctx_(sctx) {}

void OptionsControlsRuntime::StartRebind(int row, int slot){
	if(rebind_active_slot_ >= 0) return;
	rebind_active_slot_      = (slot == 0) ? row : (CONTROLS_SECONDARY_SLOT_BASE + row);
	rebind_start_tick_       = world_.tickcount;
	rebind_pending_scancode_ = -1;
	rebind_gamepad_buttons_  = sctx_.game.GetGamepadState().buttons;
	std::memcpy(rebind_gamepad_axes_, sctx_.game.GetGamepadState().axes,
	            sizeof(rebind_gamepad_axes_));
}

void OptionsControlsRuntime::Render(Surface & target, ::Renderer & renderer,
                                     int mouse_x, int mouse_y, float dt,
                              int logical_w, int logical_h, int scale){
	Context ctx{
		world_.resources,
		/*logical_w=*/logical_w,
		/*logical_h=*/logical_h,
		/*scale=*/scale,
		/*version=*/world_.GetVersion(),
	};
	ctx.mouse_x = mouse_x;
	ctx.mouse_y = mouse_y;
	ctx.dt      = dt;

	target.Clear(0);

	EnsureClayContext(ctx);
	Clay_SetPointerState(Clay_Vector2{ (float)mouse_x, (float)mouse_y }, /*pointer_down=*/false);
	Clay_UpdateScrollContainers(/*drag=*/false, Clay_Vector2{ 0.0f, 0.0f }, dt);
	Clay_SetLayoutDimensions(Clay_Dimensions{ (float)logical_w, (float)logical_h });
	Clay_BeginLayout();
	OptionsControlsHandlers handlers = BuildOptionsControlsHandlers(this, sctx_);
	OptionsControlsState live = ComputeOptionsControlsLive(sctx_, rebind_active_slot_);
	RenderOptionsControls(ctx, handlers, live);
	Clay_RenderCommandArray cmds = Clay_EndLayout();
	::ui::DrawRenderCommands(cmds, renderer, target, scale);
}

bool OptionsControlsRuntime::DispatchMouseDown(int mouse_x, int mouse_y,
                                int logical_w, int logical_h, int scale){
	// Suppress chip clicks while a rebind is in flight (legacy iface->disabled
	// gate). Save / Cancel / Preset stay reachable because the user is expected
	// to finish or time out the capture before navigating away.
	if(rebind_active_slot_ >= 0) return true;

	Context ctx{
		world_.resources,
		/*logical_w=*/logical_w,
		/*logical_h=*/logical_h,
		/*scale=*/scale,
		/*version=*/world_.GetVersion(),
	};
	ctx.mouse_x = mouse_x;
	ctx.mouse_y = mouse_y;

	// Re-lay out so OnHover userData points at `handlers` on this stack
	// frame; SetPointerState walks the just-finalised tree.
	EnsureClayContext(ctx);
	Clay_SetLayoutDimensions(Clay_Dimensions{ (float)logical_w, (float)logical_h });
	Clay_BeginLayout();
	OptionsControlsHandlers handlers = BuildOptionsControlsHandlers(this, sctx_);
	OptionsControlsState live = ComputeOptionsControlsLive(sctx_, rebind_active_slot_);
	RenderOptionsControls(ctx, handlers, live);
	(void)Clay_EndLayout();
	Clay_SetPointerState(Clay_Vector2{ (float)mouse_x, (float)mouse_y }, /*pointer_down=*/true);
	return true;
}

bool OptionsControlsRuntime::DispatchKeyDown(int sdl_scancode){
	// Latch the next scancode while a slot is armed; Tick() commits + clears.
	if(rebind_active_slot_ >= 0){
		rebind_pending_scancode_ = sdl_scancode;
		return true;
	}
	return false;
}

void OptionsControlsRuntime::Tick(){
	if(rebind_active_slot_ < 0) return;

	const int slot_uid = rebind_active_slot_;
	const int row = (slot_uid < CONTROLS_SECONDARY_SLOT_BASE) ? slot_uid
	                                                          : (slot_uid - CONTROLS_SECONDARY_SLOT_BASE);
	if(row < 0 || row >= (int)Action::Count){
		rebind_active_slot_      = -1;
		rebind_pending_scancode_ = -1;
		return;
	}
	const Action a = ACTION_TABLE[row].action;
	const GamepadState & gp = sctx_.game.GetGamepadState();
	KeyMap & km = sctx_.keymap;

	// Gamepad capture (only newly-pressed buttons / axes past deadzone).
	if(gp.connected){
		BindingKey padKey{}; bool padFound = false;
		for(int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT && !padFound; b++){
			bool was = (rebind_gamepad_buttons_ >> b) & 1;
			bool is  = (gp.buttons >> b) & 1;
			if(is && !was){
				padKey.device = BindingDevice::GamepadButton;
				padKey.code   = b;
				padKey.axisDir = 0;
				padFound = true;
			}
		}
		for(int ax = 0; ax < SDL_GAMEPAD_AXIS_COUNT && !padFound; ax++){
			int16_t was = rebind_gamepad_axes_[ax];
			int16_t is  = gp.axes[ax];
			if(std::abs(is) > AXIS_DEADZONE && std::abs(was) <= AXIS_DEADZONE){
				padKey.device  = BindingDevice::GamepadAxis;
				padKey.code    = ax;
				padKey.axisDir = (is > 0) ? 1 : -1;
				padFound = true;
			}
		}
		if(padFound){
			ForkActiveProfileIfBuiltin(km);
			auto & ab = km.Get(a);
			Binding binding; binding.keys.push_back(padKey);
			if(slot_uid < CONTROLS_SECONDARY_SLOT_BASE){
				if(ab.bindings.empty()) ab.bindings.push_back(binding);
				else                    ab.bindings[0] = binding;
			}else{
				if(ab.bindings.empty()) ab.bindings.push_back(Binding{});
				if(ab.bindings.size() < 2) ab.bindings.push_back(binding);
				else                       ab.bindings[1] = binding;
			}
			rebind_active_slot_      = -1;
			rebind_pending_scancode_ = -1;
			return;
		}
	}

	// Keyboard scancode or timeout.
	const bool timed_out = (world_.tickcount - rebind_start_tick_) > CONTROLS_REBIND_TIMEOUT;
	if(rebind_pending_scancode_ >= 0 || timed_out){
		SDL_Scancode sym = (rebind_pending_scancode_ >= 0)
			? (SDL_Scancode)rebind_pending_scancode_
			: SDL_SCANCODE_UNKNOWN;
		if(timed_out) sym = SDL_SCANCODE_UNKNOWN;
#ifndef OUYA
		if(sym == SDL_SCANCODE_ESCAPE) sym = SDL_SCANCODE_UNKNOWN;
#endif
		SDL_Scancode key1, key2;
		ControlsCurrentKeys(km, a, key1, key2);
		const bool and_was = BindingsAreAnded(km.Get(a));
		if(slot_uid < CONTROLS_SECONDARY_SLOT_BASE) key1 = sym;
		else                                        key2 = sym;
		ForkActiveProfileIfBuiltin(km);
		WriteControlsLegacy(km, a, key1, key2, and_was);
		rebind_active_slot_      = -1;
		rebind_pending_scancode_ = -1;
	}
}

}  // namespace v2
}  // namespace ui
