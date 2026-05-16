#include "lobby_connect_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "game.h"
#include "renderer.h"
#include "surface.h"
#include "lobby.h"
#include "updater.h"
#include "ambience_mixer.h"
#include "config.h"
#include "world.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/bank_text.h"
#include "primitives/scroll_text_box.h"
#include "primitives/text_input.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstring>
#include <cstdint>
#include <string>

namespace lobby_connect_screen_detail
{
using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextVariant;
using silencer::ui::primitives::ScrollTextBox;
using silencer::ui::primitives::ScrollTextBoxLine;
using silencer::ui::primitives::ScrollTextBoxOpts;
using silencer::ui::primitives::ScrollTextBoxOrigin;

// Prefixed to dodge anonymous-namespace collisions when SILENCER_UNITY_BUILD
// merges this TU with sibling screen .cpp files (other screens already use
// their own LBY_*-style prefixes).
enum LobbyConnectInput : Uint8 {
	LBY_INPUT_USERNAME = 1,
	LBY_INPUT_PASSWORD = 2,
};

constexpr uint16_t kPanelW = 280;
constexpr uint16_t kPanelH = 270;
constexpr uint16_t kPanelPadX = 5;
constexpr uint16_t kPanelPadY = 5;
constexpr uint16_t kLogW = 250;
constexpr uint16_t kLogH = 170;
constexpr uint16_t kInputW = 180;
constexpr uint16_t kInputH = 14;
constexpr uint16_t kFormGap = 7;
constexpr uint16_t kButtonGap = 5;
constexpr uint16_t kButtonW = 52;
constexpr uint16_t kButtonH = 21;
constexpr int kMaxLogLines = 128;
constexpr const char * kActionUsername = "lobby_connect.username";
constexpr const char * kActionPassword = "lobby_connect.password";
constexpr const char * kActionLogin = "lobby_connect.login";
constexpr const char * kActionCancel = "lobby_connect.cancel";
ScrollTextBoxLine g_logSlab[kMaxLogLines];

Clay_String FromCStr(const char * s)
{
	return Clay_String{ false, static_cast<int32_t>(std::strlen(s)), s };
}

Clay_String FromStd(const std::string & s)
{
	return Clay_String{ false, static_cast<int32_t>(s.size()), s.c_str() };
}

std::string ToStd(Clay_String text)
{
	return std::string(text.chars ? text.chars : "", static_cast<size_t>(text.length));
}

void CopyUiText(char * dst, int dstLen, const std::string & value)
{
	if(!dst || dstLen <= 0) return;
	int n = static_cast<int>(value.size());
	if(n > dstLen - 1) n = dstLen - 1;
	std::memcpy(dst, value.data(), n);
	dst[n] = '\0';
}

void SmallButton(Clay_String label,
                 const char * actionId,
                 silencer::ui::UiInteractionRegistry& interactions)
{
	silencer::ui::UiInteractable widget;
	widget.id = actionId;
	widget.labelText = ToStd(label);
	widget.kind = silencer::ui::UiInteractableKind::Button;
	widget.clayId = CLAY_SID(label);
	widget.hasClayId = true;
	interactions.RegisterInteractable(widget);
	CLAY({ .id = CLAY_SID(label),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(kButtonW),
	                       CLAY_SIZING_FIXED(kButtonH) },
	           .padding = { 0, 0, 8, 0 },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
	       } }) {
		bool hovered = ::Clay_Hovered();
		BankText(label, BankTextVariant::BodySm,
		         { .brightness = hovered ? static_cast<Uint8>(136)
		                                  : static_cast<Uint8>(128) });
	}
}

void RegisterInput(const char * label,
                   const char * actionId,
                   int uid,
                   char * buffer,
                   int bufferLen,
                   bool password,
                   bool inactive,
                   silencer::ui::UiInteractionRegistry& interactions)
{
	silencer::ui::UiInteractable w;
	w.id = actionId;
	w.labelText = label;
	w.kind = silencer::ui::UiInteractableKind::TextInput;
	w.uid = uid;
	w.value = buffer ? buffer : "";
	w.maxLength = bufferLen > 0 ? bufferLen - 1 : 0;
	w.isPassword = password;
	w.inactive = inactive;
	interactions.RegisterInteractable(w);
}

void RegisterWidgets(LobbyConnectScreen * screen,
                     char * username,
                     char * password,
                     bool inactive,
                     silencer::ui::UiInteractionRegistry& interactions)
{
	(void)screen;
	RegisterInput("Username", kActionUsername, LBY_INPUT_USERNAME,
	              username, 17, false, inactive, interactions);
	RegisterInput("Password", kActionPassword, LBY_INPUT_PASSWORD,
	              password, 29, true, inactive, interactions);
}

