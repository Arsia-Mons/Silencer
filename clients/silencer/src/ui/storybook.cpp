// Interactive component storybook for the ui/v2 declarative library.
//
// Layout:
//   +--------+--------------------------------+--------+
//   | Palette| Canvas                         | Props  |
//   | Button |  (drag/resize components here) | bank   |
//   | Label  |                                | index  |
//   | VStack |                                | stroke |
//   | HStack |                                | gap    |
//   | Center |                                |        |
//   | Frame  |                                |        |
//   | Scroll |                                |        |
//   | Spacer |                                |        |
//   +--------+--------------------------------+--------+
//
// Editor state (the StorybookState struct) is local to this file — it
// is NOT part of the persistent v2 Node tree. Each frame we rebuild a
// fresh ui::v2::Node per item and render through the normal Render()
// path, so the storybook is a real customer of the library, not a fork.
//
// Cross-platform: SDL3 events + Renderer/Surface only. No __APPLE__ or
// platform-specific paths.

#include "game.h"

#include "context.h"
#include "dispatch.h"
#include "layout.h"
#include "node.h"
#include "render.h"
#include "ui_state.h"

#include "config.h"
#include "renderer.h"
#include "renderdevice.h"
#include "resources.h"
#include "surface.h"
#include "world.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

// ---- Editor state ----------------------------------------------------

enum class Kind {
	Button,
	Label,
	VStack,
	HStack,
	Center,
	Frame,    // NineSliceFrame
	Scroll,
	Spacer,
	FilledRect,
};

const char * KindName(Kind k){
	switch(k){
		case Kind::Button:     return "Button";
		case Kind::Label:      return "Label";
		case Kind::VStack:     return "VStack";
		case Kind::HStack:     return "HStack";
		case Kind::Center:     return "Center";
		case Kind::Frame:      return "Frame";
		case Kind::Scroll:     return "Scroll";
		case Kind::Spacer:     return "Spacer";
		case Kind::FilledRect: return "FillRect";
	}
	return "?";
}

struct Item {
	Kind kind;
	int  x, y;
	int  w, h;
	std::string text;
	int  button_type;   // ButtonType enum index
	int  bank;
	int  index;         // sprite index for Frame
	int  stroke_color;
	int  stroke_width;
	int  fill_color;
	int  gap;
	int  pad;
};

Item DefaultItem(Kind k){
	Item it{};
	it.kind = k;
	it.w = 120; it.h = 40;
	it.text = "Hello";
	it.button_type = (int)ui::v2::ButtonType::B196x33;
	it.bank = 7; it.index = 24;
	it.stroke_color = 14; it.stroke_width = 0;
	it.fill_color   = 14;
	it.gap = 4; it.pad = 4;
	switch(k){
		case Kind::Button:     it.text = "Button"; it.w = 196; it.h = 33; break;
		case Kind::Label:      it.text = "Label";  it.w = 60;  it.h = 17; break;
		case Kind::VStack:     it.w = 80;  it.h = 120; it.stroke_width = 1; break;
		case Kind::HStack:     it.w = 200; it.h = 40;  it.stroke_width = 1; break;
		case Kind::Center:     it.w = 160; it.h = 80;  it.stroke_width = 1; break;
		case Kind::Frame:      it.w = 160; it.h = 80;  break;
		case Kind::Scroll:     it.w = 120; it.h = 100; it.stroke_width = 1; break;
		case Kind::Spacer:     it.w = 40;  it.h = 40;  it.stroke_width = 1; break;
		case Kind::FilledRect: it.w = 60;  it.h = 40;  break;
	}
	return it;
}

struct StorybookState {
	std::vector<Item> items;
	int  selected = -1;       // index into items, -1 = nothing selected
	bool dragging = false;
	bool resizing = false;
	int  drag_grab_dx = 0;
	int  drag_grab_dy = 0;
	int  log_index = 0;       // unique id seed for spawn labels
};

// ---- Layout regions --------------------------------------------------

constexpr int kPaletteW = 84;
constexpr int kPropsW   = 120;
constexpr int kHandleSz = 8;
constexpr int kRowH     = 14;
constexpr int kBtnH     = 14;

struct Rect { int x, y, w, h; };

bool RectHit(const Rect & r, int mx, int my){
	return mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h;
}

Rect CanvasRect(int logical_w, int logical_h){
	return Rect{kPaletteW, 0, logical_w - kPaletteW - kPropsW, logical_h};
}

