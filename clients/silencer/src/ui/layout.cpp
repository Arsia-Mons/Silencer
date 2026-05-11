#include "layout.h"

#include "button_chrome.h"
#include "context.h"
#include "node.h"

#include "clay/clay.h"
#include "resources.h"

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
};

// Stable Clay ID derivation, in priority order:
//   1. `n.key` non-empty  → `Clay_GetElementId(key)` (semantic, stable).
//   2. Top-level root     → `CLAY_ID("ui::v2::Root")` (semantic, stable).
//   3. Anonymous child    → `CLAY_SIDI_LOCAL("c", local_idx)` — hashed
//      under the currently open parent. Equivalent to `CLAY_IDI_LOCAL`
//      with a runtime index; stable across frames so long as the parent
//      doesn't reorder its children, and the per-parent scoping avoids
//      collisions between distant subtrees that share a child position.
//
// Clay_AUTO_ID is the spiritual goal (source-location hash) but the
// vendored clay.h here predates that macro; the local-scoped index is
// the closest idiomatic substitute available.
Clay_ElementId IdFor(const Node & n, bool is_root, uint32_t local_idx) {
	if(!n.key.empty()){
		Clay_String s{};
		s.length = (int32_t)n.key.size();
		s.chars  = n.key.c_str();
		return Clay_GetElementId(s);
	}
	if(is_root){
		return CLAY_ID("ui::v2::Root");
	}
	return CLAY_SIDI_LOCAL(CLAY_STRING("c"), local_idx);
}

Clay_ElementDeclaration BuildDecl(const Node & n, bool is_root, uint32_t local_idx) {
	Clay_ElementDeclaration d{};
	d.id = IdFor(n, is_root, local_idx);

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
		case NodeKind::Scroll: {
			// Clay drives the scroll offset via Clip's childOffset; the
			// render path consumes it via the surface scissor stack. The
			// fixed sizing comes from fill_w/h — Scroll containers always
			// have a finite scroll viewport.
			if(n.fill_w > 0){
				lc.sizing.width  = SizingFixed((float)n.fill_w);
				lc.sizing.height = SizingFixed((float)n.fill_h);
			}else{
				lc.sizing.width  = SizingGrow();
				lc.sizing.height = SizingGrow();
			}
			Clay_ClipElementConfig clip{};
			clip.vertical = true;
			clip.childOffset = Clay_GetScrollOffset();
			d.clip = clip;
			break;
		}
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

void EmitNode(Node & n, LayoutState & ls, const Context & ctx, bool in_managed, bool is_root, uint32_t local_idx) {
	const bool is_container = IsContainer(n.kind);
	const bool will_emit    = is_container || in_managed;

	if(!will_emit){
		// Outside any layout subtree — still descend so nested containers
		// (e.g. a Center embedded inside a Background) get laid out.
		uint32_t i = 0;
		for(Node & child : n.children){
			EmitNode(child, ls, ctx, false, false, i++);
		}
		return;
	}

	Clay_ElementDeclaration d = BuildDecl(n, is_root, local_idx);
	Clay__OpenElement();
	Clay__ConfigureOpenElement(d);

	ls.records.push_back(EmitRecord{ &n, d.id });

	uint32_t i = 0;
	for(Node & child : n.children){
		EmitNode(child, ls, ctx, true, false, i++);
	}

	Clay__CloseElement();
}

}  // namespace

void EnsureClayContext(const Context & ctx) {
	if(!g_initialized){
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
	Clay_SetCurrentContext(g_clay_ctx);
}

Clay_RenderCommandArray Layout(Node & root, const Context & ctx) {
	EnsureClayContext(ctx);
	Clay_SetLayoutDimensions(Clay_Dimensions{ (float)ctx.logical_w, (float)ctx.logical_h });
	Clay_BeginLayout();

	LayoutState ls;
	EmitNode(root, ls, ctx, false, true, 0);

	Clay_RenderCommandArray cmds = Clay_EndLayout();

	for(const EmitRecord & r : ls.records){
		r.node->clay_id = r.id;
	}
	return cmds;
}

namespace {

// Absolute-path hit-test for a Button whose layout was NOT emitted into
// Clay (clay_id.id == 0). Chrome top-left = (n.x - sprite_offset_x,
// n.y - sprite_offset_y) where the offset is the sprite asset's baked
// anchor offset (zero for chrome-less B52x21). Matches the legacy
// `Button::MouseInside` semantics that screens with absolute `.at()`
// positions depend on.
bool AbsoluteChromeHit(const Node & n, const Context & ctx) {
	if(ctx.mouse_x < 0 || ctx.mouse_y < 0) return false;
	ButtonChrome c = ChromeFor(n.button_type);
	int off_x = 0, off_y = 0;
	if(c.bank != 0xFF){
		off_x = ctx.resources.spriteoffsetx[c.bank][c.base_index];
		off_y = ctx.resources.spriteoffsety[c.bank][c.base_index];
	}
	int x1 = n.x - off_x;
	int y1 = n.y - off_y;
	int x2 = x1 + c.width;
	int y2 = y1 + c.height;
	return ctx.mouse_x > x1 && ctx.mouse_x < x2 &&
	       ctx.mouse_y > y1 && ctx.mouse_y < y2;
}

void DispatchInto(const Node & n, const Context & ctx) {
	if(n.kind == NodeKind::Button && n.on_click){
		bool hit = false;
		if(n.clay_id.id != 0){
			// Layout-managed: read the current bounding box from Clay and
			// test ctx.mouse_x/y inline. Using Clay_GetElementData here
			// instead of Clay_PointerOver avoids depending on Clay's
			// pointer state (which lags by a frame in the dispatch path —
			// SetPointerState in Render() runs once per frame, while a
			// mouse-down can fire with a fresher (mouse_x, mouse_y)).
			// Clay_PointerOver is still the canonical hover query in the
			// render path (render.cpp), where SetPointerState was just
			// called for this frame.
			Clay_ElementData ed = Clay_GetElementData(n.clay_id);
			if(ed.found && ctx.mouse_x >= 0 && ctx.mouse_y >= 0){
				int x1 = (int)ed.boundingBox.x;
				int y1 = (int)ed.boundingBox.y;
				int x2 = x1 + (int)ed.boundingBox.width;
				int y2 = y1 + (int)ed.boundingBox.height;
				hit = ctx.mouse_x > x1 && ctx.mouse_x < x2 &&
				      ctx.mouse_y > y1 && ctx.mouse_y < y2;
			}
		}else{
			hit = AbsoluteChromeHit(n, ctx);
		}
		if(hit) n.on_click();
	}
	for(const Node & child : n.children){
		DispatchInto(child, ctx);
	}
}

}  // namespace

void DispatchClicks(const Node & root, const Context & ctx) {
	DispatchInto(root, ctx);
}

}  // namespace v2
}  // namespace ui
