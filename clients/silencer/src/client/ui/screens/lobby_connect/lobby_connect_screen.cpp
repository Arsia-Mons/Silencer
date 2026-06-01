#include "lobby_connect_screen.h"

#include "client/ui/screens/lobby_connect/lobby_connect_frame.h"
#include "client/ui/hooks/use_lobby.h"
#include "client/ui/hooks/use_navigation.h"
#include "character_create_screen.h"
#include "lobby_screen.h"
#include "main_menu_screen.h"
#include "update_screen.h"
#include "screen_context.h"
#include "renderer.h"
#include "surface.h"

#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>

namespace lobby_connect_screen_detail
{
// Prefixed to dodge anonymous-namespace collisions when SILENCER_UNITY_BUILD
// merges this TU with sibling screen .cpp files (other screens already use
// their own LBY_*-style prefixes).
enum LobbyConnectInput : Uint8 {
	LBY_INPUT_USERNAME = 1,
	LBY_INPUT_PASSWORD = 2,
};

constexpr int kVisibleLogLines = 15;
constexpr const char * kActionUsername = "lobby_connect.username";
constexpr const char * kActionPassword = "lobby_connect.password";

void CopyUiText(char * dst, int dstLen, const char * value)
{
	if(!dst || dstLen <= 0) return;
	const char * src = value ? value : "";
	int n = static_cast<int>(std::strlen(src));
	if(n > dstLen - 1) n = dstLen - 1;
	std::memcpy(dst, src, n);
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
} // namespace lobby_connect_screen_detail

void LobbyConnectScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(2);
	silencer::client_ui::use_lobby(
		silencer::client_ui::MakeLobbyProvider(ctx)).connection.reset();
	motdprinted = false;
	logLines.clear();
	username[0] = '\0';
	password[0] = '\0';
}

void LobbyConnectScreen::Tick(ScreenContext & ctx)
{
	// Keep the legacy connection gate: nothing happens until the menu music
	// has crossfaded in. The lobby state machine starts in
	// WAITING and only kicks off the TCP connect on the first tick after
	// the gate opens, so this delay shapes when the user sees "Connecting
	// to ..." appear in the textbox.
	silencer::client_ui::LobbyModel lobby =
		silencer::client_ui::use_lobby(
			silencer::client_ui::MakeLobbyProvider(ctx));
	if(!lobby.connection.ready()) return;

	silencer::client_ui::LobbyConnectionTickResult connection =
		lobby.connection.tick(motdprinted);
	for(const std::string& line : connection.log_lines){
		AppendLog(line.c_str());
	}
	if(connection.motd_printed) motdprinted = true;
	if(connection.destination == silencer::client_ui::LobbyConnectionDestination::Update){
		silencer::client_ui::use_navigation()
			.reset_to(std::make_unique<UpdateScreen>());
		return;
	}
	if(connection.destination == silencer::client_ui::LobbyConnectionDestination::CharacterCreate){
		silencer::client_ui::use_navigation()
			.reset_to(std::make_unique<CharacterCreateScreen>());
		return;
	}
	if(connection.destination == silencer::client_ui::LobbyConnectionDestination::Lobby){
		silencer::client_ui::use_navigation()
			.reset_to(std::make_unique<LobbyScreen>());
		return;
	}
}

void LobbyConnectScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, const silencer::ui::UiInputState&, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;
	silencer::client_ui::LobbyModel lobby =
		silencer::client_ui::use_lobby(
			silencer::client_ui::MakeLobbyProvider(ctx));
	silencer::client_ui::Navigation navigation =
		silencer::client_ui::use_navigation();
	bool inactive = lobby.connection.credentials_pending();
	lobby_connect_screen_detail::RegisterWidgets(this, username, password, inactive, interactions);
	if(!inactive && !interactions.HasFocus()){
		interactions.FocusTextInputByUid(lobby_connect_screen_detail::LBY_INPUT_USERNAME);
	}
	const bool usernameFocused =
		interactions.IsTextInputFocused(lobby_connect_screen_detail::LBY_INPUT_USERNAME);
	const bool passwordFocused =
		interactions.IsTextInputFocused(lobby_connect_screen_detail::LBY_INPUT_PASSWORD);
	const bool blink = (ctx.renderer.GetHudAnimationPhase() % 32) < 16;

	UpdateLogText();
	usernameDisplay = username;
	if(usernameFocused && blink) usernameDisplay += "|";
	passwordDisplay.assign(std::strlen(password), '*');
	if(passwordFocused && blink) passwordDisplay += "|";

	const float uiScale = silencer::clay_bridge::UiScale();
	const int virtualW = std::max(1, static_cast<int>(dst.w / uiScale));
	const int virtualH = std::max(1, static_cast<int>(dst.h / uiScale));
	silencer::client_ui::LobbyConnectFrameProps props{
		.key = "lobby-connect",
		.log_text = logText.c_str(),
		.username_display = usernameDisplay.c_str(),
		.password_display = passwordDisplay.c_str(),
		.inactive = inactive,
		.set_username = [this](const char * value) {
			lobby_connect_screen_detail::CopyUiText(
				username, static_cast<int>(sizeof(username)), value);
		},
		.set_password = [this](const char * value) {
			lobby_connect_screen_detail::CopyUiText(
				password, static_cast<int>(sizeof(password)), value);
		},
		.submit_username = [this, lobby](const char * value) {
			lobby_connect_screen_detail::CopyUiText(
				username, static_cast<int>(sizeof(username)), value);
			lobby.connection.submit_credentials(username, password);
		},
		.submit_password = [this, lobby](const char * value) {
			lobby_connect_screen_detail::CopyUiText(
				password, static_cast<int>(sizeof(password)), value);
			lobby.connection.submit_credentials(username, password);
		},
		.login = [this, lobby]() {
			lobby.connection.submit_credentials(username, password);
		},
		.cancel = [lobby, navigation]() {
			lobby.connection.cancel();
			navigation.reset_to(std::make_unique<MainMenuScreen>());
		},
	};
	retainedFrame_.Build([&]() {
		                     return silencer::client_ui::LobbyConnectFrame(props);
	                     },
	                     virtualW,
	                     virtualH,
	                     interactions);
	lobby_connect_screen_detail::RegisterWidgets(this, username, password, inactive, interactions);
}

void LobbyConnectScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool LobbyConnectScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		silencer::client_ui::LobbyModel lobby =
			silencer::client_ui::use_lobby(
				silencer::client_ui::MakeLobbyProvider(ctx));
		lobby.connection.cancel();
		silencer::client_ui::use_navigation()
			.reset_to(std::make_unique<MainMenuScreen>());
		return true;
	}
	return retainedFrame_.HandleUiIntent(action);
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

void LobbyConnectScreen::UpdateLogText()
{
	logText.clear();
	const int count = static_cast<int>(logLines.size());
	const int start = std::max(0, count - lobby_connect_screen_detail::kVisibleLogLines);
	for(int i = start; i < count; ++i){
		if(!logText.empty()) logText += "\n";
		logText += logLines[static_cast<std::size_t>(i)];
	}
}

const ::ui::DrawCommandList * LobbyConnectScreen::RetainedDrawCommands() const
{
	return &retainedFrame_.Commands();
}
