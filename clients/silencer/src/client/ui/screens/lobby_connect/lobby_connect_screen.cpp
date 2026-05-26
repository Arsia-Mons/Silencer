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
#include "primitives/text.h"
#include "primitives/button.h"
#include "primitives/scroll_text_box.h"
#include "primitives/text_input.h"

#include <algorithm>
#include <cstring>
#include <cstdint>
#include <string>

namespace lobby_connect_screen_detail
{
using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextEffect;
using silencer::ui::primitives::TextSize;
using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;
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

constexpr uint16_t kPanelW = 284;
constexpr uint16_t kPanelH = 277;
constexpr uint16_t kLogW = 250;
constexpr uint16_t kLogH = 170;
constexpr uint16_t kLogX = 7;
constexpr uint16_t kLogY = 8;
constexpr uint16_t kFormRowX = 4;
constexpr uint16_t kFormRowY = 195;
constexpr uint16_t kFormRowH = 21;
constexpr uint16_t kFormRowGap = 6;
constexpr uint16_t kLabelW = 86;
constexpr uint16_t kInputW = 183;
constexpr uint16_t kInputInsetX = 7;
constexpr uint16_t kButtonRowX = 86;
constexpr uint16_t kButtonRowY = 246;
constexpr uint16_t kButtonGap = 5;
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

template <typename Text>
void CopyUiText(char * dst, int dstLen, const Text & value)
{
	if(!dst || dstLen <= 0) return;
	int n = static_cast<int>(value.size());
	if(n > dstLen - 1) n = dstLen - 1;
	std::memcpy(dst, value.data(), n);
	dst[n] = '\0';
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
		g_logSlab[count].effect = TextEffect::Default();
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
	focusUsernameRequested = true;
	logLines.clear();
	username[0] = '\0';
	password[0] = '\0';
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
			if(!world.lobby.charactersreceived){
				break;
			}
			AppendLog("Authenticated");
			{
				const bool needsCharacter = world.lobby.characters.empty();
				world.lobby.UnlockMutex();
				ctx.GoToState(needsCharacter ? GameState::CREATECHARACTER : GameState::LOBBY);
				return;
			}
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
	if(focusUsernameRequested){
		interactions.RequestTextInputFocusByUid(lobby_connect_screen_detail::LBY_INPUT_USERNAME);
		focusUsernameRequested = false;
	}
	const bool usernameFocused =
		interactions.IsTextInputFocused(lobby_connect_screen_detail::LBY_INPUT_USERNAME);
	const bool passwordFocused =
		interactions.IsTextInputFocused(lobby_connect_screen_detail::LBY_INPUT_PASSWORD);
	const bool blink = (ctx.renderer.GetHudAnimationPhase() % 32) < 16;

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
		           .childGap = 0,
		           .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .image = { .imageData = PackImage(7, 2) } }) {
			CLAY({ .id = CLAY_ID("LobbyConnectLogSlot"),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0),
			                       CLAY_SIZING_FIXED(lobby_connect_screen_detail::kLogY) },
			       } }) {}

			CLAY({ .id = CLAY_ID("LobbyConnectLogRow"),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0),
			                       CLAY_SIZING_FIXED(lobby_connect_screen_detail::kLogH) },
			           .padding = { lobby_connect_screen_detail::kLogX, 0, 0, 0 },
			           .childGap = 0,
			           .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       } }) {
				lobby_connect_screen_detail::ScrollTextBox(CLAY_STRING("LobbyConnectLog"),
				              lobby_connect_screen_detail::g_logSlab,
				              lineCount,
				              scroll,
				              { .width = lobby_connect_screen_detail::kLogW,
				                .height = lobby_connect_screen_detail::kLogH,
				                .lineHeight = 11,
				                .text = { .size = lobby_connect_screen_detail::TextSize::Body },
				                .origin = lobby_connect_screen_detail::ScrollTextBoxOrigin::TopDown });
			}

			CLAY({ .id = CLAY_ID("LobbyConnectPreFormGap"),
			       .layout = {
			           .sizing = {
			               CLAY_SIZING_GROW(0),
			               CLAY_SIZING_FIXED(lobby_connect_screen_detail::kFormRowY -
			                                  lobby_connect_screen_detail::kLogY -
			                                  lobby_connect_screen_detail::kLogH) },
			       } }) {
			}

			CLAY({ .id = CLAY_ID("LobbyConnectForm"),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0),
			                       CLAY_SIZING_FIXED((lobby_connect_screen_detail::kFormRowH * 2) +
			                                         lobby_connect_screen_detail::kFormRowGap) },
			           .childGap = lobby_connect_screen_detail::kFormRowGap,
			           .layoutDirection = CLAY_TOP_TO_BOTTOM,
			       } }) {
				CLAY({ .id = CLAY_ID("LobbyConnectUsernameRow"),
				       .layout = {
				           .sizing = { CLAY_SIZING_GROW(0),
				                       CLAY_SIZING_FIXED(lobby_connect_screen_detail::kFormRowH) },
				           .padding = { lobby_connect_screen_detail::kFormRowX, 0, 0, 0 },
				           .childGap = 0,
				           .childAlignment = { CLAY_ALIGN_X_LEFT,
				                               CLAY_ALIGN_Y_CENTER },
				           .layoutDirection = CLAY_LEFT_TO_RIGHT,
				       } }) {
					CLAY({ .id = CLAY_ID("LobbyConnectUsernameLabel"),
					       .layout = {
					           .sizing = { CLAY_SIZING_FIXED(lobby_connect_screen_detail::kLabelW),
					                       CLAY_SIZING_FIXED(lobby_connect_screen_detail::kFormRowH) },
					           .childGap = 0,
					           .childAlignment = { CLAY_ALIGN_X_CENTER,
					                               CLAY_ALIGN_Y_CENTER },
					           .layoutDirection = CLAY_LEFT_TO_RIGHT,
					       } }) {
						lobby_connect_screen_detail::Text(
							CLAY_STRING("Username"),
							{ .size = lobby_connect_screen_detail::TextSize::Heading });
					}
					silencer::ui::primitives::TextInput(
						CLAY_STRING("LobbyConnectUsernameInput"),
						username,
						{ .widthPx = lobby_connect_screen_detail::kInputW,
						  .heightPx = lobby_connect_screen_detail::kFormRowH,
						  .textSize = lobby_connect_screen_detail::TextSize::Body,
						  .inactive = inactive,
						  .showCaret = usernameFocused && blink,
						  .contentInsetX = lobby_connect_screen_detail::kInputInsetX },
						{ nullptr, lobby_connect_screen_detail::kActionUsername,
						  "Username", &interactions,
						  lobby_connect_screen_detail::LBY_INPUT_USERNAME, 16 });
				}

				CLAY({ .id = CLAY_ID("LobbyConnectPasswordRow"),
				       .layout = {
				           .sizing = { CLAY_SIZING_GROW(0),
				                       CLAY_SIZING_FIXED(lobby_connect_screen_detail::kFormRowH) },
				           .padding = { lobby_connect_screen_detail::kFormRowX, 0, 0, 0 },
				           .childGap = 0,
				           .childAlignment = { CLAY_ALIGN_X_LEFT,
				                               CLAY_ALIGN_Y_CENTER },
				           .layoutDirection = CLAY_LEFT_TO_RIGHT,
				       } }) {
					CLAY({ .id = CLAY_ID("LobbyConnectPasswordLabel"),
					       .layout = {
					           .sizing = { CLAY_SIZING_FIXED(lobby_connect_screen_detail::kLabelW),
					                       CLAY_SIZING_FIXED(lobby_connect_screen_detail::kFormRowH) },
					           .childGap = 0,
					           .childAlignment = { CLAY_ALIGN_X_CENTER,
					                               CLAY_ALIGN_Y_CENTER },
					           .layoutDirection = CLAY_LEFT_TO_RIGHT,
					       } }) {
						lobby_connect_screen_detail::Text(
							CLAY_STRING("Password"),
							{ .size = lobby_connect_screen_detail::TextSize::Heading });
					}
					silencer::ui::primitives::TextInput(
						CLAY_STRING("LobbyConnectPasswordInput"),
						password,
						{ .widthPx = lobby_connect_screen_detail::kInputW,
						  .heightPx = lobby_connect_screen_detail::kFormRowH,
						  .textSize = lobby_connect_screen_detail::TextSize::Body,
						  .password = true,
						  .inactive = inactive,
						  .showCaret = passwordFocused && blink,
						  .contentInsetX = lobby_connect_screen_detail::kInputInsetX },
						{ nullptr, lobby_connect_screen_detail::kActionPassword,
						  "Password", &interactions,
						  lobby_connect_screen_detail::LBY_INPUT_PASSWORD, 28 });
				}
			}

			CLAY({ .id = CLAY_ID("LobbyConnectPreButtonGap"),
			       .layout = {
			           .sizing = {
			               CLAY_SIZING_GROW(0),
			               CLAY_SIZING_FIXED(lobby_connect_screen_detail::kButtonRowY -
			                                  lobby_connect_screen_detail::kFormRowY -
			                                  (lobby_connect_screen_detail::kFormRowH * 2) -
			                                  lobby_connect_screen_detail::kFormRowGap) },
			       } }) {
			}

			CLAY({ .id = CLAY_ID("LobbyConnectButtons"),
			       .layout = {
			           .sizing = { CLAY_SIZING_GROW(0),
			                       CLAY_SIZING_FIXED(lobby_connect_screen_detail::kButtonH) },
			           .padding = { lobby_connect_screen_detail::kButtonRowX, 0, 0, 0 },
			           .childGap = lobby_connect_screen_detail::kButtonGap,
			           .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       } }) {
				lobby_connect_screen_detail::Button(CLAY_STRING("LobbyConnectLoginButton"), CLAY_STRING("Login"),
					lobby_connect_screen_detail::ButtonOpts{ .variant = lobby_connect_screen_detail::ButtonVariant::Text,
					                                         .size = lobby_connect_screen_detail::ButtonSize::Compact },
					lobby_connect_screen_detail::ButtonHandle{ nullptr, lobby_connect_screen_detail::kActionLogin, &interactions });
				lobby_connect_screen_detail::Button(CLAY_STRING("LobbyConnectCancelButton"), CLAY_STRING("Cancel"),
					lobby_connect_screen_detail::ButtonOpts{ .variant = lobby_connect_screen_detail::ButtonVariant::Text,
					                                         .size = lobby_connect_screen_detail::ButtonSize::Compact },
					lobby_connect_screen_detail::ButtonHandle{ nullptr, lobby_connect_screen_detail::kActionCancel, &interactions });
			}
		}
	}

	lobby_connect_screen_detail::RegisterWidgets(this, username, password, inactive, interactions);
}

void LobbyConnectScreen::Destroy(ScreenContext & ctx)
{
	ctx.ClearUiFocus();
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
