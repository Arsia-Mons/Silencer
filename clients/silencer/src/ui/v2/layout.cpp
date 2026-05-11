#include "layout.h"

#include "button_chrome.h"
#include "context.h"
#include "node.h"

#include "clay/clay.h"

#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

namespace ui {
namespace v2 {

namespace {

bool          g_initialized = false;
char *        g_arena_mem   = nullptr;
Clay_Context * g_clay_ctx   = nullptr;

void ClayError(Clay_ErrorData err) {
	fprintf(stderr, "[clay] error %d: %.*s\n",
	        (int)err.errorType,
	        (int)err.errorText.length, err.errorText.chars);
}

// Sprite-font glyphs are fixed-width per bank. cfg->fontSize carries the
// per-glyph advance (font_width) and cfg->lineHeight carries the line
// box height. The measurement is purely arithmetic — there is no kerning
// or font metric to query.
Clay_Dimensions MeasureSpriteText(Clay_StringSlice text, Clay_TextElementConfig * cfg, void *) {
	float w = (float)(text.length * (cfg->fontSize > 0 ? cfg->fontSize : 11));
	float h = (float)(cfg->lineHeight > 0 ? cfg->lineHeight : 17);
	return Clay_Dimensions{ w, h };
}

void EnsureInit(const Context & ctx) {
	if(g_initialized) return;
	uint32_t need = Clay_MinMemorySize();
	g_arena_mem = (char *)std::malloc(need);
	Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(need, g_arena_mem);
	Clay_ErrorHandler eh{};
	eh.errorHandlerFunction = ClayError;
	g_clay_ctx = Clay_Initialize(arena,
	                             Clay_Dimensions{ (float)ctx.logical_w, (float)ctx.logical_h },
	                             eh);
	Clay_SetMeasureTextFunction(MeasureSpriteText, nullptr);
	g_initialized = true;
}

// ----- Clay sizing/layout helpers -----
// Implemented as plain functions rather than relying on Clay's macro
// helpers (CLAY_SIZING_FIT() etc.) — empty variadic macro args are
// non-portable and the C++ aggregate-init forms read fine.

Clay_SizingAxis SizingFit() {
	Clay_SizingAxis a{};
	a.type = CLAY__SIZING_TYPE_FIT;
	return a;
}
Clay_SizingAxis SizingGrow() {
	Clay_SizingAxis a{};
	a.type = CLAY__SIZING_TYPE_GROW;
	return a;
}
Clay_SizingAxis SizingFixed(float v) {
	Clay_SizingAxis a{};
	a.size.minMax = Clay_SizingMinMax{ v, v };
	a.type = CLAY__SIZING_TYPE_FIXED;
	return a;
}

struct EmitRecord { Node * node; Clay_ElementId id; };

struct LayoutState {
	std::vector<EmitRecord> records;
	uint32_t next_idx = 0;
};

Clay_ElementId IdFor(LayoutState & ls) {
	// Single static base string + per-emit index. Different IDs per
	// emit; we don't lean on Clay's per-ID memory (hover/scroll), so
	// instability across frames is irrelevant.
	Clay_String base{};
	base.isStaticallyAllocated = true;
	base.length = 8;
	base.chars  = "ui-v2-id";
	return Clay_GetElementIdWithIndex(base, ls.next_idx++);
}

Clay_ElementDeclaration BuildDecl(const Node & n, LayoutState & ls) {
	Clay_ElementDeclaration d{};
	d.id = IdFor(ls);

	Clay_LayoutConfig lc{};
	switch(n.kind){
		case NodeKind::VStack:
			lc.sizing.width    = SizingFit();
			lc.sizing.height   = SizingFit();
			lc.childGap        = n.gap;
			lc.layoutDirection = CLAY_TOP_TO_BOTTOM;
			break;
		case NodeKind::HStack:
			lc.sizing.width    = SizingFit();
			lc.sizing.height   = SizingFit();
			lc.childGap        = n.gap;
			lc.layoutDirection = CLAY_LEFT_TO_RIGHT;
			break;
		case NodeKind::Center:
			lc.sizing.width        = SizingGrow();
			lc.sizing.height       = SizingGrow();
			lc.childAlignment.x    = CLAY_ALIGN_X_CENTER;
			lc.childAlignment.y    = CLAY_ALIGN_Y_CENTER;
			break;
		case NodeKind::Padding:
			lc.sizing.width    = SizingFit();
			lc.sizing.height   = SizingFit();
			lc.padding         = Clay_Padding{ n.pad, n.pad, n.pad, n.pad };
			break;
		case NodeKind::Spacer:
			lc.sizing.width  = SizingGrow();
			lc.sizing.height = SizingGrow();
			break;
		case NodeKind::Button: {
			ButtonChrome c = ChromeFor(n.button_type);
			lc.sizing.width  = SizingFixed((float)c.width);
			lc.sizing.height = SizingFixed((float)c.height);
			break;
		}
		case NodeKind::Label: {
			float w = (float)((int)n.text.size() * (int)n.text_width);
			lc.sizing.width  = SizingFixed(w);
			lc.sizing.height = SizingFixed(17.0f);
			break;
		}
		case NodeKind::NineSliceFrame: {
			// Fixed size when fill_w/h is set (typical absolute path); fit
			// when used as a wrapping container.
			if(n.fill_w > 0){
				lc.sizing.width  = SizingFixed((float)n.fill_w);
				lc.sizing.height = SizingFixed((float)n.fill_h);
			}else{
				lc.sizing.width  = SizingFit();
				lc.sizing.height = SizingFit();
			}
			break;
		}
		case NodeKind::Sprite:
		case NodeKind::Background:
		case NodeKind::FilledRect:
		case NodeKind::Group:
			// These kinds don't naturally appear inside a layout-managed
			// subtree today. Treat as zero-sized passthrough so the
			// parent's flow isn't broken if one slips in.
			lc.sizing.width  = SizingFit();
			lc.sizing.height = SizingFit();
			break;
	}
	d.layout = lc;
	return d;
}

void EmitNode(Node & n, LayoutState & ls, const Context & ctx, bool in_managed) {
	const bool is_container = IsContainer(n.kind);
	const bool will_emit    = is_container || in_managed;

	if(!will_emit){
		// Outside any layout subtree — still descend so nested containers
		// (e.g. a Center embedded inside a Background) get laid out.
		for(Node & child : n.children){
			EmitNode(child, ls, ctx, false);
		}
		return;
	}

	Clay_ElementDeclaration d = BuildDecl(n, ls);
	Clay__OpenElement();
	Clay__ConfigureOpenElement(d);

	ls.records.push_back(EmitRecord{ &n, d.id });

	for(Node & child : n.children){
		EmitNode(child, ls, ctx, true);
	}

	Clay__CloseElement();
}

}  // namespace

void Layout(Node & root, const Context & ctx) {
	EnsureInit(ctx);
	Clay_SetCurrentContext(g_clay_ctx);
	Clay_SetLayoutDimensions(Clay_Dimensions{ (float)ctx.logical_w, (float)ctx.logical_h });
	Clay_BeginLayout();

	LayoutState ls;
	EmitNode(root, ls, ctx, false);

	(void)Clay_EndLayout();

	for(const EmitRecord & r : ls.records){
		Clay_ElementData ed = Clay_GetElementData(r.id);
		if(ed.found){
			r.node->rect_x = (Sint16)ed.boundingBox.x;
			r.node->rect_y = (Sint16)ed.boundingBox.y;
			r.node->rect_w = (Uint16)ed.boundingBox.width;
			r.node->rect_h = (Uint16)ed.boundingBox.height;
		}
	}
}

}  // namespace v2
}  // namespace ui