Rect PaletteRect(int /*lw*/, int logical_h){
	return Rect{0, 0, kPaletteW, logical_h};
}

Rect PropsRect(int logical_w, int logical_h){
	return Rect{logical_w - kPropsW, 0, kPropsW, logical_h};
}

Rect ItemRect(const Item & it){
	return Rect{it.x, it.y, it.w, it.h};
}

Rect HandleRect(const Item & it){
	return Rect{it.x + it.w - kHandleSz, it.y + it.h - kHandleSz, kHandleSz, kHandleSz};
}

// ---- Build a v2 Node tree for a single item -------------------------
//
// We could batch all items into one tree, but giving each item its own
// tiny tree keeps spatial positioning trivial (.at()-driven) and means a
// crash in one item can't corrupt the others. Each item draws fully
// independently via the v2 render path.
ui::v2::Node BuildItemNode(const Item & it){
	switch(it.kind){
		case Kind::Button: {
			ui::v2::Node n = ui::v2::Button(it.text, (ui::v2::ButtonType)it.button_type)
			    .at((Sint16)it.x, (Sint16)it.y);
			n.fill_w = (Uint16)it.w; n.fill_h = (Uint16)it.h;
			if(it.stroke_width > 0) n.stroke((Uint8)it.stroke_color, (Uint8)it.stroke_width);
			// Layout-managed buttons consume rect_*; setting it here keeps
			// the chrome anchored at (it.x, it.y) without depending on the
			// absolute sprite-anchor fallback.
			n.rect_x = (Sint16)it.x; n.rect_y = (Sint16)it.y;
			n.rect_w = (Uint16)it.w; n.rect_h = (Uint16)it.h;
			return n;
		}
		case Kind::Label: {
			ui::v2::Node n = ui::v2::Label(it.text, 135, 11)
			    .at((Sint16)it.x, (Sint16)it.y);
			if(it.stroke_width > 0){
				n.fill_w = (Uint16)it.w; n.fill_h = (Uint16)it.h;
				n.stroke((Uint8)it.stroke_color, (Uint8)it.stroke_width);
			}
			return n;
		}
		case Kind::Frame: {
			ui::v2::Node n = ui::v2::NineSliceFrame((Uint8)it.bank, (Uint8)it.index,
			    (Uint16)it.w, (Uint16)it.h);
			n.x = (Sint16)it.x; n.y = (Sint16)it.y;
			if(it.stroke_width > 0) n.stroke((Uint8)it.stroke_color, (Uint8)it.stroke_width);
			return n;
		}
		case Kind::FilledRect: {
			ui::v2::Node n = ui::v2::FilledRect((Uint16)it.w, (Uint16)it.h, (Uint8)it.fill_color);
			n.x = (Sint16)it.x; n.y = (Sint16)it.y;
			if(it.stroke_width > 0) n.stroke((Uint8)it.stroke_color, (Uint8)it.stroke_width);
			return n;
		}
		case Kind::VStack:
		case Kind::HStack:
		case Kind::Center:
		case Kind::Scroll:
		case Kind::Spacer: {
			// Containers don't render anything of their own — we show
			// them as a bordered box so authors can see + manipulate
			// them on the canvas. The stroke is the visual; semantics
			// (children layout) aren't exercised here since the
			// storybook items are siblings, not a tree.
			ui::v2::Node n;
			n.kind = (it.kind == Kind::VStack) ? ui::v2::NodeKind::VStack
			       : (it.kind == Kind::HStack) ? ui::v2::NodeKind::HStack
			       : (it.kind == Kind::Center) ? ui::v2::NodeKind::Center
			       : (it.kind == Kind::Scroll) ? ui::v2::NodeKind::Scroll
			                                   : ui::v2::NodeKind::Spacer;
			n.x = (Sint16)it.x; n.y = (Sint16)it.y;
			n.fill_w = (Uint16)it.w; n.fill_h = (Uint16)it.h;
			n.rect_x = (Sint16)it.x; n.rect_y = (Sint16)it.y;
			n.rect_w = (Uint16)it.w; n.rect_h = (Uint16)it.h;
			n.gap = (Uint16)it.gap; n.pad = (Uint16)it.pad;
			Uint8 sw = (Uint8)(it.stroke_width > 0 ? it.stroke_width : 1);
			n.stroke((Uint8)it.stroke_color, sw);
			return n;
		}
	}
	return ui::v2::Group({});
}

