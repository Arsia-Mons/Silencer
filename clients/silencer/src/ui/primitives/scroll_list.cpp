#include "scroll_list.h"

#include "clay_ui_payloads.h"

#include <cstdint>

namespace silencer::ui::primitives {

namespace {

// Per-row click adapter. We need one allocation per row so the proxy
// callback can route to the right index — Clay_OnHover threads a single
// void* through to the proxy.
struct RowAdapter {
	ScrollListSelectFn fn;
	void *             user;
	int                index;
};

constexpr int kAdapterCapacity = 1024;
RowAdapter g_adapters[kAdapterCapacity];
int g_adapterCount = 0;

constexpr int kPayloadCapacity = 64;
silencer::clay_bridge::ScrollBarPayload g_payloads[kPayloadCapacity];
int g_payloadCount = 0;

constexpr int kCustomDataCapacity = 64;
silencer::clay_bridge::ClayCustomData g_customData[kCustomDataCapacity];
int g_customDataCount = 0;

RowAdapter * AllocAdapter(ScrollListSelectFn fn, void * user, int index) {
	if(g_adapterCount >= kAdapterCapacity) return nullptr;
	auto * a = &g_adapters[g_adapterCount++];
	a->fn = fn;
	a->user = user;
	a->index = index;
	return a;
}

silencer::clay_bridge::ScrollBarPayload *
AllocScrollBarPayload(Uint8 bank, Uint16 trackIdx, Uint16 thumbIdx,
                      Uint16 scrollPosition, Uint16 scrollMax) {
	if(g_payloadCount >= kPayloadCapacity) return nullptr;
	auto * p = &g_payloads[g_payloadCount++];
	p->bank = bank;
	p->trackIndex = trackIdx;
	p->thumbIndex = thumbIdx;
	p->scrollPosition = scrollPosition;
	p->scrollMax = scrollMax;
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
	auto * a = reinterpret_cast<RowAdapter *>(userPtr);
	if(a && a->fn) a->fn(a->user, a->index);
}

}  // namespace

void ScrollListBeginFrame() {
	g_adapterCount = 0;
	g_payloadCount = 0;
	g_customDataCount = 0;
}

void ScrollList(Clay_String id,
                const Clay_String * items,
                int itemCount,
                int selectedIndex,
                Uint16 scrollPosition,
                ScrollListOpts opts,
                ScrollListHandle handle) {
	const int visibleLines = (opts.lineHeight > 0)
		? (opts.height / opts.lineHeight)
		: 0;
	int scrollMax = itemCount - visibleLines;
	if(scrollMax < 0) scrollMax = 0;
	if(scrollPosition > scrollMax) scrollPosition = static_cast<Uint16>(scrollMax);

	const float rootW = static_cast<float>(opts.width);
	const float rootH = static_cast<float>(opts.height);
	// Mirror the legacy `scrollbar->draw = items.size() > visible` toggle:
	// the scrollbar only renders when the list actually overflows. The rows
	// column reclaims the scrollbar's pixels when nothing overflows.
	const bool  drawSb = scrollMax > 0;
	const float sbW   = drawSb ? static_cast<float>(opts.scrollbarWidth) : 0.0f;
	const float gap   = drawSb ? static_cast<float>(opts.scrollbarGap) : 0.0f;
	float rowsW       = rootW - sbW - gap;
	if(rowsW < 0.0f) rowsW = 0.0f;
	const float rowH  = static_cast<float>(opts.lineHeight);

	CLAY({ .id = CLAY_SID(id),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(rootW), CLAY_SIZING_FIXED(rootH) },
	           .childGap = static_cast<Uint16>(opts.scrollbarGap),
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       } }) {
		bool rootHovered = ::Clay_Hovered();
		if(handle.hoveredOut) *handle.hoveredOut = rootHovered;

		// Row column.
		CLAY({ .id = CLAY_SIDI(id, 0),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(rowsW), CLAY_SIZING_FIXED(rootH) },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       } }) {
			for(int line = 0; line < visibleLines; line++){
				const int i = static_cast<int>(scrollPosition) + line;
				if(i < 0 || i >= itemCount) break;
				const bool isSelected = (i == selectedIndex);
				const Uint8 bgIdx = isSelected ? opts.highlightColor : 0;

				CLAY({ .id = CLAY_SIDI(id, static_cast<uint32_t>(i + 1)),
				       .layout = {
				           .sizing = { CLAY_SIZING_FIXED(rowsW),
				                       CLAY_SIZING_FIXED(rowH) },
				       },
				       .backgroundColor = { static_cast<float>(bgIdx),
				                            0.0f, 0.0f, 255.0f } }) {
					if(handle.onSelect){
						auto * a = AllocAdapter(handle.onSelect, handle.user, i);
						if(a) ::Clay_OnHover(ClickProxy,
						                    reinterpret_cast<std::intptr_t>(a));
					}
					BankText(items[i], opts.textVariant,
					         { .effectColor = opts.textEffectColor });
				}
			}
		}

		if(drawSb){
			// Scrollbar — only emit when overflow exists. Mirrors the
			// legacy `scrollbar->draw = true/false` toggle.
			auto * payload = AllocScrollBarPayload(
				opts.scrollbarBank,
				opts.scrollbarTrackIndex,
				opts.scrollbarThumbIndex,
				scrollPosition,
				static_cast<Uint16>(scrollMax));
			auto * ccd = AllocCustomData(
				silencer::clay_bridge::CustomKind::ScrollBar, payload);
			CLAY({ .id = CLAY_SIDI(id, 0xFFFF0000u),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIXED(sbW),
			                       CLAY_SIZING_FIXED(rootH) },
			       },
			       .custom = { .customData = ccd } }) {}
		}
	}
}

}  // namespace silencer::ui::primitives
