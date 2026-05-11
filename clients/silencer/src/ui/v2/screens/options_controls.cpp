#include "options_controls.h"

#include "context.h"
#include "dispatch.h"
#include "layout.h"
#include "node.h"
#include "render.h"

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
#include <cstdlib>
#include <cstring>
#include <string>

namespace ui {
namespace v2 {

Node BuildOptionsControls(const Context & ctx,
                          const OptionsControlsHandlers & handlers,
                          const OptionsControlsState * state)
{
	// Title centered at y=14, textbank=135, textwidth=12. Legacy formula:
	//   x = 320 - (len * textwidth) / 2
	// "Configure Controls" = 18 chars → x = 320 - 108 = 212.
	const std::string title = "Configure Controls";
	const int title_x = 320 - ((int)title.size() * 12) / 2;

	// Preset button's anchor bakes the chrome-offset delta between
	// B220x33 (sprite index 23) and B112x33 (sprite index 28). Legacy
	// math:  x = -30 + spriteoffsetx[6][23] - spriteoffsetx[6][28]
	// — keeps the visible chrome left edge aligned with the row-0 c1
	// button (which is B112x33 at x=-30). The v2 absolute `.at()` path
	// applies the same B220x33 offset at render time, so passing the
	// same anchor reproduces the legacy chrome top-left pixel-exactly.
	const int preset_x = -30 + ctx.resources.spriteoffsetx[6][23]
	                         - ctx.resources.spriteoffsetx[6][28];

	// Per-row factory. slot = i+1 because the preset row occupies slot 0.
	// Legacy: c1 y = slot*53,        x = -30
	//         c2 y = slot*53,        x = 120
	//         keyname/op overlays render no pixels in the static (pre-Tick)
	//         build state. With `state` we mirror the legacy Tick output:
	//         keyname Label at (80, 95+slot*53), OR/AND Label centered
	//         inside the BNONE button rect (40×30 at x=383, y=95+slot*53).
	auto row = [&](int i) {
		const int slot = i + 1;
		const int by = slot * 53;
		const int y  = 95 + slot * 53;

		std::vector<Node> children;

		if(state && !state->rows[i].keyname.empty()){
			children.push_back(
				Label(state->rows[i].keyname, /*font_bank=*/134, /*font_width=*/10)
					.at(80, (Sint16)y));
		}

		{
			Node c1 = Button(state ? state->rows[i].c1_text : "", ButtonType::B112x33)
				.at(-30, (Sint16)by)
				.withKey("ctrl_c1_" + std::to_string(i));
			if(handlers.on_rebind_key){
				auto h = handlers.on_rebind_key;
				c1.onClick([h, i](){ h(i, 0); });
			}
			children.push_back(std::move(c1));
		}

		// OR/AND text. BNONE button in legacy: width=40, textwidth=9,
		// no chrome. xoff = (40 - len*9)/2 centers within the 40px box;
		// yoff = 0 (BNONE not in Button::GetTextOffset switch).
		// alpha=true mirrors the legacy Button::DrawText code path so
		// glyph pixels land identically. Click-to-toggle isn't wired
		// in this iteration (read-only display).
		if(state && !state->rows[i].op_text.empty()){
			const std::string & op = state->rows[i].op_text;
			int xoff = (40 - (int)op.size() * 9) / 2;
			children.push_back(
				Label(op, /*font_bank=*/134, /*font_width=*/9)
					.at((Sint16)(383 + xoff), (Sint16)y)
					.withAlpha());
		}

		{
			Node c2 = Button(state ? state->rows[i].c2_text : "", ButtonType::B112x33)
				.at(120, (Sint16)by)
				.withKey("ctrl_c2_" + std::to_string(i));
			if(handlers.on_rebind_key){
				auto h = handlers.on_rebind_key;
				c2.onClick([h, i](){ h(i, 1); });
			}
			children.push_back(std::move(c2));
		}

		return Group(std::move(children));
	};

	return Background(/*bank=*/6, /*index=*/0, {
		// Secondary panel sprite (bank 7 frame 7) layered on top of the
		// base backdrop. Anchor (0,0) matches the legacy Overlay default.
		Sprite(7, 7),

		Label(title, /*font_bank=*/135, /*font_width=*/12).at((Sint16)title_x, 14),

		// Preset row: static "Preset:" label at the same x/y as binding row
		// labels, cycle button on the right.
		Label("Preset:", /*font_bank=*/134, /*font_width=*/10).at(80, 95),
		Button(state ? state->preset_text : std::string(), ButtonType::B220x33)
			.at((Sint16)preset_x, 0)
			.withKey("ctrl_preset")
			.onClick(handlers.on_preset),

		row(0),
		row(1),
		row(2),
		row(3),
		row(4),

		Button("Save")  .at(-200, 117).onClick(handlers.on_save),
		Button("Cancel").at(  20, 117).onClick(handlers.on_cancel),
	});
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

}  // namespace

OptionsControlsRuntime::OptionsControlsRuntime(World & world, ScreenContext & sctx)
	: world_(world), sctx_(sctx) {}

namespace {

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
                                     int mouse_x, int mouse_y, float dt){
	Context ctx{
		world_.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world_.GetVersion(),
	};
	ctx.mouse_x = mouse_x;
	ctx.mouse_y = mouse_y;
	ctx.state   = &state_;
	ctx.dt      = dt;

	OptionsControlsHandlers handlers = BuildOptionsControlsHandlers(this, sctx_);
	OptionsControlsState live = ComputeOptionsControlsLive(sctx_, rebind_active_slot_);
	target.Clear(0);
	state_.BeginFrame();
	Node tree = BuildOptionsControls(ctx, handlers, &live);
	Layout(tree, ctx);
	::ui::v2::Render(tree, ctx, target, renderer);
	state_.EndFrame();
}

bool OptionsControlsRuntime::DispatchMouseDown(int mouse_x, int mouse_y){
	// Suppress chip clicks while a rebind is in flight (legacy iface->disabled
	// gate). Save / Cancel / Preset stay reachable because the user is expected
	// to finish or time out the capture before navigating away.
	if(rebind_active_slot_ >= 0) return true;

	Context ctx{
		world_.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world_.GetVersion(),
	};
	ctx.mouse_x = mouse_x;
	ctx.mouse_y = mouse_y;
	ctx.state   = &state_;
	OptionsControlsHandlers handlers = BuildOptionsControlsHandlers(this, sctx_);
	OptionsControlsState live = ComputeOptionsControlsLive(sctx_, rebind_active_slot_);
	Node tree = BuildOptionsControls(ctx, handlers, &live);
	Layout(tree, ctx);
	DispatchClick(tree, ctx);
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
