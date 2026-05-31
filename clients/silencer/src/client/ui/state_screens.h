#ifndef SILENCER_CLIENT_UI_STATE_SCREENS_H
#define SILENCER_CLIENT_UI_STATE_SCREENS_H

#include <SDL3/SDL_stdinc.h>

class ScreenContext;

namespace silencer {
namespace client_ui {

class ClientUi;

void ShowStateScreen(ClientUi & clientUi, ScreenContext & ctx, Uint8 uiState);

}  // namespace client_ui
}  // namespace silencer

#endif
