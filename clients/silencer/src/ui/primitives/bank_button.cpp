#include "bank_button.h"

#include "bank_text.h"
#include "clay_ui_payloads.h"
#include "runtime/UiInteractionRegistry.h"

#include <string>

namespace silencer::ui::primitives {

namespace {

// Per-frame bump arenas for the three transient payload types attached to
// BankButton elements. Pointer-stable for the duration of one Clay layout
// pass (Clay captures the pointers into render commands at EndLayout).

constexpr int kChromePayloadCapacity = 256;
silencer::clay_bridge::BankButtonChromePayload g_chromePayloads[kChromePayloadCapacity];
int g_chromePayloadCount = 0;

constexpr int kCustomDataCapacity = 256;
silencer::clay_bridge::ClayCustomData g_customData[kCustomDataCapacity];
int g_customDataCount = 0;

std::string ToStd(Clay_String text) {
	return std::string(text.chars ? text.chars : "", static_cast<size_t>(text.length));
}

silencer::clay_bridge::BankButtonChromePayload *
AllocChromePayload(Uint8 bank, Uint16 index, Uint8 brightness) {
	if(g_chromePayloadCount >= kChromePayloadCapacity) return nullptr;
	auto * p = &g_chromePayloads[g_chromePayloadCount++];
	p->bank = bank;
	p->index = index;
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

void RegisterButtonWidget(Clay_String label,
                          UiInteractableKind kind,
                          BankButtonHandle handle,
                          bool selected) {
	if(!handle.interactions || !handle.actionId || !*handle.actionId) return;
	silencer::ui::UiInteractable widget;
	widget.id = handle.actionId;
	widget.labelText = ToStd(label);
	widget.kind = kind;
	widget.selected = selected;
	widget.clayId = CLAY_SID(label);
	widget.hasClayId = true;
	handle.interactions->RegisterInteractable(widget);
}

}  // namespace

void BankButtonBeginFrame() {
	g_chromePayloadCount = 0;
	g_customDataCount = 0;
}

void BankButton(Clay_String label,
                BankButtonVariant variant,
                BankButtonOpts opts,
                BankButtonHandle handle) {
	switch(variant){
		case BankButtonVariant::Chrome: {
			// Allocate the chrome payload up front so it can be referenced by
			// the CLAY declaration. brightness starts neutral; if hovered we
			// mutate the payload in place inside the block (Clay holds the
			// pointer by value into the render command at EndLayout, so the
			// mutation is visible to the bridge).
			auto * payload = AllocChromePayload(7, 24, 128);
			auto * ccd = AllocCustomData(
				silencer::clay_bridge::CustomKind::BankButtonChrome, payload);

			CLAY({ .id = CLAY_SID(label),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIXED(156), CLAY_SIZING_FIXED(21) },
			           .padding = { 0, 0, 4, 0 },  // legacy yoff=4 for B156x21 text.
			           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
			       },
			       .custom = { .customData = ccd } }) {
				bool hovered = ::Clay_Hovered();
				if(payload) payload->brightness = hovered ? 136 : 128;
				if(handle.hoveredOut) *handle.hoveredOut = hovered;
				RegisterButtonWidget(label, UiInteractableKind::Button, handle, opts.selected);
				BankText(label,
				         BankTextVariant::Heading,
				         { .effectColor = opts.effectColor,
				           .brightness = hovered ? static_cast<Uint8>(136)
				                                 : static_cast<Uint8>(128) });
			}
			break;
		}
		case BankButtonVariant::Inline: {
			CLAY({ .id = CLAY_SID(label),
			       .layout = {
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       } }) {
				bool hovered = ::Clay_Hovered();
				if(handle.hoveredOut) *handle.hoveredOut = hovered;
				RegisterButtonWidget(label, UiInteractableKind::Button, handle, opts.selected);
				Uint8 brightness = opts.textBrightness;
				if(brightness == 128 && hovered) brightness = 136;
				BankText(label,
				         BankTextVariant::BodySm,
				         { .effectColor = opts.effectColor,
				           .brightness = brightness });
			}
			break;
		}
		case BankButtonVariant::Checkbox: {
			Uint16 idx = opts.selected ? 18 : 19;
			CLAY({ .id = CLAY_SID(label),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIXED(13), CLAY_SIZING_FIXED(13) },
			       },
			       .image = { .imageData =
			                    silencer::clay_bridge::PackImage(7, idx) } }) {
				bool hovered = ::Clay_Hovered();
				if(handle.hoveredOut) *handle.hoveredOut = hovered;
				RegisterButtonWidget(label, UiInteractableKind::Toggle, handle, opts.selected);
			}
			break;
		}
	}
}

}  // namespace silencer::ui::primitives
