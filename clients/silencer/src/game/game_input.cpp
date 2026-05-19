#include "game.h"
#include "player.h"
#include "screen.h"
#include <cstring>

using namespace GameState;

void Game::TickRumble(){
	if(!gamepad || world.gameplaystate != World::INGAME) return;
	Player* player = world.GetPeerPlayer(world.localpeerid);
	if(!player) return;

	// Fire: short high-frequency click
	if(player->rumbleFire){
		player->rumbleFire = false;
		SDL_RumbleGamepad(gamepad, 0, 12000, 80);
	}
	// Hit: strong punch on both motors
	if(player->rumbleHit){
		player->rumbleHit = false;
		SDL_RumbleGamepad(gamepad, 30000, 15000, 200);
	}
	// Land: low thud (left motor only)
	if(player->rumbleLand){
		player->rumbleLand = false;
		SDL_RumbleGamepad(gamepad, 18000, 0, 120);
	}
}


void Game::TickGamepadMenuNav(){
	if(!gamepadstate.connected) return;
	Player * localplayer = world.GetPeerPlayer(world.localpeerid);
	bool inGameUi = localplayer && (localplayer->chatActive || localplayer->isbuying || localplayer->techstationactive);
	Screen * top = GetTopScreen();
	if(!top && !inGameUi) return;

	Uint32 now = SDL_GetTicks();

	// Helper: fire a nav key press with software repeat on held direction.
	auto tick = [&](GamepadNavDir& dir, Action action, silencer::ui::UiNavAction navAction){
		bool pressed = keymap.IsPressed(action, keystate, gamepadstate);
		if(!pressed){
			dir.held    = false;
			dir.nextfire = 0;
			return;
		}
		if(!dir.held){
			// First frame held — fire immediately.
			dir.held     = true;
			dir.nextfire = now + GAMEPAD_NAV_DELAY_MS;
			clientUiInput.QueueNavAction(navAction);
		} else if(now >= dir.nextfire){
			// Repeat.
			dir.nextfire = now + GAMEPAD_NAV_REPEAT_MS;
			clientUiInput.QueueNavAction(navAction);
		}
	};

	tick(gamepadNavUp,    Action::UiUp,    silencer::ui::UiNavAction::Up);
	tick(gamepadNavDown,  Action::UiDown,  silencer::ui::UiNavAction::Down);
	tick(gamepadNavLeft,  Action::UiLeft,  silencer::ui::UiNavAction::Left);
	tick(gamepadNavRight, Action::UiRight, silencer::ui::UiNavAction::Right);

	// Confirm (A/Cross) is edge-triggered; directional nav handles repeat.
	{
		bool confirmNow = keymap.IsPressed(Action::UiConfirm, keystate, gamepadstate);
		static bool confirmPrev = false;
		if(confirmNow && !confirmPrev){
			clientUiInput.QueueNavAction(silencer::ui::UiNavAction::Confirm);
		}
		confirmPrev = confirmNow;
	}

	{
		bool cancelNow = keymap.IsPressed(Action::UiCancel, keystate, gamepadstate);
		static bool cancelPrev = false;
		if(cancelNow && !cancelPrev){
			clientUiInput.QueueNavAction(silencer::ui::UiNavAction::Cancel);
		}
		cancelPrev = cancelNow;
	}
}

const char * Game::GetActionKeyDisplayName(Action a){
	static thread_local char buf[32];
	const auto& ab = keymap.Get(a);
	// Prefer keyboard binding; fall back to any other device (gamepad/mouse).
	const BindingKey* fallback = nullptr;
	for(const auto& b : ab.bindings){
		if(b.keys.empty()) continue;
		const auto& k = b.keys[0];
		if(k.device == BindingDevice::Keyboard){
			return KeyMap::GetKeyName((SDL_Scancode)k.code);
		}
		if(!fallback) fallback = &k;
	}
	if(fallback){
		std::string s = Stringify(*fallback);
		auto colon = s.find(':');
		std::string raw = (colon != std::string::npos) ? s.substr(colon + 1) : s;
		std::string label = GamepadShortLabel(raw, gamepad ? SDL_GetGamepadType(gamepad) : SDL_GAMEPAD_TYPE_UNKNOWN);
		std::strncpy(buf, label.c_str(), sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';
		return buf;
	}
	std::strncpy(buf, "(unbound)", sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';
	return buf;
}
