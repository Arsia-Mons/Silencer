#ifndef SILENCER_UI_V2_MODALS_MESSAGE_H
#define SILENCER_UI_V2_MODALS_MESSAGE_H

#include <functional>
#include <string>

namespace ui {
namespace v2 {

struct Node;
struct Context;

struct MessageHandlers {
	std::function<void()> on_ok;
};

// Mirrors MessageModal::Build (clients/silencer/src/ui/modals/message_modal.cpp).
// `has_ok=false` is the Progress variant — text shifts down to y=218 and no
// OK button is emitted; caller pops the modal externally when its work
// completes.
Node BuildMessage(const Context & ctx,
                  const std::string & message,
                  bool has_ok = true,
                  const MessageHandlers & handlers = {});

}  // namespace v2
}  // namespace ui

#endif
