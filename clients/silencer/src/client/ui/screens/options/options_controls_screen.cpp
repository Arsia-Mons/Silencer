#include "options_controls_screen.h"

#include "screen_context.h"
#include "game.h"
#include "game_state.h"
#include "config.h"
#include "surface.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiAutomationRegistry.h"
#include "primitives/bank_text.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

namespace
{
using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextBeginFrame;
using silencer::ui::primitives::BankTextVariant;

constexpr int VISIBLE_ROWS = 5;
constexpr int REBIND_TIMEOUT_TICKS = 72;
constexpr uint16_t kPanelW = 540;
constexpr uint16_t kPanelPadX = 20;
constexpr uint16_t kPanelPadY = 24;
constexpr uint16_t kRowH = 40;
constexpr uint16_t kRowGap = 8;
constexpr uint16_t kActionNameW = 168;
constexpr uint16_t kKeyButtonW = 112;
constexpr uint16_t kKeyButtonH = 33;
constexpr uint16_t kOperatorW = 45;
constexpr uint16_t kActionGap = 12;

struct ClickAdapter {
	void (*fn)(void *);
	void * user;
	int arg0;
	int arg1;
	std::string id;
};

constexpr int kAdapterCapacity = 192;
ClickAdapter g_adapters[kAdapterCapacity];
int g_adapterCount = 0;

Clay_String FromCStr(const char * s)
{
	return Clay_String{ false, static_cast<int32_t>(std::strlen(s)), s };
}

Clay_String FromStd(const std::string & s)
{
	return Clay_String{ false, static_cast<int32_t>(s.size()), s.c_str() };
}

std::string ToStd(Clay_String text)
{
	return std::string(text.chars ? text.chars : "", static_cast<size_t>(text.length));
}

std::string AutomationLabel(Clay_String id, Clay_String text)
{
	std::string idText = ToStd(id);
	if(idText == "PresetButton") return "Preset";
	if(idText == "ScrollUp") return "Scroll Up";
	if(idText == "ScrollDown") return "Scroll Down";
	return ToStd(text);
}

ClickAdapter * AllocAdapter(void (*fn)(void *), void * user, int arg0 = 0, int arg1 = 0, std::string id = {})
{
	if(g_adapterCount >= kAdapterCapacity) return nullptr;
	auto * a = &g_adapters[g_adapterCount++];
	a->fn = fn;
	a->user = user;
	a->arg0 = arg0;
	a->arg1 = arg1;
	a->id = std::move(id);
	return a;
}

void PresetClicked(void * user)
{
	auto * screen = static_cast<OptionsControlsScreen *>(user);
	if(screen) screen->NotifyPresetClicked();
}

void SaveClicked(void * user)
{
	auto * screen = static_cast<OptionsControlsScreen *>(user);
	if(screen) screen->NotifySaveClicked();
}

void CancelClicked(void * user)
{
	auto * screen = static_cast<OptionsControlsScreen *>(user);
	if(screen) screen->NotifyCancelClicked();
}

void ScrollUpClicked(void * user)
{
	auto * screen = static_cast<OptionsControlsScreen *>(user);
	if(screen) screen->NotifyScrollUpClicked();
}

void ScrollDownClicked(void * user)
{
	auto * screen = static_cast<OptionsControlsScreen *>(user);
	if(screen) screen->NotifyScrollDownClicked();
}

void BindClicked(void * user)
{
	auto * a = static_cast<ClickAdapter *>(user);
	auto * screen = a ? static_cast<OptionsControlsScreen *>(a->user) : nullptr;
	if(screen) screen->NotifyBindClicked(a->arg0, a->arg1);
}

void OperatorClicked(void * user)
{
	auto * a = static_cast<ClickAdapter *>(user);
	auto * screen = a ? static_cast<OptionsControlsScreen *>(a->user) : nullptr;
	if(screen) screen->NotifyOperatorClicked(a->arg0);
}

void RegisterButton(const char * label,
                    int x,
                    int y,
                    int w,
                    int h,
                    void (*onClick)(void *),
                    void * user,
                    int rowIndex = -1)
{
	silencer::ui::automation::Widget widget;
	widget.label = label;
	widget.kind = rowIndex >= 0
		? silencer::ui::automation::WidgetKind::ListRow
		: silencer::ui::automation::WidgetKind::Button;
	widget.x = x; widget.y = y; widget.w = w; widget.h = h;
	widget.onClick = onClick;
	widget.clickUser = user;
	widget.onClickRow = nullptr;
	widget.rowIndex = rowIndex;
	silencer::ui::automation::Register(widget);
}

void RegisterWidgets(OptionsControlsScreen * screen, int surfaceW)
{
	const int panelX = (surfaceW - kPanelW) / 2;
	const int panelY = 34;
	const int x = panelX + kPanelPadX;
	int y = panelY + kPanelPadY + 34;
	RegisterButton("Preset", x + kActionNameW, y, 220, 33, &PresetClicked, screen);
	y += kRowH + kRowGap;
	for(int i = 0; i < VISIBLE_ROWS; i++){
		RegisterButton("Primary binding", x + kActionNameW, y + 4, kKeyButtonW,
		               kKeyButtonH, &BindClicked, screen, i);
		RegisterButton("Binding operator", x + kActionNameW + kKeyButtonW + 12,
		               y + 4, kOperatorW, kKeyButtonH, &OperatorClicked, screen, i);
		RegisterButton("Secondary binding",
		               x + kActionNameW + kKeyButtonW + 12 + kOperatorW + 12,
		               y + 4, kKeyButtonW, kKeyButtonH, &BindClicked, screen, i);
		y += kRowH + kRowGap;
	}
	RegisterButton("Scroll Up", panelX + kPanelW - 42, panelY + 92, 24, 24,
	               &ScrollUpClicked, screen);
	RegisterButton("Scroll Down", panelX + kPanelW - 42, panelY + 280, 24, 24,
	               &ScrollDownClicked, screen);
	RegisterButton("Save", panelX + 102, panelY + 338, 156, 21, &SaveClicked, screen);
	RegisterButton("Cancel", panelX + 282, panelY + 338, 156, 21, &CancelClicked, screen);
}

bool IsBuiltinKeybindProfile(const std::string & name)
{
	return name == "default" || name == "wasd" || name == "gamepad";
}

void ButtonElement(Clay_String id,
                   Clay_String text,
                   uint16_t width,
                   uint16_t height,
                   uint16_t imageIndex,
                   BankTextVariant textVariant,
                   void (*onClick)(void *),
                   void * user)
{
	if(onClick){
		silencer::ui::automation::Widget widget;
		widget.id = ToStd(id);
		widget.labelText = AutomationLabel(id, text);
		widget.label = widget.labelText.c_str();
		widget.kind = silencer::ui::automation::WidgetKind::Button;
		widget.onClick = onClick;
		widget.clickUser = user;
		widget.clayId = CLAY_SID(id);
		widget.hasClayId = true;
		silencer::ui::automation::Register(widget);
	}
	CLAY({ .id = CLAY_SID(id),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(width)),
	                       CLAY_SIZING_FIXED(static_cast<float>(height)) },
	           .padding = { 0, 0,
	                        static_cast<uint16_t>(height > 25 ? 8 : 4), 0 },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
	       },
	       .image = { .imageData = silencer::clay_bridge::PackImage(6, imageIndex) } }) {
		bool hovered = ::Clay_Hovered();
		BankText(text, textVariant,
		         { .brightness = hovered ? static_cast<Uint8>(136)
		                                  : static_cast<Uint8>(128) });
	}
}

