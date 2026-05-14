#include "toggle.h"

#include "clay_ui_payloads.h"
#include "runtime/UiAutomationRegistry.h"

#include <string>

namespace silencer::ui::primitives {

namespace {

constexpr int kPayloadCapacity = 256;
silencer::clay_bridge::TogglePayload g_payloads[kPayloadCapacity];
int g_payloadCount = 0;

constexpr int kCustomDataCapacity = 256;
silencer::clay_bridge::ClayCustomData g_customData[kCustomDataCapacity];
int g_customDataCount = 0;

std::string ToStd(Clay_String text) {
	return std::string(text.chars ? text.chars : "", static_cast<size_t>(text.length));
}

silencer::clay_bridge::TogglePayload *
AllocPayload(Uint8 bank, Uint16 index, Uint8 effectColor, Uint8 brightness) {
	if(g_payloadCount >= kPayloadCapacity) return nullptr;
	auto * p = &g_payloads[g_payloadCount++];
	p->bank = bank;
	p->index = index;
	p->effectColor = effectColor;
	p->brightness = brightness;
	return p;
}

silencer::clay_bridge::ClayCustomData *
AllocCustomData(silencer::clay_bridge::CustomKind kind, void * payload) {
	if(g_customDataCount >= kCustomDataCapacity) return nullptr;
	auto * c = &g_customData[g_customDataCount++];
	c->kind = kind;
	c->payload = payload;
	return c;
}

void RegisterToggleWidget(Clay_String id,
                          bool selected,
                          ToggleHandle handle) {
	if(!handle.actionId || !*handle.actionId) return;
	silencer::ui::automation::Widget widget;
	widget.id = handle.actionId;
	widget.labelText = ToStd(id);
	widget.kind = UiAutomationWidgetKind::Toggle;
	widget.selected = selected;
	widget.clayId = CLAY_SID(id);
	widget.hasClayId = true;
	silencer::ui::automation::Register(widget);
}

}  // namespace

void ToggleBeginFrame() {
	g_payloadCount = 0;
	g_customDataCount = 0;
}

void Toggle(Clay_String id,
            Uint8 spriteBank,
            Uint16 spriteIndex,
            bool selected,
            ToggleOpts opts,
            ToggleHandle handle) {
	Uint8 brightness =
		selected ? opts.selectedBrightness : opts.unselectedBrightness;
	auto * payload = AllocPayload(
		spriteBank, spriteIndex, opts.effectColor, brightness);
	auto * ccd = AllocCustomData(
		silencer::clay_bridge::CustomKind::ToggleSprite, payload);

	const float boxW = static_cast<float>(opts.width);
	const float boxH = static_cast<float>(opts.height);
	CLAY({ .id = CLAY_SID(id),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(boxW),
	                       CLAY_SIZING_FIXED(boxH) },
	       },
	       .custom = { .customData = ccd } }) {
		bool hovered = ::Clay_Hovered();
		if(handle.hoveredOut) *handle.hoveredOut = hovered;
		RegisterToggleWidget(id, selected, handle);
	}
}

}  // namespace silencer::ui::primitives
