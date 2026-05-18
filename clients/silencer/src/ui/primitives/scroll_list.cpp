#include "scroll_list.h"

#include "clay_ui_payloads.h"
#include "runtime/UiInteractionRegistry.h"

#include <cstdint>
#include <string>

namespace silencer::ui::primitives {

namespace {

// Legacy SelectBox drew the selection bar 11px tall inside a 14px row slot
// and inset from the slot edges, so consecutive rows read as separate items
// with breathing room. The Clay migration filled the whole slot edge-to-edge,
// which made list items look cramped (issue #178). Re-inset the row body so
// the highlight and text sit inside the slot like legacy.
constexpr Uint16 kRowInsetX     = 4;  // left/right gap between bar and slot
constexpr Uint16 kRowBottomGap  = 3;  // separation between consecutive rows
constexpr Uint16 kRowTextPadX   = 6;  // text inset from the bar's left edge

constexpr int kPayloadCapacity = 64;
silencer::clay_bridge::ScrollBarPayload g_payloads[kPayloadCapacity];
int g_payloadCount = 0;

constexpr int kCustomDataCapacity = 64;
silencer::clay_bridge::ClayCustomData g_customData[kCustomDataCapacity];
int g_customDataCount = 0;

std::string ToStd(Clay_String text) {
	return std::string(text.chars ? text.chars : "", static_cast<size_t>(text.length));
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

void RegisterRowWidget(Clay_String id,
                       const Clay_String& label,
                       int index,
                       bool selected,
                       ScrollListHandle handle) {
	if(!handle.interactions || !handle.actionId || !*handle.actionId) return;
	silencer::ui::UiInteractable widget;
	widget.id = std::string(handle.actionId) + "." + std::to_string(index);
	widget.labelText = ToStd(label);
	widget.kind = UiInteractableKind::ListRow;
	widget.index = index;
	widget.selected = selected;
	widget.clayId = CLAY_SIDI(id, static_cast<uint32_t>(index + 1));
	widget.hasClayId = true;
	handle.interactions->RegisterInteractable(widget);
}

}  // namespace

void ScrollListBeginFrame() {
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
	if(handle.hoveredIndexOut) *handle.hoveredIndexOut = -1;
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

				// The slot keeps the full legacy row pitch and stays the
				// clickable/hit-tested element. The inset inner body carries
				// the highlight + text so consecutive rows read as separate
				// items with margins, like legacy SelectBox (issue #178).
				CLAY({ .id = CLAY_SIDI(id, static_cast<uint32_t>(i + 1)),
				       .layout = {
				           .sizing = { CLAY_SIZING_FIXED(rowsW),
				                       CLAY_SIZING_FIXED(rowH) },
				           .padding = { kRowInsetX, kRowInsetX,
				                        0, kRowBottomGap },
				       } }) {
					if(handle.hoveredIndexOut && ::Clay_Hovered()){
						*handle.hoveredIndexOut = i;
					}
					RegisterRowWidget(id, items[i], i, isSelected, handle);
					CLAY({ .id = CLAY_SIDI(id, 0x20000000u + static_cast<uint32_t>(i)),
					       .layout = {
					           .sizing = { CLAY_SIZING_GROW(0),
					                       CLAY_SIZING_GROW(0) },
					           .padding = { kRowTextPadX, kRowTextPadX, 0, 0 },
					           .childAlignment = { CLAY_ALIGN_X_LEFT,
					                               CLAY_ALIGN_Y_CENTER },
					       },
					       .backgroundColor = { static_cast<float>(bgIdx),
					                            0.0f, 0.0f, 255.0f } }) {
						Text(items[i], opts.text);
					}
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