int FillLogSlab(const std::vector<std::string> & lines)
{
	int start = 0;
	if(static_cast<int>(lines.size()) > kMaxLogLines){
		start = static_cast<int>(lines.size()) - kMaxLogLines;
	}
	int count = 0;
	for(int i = start; i < static_cast<int>(lines.size()); i++){
		g_logSlab[count].text = FromStd(lines[i]);
		g_logSlab[count].effectColor = 0;
		g_logSlab[count].brightness = 128;
		g_logSlab[count].indent = 0;
		count++;
	}
	return count;
}
} // namespace lobby_connect_screen_detail

void LobbyConnectScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(2);
	motdprinted = false;
	loginClicked = false;
	cancelClicked = false;
	logLines.clear();
	username[0] = '\0';
	password[0] = '\0';

	lobby_connect_screen_detail::RegisterWidgets(this, username, password, false,
	                ctx.game.UiInteractions());
	ctx.game.UiInteractions().FocusTextInputByUid(lobby_connect_screen_detail::LBY_INPUT_USERNAME);
}

void LobbyConnectScreen::Tick(ScreenContext & ctx)
{
	// Mirror the legacy LOBBYCONNECT case body's gate: nothing happens until
	// the menu music has crossfaded in. The lobby state machine starts in
	// WAITING and only kicks off the TCP connect on the first tick after
	// the gate opens, so this delay shapes when the user sees "Connecting
	// to ..." appear in the textbox.
	if(!ctx.ambienceMixer.FadedIn()) return;

	World & world = ctx.world;
	world.lobby.LockMutex();
	switch(world.lobby.state){
		case Lobby::CONNECTING:

		break;
		case Lobby::WAITINGFORRESOLVER:

		break;
		case Lobby::AUTHSENT:

		break;
		case Lobby::IDLE:

		break;
		case Lobby::WAITING:{
			char line[128];
			snprintf(line, sizeof(line), "Connecting to %s:%d",
			         Config::GetInstance().lobbyhost,
			         Config::GetInstance().lobbyport);
			AppendLog(line);
			world.lobby.Connect(Config::GetInstance().lobbyhost,
			                    Config::GetInstance().lobbyport);
			//world.lobby.state = Lobby::AUTHENTICATED;
		}break;
		case Lobby::RESOLVING:
			AppendLog("Resolving hostname...");
			world.lobby.state = Lobby::WAITINGFORRESOLVER;
		break;
		case Lobby::RESOLVEFAILED:
			AppendLog("Could not resolve hostname");
			//world.lobby.Disconnect();
			world.lobby.state = Lobby::IDLE;
		break;
		case Lobby::RESOLVED:
			AppendLog("Hostname resolved");
			world.lobby.Connect(Config::GetInstance().lobbyhost,
			                    Config::GetInstance().lobbyport);
		break;
		case Lobby::CONNECTED:
			AppendLog("Connected");
			AppendLog("Checking version...");
			world.lobby.SendVersion();
			world.lobby.state = Lobby::CHECKINGVERSION;
		break;
		case Lobby::CHECKINGVERSION:
			if(world.lobby.versionchecked){
				if(world.lobby.versionok){
					AppendLog("Software version is current");
					world.lobby.state = Lobby::AUTHENTICATING;
				}else{
					if(world.lobby.updateavailable){
						// Route into the auto-updater flow.
						ctx.updater.PresentUpdate(world.lobby.updateurl,
						                          world.lobby.updatesha256);
						world.lobby.Disconnect();
						world.lobby.state = Lobby::IDLE;
						world.lobby.UnlockMutex();
						ctx.GoToState(GameState::UPDATING);
						return;
					}else{
						AppendLog("Software is out of date");
						AppendLog("Get latest version at:");
						AppendLog("https://github.com/Arsia-Mons/Silencer");
						world.lobby.Disconnect();
						world.lobby.state = Lobby::IDLE;
					}
				}
			}
		break;
		case Lobby::AUTHENTICATING:
			//world.lobby.state = Lobby::AUTHENTICATED;
		break;
		case Lobby::AUTHFAILED:
			AppendLog("Authentication failed");
			if(strlen(world.lobby.failmessage) > 0){
				AppendLog(world.lobby.failmessage);
			}
			world.lobby.state = Lobby::AUTHENTICATING;
			//world.lobby.Disconnect();
		break;
		case Lobby::AUTHENTICATED:
			AppendLog("Authenticated");
			world.lobby.UnlockMutex();
			ctx.GoToState(GameState::LOBBY);
			return;
		case Lobby::CONNECTIONFAILED:
			AppendLog("Connection failed");
			world.lobby.state = Lobby::IDLE;
		break;
		case Lobby::DISCONNECTED:
			AppendLog("Disconnected");
			world.lobby.state = Lobby::IDLE;
		break;
	}
	if(world.lobby.motdreceived && !motdprinted){
		char * line = strtok(world.lobby.motd, "\n");
		while(line != 0){
			AppendLog(line);
			line = strtok(NULL, "\n");
		}
		motdprinted = true;
	}
	if(loginClicked){
		loginClicked = false;
		if(world.lobby.state == Lobby::AUTHENTICATING){
			world.lobby.SetLocalUsername(username);
			world.lobby.SendCredentials(username, password);
			world.lobby.state = Lobby::AUTHSENT;
		}
	}
	world.lobby.UnlockMutex();

	if(cancelClicked){
		cancelClicked = false;
		ctx.GoToState(GameState::MAINMENU);
	}
}

void LobbyConnectScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;
	using namespace silencer::clay_bridge;

	int lineCount = lobby_connect_screen_detail::FillLogSlab(logLines);
	Uint16 scroll = 0;
	const int visibleLines = lobby_connect_screen_detail::kLogH / 11;
	if(lineCount > visibleLines){
		scroll = static_cast<Uint16>(lineCount - visibleLines);
	}
	bool inactive = ctx.world.lobby.state == Lobby::AUTHSENT;
	const bool usernameFocused =
		interactions.IsTextInputFocused(lobby_connect_screen_detail::LBY_INPUT_USERNAME);
	const bool passwordFocused =
		interactions.IsTextInputFocused(lobby_connect_screen_detail::LBY_INPUT_PASSWORD);
	const bool blink = ((SDL_GetTicks() / 250) % 2) == 0;

	CLAY({ .id = CLAY_ID("LobbyConnectRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_GROW(0) },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
	       } }) {
		CLAY({ .id = CLAY_ID("LobbyConnectPanel"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(lobby_connect_screen_detail::kPanelW),
		                       CLAY_SIZING_FIXED(lobby_connect_screen_detail::kPanelH) },
		           .padding = { lobby_connect_screen_detail::kPanelPadX, lobby_connect_screen_detail::kPanelPadX,
		                        lobby_connect_screen_detail::kPanelPadY, lobby_connect_screen_detail::kPanelPadY },
		           .childGap = 14,
		           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .image = { .imageData = PackImage(7, 2) } }) {
			lobby_connect_screen_detail::ScrollTextBox(CLAY_STRING("LobbyConnectLog"),
			              lobby_connect_screen_detail::g_logSlab,
			              lineCount,
			              scroll,
			              { .width = lobby_connect_screen_detail::kLogW,
			                .height = lobby_connect_screen_detail::kLogH,
			                .lineHeight = 11,
			                .textVariant = lobby_connect_screen_detail::BankTextVariant::Body,
			                .origin = lobby_connect_screen_detail::ScrollTextBoxOrigin::TopDown });

			CLAY({ .id = CLAY_ID("LobbyConnectForm"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIXED(270), CLAY_SIZING_FIT(0) },
			           .childGap = lobby_connect_screen_detail::kFormGap,
			           .layoutDirection = CLAY_TOP_TO_BOTTOM,
			       } }) {
				CLAY({ .id = CLAY_ID("LobbyConnectUsernameRow"),
				       .layout = {
				           .sizing = { CLAY_SIZING_GROW(0),
				                       CLAY_SIZING_FIXED(20) },
				           .childGap = 12,
				           .childAlignment = { CLAY_ALIGN_X_LEFT,
				                               CLAY_ALIGN_Y_CENTER },
				           .layoutDirection = CLAY_LEFT_TO_RIGHT,
				       } }) {
					CLAY({ .id = CLAY_ID("LobbyConnectUsernameLabel"),
					       .layout = {
					           .sizing = { CLAY_SIZING_GROW(0),
					                       CLAY_SIZING_FIT(0) },
					       } }) {
						lobby_connect_screen_detail::BankText(CLAY_STRING("Username"),
						         lobby_connect_screen_detail::BankTextVariant::Heading, {});
					}
					silencer::ui::primitives::TextInput(
						CLAY_STRING("LobbyConnectUsernameInput"),
						username,
						{ .widthPx = lobby_connect_screen_detail::kInputW,
						  .heightPx = lobby_connect_screen_detail::kInputH,
						  .fontBank = 133,
						  .fontWidth = 6,
						  .inactive = inactive,
						  .showCaret = usernameFocused && blink },
						{ nullptr, lobby_connect_screen_detail::kActionUsername,
						  "Username", &interactions,
						  lobby_connect_screen_detail::LBY_INPUT_USERNAME, 16 });
				}

				CLAY({ .id = CLAY_ID("LobbyConnectPasswordRow"),
				       .layout = {
				           .sizing = { CLAY_SIZING_GROW(0),
				                       CLAY_SIZING_FIXED(20) },
				           .childGap = 12,
				           .childAlignment = { CLAY_ALIGN_X_LEFT,
				                               CLAY_ALIGN_Y_CENTER },
				           .layoutDirection = CLAY_LEFT_TO_RIGHT,
				       } }) {
					CLAY({ .id = CLAY_ID("LobbyConnectPasswordLabel"),
					       .layout = {
					           .sizing = { CLAY_SIZING_GROW(0),
					                       CLAY_SIZING_FIT(0) },
					       } }) {
						lobby_connect_screen_detail::BankText(CLAY_STRING("Password"),
						         lobby_connect_screen_detail::BankTextVariant::Heading, {});
					}
					silencer::ui::primitives::TextInput(
						CLAY_STRING("LobbyConnectPasswordInput"),
						password,
						{ .widthPx = lobby_connect_screen_detail::kInputW,
						  .heightPx = lobby_connect_screen_detail::kInputH,
						  .fontBank = 133,
						  .fontWidth = 6,
						  .password = true,
						  .inactive = inactive,
						  .showCaret = passwordFocused && blink },
						{ nullptr, lobby_connect_screen_detail::kActionPassword,
						  "Password", &interactions,
						  lobby_connect_screen_detail::LBY_INPUT_PASSWORD, 28 });
				}
			}

			CLAY({ .id = CLAY_ID("LobbyConnectButtons"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(lobby_connect_screen_detail::kButtonH) },
			           .childGap = lobby_connect_screen_detail::kButtonGap,
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       } }) {
				lobby_connect_screen_detail::SmallButton(CLAY_STRING("Login"), lobby_connect_screen_detail::kActionLogin, interactions);
				lobby_connect_screen_detail::SmallButton(CLAY_STRING("Cancel"), lobby_connect_screen_detail::kActionCancel, interactions);
			}
		}
	}

	lobby_connect_screen_detail::RegisterWidgets(this, username, password, inactive, interactions);
}

