#ifndef SILENCER_UI_V2_NODE_H
#define SILENCER_UI_V2_NODE_H

#include "shared.h"
#include <functional>
#include <string>
#include <vector>

namespace ui {
namespace v2 {

enum class NodeKind : Uint8 {
	Group,        // No drawing of its own; just contains children.
	Background,   // Sprite covering the screen — drawn first.
	Sprite,       // Sprite at logical (x, y); the asset's baked anchor
	              // offset is honored by the renderer (legacy semantics).
	Label,        // Text drawn at logical (x, y) using a sprite-font bank.
	Button,       // Pressable widget — chrome + centered label.
	FilledRect,   // Solid color rectangle (palette index) at (x, y) sized w×h.
	NineSliceFrame, // Sprite-based 9-slice container — corners + tiled edges
	                // + center fill, sized to the layout rect (or .at()+w/h).

	// Container kinds — laid out by the Clay layout pass. Walk emits a
	// Clay scope per container; descendants get computed rects written
	// into rect_x/y/w/h, which the render + dispatch passes consume.
	VStack,       // Children stacked top→bottom, .gap(n) between.
	HStack,       // Children stacked left→right, .gap(n) between.
	Center,       // Single-axis "fill available space, center child(ren)".
	Padding,      // Inset its single child by .padding(n) on all sides.
	Spacer,       // Empty growable element — pushes siblings apart.
};

// 1:1 mapping to the legacy Button::Type values.
enum class ButtonType : Uint8 {
	B112x33,
	B220x33,
	B196x33,
	B236x27,
	B52x21,
	B156x21,
};

struct Node {
	NodeKind kind = NodeKind::Group;

	// Logical-pixel anchor. Sprite assets carry their own offset, applied
	// by the renderer — this matches today's widget semantics so a node
	// at the same (x, y) as the legacy widget produces the same pixels.
	Sint16 x = 0;
	Sint16 y = 0;

	// Sprite / Background / Button: which sprite-bank slot to draw.
	Uint8 sprite_bank = 0;
	Uint8 sprite_index = 0;

	// Label / Button: rendered text + the sprite-font bank and per-glyph
	// advance used to draw it.
	std::string text;
	Uint8 text_bank = 0;
	Uint8 text_width = 0;

	// Button-specific.
	ButtonType button_type = ButtonType::B196x33;
	std::function<void()> on_click;

	// Stable identity for `UIState` lookups (hover animation phase,
	// focus, etc.). Empty = "no per-instance state" — fine for
	// purely visual nodes. The `Button` factory auto-fills this from
	// the label since labels are unique within a screen; authors
	// override via `.key("foo")` for cases where the default
	// collides (e.g. two buttons with identical text in different
	// panels) or where a non-Button node needs animation.
	std::string key;

	// Container-only: per-child gap (VStack/HStack) and uniform padding
	// (Padding). 0 = no gap / no padding. Reserved for future expansion
	// to {top,right,bottom,left} if uniform proves insufficient.
	Uint16 gap = 0;
	Uint16 pad = 0;

	// FilledRect / NineSliceFrame: pixel dimensions used when no container
	// subtree has set rect_*. NineSliceFrame uses these to size its box on
	// the absolute `.at()` path; FilledRect uses them as it always has.
	// Defaults to (0, 0) so empty values are harmless.
	Uint16 fill_w = 0;
	Uint16 fill_h = 0;
	Uint8  fill_color = 0;

	// NineSliceFrame-only: which chrome sprite to slice. (bank, index) keys
	// into the NineSliceMeta table for the corner sizes; the renderer reads
	// `spritewidth[bank][index]` for the source dimensions.
	Uint8  nine_bank  = 0;
	Uint8  nine_index = 0;

	// Label-only: effect color / brightness / ramp passed to
	// `Renderer::DrawText`. Defaults match the legacy Overlay-text path
	// (effectcolor=0, effectbrightness=128, textcolorramp=false), so
	// untouched Labels render identically to before this field set existed.
	Uint8 effect_color      = 0;
	Uint8 effect_brightness = 128;
	bool  text_color_ramp   = false;
	// Label-only: when true, glyphs blit through `DrawAlphaed` (palette-blend
	// vs. dst) rather than the default `BlitSurface` (write src directly).
	// Required to mirror the legacy `Button::DrawText(... alpha=true)` code
	// path used by chrome-less BNONE buttons (e.g. game_create's "Security:"
	// row toggle text).
	bool  text_alpha        = false;

