#include "primitives/box.h"

#include "clay_bridge.h"

namespace silencer::ui::primitives {

namespace {

// Per-frame payload arena. Sized for chrome-scale use (one Box per panel
// times a handful of halo-using scenes per frame is well under 64).
constexpr int kPayloadCapacity = 64;
silencer::clay_bridge::BoxStrokePayload g_payloads[kPayloadCapacity];
int g_payloadCount = 0;

constexpr int kCustomDataCapacity = 64;
silencer::clay_bridge::ClayCustomData g_customData[kCustomDataCapacity];
int g_customDataCount = 0;

silencer::clay_bridge::BoxStrokePayload *
AllocPayload(Uint8 strokeColor, Uint8 strokeWidth,
             Uint8 outerHaloColor, Uint8 outerHaloWidth,
             Uint8 innerHaloColor, Uint8 innerHaloWidth) {
	if(g_payloadCount >= kPayloadCapacity) return nullptr;
	auto * p = &g_payloads[g_payloadCount++];
	p->strokeColor     = strokeColor;
	p->strokeWidth     = strokeWidth;
	p->outerHaloColor  = outerHaloColor;
	p->outerHaloWidth  = outerHaloWidth;
	p->innerHaloColor  = innerHaloColor;
	p->innerHaloWidth  = innerHaloWidth;
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

void Box(Clay_String id,
         Uint16 width,
         Uint16 height,
         Uint8  fillPaletteColor,
         Uint8  fillOpacity,
         Uint8  strokePaletteColor,
         Uint8  strokeWidth,
         Uint8  strokeOuterHaloColor,
         Uint8  strokeOuterHaloWidth,
         Uint8  strokeInnerHaloColor,
         Uint8  strokeInnerHaloWidth) {
	// Sizing: 0 on either axis means "grow into the parent's available space"
	// (use inside a flex container). Non-zero is a fixed pixel size.
	Clay_SizingAxis xSizing = (width == 0)
		? CLAY_SIZING_GROW(0)
		: CLAY_SIZING_FIXED(static_cast<float>(width));
	Clay_SizingAxis ySizing = (height == 0)
		? CLAY_SIZING_GROW(0)
		: CLAY_SIZING_FIXED(static_cast<float>(height));

	// Fill: Clay only emits a RECTANGLE render command when
	// `.backgroundColor.a > 0`. Set alpha to the requested opacity (which
	// the C1 bridge routes through the palette alpha-blend LUT).
	// `.r` carries the palette index for the bridge.
	Clay_Color bg{
		/*r=*/static_cast<float>(fillPaletteColor),
		/*g=*/0.0f,
		/*b=*/0.0f,
		/*a=*/static_cast<float>(fillOpacity),
	};

	bool haveHalos = (strokeOuterHaloWidth > 0) || (strokeInnerHaloWidth > 0);

	if(haveHalos){
		// Route the entire stroke (primary + halos) through CustomKind::BoxStroke
		// so the bridge can render concentric rings. Skip .border — the CUSTOM
		// command supplies the stroke.
		auto * payload = AllocPayload(
			strokePaletteColor, strokeWidth,
			strokeOuterHaloColor, strokeOuterHaloWidth,
			strokeInnerHaloColor, strokeInnerHaloWidth);
		auto * ccd = AllocCustomData(
			silencer::clay_bridge::CustomKind::BoxStroke, payload);
		CLAY({
			.id = Clay_GetElementId(id),
			.layout = { .sizing = { xSizing, ySizing } },
			.backgroundColor = bg,
			.custom = { .customData = ccd },
		}) {}
		return;
	}

	// No halos: Clay's native .border emit (cheap; no CUSTOM dispatch).
	// strokeWidth==0 disables the border entirely.
	Clay_BorderElementConfig border{
		.color = {
			/*r=*/static_cast<float>(strokePaletteColor),
			/*g=*/0.0f, /*b=*/0.0f, /*a=*/255.0f,
		},
		.width = {
			/*left=*/strokeWidth, /*right=*/strokeWidth,
			/*top=*/strokeWidth, /*bottom=*/strokeWidth,
			/*betweenChildren=*/0,
		},
	};

	CLAY({
		.id = Clay_GetElementId(id),
		.layout = { .sizing = { xSizing, ySizing } },
		.backgroundColor = bg,
		.border = border,
	}) {}
}

}  // namespace silencer::ui::primitives
