#pragma once

#include <functional>
#include <string>

namespace client::ui {

// The lobby connection model (doc §6): the connect/auth phase the LobbyConnect
// screen renders, plus the credential-submit and cancel intents. The connect
// *orchestration* (Connect → version-check → auth → route) is lobby protocol
// and runs on the game tick alongside DoNetwork (src/game/session/
// lobby_connect_flow); this model is the read projection of that flow plus the
// two user intents over the public lobby seam. No raw Lobby*/mutex/DoNetwork is
// ever exposed to the screen.
struct LobbySession {
  // The accumulated connection log + MOTD (last lines, newline-joined) — the
  // text the connect screen shows while the flow advances.
  std::string status_log = {};
  // In the connect pipeline (resolving/connecting/checking version) — not yet
  // ready for credentials and not yet authenticated.
  bool connecting = false;
  // The flow has reached AUTHENTICATING: the lobby is ready to accept the typed
  // credentials. `connect()` is a no-op before this point (matches the legacy
  // gate) so an early submit is harmless.
  bool awaiting_credentials = false;
  // Credentials were sent (AUTHSENT) and we are waiting on the auth reply.
  bool credentials_pending = false;
  bool authenticated = false;

  // Submit the typed credentials (SetLocalUsername + SendCredentials), effective
  // only while `awaiting_credentials`. Queued, drained after render.
  std::function<void(const std::string &, const std::string &)> connect = {};
  // Disconnect and return to the main menu. Queued, drained after render.
  std::function<void()> cancel = {};
};

LobbySession use_lobby_session();

} // namespace client::ui
