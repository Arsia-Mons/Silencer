#include "lobby_connect.h"

#include "context.h"
#include "dispatch.h"
#include "layout.h"
#include "node.h"
#include "render.h"

#include "ambience_mixer.h"
#include "config.h"
#include "game.h"
#include "game_state.h"
#include "lobby.h"
#include "screen_context.h"
#include "updater.h"
#include "world.h"
#include "renderer.h"
#include "surface.h"

#include <SDL3/SDL_scancode.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace ui {
namespace v2 {

Node BuildLobbyConnect(const Context & ctx, const LobbyConnectHandlers & handlers, const LobbyConnectState * state)
{
	(void)ctx;
	// Mirrors LobbyConnectScreen::Build (clients/silencer/src/ui/screens/
	// lobby_connect/lobby_connect_screen.cpp). At preview gate state
	// (post-Build, after iface->ActiveChanged(mouse=false), pre-Tick) the
	// rendered pixels are:
	//   - background sprite (bank=7, idx=2)
	//   - "Username" / "Password" Overlay text (bank=134, width=9)
	//   - Login / Cancel button text (B52x21: no chrome, text only)
	//   - The active username TextInput's caret (1×11 rect, color 140) —
	//     iface->ActiveChanged sets showcaret=true on the active input;
	//     renderer.cpp:1797 draws it because state_i % 32 < 16 holds with
	//     state_i==0 (preview gate skips renderer.Tick).
	// TextBox/TextInput sprites + Interface itself contribute no pixels
	// (empty text, res_bank=0xFF/135 with res_index=0 = empty glyph).
	std::vector<Node> children;
	children.push_back(Label("Username", /*font_bank=*/134, /*font_width=*/9).at(190, 291));
	children.push_back(Label("Password", /*font_bank=*/134, /*font_width=*/9).at(190, 318));
	children.push_back(Button("Login",  ButtonType::B52x21).at(264, 339).onClick(handlers.on_login));
	children.push_back(Button("Cancel", ButtonType::B52x21).at(321, 339).onClick(handlers.on_cancel));

	if(state == nullptr){
		// Preview / PPM dump path — replicate legacy pre-Tick output exactly:
		// the username field is the activeobject and its caret is drawn at
		// offset 0 (empty buffer).
		children.push_back(FilledRect(1, 11, 140).at(275, 292));
	}else{
		// Live engine path. Inputs render their own text + caret; status
		// lines accumulated by Game::TickLobbyConnectV2 render as Labels at
		// the legacy textbox (x=185, y=101, lineheight=11, fontwidth=6,
		// bank=133).
		const char * u = state->username ? state->username : "";
		const char * p = state->password ? state->password : "";
		size_t ulen = std::strlen(u);
		size_t plen = std::strlen(p);
		// Effect-brightness 64 mirrors the legacy `if(inactive) brightness = 64`
		// path in Renderer::DrawTextInput.
		Uint8 b = state->inactive ? 64u : 128u;
		if(ulen > 0){
			children.push_back(
				Label(std::string(u), /*font_bank=*/133, /*font_width=*/6)
				.at(275, 293).withBrightness(b));
		}
		if(plen > 0){
			std::string masked(plen, '*');
			children.push_back(
				Label(masked, /*font_bank=*/133, /*font_width=*/6)
				.at(275, 320).withBrightness(b));
		}
		if(!state->inactive && state->caret_visible){
			if(state->active_field == 1){
				int cx = 275 + (int)ulen * 6;
				children.push_back(FilledRect(1, 11, 140).at((Sint16)cx, 292));
			}else if(state->active_field == 2){
				int cx = 275 + (int)plen * 6;
				children.push_back(FilledRect(1, 11, 140).at((Sint16)cx, 319));
			}
		}
		// Textbox lines (no scrolling — line count is small in practice).
		int line = 0;
		int max_lines = 170 / 11;  // legacy textbox height/lineheight
		for(const auto & s : state->textbox_lines){
			if(line >= max_lines) break;
			if(!s.empty()){
				children.push_back(Label(s, /*font_bank=*/133, /*font_width=*/6)
					.at(185, (Sint16)(101 + line * 11)));
			}
			line++;
		}
	}
	return Background(/*bank=*/7, /*index=*/2, std::move(children));
}

// -----------------------------------------------------------------------------
// LobbyConnectRuntime — engine wire-in for GameState::LOBBYCONNECT.
// -----------------------------------------------------------------------------

namespace {

LobbyConnectHandlers BuildLobbyConnectHandlers(LobbyConnectRuntime * self){
	LobbyConnectHandlers h;
	h.on_login  = [self](){ self->DispatchKeyDown(SDL_SCANCODE_RETURN); };
	h.on_cancel = [self](){ self->DispatchKeyDown(SDL_SCANCODE_ESCAPE); };
	return h;
}

LobbyConnectState BuildLive(const World & world, const char * username,
                             const char * password, int active_field,
                             const std::vector<std::string> & lines, unsigned frames){
	LobbyConnectState s;
	s.username      = username;
	s.password      = password;
	s.active_field  = active_field;
	// AUTHSENT = "Sending credentials" — legacy disables both TextInputs while
	// waiting on the server's response.
	s.inactive      = (world.lobby.state == Lobby::AUTHSENT);
	// 24 fps legacy blink: state_i % 32 < 16. We tick our own counter
	// in Tick() to match Game::frames' cadence.
	s.caret_visible = ((frames & 31) < 16);
	s.textbox_lines = lines;
	return s;
}

}  // namespace

LobbyConnectRuntime::LobbyConnectRuntime(World & world, ScreenContext & sctx)
	: world_(world), sctx_(sctx) {
	frames_ = (unsigned)sctx_.game.GetFrameCount();
}

void LobbyConnectRuntime::Submit(){
	// Mirrors LobbyConnectScreen::Tick's LBY_BTN_LOGIN handler.
	if(world_.lobby.state == Lobby::AUTHENTICATING){
		world_.lobby.SetLocalUsername(username_);
		world_.lobby.SendCredentials(username_, password_);
		world_.lobby.state = Lobby::AUTHSENT;
	}
}

void LobbyConnectRuntime::Cancel(){
	sctx_.GoToState(GameState::MAINMENU);
}

void LobbyConnectRuntime::CycleField(){
	active_field_ = (active_field_ == 1) ? 2 : 1;
}

void LobbyConnectRuntime::AppendChar(char c){
	if(world_.lobby.state == Lobby::AUTHSENT) return;
	if(active_field_ == 1){
		size_t len = std::strlen(username_);
		if(len < sizeof(username_) - 1){
			username_[len]   = c;
			username_[len+1] = '\0';
		}
	}else if(active_field_ == 2){
		size_t len = std::strlen(password_);
		if(len < sizeof(password_) - 1){
			password_[len]   = c;
			password_[len+1] = '\0';
		}
	}
}

void LobbyConnectRuntime::Backspace(){
	if(world_.lobby.state == Lobby::AUTHSENT) return;
	if(active_field_ == 1){
		size_t len = std::strlen(username_);
		if(len > 0) username_[len-1] = '\0';
	}else if(active_field_ == 2){
		size_t len = std::strlen(password_);
		if(len > 0) password_[len-1] = '\0';
	}
}

void LobbyConnectRuntime::Render(Surface & target, ::Renderer & renderer,
                                  int mouse_x, int mouse_y, float dt,
                              int logical_w, int logical_h, int scale){
	Context ctx{
		world_.resources,
		/*logical_w=*/logical_w,
		/*logical_h=*/logical_h,
		/*scale=*/scale,
		/*version=*/world_.GetVersion(),
	};
	ctx.mouse_x = mouse_x;
	ctx.mouse_y = mouse_y;
	ctx.state   = &state_;
	ctx.dt      = dt;

	LobbyConnectHandlers handlers = BuildLobbyConnectHandlers(this);
	LobbyConnectState live = BuildLive(world_, username_, password_, active_field_,
	                                    textbox_lines_, frames_);
	target.Clear(0);
	state_.BeginFrame();
	Node tree = BuildLobbyConnect(ctx, handlers, &live);
	Layout(tree, ctx);
	::ui::v2::Render(tree, ctx, target, renderer);
	state_.EndFrame();
}

bool LobbyConnectRuntime::DispatchMouseDown(int mouse_x, int mouse_y,
                                int logical_w, int logical_h, int scale){
	// Hit-test the username/password input rects directly to swap active field
	// — Buttons handle Login/Cancel via the v2 dispatch pass.
	// Username box: x=[275, 275+180), y=[293, 293+14)
	// Password box: x=[275, 275+180), y=[320, 320+14)
	if(world_.lobby.state != Lobby::AUTHSENT){
		if(mouse_x >= 275 && mouse_x < 275 + 180){
			if(mouse_y >= 293 && mouse_y < 293 + 14){
				active_field_ = 1;
			}else if(mouse_y >= 320 && mouse_y < 320 + 14){
				active_field_ = 2;
			}
		}
	}
	Context ctx{
		world_.resources,
		/*logical_w=*/logical_w,
		/*logical_h=*/logical_h,
		/*scale=*/scale,
		/*version=*/world_.GetVersion(),
	};
	ctx.mouse_x = mouse_x;
	ctx.mouse_y = mouse_y;
	ctx.state   = &state_;
	LobbyConnectHandlers handlers = BuildLobbyConnectHandlers(this);
	LobbyConnectState live = BuildLive(world_, username_, password_, active_field_,
	                                    textbox_lines_, frames_);
	Node tree = BuildLobbyConnect(ctx, handlers, &live);
	Layout(tree, ctx);
	DispatchClick(tree, ctx);
	return true;
}

bool LobbyConnectRuntime::DispatchKeyDown(int sdl_scancode){
	switch(sdl_scancode){
		case SDL_SCANCODE_BACKSPACE: Backspace();    return true;
		case SDL_SCANCODE_RETURN:    Submit();       return true;
		case SDL_SCANCODE_ESCAPE:    Cancel();       return true;
		case SDL_SCANCODE_TAB:       CycleField();   return true;
		default: return false;
	}
}

bool LobbyConnectRuntime::DispatchTextInput(char ascii){
	AppendChar(ascii);
	return true;
}

void LobbyConnectRuntime::Tick(){
	frames_++;
	// Mirrors LobbyConnectScreen::Tick. Gate on ambience-fade-in so the
	// "Connecting to ..." line doesn't print before the music crossfade lands.
	if(!sctx_.ambienceMixer.FadedIn()) return;
	auto push_line = [this](const char * s){ textbox_lines_.emplace_back(s); };
	world_.lobby.LockMutex();
	switch(world_.lobby.state){
		case Lobby::CONNECTING:
		case Lobby::WAITINGFORRESOLVER:
		case Lobby::AUTHSENT:
		case Lobby::IDLE:
		break;
		case Lobby::WAITING:{
			char line[128];
			std::snprintf(line, sizeof(line), "Connecting to %s:%d",
				Config::GetInstance().lobbyhost, Config::GetInstance().lobbyport);
			push_line(line);
			world_.lobby.Connect(Config::GetInstance().lobbyhost, Config::GetInstance().lobbyport);
		}break;
		case Lobby::RESOLVING:
			push_line("Resolving hostname...");
			world_.lobby.state = Lobby::WAITINGFORRESOLVER;
		break;
		case Lobby::RESOLVEFAILED:
			push_line("Could not resolve hostname");
			world_.lobby.state = Lobby::IDLE;
		break;
		case Lobby::RESOLVED:
			push_line("Hostname resolved");
			world_.lobby.Connect(Config::GetInstance().lobbyhost, Config::GetInstance().lobbyport);
		break;
		case Lobby::CONNECTED:
			push_line("Connected");
			push_line("Checking version...");
			world_.lobby.SendVersion();
			world_.lobby.state = Lobby::CHECKINGVERSION;
		break;
		case Lobby::CHECKINGVERSION:
			if(world_.lobby.versionchecked){
				if(world_.lobby.versionok){
					push_line("Software version is current");
					world_.lobby.state = Lobby::AUTHENTICATING;
				}else{
					if(world_.lobby.updateavailable){
						sctx_.updater.PresentUpdate(world_.lobby.updateurl, world_.lobby.updatesha256);
						world_.lobby.Disconnect();
						world_.lobby.state = Lobby::IDLE;
						world_.lobby.UnlockMutex();
						sctx_.GoToState(GameState::UPDATING);
						return;
					}else{
						push_line("Software is out of date");
						push_line("Get latest version at:");
						push_line("https://github.com/Arsia-Mons/Silencer");
						world_.lobby.Disconnect();
						world_.lobby.state = Lobby::IDLE;
					}
				}
			}
		break;
		case Lobby::AUTHENTICATING:
		break;
		case Lobby::AUTHFAILED:
			push_line("Authentication failed");
			if(std::strlen(world_.lobby.failmessage) > 0) push_line(world_.lobby.failmessage);
			world_.lobby.state = Lobby::AUTHENTICATING;
		break;
		case Lobby::AUTHENTICATED:
			push_line("Authenticated");
			world_.lobby.UnlockMutex();
			sctx_.GoToState(GameState::LOBBY);
			return;
		case Lobby::CONNECTIONFAILED:
			push_line("Connection failed");
			world_.lobby.state = Lobby::IDLE;
		break;
		case Lobby::DISCONNECTED:
			push_line("Disconnected");
			world_.lobby.state = Lobby::IDLE;
		break;
	}
	if(world_.lobby.motdreceived && !motd_printed_){
		char * line = std::strtok(world_.lobby.motd, "\n");
		while(line != 0){
			push_line(line);
			line = std::strtok(NULL, "\n");
		}
		motd_printed_ = true;
	}
	world_.lobby.UnlockMutex();
}

}  // namespace v2
}  // namespace ui
