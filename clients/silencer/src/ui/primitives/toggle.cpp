#include "toggle.h"

#include "clay_ui_payloads.h"

#include <cstdint>

namespace silencer::ui::primitives {

namespace {

struct ClickAdapter {
	ToggleClickFn fn;
	void *        user;
};

constexpr int kAdapterCapacity = 256;
ClickAdapter g_adapters[kAdapterCapacity];
int g_adapterCount = 0;

constexpr int kPayloadCapacity = 256;
silencer::clay_bridge::TogglePayload g_payloads[kPayloadCapacity];
int g_payloadCount = 0;

constexpr int kCustomDataCapacity = 256;
silencer::clay_bridge::ClayCustomData g_customData[kCustomDataCapacity];
int g_customDataCount = 0;

ClickAdapter * AllocAdapter(ToggleClickFn fn, void * user) {
	if(g_adapterCount >= kAdapterCapacity) return nullptr;
	auto * a = &g_adapters[g_adapterCount++];
	a->fn = fn;
	a->user = user;
	return a;
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

void ClickProxy(::Clay_ElementId /*id*/,
                ::Clay_PointerData data,
                std::intptr_t userPtr) {
	if(data.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
	auto * a = reinterpret_cast<ClickAdapter *>(userPtr);
	if(a && a->fn) a->fn(a->user);
}

}  // namespace

void ToggleBeginFrame() {
	g_adapterCount = 0;
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
		if(handle.onClick){
			auto * a = AllocAdapter(handle.onClick, handle.user);
			if(a) ::Clay_OnHover(ClickProxy, reinterpret_cast<std::intptr_t>(a));
		}
	}
}

}  // namespace silencer::ui::primitives