void TextButton(Clay_String id,
                Clay_String text,
                uint16_t width,
                uint16_t height,
                void (*onClick)(void *),
                void * user)
{
	if(onClick){
		silencer::ui::automation::Widget widget;
		widget.id = ToStd(id);
		widget.labelText = AutomationLabel(id, text);
		widget.label = widget.labelText.c_str();
		widget.kind = silencer::ui::automation::WidgetKind::Button;
		widget.onClick = onClick;
		widget.clickUser = user;
		widget.clayId = CLAY_SID(id);
		widget.hasClayId = true;
		silencer::ui::automation::Register(widget);
	}
	CLAY({ .id = CLAY_SID(id),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(width)),
	                       CLAY_SIZING_FIXED(static_cast<float>(height)) },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
	       } }) {
		bool hovered = ::Clay_Hovered();
		BankText(text, BankTextVariant::Heading,
		         { .brightness = hovered ? static_cast<Uint8>(136)
		                                  : static_cast<Uint8>(128) });
	}
}

void RowActionButton(Clay_String id,
                     const std::string & text,
                     int row,
                     int slot,
                     bool rebinding,
                     OptionsControlsScreen * screen)
{
	std::string display = rebinding ? "-" : text;
	auto * adapter = AllocAdapter(&BindClicked, screen, row, slot);
	ButtonElement(id, FromStd(display), kKeyButtonW, kKeyButtonH, 28,
	              BankTextVariant::Title, adapter ? &BindClicked : nullptr,
	              adapter);
}

