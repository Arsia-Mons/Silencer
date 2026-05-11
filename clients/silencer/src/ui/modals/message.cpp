#include "message.h"

#include "context.h"
#include "node.h"

namespace ui {
namespace v2 {

Node BuildMessage(const Context & ctx,
                  const std::string & message,
                  bool has_ok,
                  const MessageHandlers & handlers)
{
	(void)ctx;
	const int text_x = 320 - ((int)message.size() * 8) / 2;
	const int text_y = has_ok ? 200 : 218;

	std::vector<Node> children;
	children.push_back(Label(message, /*font_bank=*/134, /*font_width=*/8)
	                       .at((Sint16)text_x, (Sint16)text_y));
	if(has_ok){
		children.push_back(Button("OK", ButtonType::B156x21)
		                       .at(242, 230)
		                       .onClick(handlers.on_ok));
	}
	return Background(/*bank=*/40, /*index=*/4, std::move(children));
}

}  // namespace v2
}  // namespace ui
