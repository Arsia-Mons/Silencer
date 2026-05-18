#include "primitives/box.h"

#include "clay_ui_payloads.h"

namespace silencer::ui::primitives {

namespace {

constexpr int kPayloadCapacity = 64;
silencer::clay_bridge::BoxStrokePayload g_payloads[kPayloadCapacity];
int g_payloadCount = 0;

constexpr int kCustomDataCapacity = 64;
silencer::clay_bridge::ClayCustomData g_customData[kCustomDataCapacity];
int g_customDataCount = 0;

silencer::clay_bridge::BoxStrokePayload *
AllocPayload(const BoxStrokeStyle & s) {
	if(g_payloadCount >= kPayloadCapacity) return nullptr;
	auto * p = &g_payloads[g_payloadCount++];
	p->strokeColor    = s.strokeColor;
	p->strokeWidth    = s.strokeWidth;
	p->outerHaloColor = s.outerHaloColor;
	p->outerHaloWidth = s.outerHaloWidth;
	p->innerHaloColor = s.innerHaloColor;
	p->innerHaloWidth = s.innerHaloWidth;
	p->haloOpacity    = s.haloOpacity;
	p->glowWidth      = s.glowWidth;
	p->sides          = s.sides;
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

}  // namespace

void BoxBeginFrame() {
	g_payloadCount = 0;
	g_customDataCount = 0;
}

Clay_ElementDeclaration Box(BoxStrokeStyle style, Clay_ElementDeclaration extras) {
	bool haveHalos = (style.outerHaloWidth > 0) || (style.innerHaloWidth > 0) ||
	                 (style.glowWidth > 0);

	if(haveHalos){
		// Route the entire stroke (primary + halos/glow) through
		// CustomKind::BoxStroke so the bridge renders concentric rings.
		// Native .border stays untouched — the CUSTOM command supplies
		// the stroke.
		auto * payload = AllocPayload(style);
		auto * ccd     = AllocCustomData(
			silencer::clay_bridge::CustomKind::BoxStroke, payload);
		extras.custom.customData = ccd;
		return extras;
	}

	if(style.strokeWidth > 0){
		// No halos → Clay's native .border path (cheap; no CUSTOM
		// dispatch). Per-side widths come from the side mask; suppressed
		// sides get width 0. The bridge reads palette idx from `.r`.
		Uint8 sides = style.sides ? style.sides : BoxSides::All;
		Uint16 w = style.strokeWidth;
		extras.border.color = {
			/*r=*/static_cast<float>(style.strokeColor),
			/*g=*/0.0f, /*b=*/0.0f, /*a=*/255.0f,
		};
		extras.border.width = {
			/*left=*/  (sides & BoxSides::Left)   ? w : (Uint16)0,
			/*right=*/ (sides & BoxSides::Right)  ? w : (Uint16)0,
			/*top=*/   (sides & BoxSides::Top)    ? w : (Uint16)0,
			/*bottom=*/(sides & BoxSides::Bottom) ? w : (Uint16)0,
			/*betweenChildren=*/0,
		};
	}

	return extras;
}

}  // namespace silencer::ui::primitives
