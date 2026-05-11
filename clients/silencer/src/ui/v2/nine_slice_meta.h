#ifndef SILENCER_UI_V2_NINE_SLICE_META_H
#define SILENCER_UI_V2_NINE_SLICE_META_H

#include "shared.h"

namespace ui {
namespace v2 {

// 9-slice metadata for a chrome sprite. The sprite is sliced into 9 regions:
//
//     +----+--------+----+
//     | TL |   T    | TR |
//     +----+--------+----+
//     |  L |        |  R |
//     |    |   C    |    |
//     +----+--------+----+
//     | BL |   B    | BR |
//     +----+--------+----+
//
// Where the corners are blitted 1:1 at the new rect's corners, the edges
// (T / B / L / R) tile along their axis, and the center C tiles across
// the remaining area. `corner_l/r/t/b` describe the corner sizes in
// source pixels; the inner span is whatever's left of the source sprite
// after removing those margins.
//
// Coverage: bank 7 chrome used by the lobby (eyeballed corner sizes —
// kept conservative so the slice borders never overlap each other for
// the smallest target rects used in practice). The metadata is data,
// not behavior — adding new entries is a one-line addition below.
struct NineSliceMeta {
	Uint8 bank;
	Uint8 index;
	Uint8 corner_l;
	Uint8 corner_r;
	Uint8 corner_t;
	Uint8 corner_b;
	bool  has_center;  // false ⇒ leave the inside transparent (frame-only)
};

// Returns true and fills `out` if a metadata entry exists for (bank, index).
// Falls back to (8, 8, 8, 8, true) if nothing matches — keeps the renderer
// from crashing on an unconfigured sprite. Authors are expected to add the
// real entry as soon as they notice the fallback.
inline bool LookupNineSlice(Uint8 bank, Uint8 index, NineSliceMeta & out) {
	static const NineSliceMeta table[] = {
		// bank 7, index 11 — chat scrollback border (large outer frame).
		{ 7, 11, 8, 8, 8, 8, true  },
		// bank 7, index 12 — scrollbar bar (narrow vertical chrome).
		{ 7, 12, 4, 4, 4, 4, true  },
		// bank 7, index 14 — chat input border (thin one-line frame).
		{ 7, 14, 6, 6, 6, 6, true  },
		// bank 7, index 24 — B156x21 button chrome (frame + interior).
		{ 7, 24, 6, 6, 6, 6, true  },
	};
	for(const NineSliceMeta & m : table){
		if(m.bank == bank && m.index == index){
			out = m;
			return true;
		}
	}
	out = NineSliceMeta{ bank, index, 8, 8, 8, 8, true };
	return false;
}

}  // namespace v2
}  // namespace ui

#endif
