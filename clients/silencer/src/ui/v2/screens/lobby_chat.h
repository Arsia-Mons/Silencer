#ifndef SILENCER_UI_V2_SCREENS_LOBBY_CHAT_H
#define SILENCER_UI_V2_SCREENS_LOBBY_CHAT_H

#include "shared.h"

namespace ui {
namespace v2 {

struct Node;
struct Context;

// Chat panel: two chrome sprites (bank 7 idx 11 = scrollback/presence
// border, bank 7 idx 14 = chat input border) plus the chat input caret.
//
// At preview-gate time the channel-name overlay, scrollback textbox,
// presence textbox and chat input itself all have empty text and
// contribute no pixels. The scrollbar's `draw` flag defaults to false
// (ScrollBar::ScrollBar) and the legacy renderer short-circuits with
// src=0, so it contributes no pixels either.
//
// The chat input is the active object of the chat sub-interface — the
// parent LobbyScreen calls Interface::ActiveChanged(mouse=false) after
// chat.Build, which sets showcaret=true on it. The renderer's
// state_i % 32 < 16 gate is true at frame 0, so the caret rect renders.
// FilledRect(1, 11, 140) at (18, 436) mirrors DrawTextInput's rect:
// width=1, height=(int)(14 * 0.8)=11, color=textinput.caretcolor=140.
Node BuildChatPanel(const Context & ctx);

}  // namespace v2
}  // namespace ui

#endif
