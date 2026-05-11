#include "lobby_chat.h"

#include "context.h"
#include "node.h"

namespace ui {
namespace v2 {

Node BuildChatPanel(const Context & ctx)
{
	(void)ctx;
	return Group({
		Sprite(/*bank=*/7, /*index=*/11).at(0, 0),
		Sprite(/*bank=*/7, /*index=*/14).at(0, 0),
		FilledRect(/*w=*/1, /*h=*/11, /*color=*/140).at(18, 436),
	});
}

}  // namespace v2
}  // namespace ui