	// Layout output. Set by `layout.cpp` for nodes inside a container
	// subtree. `rect_w == 0` means "not laid out, fall back to (x, y)
	// + ChromeFor.width/height" — preserves the absolute `.at()`
	// escape hatch for screens that need pixel-identical legacy parity.
	Sint16 rect_x = 0;
	Sint16 rect_y = 0;
	Uint16 rect_w = 0;
	Uint16 rect_h = 0;

	// Children. Drawn after self in declaration order.
	std::vector<Node> children;

	// ----- Chainable modifiers -----
	// Return Node& so call sites can read JSX-like:
	//     Button("Exit").at(0, 67).onClick(...)
	// The chain operates on the temporary; the final value is copied or
	// moved into its destination (vector element, return, etc.).

	Node & at(Sint16 nx, Sint16 ny) { x = nx; y = ny; return *this; }
	Node & onClick(std::function<void()> handler) { on_click = std::move(handler); return *this; }
	Node & withKey(std::string k) { key = std::move(k); return *this; }
	Node & withGap(Uint16 g) { gap = g; return *this; }
	Node & withPadding(Uint16 p) { pad = p; return *this; }
	Node & withColor(Uint8 c) { effect_color = c; return *this; }
	Node & withBrightness(Uint8 b) { effect_brightness = b; return *this; }
	Node & withRamp(bool r = true) { text_color_ramp = r; return *this; }
	Node & withAlpha(bool a = true) { text_alpha = a; return *this; }
	Node & withSize(Uint16 w, Uint16 h) { fill_w = w; fill_h = h; return *this; }
};

inline bool IsContainer(NodeKind k) {
	return k == NodeKind::VStack || k == NodeKind::HStack ||
	       k == NodeKind::Center || k == NodeKind::Padding ||
	       k == NodeKind::Spacer;
}

// ----- Factories -----

inline Node Group(std::vector<Node> children) {
	Node n;
	n.kind = NodeKind::Group;
	n.children = std::move(children);
	return n;
}

inline Node Background(Uint8 bank, Uint8 index, std::vector<Node> children = {}) {
	Node n;
	n.kind = NodeKind::Background;
	n.sprite_bank = bank;
	n.sprite_index = index;
	n.children = std::move(children);
	return n;
}

inline Node Sprite(Uint8 bank, Uint8 index = 0) {
	Node n;
	n.kind = NodeKind::Sprite;
	n.sprite_bank = bank;
	n.sprite_index = index;
	return n;
}

inline Node Label(std::string text, Uint8 font_bank, Uint8 font_width) {
	Node n;
	n.kind = NodeKind::Label;
	n.text = std::move(text);
	n.text_bank = font_bank;
	n.text_width = font_width;
	return n;
}

inline Node VStack(std::vector<Node> children) {
	Node n;
	n.kind = NodeKind::VStack;
	n.children = std::move(children);
	return n;
}

inline Node HStack(std::vector<Node> children) {
	Node n;
	n.kind = NodeKind::HStack;
	n.children = std::move(children);
	return n;
}

inline Node Center(std::vector<Node> children) {
	Node n;
	n.kind = NodeKind::Center;
	n.children = std::move(children);
	return n;
}

inline Node Padding(Uint16 amount, std::vector<Node> children) {
	Node n;
	n.kind = NodeKind::Padding;
	n.pad = amount;
	n.children = std::move(children);
	return n;
}

inline Node Spacer() {
	Node n;
	n.kind = NodeKind::Spacer;
	return n;
}

inline Node NineSliceFrame(Uint8 bank, Uint8 index, Uint16 w = 0, Uint16 h = 0, std::vector<Node> children = {}) {
	Node n;
	n.kind        = NodeKind::NineSliceFrame;
	n.nine_bank   = bank;
	n.nine_index  = index;
	n.fill_w      = w;
	n.fill_h      = h;
	n.children    = std::move(children);
	return n;
}

inline Node FilledRect(Uint16 w, Uint16 h, Uint8 color) {
	Node n;
	n.kind = NodeKind::FilledRect;
	n.fill_w = w;
	n.fill_h = h;
	n.fill_color = color;
	return n;
}

inline Node Button(std::string text, ButtonType type = ButtonType::B196x33) {
	Node n;
	n.kind = NodeKind::Button;
	n.text = text;
	n.button_type = type;
	// Default key is the label — cheap, unique within a screen for the
	// usual case, and shows up readably in any state-debug dump.
	// `withKey()` overrides if the author needs disambiguation.
	n.key = "btn:" + std::move(text);
	return n;
}

}  // namespace v2
}  // namespace ui

#endif
