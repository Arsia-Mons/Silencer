#include "lobby_chat.h"

#include "context.h"
#include "node.h"

namespace ui {
namespace v2 {

Node BuildChatPanel(const Context & ctx, bool chat_active)
{
	(void)ctx;
	std::vector<Node> children;
	children.reserve(3);
	children.push_back(Sprite(/*bank=*/7, /*index=*/11).at(0, 0));
	children.push_back(Sprite(/*bank=*/7, /*index=*/14).at(0, 0));
	if(chat_active){
		children.push_back(FilledRect(/*w=*/1, /*h=*/11, /*color=*/140).at(18, 436));
	}
	return Group(std::move(children));
}

}  // namespace v2
}  // namespace ui