void RowOperatorButton(Clay_String id,
                       const char * text,
                       int row,
                       OptionsControlsScreen * screen)
{
	auto * adapter = AllocAdapter(&OperatorClicked, screen, row, 0);
	TextButton(id, FromCStr(text), kOperatorW, kKeyButtonH,
	           adapter ? &OperatorClicked : nullptr, adapter);
}
}

OptionsControlsScreen::LegacyView OptionsControlsScreen::ViewLegacy(const KeyMap & km, Action a)
{
	LegacyView v;
	const auto & ab = km.Get(a);
	if(ab.bindings.empty()) return v;
	const auto & b0 = ab.bindings[0];
	if(b0.keys.size() >= 2 &&
	   b0.keys[0].device == BindingDevice::Keyboard &&
	   b0.keys[1].device == BindingDevice::Keyboard){
		v.key1 = (SDL_Scancode)b0.keys[0].code;
		v.key2 = (SDL_Scancode)b0.keys[1].code;
		v.and_ = true;
		return v;
	}
	if(!b0.keys.empty() && b0.keys[0].device == BindingDevice::Keyboard){
		v.key1 = (SDL_Scancode)b0.keys[0].code;
	}
	if(ab.bindings.size() >= 2){
		const auto & b1 = ab.bindings[1];
		if(!b1.keys.empty() && b1.keys[0].device == BindingDevice::Keyboard){
			v.key2 = (SDL_Scancode)b1.keys[0].code;
		}
	}
	return v;
}

void OptionsControlsScreen::WriteLegacy(KeyMap & km, Action a, SDL_Scancode key1, SDL_Scancode key2, bool and_)
{
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

std::string OptionsControlsScreen::GetBindingLabel(ScreenContext & ctx, Action a, int slot) const
{
	const auto & ab = ctx.keymap.Get(a);
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
			SDL_Gamepad * pad = ctx.game.GetGamepad();
			return GamepadShortLabel(raw, pad ? SDL_GetGamepadType(pad) : SDL_GAMEPAD_TYPE_UNKNOWN);
		}
		found++;
	}
	return KeyMap::GetKeyName(SDL_SCANCODE_UNKNOWN);
}

int OptionsControlsScreen::MaxScroll() const
{
	int max = (int)Action::Count - VISIBLE_ROWS;
	return max < 0 ? 0 : max;
}

void OptionsControlsScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(1);
	ctx.renderer.camera.SetPosition(320, 240);
	scrollPosition = 0;
	rebindRow = -1;
	rebindSlot = -1;
	presetClicked = false;
	saveClicked = false;
	cancelClicked = false;
	scrollDelta = 0;
	operatorClickedRow = -1;
}

void OptionsControlsScreen::NotifyBindClicked(int row, int slot)
{
	int absolute = scrollPosition + row;
	if(absolute < 0 || absolute >= (int)Action::Count) return;
	rebindRow = absolute;
	rebindSlot = slot;
}

void OptionsControlsScreen::NotifyOperatorClicked(int row)
{
	int absolute = scrollPosition + row;
	if(absolute < 0 || absolute >= (int)Action::Count) return;
	operatorClickedRow = absolute;
}

void OptionsControlsScreen::FinishKeyboardRebind(ScreenContext & ctx, SDL_Scancode sym)
{
	if(rebindRow < 0 || rebindRow >= (int)Action::Count) return;
#ifndef OUYA
	if(sym == SDL_SCANCODE_ESCAPE) sym = SDL_SCANCODE_UNKNOWN;
#endif
	Action a = ACTION_TABLE[rebindRow].action;
	LegacyView v = ViewLegacy(ctx.keymap, a);
	if(rebindSlot == 0) v.key1 = sym; else v.key2 = sym;
	ForkActiveProfileIfBuiltin(ctx.keymap);
	WriteLegacy(ctx.keymap, a, v.key1, v.key2, v.and_);
	rebindRow = -1;
	rebindSlot = -1;
}