void LobbyConnectScreen::Destroy(ScreenContext & ctx)
{
	ctx.game.UiInteractions().ClearFocus();
}

bool LobbyConnectScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	(void)ctx;
	if(action.kind == silencer::ui::UiActionKind::SetText){
		if(action.id == lobby_connect_screen_detail::kActionUsername){
			lobby_connect_screen_detail::CopyUiText(username, static_cast<int>(sizeof(username)), action.value);
			return true;
		}
		if(action.id == lobby_connect_screen_detail::kActionPassword){
			lobby_connect_screen_detail::CopyUiText(password, static_cast<int>(sizeof(password)), action.value);
			return true;
		}
		return false;
	}
	if(action.kind == silencer::ui::UiActionKind::SubmitText &&
	   (action.id == lobby_connect_screen_detail::kActionUsername || action.id == lobby_connect_screen_detail::kActionPassword)){
		if(action.id == lobby_connect_screen_detail::kActionUsername){
			lobby_connect_screen_detail::CopyUiText(username, static_cast<int>(sizeof(username)), action.value);
		}else{
			lobby_connect_screen_detail::CopyUiText(password, static_cast<int>(sizeof(password)), action.value);
		}
		loginClicked = true;
		return true;
	}
	if(action.kind == silencer::ui::UiActionKind::Activate && action.id == lobby_connect_screen_detail::kActionLogin){
		loginClicked = true;
		return true;
	}
	if((action.kind == silencer::ui::UiActionKind::Activate && action.id == lobby_connect_screen_detail::kActionCancel) ||
	   action.kind == silencer::ui::UiActionKind::Cancel){
		cancelClicked = true;
		return true;
	}
	return false;
}

void LobbyConnectScreen::AppendLog(const char * text)
{
	if(!text) return;
	logLines.push_back(text);
	if(logLines.size() > 256){
		logLines.erase(logLines.begin(),
		               logLines.begin() + (logLines.size() - 256));
	}
}