// ---- Property descriptor (UI rows) ----------------------------------
//
// Each row is one editable field. We keep this as a simple list of
// "row descriptor" structs that points to the field on the selected
// item; the panel walks the list and renders +/- buttons or a single
// cycle button as appropriate.
enum class FieldKind { Int, Enum, String };
struct Row {
	const char * label;
	FieldKind    kind;
	int *        ip;
	int          min_v, max_v;
	int          step;
	const char * const * enum_names;
	int          enum_count;
};

static const char * kButtonTypeNames[] = {
	"B112x33", "B220x33", "B196x33", "B236x27", "B52x21", "B156x21"
};

// Returns a row list for the currently selected item. The pointers
// reference fields on `it` directly, so editing through the rows
// mutates the item in-place. Rebuilt every frame — no caching, no
// stale-pointer risk.
std::vector<Row> RowsFor(Item & it){
	std::vector<Row> rows;
	rows.push_back({"x",       FieldKind::Int, &it.x, -200, 640, 5, nullptr, 0});
	rows.push_back({"y",       FieldKind::Int, &it.y, -200, 480, 5, nullptr, 0});
	rows.push_back({"w",       FieldKind::Int, &it.w, 1,    640, 5, nullptr, 0});
	rows.push_back({"h",       FieldKind::Int, &it.h, 1,    480, 5, nullptr, 0});
	switch(it.kind){
		case Kind::Button:
			rows.push_back({"type",  FieldKind::Enum, &it.button_type, 0, 5, 1,
			                kButtonTypeNames, 6});
			break;
		case Kind::Frame:
			rows.push_back({"bank",  FieldKind::Int, &it.bank,  0, 255, 1, nullptr, 0});
			rows.push_back({"index", FieldKind::Int, &it.index, 0, 255, 1, nullptr, 0});
			break;
		case Kind::FilledRect:
			rows.push_back({"color", FieldKind::Int, &it.fill_color, 0, 255, 1, nullptr, 0});
			break;
		case Kind::VStack:
		case Kind::HStack:
			rows.push_back({"gap", FieldKind::Int, &it.gap, 0, 100, 1, nullptr, 0});
			break;
		case Kind::Center:
			rows.push_back({"pad", FieldKind::Int, &it.pad, 0, 100, 1, nullptr, 0});
			break;
		default: break;
	}
	rows.push_back({"strokeC", FieldKind::Int, &it.stroke_color, 0, 255, 1, nullptr, 0});
	rows.push_back({"strokeW", FieldKind::Int, &it.stroke_width, 0, 8,   1, nullptr, 0});
	return rows;
}

// ---- Hit list -------------------------------------------------------
//
// Buttons for spawning palette items, the +/- on each property row, and
// the delete button — all share a tiny "rect + action callback" shape so
// the dispatch path is uniform: walk the list, fire the first hit.
struct ClickRegion {
	Rect rect;
	int  action;   // opaque tag; the dispatcher switch interprets these
	int  payload;  // row index, palette kind index, etc.
};
constexpr int kActSpawn    = 1;
constexpr int kActDelete   = 2;
constexpr int kActRowMinus = 3;
constexpr int kActRowPlus  = 4;
constexpr int kActRowCycle = 5;

}  // namespace