void OptionsControlsScreen::FinishGamepadRebind(ScreenContext & ctx)
{
	if(rebindRow < 0 || rebindRow >= (int)Action::Count) return;
	const GamepadState & gp = ctx.game.GetGamepadState();
	if(!gp.connected) return;
	BindingKey padKey{};
	bool found = false;
	for(int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT && !found; b++){
		bool was = (rebindGamepadButtons >> b) & 1;
		bool is = (gp.buttons >> b) & 1;
		if(is && !was){
			padKey.device = BindingDevice::GamepadButton;
			padKey.code = b;
			padKey.axisDir = 0;
			found = true;
		}
	}
	for(int ax = 0; ax < SDL_GAMEPAD_AXIS_COUNT && !found; ax++){
		int16_t was = rebindGamepadAxes[ax];
		int16_t is = gp.axes[ax];
		if(std::abs(is) > AXIS_DEADZONE && std::abs(was) <= AXIS_DEADZONE){
			padKey.device = BindingDevice::GamepadAxis;
			padKey.code = ax;
			padKey.axisDir = (is > 0) ? 1 : -1;
			found = true;
		}
	}
	if(!found) return;
	ForkActiveProfileIfBuiltin(ctx.keymap);
	auto & ab = ctx.keymap.Get(ACTION_TABLE[rebindRow].action);
	Binding binding;
	binding.keys.push_back(padKey);
	if(rebindSlot == 0){
		if(ab.bindings.empty()) ab.bindings.push_back(binding);
		else ab.bindings[0] = binding;
	}else{
		if(ab.bindings.empty()) ab.bindings.push_back(Binding{});
		if(ab.bindings.size() < 2) ab.bindings.push_back(binding);
		else ab.bindings[1] = binding;
	}
	rebindRow = -1;
	rebindSlot = -1;
}

void OptionsControlsScreen::Tick(ScreenContext & ctx)
{
	if(scrollDelta != 0){
		scrollPosition = std::max(0, std::min(MaxScroll(), scrollPosition + scrollDelta));
		scrollDelta = 0;
	}
	if(presetClicked){
		presetClicked = false;
		CycleKeybindPreset(ctx.keymap);
	}
	if(operatorClickedRow >= 0 && operatorClickedRow < (int)Action::Count){
		Action a = ACTION_TABLE[operatorClickedRow].action;
		LegacyView v = ViewLegacy(ctx.keymap, a);
		v.and_ = !v.and_;
		ForkActiveProfileIfBuiltin(ctx.keymap);
		WriteLegacy(ctx.keymap, a, v.key1, v.key2, v.and_);
		operatorClickedRow = -1;
	}
	if(rebindRow >= 0){
		if(optionscontrolstick == 0){
			optionscontrolstick = ctx.world.tickcount;
			const GamepadState & gp = ctx.game.GetGamepadState();
			rebindGamepadButtons = gp.buttons;
			memcpy(rebindGamepadAxes, gp.axes, sizeof(rebindGamepadAxes));
		}
		FinishGamepadRebind(ctx);
		if(rebindRow >= 0 &&
		   ctx.world.tickcount - optionscontrolstick > REBIND_TIMEOUT_TICKS){
			FinishKeyboardRebind(ctx, SDL_SCANCODE_UNKNOWN);
		}
	}else{
		optionscontrolstick = 0;
	}
	if(saveClicked){
		saveClicked = false;
		const std::string active = Config::GetInstance().active_keybind_profile;
		if(!IsBuiltinKeybindProfile(active)){
			ctx.keymap.SaveFile(WritableProfilePath(active));
		}
		Config::GetInstance().Save();
		ctx.GoToState(GameState::OPTIONS);
		return;
	}
	if(cancelClicked){
		cancelClicked = false;
		LoadActiveKeymap(ctx.keymap);
		Config::GetInstance().Load();
		ctx.GoToState(GameState::OPTIONS);
	}
}

bool OptionsControlsScreen::CaptureRawKeyDown(ScreenContext & ctx, int keyCode)
{
	if(rebindRow < 0) return false;
	FinishKeyboardRebind(ctx, static_cast<SDL_Scancode>(keyCode));
	return true;
}

void OptionsControlsScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime)
{
	(void)frametime;
	using namespace silencer::clay_bridge;


	g_adapterCount = 0;

	std::string presetText = !ctx.keymap.label.empty() ? ctx.keymap.label
	                       : !ctx.keymap.name.empty() ? ctx.keymap.name
	                       : std::string(Config::GetInstance().active_keybind_profile);
	std::string actionLabels[VISIBLE_ROWS];
	std::string primaryLabels[VISIBLE_ROWS];
	std::string secondaryLabels[VISIBLE_ROWS];
	std::string operatorLabels[VISIBLE_ROWS];
	std::string primaryIds[VISIBLE_ROWS];
	std::string secondaryIds[VISIBLE_ROWS];
	std::string operatorIds[VISIBLE_ROWS];
	bool rebindingPrimary[VISIBLE_ROWS] = {};
	bool rebindingSecondary[VISIBLE_ROWS] = {};
	for(int i = 0; i < VISIBLE_ROWS; i++){
		int row = scrollPosition + i;
		if(row >= (int)Action::Count) break;
		Action action = ACTION_TABLE[row].action;
		LegacyView v = ViewLegacy(ctx.keymap, action);
		actionLabels[i] = std::string(GetActionInfo(action).label) + ":";
		primaryLabels[i] = GetBindingLabel(ctx, action, 0);
		secondaryLabels[i] = GetBindingLabel(ctx, action, 1);
		operatorLabels[i] = v.and_ ? "AND" : "OR";
		primaryIds[i] = "Primary" + std::to_string(i);
		secondaryIds[i] = "Secondary" + std::to_string(i);
		operatorIds[i] = "Operator" + std::to_string(i);
		rebindingPrimary[i] = (rebindRow == row && rebindSlot == 0);
		rebindingSecondary[i] = (rebindRow == row && rebindSlot == 1);
	}

	CLAY({ .id = CLAY_ID("OptionsControlsRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED((float)dst.w),
	                       CLAY_SIZING_FIXED((float)dst.h) },
	           .padding = { 0, 0, 34, 0 },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
	       },
	       .image = { .imageData = PackImage(6, 0) } }) {
		CLAY({ .id = CLAY_ID("OptionsControlsPanel"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(kPanelW),
		                       CLAY_SIZING_FIT(0) },
		           .padding = { kPanelPadX, kPanelPadX,
		                        kPanelPadY, kPanelPadY },
		           .childGap = 12,
		           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .image = { .imageData = PackImage(7, 7) } }) {
			BankText(CLAY_STRING("Configure Controls"), BankTextVariant::Title, {});
			CLAY({ .id = CLAY_ID("ControlsPresetRow"),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(kRowH) },
			           .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       } }) {
				CLAY({ .id = CLAY_ID("PresetLabel"),
				       .layout = {
				           .sizing = { CLAY_SIZING_FIXED(kActionNameW),
				                       CLAY_SIZING_FIT(0) },
				       } }) {
					BankText(CLAY_STRING("Preset:"), BankTextVariant::Heading, {});
				}
				ButtonElement(CLAY_STRING("PresetButton"), FromStd(presetText), 220, 33,
				              23, BankTextVariant::Title, &PresetClicked, this);
			}

			CLAY({ .id = CLAY_ID("ControlsRows"),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
			           .childGap = kRowGap,
			           .layoutDirection = CLAY_TOP_TO_BOTTOM,
			       } }) {
				for(int i = 0; i < VISIBLE_ROWS; i++){
					int row = scrollPosition + i;
					if(row >= (int)Action::Count) break;
					CLAY({ .id = CLAY_IDI("ControlsRow", (uint32_t)i),
					       .layout = {
					           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(kRowH) },
					           .childGap = 12,
					           .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
					           .layoutDirection = CLAY_LEFT_TO_RIGHT,
					       } }) {
						CLAY({ .id = CLAY_IDI("ControlsAction", (uint32_t)i),
						       .layout = {
						           .sizing = { CLAY_SIZING_FIXED(kActionNameW),
						                       CLAY_SIZING_FIT(0) },
						       } }) {
							BankText(FromStd(actionLabels[i]), BankTextVariant::Heading, {});
						}
						RowActionButton(FromStd(primaryIds[i]),
						                primaryLabels[i], i, 0, rebindingPrimary[i], this);
						RowOperatorButton(FromStd(operatorIds[i]),
						                  operatorLabels[i].c_str(), i, this);
						RowActionButton(FromStd(secondaryIds[i]),
						                secondaryLabels[i], i, 1, rebindingSecondary[i], this);
					}
				}
			}

			CLAY({ .id = CLAY_ID("ControlsScrollRow"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
			           .childGap = 20,
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       } }) {
				TextButton(CLAY_STRING("ScrollUp"), CLAY_STRING("Up"), 60, 24,
				           &ScrollUpClicked, this);
				TextButton(CLAY_STRING("ScrollDown"), CLAY_STRING("Down"), 80, 24,
				           &ScrollDownClicked, this);
			}

			CLAY({ .id = CLAY_ID("ControlsActions"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
			           .childGap = kActionGap,
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       } }) {
				ButtonElement(CLAY_STRING("Save"), CLAY_STRING("Save"), 156, 21, 24,
				              BankTextVariant::Heading, &SaveClicked, this);
				ButtonElement(CLAY_STRING("Cancel"), CLAY_STRING("Cancel"), 156, 21, 24,
				              BankTextVariant::Heading, &CancelClicked, this);
			}
		}
	}
}

void OptionsControlsScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}
