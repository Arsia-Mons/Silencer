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
};

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