int Game::RunStorybook(){
	if(!window || !renderdevice){
		fprintf(stderr, "[storybook] needs a window — drop --headless\n");
		return 5;
	}

	// Palette 1 matches the migrated menu screens; the storybook borrows
	// the menu palette so bank-6/7 chrome looks right out of the box.
	if(!renderer.palette.SetPalette(1)){
		fprintf(stderr, "[storybook] palette 1 load failed\n");
		return 4;
	}
	SetColors(renderer.palette.GetColors());

	StorybookState state;
	ui::v2::UIState ui_state;

	const int logical_w = 640;
	const int logical_h = 480;
	const Rect palette_r = PaletteRect(logical_w, logical_h);
	const Rect canvas_r  = CanvasRect(logical_w, logical_h);
	const Rect props_r   = PropsRect(logical_w, logical_h);

	// Window→logical mapping, identical convention to RunPreview's
	// interactive loop.
	auto window_to_logical = [&](float wx, float wy, int & ox, int & oy){
		int ww, wh;
		SDL_GetWindowSize(window, &ww, &wh);
		if(ww <= 0 || wh <= 0){ ox = -1; oy = -1; return; }
		ox = (int)((wx / (float)ww) * (float)logical_w);
		oy = (int)((wy / (float)wh) * (float)logical_h);
	};

	int  mouse_x = -1, mouse_y = -1;
	bool running = true;
	float frame_dt = 0.0f;
	Uint64 last_ticks = 0;

	const std::vector<Kind> palette_items = {
		Kind::Button, Kind::Label,  Kind::VStack, Kind::HStack,
		Kind::Center, Kind::Frame,  Kind::Scroll, Kind::Spacer,
		Kind::FilledRect,
	};

	printf("[storybook] interactive  (Esc to exit, Delete to remove selection)\n");

	while(running){
		// ---- collect frame events ----
		bool mouse_clicked = false;
		bool mouse_released = false;
		bool delete_pressed = false;
		SDL_Event ev;
		while(SDL_PollEvent(&ev)){
			if(ev.type == SDL_EVENT_QUIT){
				running = false;
			}else if(ev.type == SDL_EVENT_KEY_DOWN){
				if(ev.key.scancode == SDL_SCANCODE_ESCAPE) running = false;
				else if(ev.key.scancode == SDL_SCANCODE_DELETE ||
				        ev.key.scancode == SDL_SCANCODE_BACKSPACE){
					delete_pressed = true;
				}
			}else if(ev.type == SDL_EVENT_MOUSE_MOTION){
				window_to_logical(ev.motion.x, ev.motion.y, mouse_x, mouse_y);
			}else if(ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
			         ev.button.button == SDL_BUTTON_LEFT){
				window_to_logical(ev.button.x, ev.button.y, mouse_x, mouse_y);
				mouse_clicked = true;
			}else if(ev.type == SDL_EVENT_MOUSE_BUTTON_UP &&
			         ev.button.button == SDL_BUTTON_LEFT){
				mouse_released = true;
			}
		}

		Uint64 now = SDL_GetTicks();
		frame_dt = (last_ticks == 0) ? 0.0f : (float)(now - last_ticks) / 1000.0f;
		last_ticks = now;

		// ---- mouse-up ends drag/resize ----
		if(mouse_released){
			state.dragging = false;
			state.resizing = false;
		}

		// ---- drag / resize update ----
		if(state.selected >= 0 && state.selected < (int)state.items.size()){
			Item & sel = state.items[state.selected];
			if(state.dragging){
				sel.x = mouse_x - state.drag_grab_dx;
				sel.y = mouse_y - state.drag_grab_dy;
			}else if(state.resizing){
				sel.w = (mouse_x - sel.x);
				sel.h = (mouse_y - sel.y);
				if(sel.w < 4) sel.w = 4;
				if(sel.h < 4) sel.h = 4;
			}
		}

		// ---- build click regions for chrome (palette + props panel) ----
		std::vector<ClickRegion> regions;

		// Palette: one row per kind.
		for(int i = 0; i < (int)palette_items.size(); i++){
			Rect r{4, 8 + i * (kRowH + 2), kPaletteW - 8, kRowH};
			regions.push_back({r, kActSpawn, i});
		}

		// Properties: rows + delete button (only when something selected).
		const bool have_sel = (state.selected >= 0 && state.selected < (int)state.items.size());
		std::vector<Row> rows_snapshot;
		if(have_sel){
			rows_snapshot = RowsFor(state.items[state.selected]);
			int ry = 24;
			for(int i = 0; i < (int)rows_snapshot.size(); i++){
				int bx_minus = props_r.x + 56;
				int bx_plus  = props_r.x + 92;
				regions.push_back({Rect{bx_minus, ry, 12, kBtnH},
				                   rows_snapshot[i].kind == FieldKind::Enum ? kActRowCycle
				                                                            : kActRowMinus, i});
				regions.push_back({Rect{bx_plus, ry, 12, kBtnH},
				                   rows_snapshot[i].kind == FieldKind::Enum ? kActRowCycle
				                                                            : kActRowPlus, i});
				ry += kBtnH + 2;
			}
			Rect del{props_r.x + 8, ry + 6, kPropsW - 16, kBtnH};
			regions.push_back({del, kActDelete, 0});
		}

		// ---- dispatch a fresh click ----
		if(mouse_clicked){
			// 1) palette / props panel chrome (top priority — chrome
			//    always wins over canvas hit-tests).
			bool consumed = false;
			for(const ClickRegion & cr : regions){
				if(RectHit(cr.rect, mouse_x, mouse_y)){
					switch(cr.action){
						case kActSpawn: {
							Item nit = DefaultItem(palette_items[cr.payload]);
							nit.x = canvas_r.x + canvas_r.w / 2 - nit.w / 2;
							nit.y = canvas_r.y + canvas_r.h / 2 - nit.h / 2;
							state.items.push_back(nit);
							state.selected = (int)state.items.size() - 1;
							state.log_index++;
							printf("[storybook] spawn %s (#%d)\n",
							       KindName(nit.kind), state.log_index);
							break;
						}
						case kActDelete:
							if(have_sel){
								state.items.erase(state.items.begin() + state.selected);
								state.selected = -1;
							}
							break;
						case kActRowPlus:
						case kActRowMinus: {
							const Row & r = rows_snapshot[cr.payload];
							int delta = (cr.action == kActRowPlus) ? r.step : -r.step;
							int v = *r.ip + delta;
							if(v < r.min_v) v = r.min_v;
							if(v > r.max_v) v = r.max_v;
							*r.ip = v;
							break;
						}
						case kActRowCycle: {
							const Row & r = rows_snapshot[cr.payload];
							int v = *r.ip + 1;
							if(v > r.max_v) v = r.min_v;
							*r.ip = v;
							break;
						}
					}
					consumed = true;
					break;
				}
			}

			// 2) canvas: click on an item (handle wins over body so
			//    you can resize a fully-covered item without it
			//    starting a drag).
			if(!consumed && RectHit(canvas_r, mouse_x, mouse_y)){
				int hit = -1;
				bool on_handle = false;
				// Iterate front-to-back so later items (drawn on top)
				// claim the hit before earlier ones.
				for(int i = (int)state.items.size() - 1; i >= 0; i--){
					if(RectHit(HandleRect(state.items[i]), mouse_x, mouse_y)){
						hit = i; on_handle = true; break;
					}
				}
				if(hit < 0){
					for(int i = (int)state.items.size() - 1; i >= 0; i--){
						if(RectHit(ItemRect(state.items[i]), mouse_x, mouse_y)){
							hit = i; break;
						}
					}
				}
				if(hit >= 0){
					state.selected = hit;
					Item & sel = state.items[hit];
					if(on_handle){
						state.resizing = true;
					}else{
						state.dragging = true;
						state.drag_grab_dx = mouse_x - sel.x;
						state.drag_grab_dy = mouse_y - sel.y;
					}
				}else{
					state.selected = -1;
				}
			}
		}

		if(delete_pressed && have_sel){
			state.items.erase(state.items.begin() + state.selected);
			state.selected = -1;
		}

		// ---- render ----
		screenbuffer.Clear(0);

		ui::v2::Context ctx{
			world.resources,
			logical_w, logical_h,
			/*scale=*/1,
			/*version=*/"00000",
		};
		ctx.mouse_x = mouse_x; ctx.mouse_y = mouse_y;
		ctx.state = &ui_state;
		ctx.dt = frame_dt;

		ui_state.BeginFrame();

		// 1) Canvas backdrop: subtle filled rect + frame stroke.
		Renderer::DrawFilledRectangle(&screenbuffer,
		                              canvas_r.x, canvas_r.y,
		                              canvas_r.x + canvas_r.w,
		                              canvas_r.y + canvas_r.h, 240);
		Renderer::DrawFilledRectangle(&screenbuffer,
		                              palette_r.x, palette_r.y,
		                              palette_r.x + palette_r.w,
		                              palette_r.y + palette_r.h, 16);
		Renderer::DrawFilledRectangle(&screenbuffer,
		                              props_r.x, props_r.y,
		                              props_r.x + props_r.w,
		                              props_r.y + props_r.h, 16);

		// 2) Each canvas item — render via the real v2 pipeline, but
		//    push a scissor so children that overflow the canvas don't
		//    smear into the palette / props panels.
		screenbuffer.PushScissor(canvas_r.x, canvas_r.y, canvas_r.w, canvas_r.h);
		for(int i = 0; i < (int)state.items.size(); i++){
			ui::v2::Node n = BuildItemNode(state.items[i]);
			ui::v2::Render(n, ctx, screenbuffer, renderer);
		}
		screenbuffer.PopScissor();

		// 3) Selection highlight + resize handle (chrome on top).
		if(have_sel){
			const Item & sel = state.items[state.selected];
			// Outline (color 200 = bright UI accent).
			Renderer::DrawFilledRectangle(&screenbuffer, sel.x,          sel.y,          sel.x + sel.w, sel.y + 1,      200);
			Renderer::DrawFilledRectangle(&screenbuffer, sel.x,          sel.y + sel.h - 1, sel.x + sel.w, sel.y + sel.h, 200);
			Renderer::DrawFilledRectangle(&screenbuffer, sel.x,          sel.y,          sel.x + 1,     sel.y + sel.h,  200);
			Renderer::DrawFilledRectangle(&screenbuffer, sel.x + sel.w - 1, sel.y,       sel.x + sel.w, sel.y + sel.h,  200);
			Rect hr = HandleRect(sel);
			Renderer::DrawFilledRectangle(&screenbuffer, hr.x, hr.y, hr.x + hr.w, hr.y + hr.h, 200);
		}

		// 4) Palette rows (background + label).
		renderer.DrawText(&screenbuffer, (Uint16)(palette_r.x + 4), (Uint16)2,
		                  "PALETTE", 133, 6);
		for(int i = 0; i < (int)palette_items.size(); i++){
			int ry = 8 + i * (kRowH + 2);
			Renderer::DrawFilledRectangle(&screenbuffer, palette_r.x + 4, ry,
			                              palette_r.x + palette_r.w - 4, ry + kRowH, 32);
			renderer.DrawText(&screenbuffer, (Uint16)(palette_r.x + 8), (Uint16)(ry + 3),
			                  KindName(palette_items[i]), 133, 6);
		}

		// 5) Props panel.
		renderer.DrawText(&screenbuffer, (Uint16)(props_r.x + 4), (Uint16)2,
		                  "PROPERTIES", 133, 6);
		if(have_sel){
			const Item & sel = state.items[state.selected];
			char title[64];
			snprintf(title, sizeof(title), "%s", KindName(sel.kind));
			renderer.DrawText(&screenbuffer, (Uint16)(props_r.x + 4), (Uint16)12,
			                  title, 133, 6);
			int ry = 24;
			for(int i = 0; i < (int)rows_snapshot.size(); i++){
				const Row & r = rows_snapshot[i];
				renderer.DrawText(&screenbuffer, (Uint16)(props_r.x + 4), (Uint16)(ry + 3),
				                  r.label, 133, 6);
				char val[32];
				if(r.kind == FieldKind::Enum && r.enum_names){
					int v = *r.ip;
					if(v < 0) v = 0;
					if(v >= r.enum_count) v = r.enum_count - 1;
					snprintf(val, sizeof(val), "%s", r.enum_names[v]);
				}else{
					snprintf(val, sizeof(val), "%d", *r.ip);
				}
				renderer.DrawText(&screenbuffer, (Uint16)(props_r.x + 36), (Uint16)(ry + 3),
				                  val, 133, 6);
				int bx_minus = props_r.x + 56;
				int bx_plus  = props_r.x + 92;
				Renderer::DrawFilledRectangle(&screenbuffer, bx_minus, ry, bx_minus + 12, ry + kBtnH, 48);
				Renderer::DrawFilledRectangle(&screenbuffer, bx_plus,  ry, bx_plus + 12,  ry + kBtnH, 48);
				renderer.DrawText(&screenbuffer, (Uint16)(bx_minus + 3), (Uint16)(ry + 3),
				                  r.kind == FieldKind::Enum ? "<" : "-", 133, 6);
				renderer.DrawText(&screenbuffer, (Uint16)(bx_plus + 3), (Uint16)(ry + 3),
				                  r.kind == FieldKind::Enum ? ">" : "+", 133, 6);
				ry += kBtnH + 2;
			}
			Rect del{props_r.x + 8, ry + 6, kPropsW - 16, kBtnH};
			Renderer::DrawFilledRectangle(&screenbuffer, del.x, del.y,
			                              del.x + del.w, del.y + del.h, 64);
			renderer.DrawText(&screenbuffer, (Uint16)(del.x + 4), (Uint16)(del.y + 3),
			                  "DELETE", 133, 6);
		}else{
			renderer.DrawText(&screenbuffer, (Uint16)(props_r.x + 4), (Uint16)12,
			                  "<no select>", 133, 6);
		}

		ui_state.EndFrame();

		renderdevice->UploadFrame(screenbuffer.pixels.data(),
		                          screenbuffer.w, screenbuffer.h);
		renderdevice->Present();
		SDL_Delay(16);
	}

	return 0;
}
