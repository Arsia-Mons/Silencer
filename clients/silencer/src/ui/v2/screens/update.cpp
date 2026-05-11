#include "update.h"

#include "context.h"
#include "dispatch.h"
#include "layout.h"
#include "node.h"
#include "render.h"

#include "game.h"
#include "game_state.h"
#include "screen_context.h"
#include "updater.h"
#include "updaterstage2.h"
#include "world.h"
#include "renderer.h"
#include "surface.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace ui {
namespace v2 {

Node BuildUpdate(const Context & ctx, const UpdateHandlers & handlers,
                 const UpdateState * state)
{
	(void)ctx;
	// Mirrors UpdateScreen::Build (clients/silencer/src/ui/screens/update/
	// update_screen.cpp). At preview gate (state == nullptr) the legacy
	// post-Build pre-Tick state renders:
	//   - background overlay sprite (bank=40, idx=4) at (0, 0)
	//   - status/progress overlays contribute no pixels (empty text)
	//   - all four B156x21 buttons draw because Tick — which gates draw on
	//     Updater state — hasn't run. Three of them stack at (161, 230)
	//     with their texts ("Update", "Retry", "Download") stamped on top
	//     of each other in objectlist order; cancel sits alone at (322, 230).
	if(state == nullptr){
		return Background(/*bank=*/40, /*index=*/4, {
			Button("Update",   ButtonType::B156x21).at(161, 230).onClick(handlers.on_update),
			Button("Cancel",   ButtonType::B156x21).at(322, 230).onClick(handlers.on_cancel),
			Button("Retry",    ButtonType::B156x21).at(161, 230).onClick(handlers.on_retry),
			Button("Download", ButtonType::B156x21).at(161, 230).onClick(handlers.on_download),
		});
	}
	// Live path: render only the buttons UpdateScreen::Tick gates as
	// active. Status/progress overlays use textbank=134, textwidth=8,
	// recentered each frame around x=320.
	std::vector<Node> children;
	switch(state->left){
		case UpdateState::LeftButton::Update:
			children.push_back(Button("Update", ButtonType::B156x21)
				.at(161, 230).onClick(handlers.on_update));
		break;
		case UpdateState::LeftButton::Retry:
			children.push_back(Button("Retry", ButtonType::B156x21)
				.at(161, 230).onClick(handlers.on_retry));
		break;
		case UpdateState::LeftButton::Download:
			children.push_back(Button("Download", ButtonType::B156x21)
				.at(161, 230).onClick(handlers.on_download));
		break;
		case UpdateState::LeftButton::None: break;
	}
	if(state->show_cancel){
		children.push_back(Button("Cancel", ButtonType::B156x21)
			.at(322, 230).onClick(handlers.on_cancel));
	}
	if(!state->status_text.empty()){
		int x = 320 - (int)((state->status_text.length() * 8) / 2);
		children.push_back(Label(state->status_text, /*bank=*/134, /*width=*/8).at(x, 200));
	}
	if(!state->progress_text.empty()){
		int x = 320 - (int)((state->progress_text.length() * 8) / 2);
		children.push_back(Label(state->progress_text, /*bank=*/134, /*width=*/8).at(x, 215));
	}
	return Background(/*bank=*/40, /*index=*/4, std::move(children));
}

// -----------------------------------------------------------------------------
// UpdateRuntime — engine wire-in for GameState::UPDATING.
// -----------------------------------------------------------------------------

namespace {

UpdateHandlers BuildUpdateHandlers(ScreenContext & sctx, Updater & updater){
	UpdateHandlers h;
	h.on_update = [&updater](){
		if(updater.GetState() == Updater::PROMPTING){
			updater.Consent();
		}
	};
	h.on_cancel = [&sctx, &updater](){
		Updater::State us = updater.GetState();
		if(us == Updater::PROMPTING || us == Updater::DOWNLOADING || us == Updater::FAILED){
			if(us == Updater::DOWNLOADING){
				updater.Cancel();
			}
			sctx.GoToState(GameState::MAINMENU);
		}
	};
	h.on_retry = [&updater](){
		if(updater.GetState() == Updater::FAILED && updater.GetRetryCount() < 3){
			updater.Retry();
		}
	};
	h.on_download = [&sctx, &updater](){
		if(updater.GetState() == Updater::FAILED && updater.GetRetryCount() >= 3){
			std::string url = updater.GetDownloadURL();
#ifdef _WIN32
			std::string cmd = "start \"\" \"" + url + "\"";
#elif defined(__APPLE__)
			std::string cmd = "open '" + url + "'";
#else
			std::string cmd = "xdg-open '" + url + "' &";
#endif
			system(cmd.c_str());
			sctx.GoToState(GameState::MAINMENU);
		}
	};
	return h;
}

UpdateState CurrentUpdate(Updater & updater){
	UpdateState s;
	Updater::State us = updater.GetState();
	switch(us){
		case Updater::PROMPTING:
			s.left = UpdateState::LeftButton::Update;
			s.show_cancel = true;
			s.status_text = "An update is required to play online.";
		break;
		case Updater::DOWNLOADING:{
			s.left = UpdateState::LeftButton::None;
			s.show_cancel = true;
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%d%%", int(updater.GetProgress() * 100));
			s.status_text = buf;
			int width = int(updater.GetProgress() * 20.0f);
			std::string bar = "[";
			for(int i = 0; i < 20; i++){
				bar += (i < width) ? "=" : " ";
			}
			bar += "]";
			s.progress_text = bar;
		}break;
		case Updater::VERIFYING:
			s.left = UpdateState::LeftButton::None;
			s.show_cancel = true;
			s.status_text = "Verifying...";
		break;
		case Updater::STAGING:
			s.left = UpdateState::LeftButton::None;
			s.show_cancel = true;
			s.status_text = "Restarting...";
		break;
		case Updater::FAILED:{
			s.show_cancel = true;
			s.status_text = updater.GetErrorMessage();
			s.left = (updater.GetRetryCount() < 3)
				? UpdateState::LeftButton::Retry
				: UpdateState::LeftButton::Download;
		}break;
		case Updater::IDLE:
		case Updater::DONE:
		default:
			s.left = UpdateState::LeftButton::None;
			s.show_cancel = true;
		break;
	}
	return s;
}

}  // namespace

UpdateRuntime::UpdateRuntime(World & world, ScreenContext & sctx)
	: world_(world), sctx_(sctx) {}

void UpdateRuntime::Render(Surface & target, ::Renderer & renderer,
                            int mouse_x, int mouse_y, float dt){
	Context ctx{
		world_.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world_.GetVersion(),
	};
	ctx.mouse_x = mouse_x;
	ctx.mouse_y = mouse_y;
	ctx.state   = &state_;
	ctx.dt      = dt;

	UpdateHandlers handlers = BuildUpdateHandlers(sctx_, sctx_.updater);
	UpdateState live = CurrentUpdate(sctx_.updater);
	target.Clear(0);
	state_.BeginFrame();
	Node tree = BuildUpdate(ctx, handlers, &live);
	Layout(tree, ctx);
	::ui::v2::Render(tree, ctx, target, renderer);
	state_.EndFrame();
}

bool UpdateRuntime::DispatchMouseDown(int mouse_x, int mouse_y){
	Context ctx{
		world_.resources,
		/*logical_w=*/640,
		/*logical_h=*/480,
		/*scale=*/1,
		/*version=*/world_.GetVersion(),
	};
	ctx.mouse_x = mouse_x;
	ctx.mouse_y = mouse_y;
	ctx.state   = &state_;
	UpdateHandlers handlers = BuildUpdateHandlers(sctx_, sctx_.updater);
	UpdateState live = CurrentUpdate(sctx_.updater);
	Node tree = BuildUpdate(ctx, handlers, &live);
	Layout(tree, ctx);
	DispatchClick(tree, ctx);
	return true;
}

void UpdateRuntime::Tick(){
	// Mirror UpdateScreen::Tick's STAGING branch: on STAGING, spawn the
	// stage-2 child; on success flag the Updater so Game::Loop returns
	// false next tick and ~Game tears down SDL/audio cleanly before the
	// new process opens the device (avoids audible pop).
	Updater & updater = sctx_.updater;
	if(updater.GetState() != Updater::STAGING) return;
	std::string zippath =
#ifdef _WIN32
		std::string(std::getenv("TEMP") ? std::getenv("TEMP") : ".") + "\\silencer-update.zip";
#else
		"/tmp/silencer-update.zip";
#endif
	std::fprintf(stderr, "[updater] UpdateRuntime::Tick invoking UpdaterStage2::Launch with zip=%s\n",
		zippath.c_str());
	if(UpdaterStage2::Launch(zippath)){
		updater.MarkStage2Spawned();
		return;
	}
	std::fprintf(stderr, "[updater] UpdaterStage2::Launch failed; returning to main menu\n");
	sctx_.GoToState(GameState::MAINMENU);
}

}  // namespace v2
}  // namespace ui
