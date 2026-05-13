#include "scroll_text_box.h"

#include "clay_ui_payloads.h"

namespace silencer::ui::primitives {

namespace {

constexpr int kPayloadCapacity = 64;
silencer::clay_bridge::ScrollBarPayload g_payloads[kPayloadCapacity];
int g_payloadCount = 0;

constexpr int kCustomDataCapacity = 64;
silencer::clay_bridge::ClayCustomData g_customData[kCustomDataCapacity];
int g_customDataCount = 0;

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

}  // namespace

void ScrollTextBoxBeginFrame() {
	g_payloadCount = 0;
	g_customDataCount = 0;
}

Uint16 ScrollTextBoxAutoScroll(Uint16 prevScrollPosition,
                               int prevLineCount,
                               int newLineCount,
                               Uint8 lineHeight,
                               Uint16 height) {
	if(lineHeight == 0) return prevScrollPosition;
	const int visibleLines = height / lineHeight;
	const int prevMax = (prevLineCount > visibleLines)
		? (prevLineCount - visibleLines) : 0;
	const int newMax = (newLineCount > visibleLines)
		? (newLineCount - visibleLines) : 0;
	if(static_cast<int>(prevScrollPosition) >= prevMax){
		return static_cast<Uint16>(newMax);
	}
	return prevScrollPosition;
}

void ScrollTextBox(Clay_String id,
                   const ScrollTextBoxLine * lines,
                   int lineCount,
                   Uint16 scrollPosition,
                   ScrollTextBoxOpts opts,
                   ScrollTextBoxHandle handle) {
	const int visibleLines = (opts.lineHeight > 0)
		? (opts.height / opts.lineHeight) : 0;
	int scrollMax = lineCount - visibleLines;
	if(scrollMax < 0) scrollMax = 0;
	if(scrollPosition > scrollMax) scrollPosition = static_cast<Uint16>(scrollMax);

	const float rootW = static_cast<float>(opts.width);
	const float rootH = static_cast<float>(opts.height);
	const float sbW   = opts.showScrollbar ? static_cast<float>(opts.scrollbarWidth) : 0.0f;
	const float gap   = opts.showScrollbar ? static_cast<float>(opts.scrollbarGap) : 0.0f;
	float rowsW       = rootW - sbW - gap;
	if(rowsW < 0.0f) rowsW = 0.0f;
	const float rowH  = static_cast<float>(opts.lineHeight);

	// Number of visible rows actually present in the line vector (capped at
	// the box's row capacity).
	int emit = lineCount - static_cast<int>(scrollPosition);
	if(emit < 0) emit = 0;
	if(emit > visibleLines) emit = visibleLines;

	// Rows column sizes to its content; the parent column aligns it inside
	// the box per `origin`. Under-fill anchors via Clay's childAlignment.y.
	const float emittedH = static_cast<float>(emit) * rowH;

	const Clay_ChildAlignment alignBottom = {
		CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_BOTTOM,
	};
	const Clay_ChildAlignment alignTop = {
		CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP,
	};

	CLAY({ .id = CLAY_SID(id),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(rootW), CLAY_SIZING_FIXED(rootH) },
	           .childGap = static_cast<Uint16>(gap),
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       } }) {
		bool rootHovered = ::Clay_Hovered();
		if(handle.hoveredOut) *handle.hoveredOut = rootHovered;

		// Outer rows container — fills the rows half of the box, aligns
		// the inner rows column per origin (top vs bottom).
		CLAY({ .id = CLAY_SIDI(id, 0),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(rowsW), CLAY_SIZING_FIXED(rootH) },
		           .childAlignment = (opts.origin == ScrollTextBoxOrigin::BottomUp)
		                                 ? alignBottom : alignTop,
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       } }) {
			// Inner rows column — sized to its content so alignment in the
			// outer container actually moves it.
			CLAY({ .id = CLAY_SIDI(id, 1),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIXED(rowsW),
			                       CLAY_SIZING_FIXED(emittedH) },
			           .layoutDirection = CLAY_TOP_TO_BOTTOM,
			       } }) {
				for(int line = 0; line < emit; line++){
					const int i = static_cast<int>(scrollPosition) + line;
					const ScrollTextBoxLine & ln = lines[i];

					CLAY({ .id = CLAY_SIDI(id, static_cast<uint32_t>(i + 2)),
					       .layout = {
					           .sizing = { CLAY_SIZING_FIXED(rowsW),
					                       CLAY_SIZING_FIXED(rowH) },
					           .padding = { ln.indent, 0, 0, 0 },
					       } }) {
						BankText(ln.text, opts.textVariant,
						         { .effectColor = ln.effectColor,
						           .brightness  = ln.brightness });
					}
				}
			}
		}

		// Optional scrollbar.
		if(opts.showScrollbar){
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
