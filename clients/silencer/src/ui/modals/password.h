#ifndef SILENCER_UI_V2_MODALS_PASSWORD_H
#define SILENCER_UI_V2_MODALS_PASSWORD_H

#include <functional>
#include <string>

namespace ui {
namespace v2 {

struct Node;
struct Context;

struct PasswordHandlers {
	// Invoked on OK click / Enter. Receives the captured password text
	// (caller is responsible for popping the modal and propagating).
	std::function<void(const std::string &)> on_submit;
};

// Mirrors PasswordModal::Build (clients/silencer/src/ui/modals/password_modal.cpp).
// `password_text` is the current contents of the password input — the live
// engine wiring tracks edits and re-builds the tree each frame. At
// preview-gate time it stays empty, so the rendered glyphs are zero
// characters and only the active caret rect appears.
Node BuildPassword(const Context & ctx,
                   const std::string & password_text,
                   const PasswordHandlers & handlers = {});

}  // namespace v2
}  // namespace ui

#endif
