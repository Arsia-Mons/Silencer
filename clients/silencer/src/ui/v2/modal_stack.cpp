#include "modal_stack.h"

#include "context.h"
#include "dispatch.h"
#include "layout.h"
#include "modals/message.h"
#include "modals/password.h"
#include "node.h"
#include "render.h"

#include "world.h"
#include "renderer.h"
#include "surface.h"

#include <SDL3/SDL_scancode.h>

namespace ui { namespace v2 {

void ModalStack::ShowMessage(const std::string & text, std::function<void()> on_close){
	Entry m;
	m.kind     = MESSAGE;
	m.text     = text;
	m.has_ok   = true;
	m.on_close = std::move(on_close);
	stack_.push_back(std::move(m));
}

void ModalStack::ShowProgress(const std::string & text){
	Entry m;
	m.kind   = MESSAGE;
	m.text   = text;
	m.has_ok = false;
	stack_.push_back(std::move(m));
}

void ModalStack::ShowPassword(std::function<void(const std::string &)> on_submit){
	Entry m;
	m.kind      = PASSWORD;
	m.on_submit = std::move(on_submit);
	stack_.push_back(std::move(m));
}

void ModalStack::SetProgressText(const std::string & text){
	if(stack_.empty()) return;
	stack_.back().text = text;
}

void ModalStack::Pop(){
	if(stack_.empty()) return;
	stack_.pop_back();
}

bool ModalStack::ProgressActive() const {
	if(stack_.empty()) return false;
	const Entry & top = stack_.back();
	return top.kind == MESSAGE && !top.has_ok;
}

void ModalStack::RenderOverlay(Surface & target, ::Renderer & renderer,
                                int logical_w, int logical_h, int scale){
	if(stack_.empty()) return;
	const Entry & top = stack_.back();

	Context ctx{
		world_.resources,
		/*logical_w=*/logical_w,
		/*logical_h=*/logical_h,
		/*scale=*/scale,
		/*version=*/world_.GetVersion(),
	};
	ctx.mouse_x = mouse_x_;
	ctx.mouse_y = mouse_y_;
	ctx.state   = &state_;

	Node tree;
	if(top.kind == MESSAGE){
		MessageHandlers h;
		h.on_ok = [this](){
			auto cb = stack_.empty() ? std::function<void()>{} : std::move(stack_.back().on_close);
			Pop();
			if(cb) cb();
		};
		tree = BuildMessage(ctx, top.text, top.has_ok, h);
	}else{
		PasswordHandlers h;
		h.on_submit = [this](const std::string & captured){
			auto cb = stack_.empty() ? std::function<void(const std::string &)>{} : std::move(stack_.back().on_submit);
			Pop();
			if(cb) cb(captured);
		};
		tree = BuildPassword(ctx, top.password_buf, h);
	}
	Layout(tree, ctx);
	::ui::v2::Render(tree, ctx, target, renderer);
}

bool ModalStack::DispatchClick(int logical_x, int logical_y,
                                int logical_w, int logical_h, int scale){
	if(stack_.empty()) return false;
	const Entry & top = stack_.back();

	Context ctx{
		world_.resources,
		/*logical_w=*/logical_w,
		/*logical_h=*/logical_h,
		/*scale=*/scale,
		/*version=*/world_.GetVersion(),
	};
	ctx.mouse_x = logical_x;
	ctx.mouse_y = logical_y;
	ctx.state   = &state_;

	Node tree;
	if(top.kind == MESSAGE){
		MessageHandlers h;
		h.on_ok = [this](){
			auto cb = stack_.empty() ? std::function<void()>{} : std::move(stack_.back().on_close);
			Pop();
			if(cb) cb();
		};
		tree = BuildMessage(ctx, top.text, top.has_ok, h);
	}else{
		PasswordHandlers h;
		h.on_submit = [this](const std::string & captured){
			auto cb = stack_.empty() ? std::function<void(const std::string &)>{} : std::move(stack_.back().on_submit);
			Pop();
			if(cb) cb(captured);
		};
		tree = BuildPassword(ctx, top.password_buf, h);
	}
	Layout(tree, ctx);
	::ui::v2::DispatchClick(tree, ctx);
	return true;
}

bool ModalStack::DispatchKey(int sdl_scancode){
	if(stack_.empty()) return false;
	Entry & top = stack_.back();
	switch(sdl_scancode){
		case SDL_SCANCODE_RETURN:{
			if(top.kind == MESSAGE){
				if(top.has_ok){
					auto cb = std::move(top.on_close);
					Pop();
					if(cb) cb();
				}
			}else{
				std::string captured = top.password_buf;
				auto cb = std::move(top.on_submit);
				Pop();
				if(cb) cb(captured);
			}
			return true;
		}
		case SDL_SCANCODE_ESCAPE:{
			// MESSAGE with OK + PASSWORD both dismiss on ESC. Progress
			// (has_ok=false) has no user-driven dismiss path — caller
			// pops when its work finishes.
			if(top.kind == MESSAGE && !top.has_ok) return true;
			if(top.kind == MESSAGE){
				auto cb = std::move(top.on_close);
				Pop();
				if(cb) cb();
			}else{
				Pop();
			}
			return true;
		}
		case SDL_SCANCODE_BACKSPACE:{
			if(top.kind == PASSWORD && !top.password_buf.empty()){
				top.password_buf.pop_back();
			}
			return true;
		}
		default: break;
	}
	// Always swallow keys while a modal is up — the underlying state
	// shouldn't receive them.
	return true;
}

bool ModalStack::DispatchText(char ascii){
	if(stack_.empty()) return false;
	Entry & top = stack_.back();
	if(top.kind == PASSWORD){
		// Mirror legacy TextInput maxchars=20 cap (password_modal.cpp:47).
		if(top.password_buf.size() < 20) top.password_buf.push_back(ascii);
	}
	return true;
}

}}  // namespace ui::v2
