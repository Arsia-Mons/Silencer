#ifndef SILENCER_NET_UI_EDITOR_PREVIEW_CONTROL_H
#define SILENCER_NET_UI_EDITOR_PREVIEW_CONTROL_H

#include "controlserver.h"

class Game;

namespace ControlDispatch {

ControlReply HandleUiEditorPreview(Game& game, ControlCommand& cmd);
ControlReply HandleUiEditorPreviewCapture(Game& game, ControlCommand& cmd);

}  // namespace ControlDispatch

#endif
