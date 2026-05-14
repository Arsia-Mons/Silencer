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
#include "runtime/UiAutomationRegistry.h"
#include "primitives/bank_text.h"
#include "primitives/scroll_text_box.h"
#include "primitives/text_input.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstring>
#include <cstdint>
#include <string>

namespace
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
enum LobbyConnectButton : Uint8 {
	LBY_BTN_LOGIN  = 0,
	LBY_BTN_CANCEL = 1,
};
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

void SmallButton(Clay_String label, const char * actionId)
{
	silencer::ui::automation::Widget widget;
	widget.id = actionId;
	widget.labelText = ToStd(label);
	widget.kind = silencer::ui::automation::WidgetKind::Button;
	widget.clayId = CLAY_SID(label);
	widget.hasClayId = true;
	silencer::ui::automation::Register(widget);
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

void RegisterButton(const char * label,
                    const char * actionId,
                    int uid,
                    int x,
                    int y)
{
	silencer::ui::automation::Widget w;
	w.id = actionId;
	w.labelText = label;
	w.kind = silencer::ui::automation::WidgetKind::Button;
	w.uid = uid;
	w.x = x; w.y = y; w.w = kButtonW; w.h = kButtonH;
	silencer::ui::automation::Register(w);
}

void RegisterInput(const char * label,
                   const char * actionId,
                   int uid,
                   int x,
                   int y,
                   char * buffer,
                   int bufferLen,
                   bool password,
                   bool inactive)
{
	silencer::ui::automation::Widget w;
	w.id = actionId;
	w.labelText = label;
	w.kind = silencer::ui::automation::WidgetKind::TextInput;
	w.uid = uid;
	w.x = x; w.y = y; w.w = kInputW; w.h = kInputH;
	w.value = buffer ? buffer : "";
	w.maxLength = bufferLen > 0 ? bufferLen - 1 : 0;
	w.isPassword = password;
	w.inactive = inactive;
	silencer::ui::automation::Register(w);
}

void RegisterWidgets(LobbyConnectScreen * screen,
                     char * username,
                     char * password,
                     int surfaceW,
                     int surfaceH,
                     bool inactive)
{
	const int panelX = (surfaceW - kPanelW) / 2;
	const int panelY = (surfaceH - kPanelH) / 2;
	const int formX = panelX + kPanelPadX;
	const int formY = panelY + kPanelPadY + kLogH + 14;
	const int inputX = formX + 85;
	const int usernameY = formY + 3;
	const int passwordY = formY + 20 + kFormGap + 3;
	const int buttonsX = panelX + (kPanelW - (kButtonW * 2 + kButtonGap)) / 2;
	const int buttonsY = formY + 20 + kFormGap + 20 + 14;

	RegisterInput("Username", kActionUsername, LBY_INPUT_USERNAME, inputX, usernameY,
	              username, 17, false, inactive);
	RegisterInput("Password", kActionPassword, LBY_INPUT_PASSWORD, inputX, passwordY,
	              password, 29, true, inactive);
	RegisterButton("Login", kActionLogin, LBY_BTN_LOGIN, buttonsX, buttonsY);
	RegisterButton("Cancel", kActionCancel, LBY_BTN_CANCEL, buttonsX + kButtonW + kButtonGap,
	               buttonsY);
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
}

void LobbyConnectScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(2);
	motdprinted = false;
	loginClicked = false;
	cancelClicked = false;
	logLines.clear();
	username[0] = '\0';
	password[0] = '\0';

	const Surface& surface = ctx.game.GetScreenBuffer();
	RegisterWidgets(this, username, password, surface.w, surface.h, false);
	silencer::ui::automation::FocusTextInputByUid(LBY_INPUT_USERNAME);
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

void LobbyConnectScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime)
{
	(void)frametime;
	using namespace silencer::clay_bridge;

	int lineCount = FillLogSlab(logLines);
	Uint16 scroll = 0;
	const int visibleLines = kLogH / 11;
	if(lineCount > visibleLines){
		scroll = static_cast<Uint16>(lineCount - visibleLines);
	}
	bool inactive = ctx.world.lobby.state == Lobby::AUTHSENT;
	const bool usernameFocused =
		silencer::ui::automation::IsTextInputFocused(LBY_INPUT_USERNAME);
	const bool passwordFocused =
		silencer::ui::automation::IsTextInputFocused(LBY_INPUT_PASSWORD);
	const bool blink = ((SDL_GetTicks() / 250) % 2) == 0;

	CLAY({ .id = CLAY_ID("LobbyConnectRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED((float)dst.w),
	                       CLAY_SIZING_FIXED((float)dst.h) },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
	       } }) {
		CLAY({ .id = CLAY_ID("LobbyConnectPanel"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(kPanelW),
		                       CLAY_SIZING_FIXED(kPanelH) },
		           .padding = { kPanelPadX, kPanelPadX,
		                        kPanelPadY, kPanelPadY },
		           .childGap = 14,
		           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .image = { .imageData = PackImage(7, 2) } }) {
			ScrollTextBox(CLAY_STRING("LobbyConnectLog"),
			              g_logSlab,
			              lineCount,
			              scroll,
			              { .width = kLogW,
			                .height = kLogH,
			                .lineHeight = 11,
			                .textVariant = BankTextVariant::Body,
			                .origin = ScrollTextBoxOrigin::TopDown });

			CLAY({ .id = CLAY_ID("LobbyConnectForm"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIXED(270), CLAY_SIZING_FIT(0) },
			           .childGap = kFormGap,
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
						BankText(CLAY_STRING("Username"),
						         BankTextVariant::Heading, {});
					}
					silencer::ui::primitives::TextInput(
						CLAY_STRING("LobbyConnectUsernameInput"),
						username,
						{ .widthPx = kInputW,
						  .heightPx = kInputH,
						  .fontBank = 133,
						  .fontWidth = 6,
						  .inactive = inactive,
						  .showCaret = usernameFocused && blink });
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
						BankText(CLAY_STRING("Password"),
						         BankTextVariant::Heading, {});
					}
					silencer::ui::primitives::TextInput(
						CLAY_STRING("LobbyConnectPasswordInput"),
						password,
						{ .widthPx = kInputW,
						  .heightPx = kInputH,
						  .fontBank = 133,
						  .fontWidth = 6,
						  .password = true,
						  .inactive = inactive,
						  .showCaret = passwordFocused && blink });
				}
			}

			CLAY({ .id = CLAY_ID("LobbyConnectButtons"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(kButtonH) },
			           .childGap = kButtonGap,
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       } }) {
				SmallButton(CLAY_STRING("Login"), kActionLogin);
				SmallButton(CLAY_STRING("Cancel"), kActionCancel);
			}
		}
	}

	RegisterWidgets(this, username, password, dst.w, dst.h, inactive);
}

void LobbyConnectScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
	silencer::ui::automation::ClearFocus();
}

bool LobbyConnectScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	(void)ctx;
	if(action.kind == silencer::ui::UiActionKind::SetText){
		if(action.id == kActionUsername){
			CopyUiText(username, static_cast<int>(sizeof(username)), action.value);
			return true;
		}
		if(action.id == kActionPassword){
			CopyUiText(password, static_cast<int>(sizeof(password)), action.value);
			return true;
		}
		return false;
	}
	if(action.kind == silencer::ui::UiActionKind::SubmitText &&
	   (action.id == kActionUsername || action.id == kActionPassword)){
		if(action.id == kActionUsername){
			CopyUiText(username, static_cast<int>(sizeof(username)), action.value);
		}else{
			CopyUiText(password, static_cast<int>(sizeof(password)), action.value);
		}
		loginClicked = true;
		return true;
	}
	if(action.kind == silencer::ui::UiActionKind::Activate && action.id == kActionLogin){
		loginClicked = true;
		return true;
	}
	if((action.kind == silencer::ui::UiActionKind::Activate && action.id == kActionCancel) ||
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
