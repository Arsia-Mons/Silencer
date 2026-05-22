#include "controldispatch.h"
#include "game.h"
#include "world.h"
#include "keybinds.h"
#include "config.h"
#include "gasloader.h"
#include "os.h"
#include "shared.h"
#include "clay_ui_tests/clay_ui_checks.h"
#include "runtime/UiInteractionRegistry.h"
#include "screen.h"
#include "ui_editor_preview_screen.h"
#include "ui_editor_preview_document.h"
#include "password_modal.h"
#include <SDL3/SDL_keyboard.h>
#ifdef SILENCER_HAVE_LOBBY_UI
#include "lobby_screen.h"
#endif
#include "screen_context.h"
#include <cstring>
#include <cstdio>
#include <fstream>
#include <memory>
#include <utility>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir((p), 0755)
#endif

namespace ControlDispatch {

namespace {
std::string g_controlPasswordModalValue;
bool g_controlPasswordModalSubmitted = false;
}

ControlCommand::Phase PhaseFor(const std::string& op) {
	if(op == "screenshot") return ControlCommand::POST_RENDER;
	if(op == "wait_frames" || op == "wait_ms" ||
	   op == "wait_for_state" || op == "step") return ControlCommand::MULTI_FRAME;
	return ControlCommand::IMMEDIATE;
}

static ControlReply OkResult(int id, nlohmann::json r){
	ControlReply rpl;
	rpl.id = id;
	rpl.ok = true;
	rpl.result = std::move(r);
	return rpl;
}

static bool StateNeedsScreen(const std::string& state){
	return state == "MAINMENU" ||
	       state == "LOBBYCONNECT" ||
	       state == "LOBBY" ||
	       state == "UPDATING" ||
	       state == "MISSIONSUMMARY" ||
	       state == "OPTIONS" ||
	       state == "OPTIONSCONTROLS" ||
	       state == "OPTIONSDISPLAY" ||
	       state == "OPTIONSAUDIO";
}

static ControlReply Err(int id, const char* code, const std::string& msg){
	ControlReply rpl;
	rpl.id = id;
	rpl.ok = false;
	rpl.code = code;
	rpl.error = msg;
	return rpl;
}

static const char * ModeName(silencer::client_ui::InGameUiControlMode mode){
	using Mode = silencer::client_ui::InGameUiControlMode;
	switch(mode){
		case Mode::Clear: return "clear";
		case Mode::Status: return "status";
		case Mode::Chat: return "chat";
		case Mode::Buy: return "buy";
		case Mode::Tech: return "tech";
		case Mode::PlayerList: return "playerlist";
		case Mode::All: return "all";
	}
	return "status";
}

static bool ParseMode(
	const std::string& value,
	silencer::client_ui::InGameUiControlMode& mode){
	using Mode = silencer::client_ui::InGameUiControlMode;
	if(value == "clear"){
		mode = Mode::Clear;
		return true;
	}
	if(value == "status"){
		mode = Mode::Status;
		return true;
	}
	if(value == "chat"){
		mode = Mode::Chat;
		return true;
	}
	if(value == "buy"){
		mode = Mode::Buy;
		return true;
	}
	if(value == "tech"){
		mode = Mode::Tech;
		return true;
	}
	if(value == "playerlist"){
		mode = Mode::PlayerList;
		return true;
	}
	if(value == "all"){
		mode = Mode::All;
		return true;
	}
	return false;
}

static nlohmann::json InGameUiControlResultToJson(
	const silencer::client_ui::InGameUiControlResult& result){
	nlohmann::json r;
	r["mode"] = ModeName(result.mode);
	r["ok"] = result.available;
	if(!result.available){
		if(!result.error.empty()) r["error"] = result.error;
		return r;
	}
	if(result.mode == silencer::client_ui::InGameUiControlMode::Clear){
		return r;
	}
	r["chat_active"] = result.chatActive;
	r["buy_active"] = result.buyActive;
	r["tech_active"] = result.techActive;
	r["show_chat_ticks"] = result.showChatTicks;
	r["show_player_list"] = result.showPlayerList;
	r["buy_item_count"] = result.buyItemCount;
	r["tech_item_count"] = result.techItemCount;
	r["buy_selected_index"] = result.buySelectedIndex;
	r["tech_selected_index"] = result.techSelectedIndex;
	return r;
}

static nlohmann::json WorldSummaryToJson(const WorldSummary& summary){
	nlohmann::json r;
	r["map"] = summary.map;
	r["peers"] = summary.peers;
	r["localpeerid"] = summary.localPeerId;
	r["viewedpeerid"] = summary.viewedPeerId;
	r["authoritypeer"] = summary.authorityPeer;
	r["lobby_accountid"] = summary.lobbyAccountId;
	r["is_local_observer"] = summary.isLocalObserver;
	r["spectator_initialized"] = summary.spectatorInitialized;
	r["spectator_freecam"] = summary.spectatorFreecam;

	nlohmann::json peerlist = nlohmann::json::array();
	for(const WorldPeerSummary& peer : summary.peerList){
		nlohmann::json p;
		p["id"] = peer.id;
		p["accountid"] = peer.accountId;
		p["observer"] = peer.observer;
		p["disconnected"] = peer.disconnected;
		nlohmann::json controlled = nlohmann::json::array();
		for(int id : peer.controlledList) controlled.push_back(id);
		p["controlledlist"] = controlled;
		peerlist.push_back(std::move(p));
	}
	r["peerlist"] = peerlist;

	nlohmann::json players = nlohmann::json::array();
	for(const WorldPlayerSummary& player : summary.players){
		nlohmann::json p;
		p["id"] = player.id;
		p["hp"] = player.hp;
		p["x"] = player.x;
		p["y"] = player.y;
		players.push_back(std::move(p));
	}
	r["players"] = players;
	r["objects_count"] = summary.objectsCount;
	r["message_text"] = summary.messageText;
	r["message_progress"] = summary.messageProgress;
	r["message_type"] = summary.messageType;
	r["message_time"] = summary.messageTime;
	r["topmessage_text"] = summary.topMessageText;
	r["topmessage_progress"] = summary.topMessageProgress;
	return r;
}

static const silencer::ui::UiInteractable * FindInteractableTarget(
	const silencer::ui::UiInteractionRegistry& interactions,
	const std::string & target){
	const silencer::ui::UiInteractable * hit = nullptr;
	int count = 0;
	for(const auto & candidate : interactions.Interactables()){
		bool matches = candidate.id == target;
		if(!matches) matches =
			silencer::ui::UiInteractableMatchesLabel(candidate, target.c_str());
		if(!matches && candidate.uid >= 0) matches = std::to_string(candidate.uid) == target;
		if(matches){
			hit = &candidate;
			count++;
		}
	}
	return count == 1 ? hit : nullptr;
}

static bool PointInInteractable(const silencer::ui::UiInteractable& widget, int x, int y){
	return x >= widget.x && y >= widget.y
	    && x < widget.x + widget.w && y < widget.y + widget.h;
}

static const silencer::ui::UiInteractable * FindInteractableAt(
	const silencer::ui::UiInteractionRegistry& interactions,
	int x,
	int y){
	const auto & widgets = interactions.Interactables();
	for(auto it = widgets.rbegin(); it != widgets.rend(); ++it){
		if(silencer::ui::UiInteractableIsInteractive(*it) &&
		   PointInInteractable(*it, x, y)) return &*it;
	}
	return nullptr;
}

static void QueueInteractableAction(Game& game,
                                    const silencer::ui::UiInteractable& widget,
                                    silencer::ui::UiActionKind kind,
                                    const std::string & value = std::string(),
                                    int amount = 0){
	silencer::ui::UiAction action;
	action.kind = kind;
	action.id = !widget.id.empty()
		? widget.id
		: (silencer::ui::UiInteractableLabel(widget)
			? std::string(silencer::ui::UiInteractableLabel(widget))
			: std::string());
	action.value = value;
	action.index = widget.index;
	action.amount = amount;
	game.UiInput().QueueControlAction(std::move(action));
}

static bool QueueControlKey(Game& game, int ascii){
	switch(ascii){
		case 1:
			game.UiInput().QueueBindingKeyDown(SDL_SCANCODE_LEFT);
			game.UiInput().QueueNavAction(silencer::ui::UiNavAction::Left);
			return true;
		case 2:
			game.UiInput().QueueBindingKeyDown(SDL_SCANCODE_RIGHT);
			game.UiInput().QueueNavAction(silencer::ui::UiNavAction::Right);
			return true;
		case 3:
			game.UiInput().QueueBindingKeyDown(SDL_SCANCODE_UP);
			game.UiInput().QueueNavAction(silencer::ui::UiNavAction::Up);
			return true;
		case 4:
			game.UiInput().QueueBindingKeyDown(SDL_SCANCODE_DOWN);
			game.UiInput().QueueNavAction(silencer::ui::UiNavAction::Down);
			return true;
		case '\t':
			game.UiInput().QueueBindingKeyDown(SDL_SCANCODE_TAB);
			game.UiInput().QueueNavAction(silencer::ui::UiNavAction::FocusNext);
			return true;
		case '\n':
			game.UiInput().QueueBindingKeyDown(SDL_SCANCODE_RETURN);
			game.UiInput().QueueNavAction(silencer::ui::UiNavAction::Confirm);
			return true;
		case 0x1B:
			game.UiInput().QueueBindingKeyDown(SDL_SCANCODE_ESCAPE);
			game.UiInput().QueueNavAction(silencer::ui::UiNavAction::Cancel);
			return true;
		case '\b':
			game.UiInput().QueueBindingKeyDown(SDL_SCANCODE_BACKSPACE);
			game.UiInput().QueueNavAction(silencer::ui::UiNavAction::Backspace);
			return true;
		default:
			if(ascii >= 0x20 && ascii <= 0x7E){
				SDL_Keymod mod = SDL_KMOD_NONE;
				SDL_Scancode scancode =
					SDL_GetScancodeFromKey(static_cast<SDL_Keycode>(ascii), &mod);
				if(scancode != SDL_SCANCODE_UNKNOWN){
					game.UiInput().QueueBindingKeyDown(scancode);
				}
				game.UiInput().QueueTextInput(static_cast<char>(ascii));
				return true;
			}
			return false;
	}
}

// Forward decl for the keybind sub-dispatcher implemented at the bottom of
// this file. Lives in the same TU because it only ever reads/mutates Game's
// KeyMap and Config; no other consumers.
static void HandleKeybind(Game& game, ControlCommand& cmd);
static void HandleGas(Game& game, ControlCommand& cmd);

void HandleImmediate(Game& game, ControlCommand& cmd) {
	if(cmd.op == "ping"){
		nlohmann::json r;
		r["version"] = SILENCER_VERSION;
		r["build"] = __DATE__ " " __TIME__;
		r["frame"] = game.GetFrameCount();
		r["paused"] = game.paused;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
	if(cmd.op == "resize"){
		int width = cmd.args.value("w", cmd.args.value("width", 0));
		int height = cmd.args.value("h", cmd.args.value("height", 0));
		if(width < 1 || height < 1){
			cmd.reply->set_value(Err(cmd.id, "BAD_ARGS",
				"resize requires --w <pixels> --h <pixels>"));
			return;
		}
		if(!game.ResizeRenderSurface(width, height)){
			cmd.reply->set_value(Err(cmd.id, "INTERNAL", "resize failed"));
			return;
		}
		nlohmann::json r;
		r["width"] = game.GetScreenBuffer().w;
		r["height"] = game.GetScreenBuffer().h;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
	if(cmd.op == "clay_button_check"){
		silencer::clay_bridge::ButtonCheckResult res{};
		bool ok = silencer::clay_bridge::RunButtonCheck(game, res);
		if(!ok){
			cmd.reply->set_value(Err(cmd.id, "INTERNAL",
				"button check failed"));
			return;
		}
		nlohmann::json r;
		r["clicks_fired_on_press"] = res.clicksFiredOnPress;
		r["clicks_fired_when_held"] = res.clicksFiredWhenHeld;
		r["chrome_brightness_hover"] = res.chromeBrightnessHover;
		r["chrome_brightness_idle"] = res.chromeBrightnessIdle;
		r["chrome_sprite_index_hover"] = res.chromeSpriteIndexHover;
		r["oval_hover_sprite_indices"] = nlohmann::json::array();
		r["oval_hover_brightness"] = nlohmann::json::array();
		r["oval_unhover_sprite_indices"] = nlohmann::json::array();
		r["oval_unhover_brightness"] = nlohmann::json::array();
		for(int i = 0; i < 5; ++i){
			r["oval_hover_sprite_indices"].push_back(res.ovalHoverSpriteIndices[i]);
			r["oval_hover_brightness"].push_back(res.ovalHoverBrightness[i]);
			r["oval_unhover_sprite_indices"].push_back(res.ovalUnhoverSpriteIndices[i]);
			r["oval_unhover_brightness"].push_back(res.ovalUnhoverBrightness[i]);
		}
		r["oval_focus_sprite_index"] = res.ovalFocusSpriteIndex;
		r["oval_focus_brightness"] = res.ovalFocusBrightness;
		r["oval_wall_clock_partial_sprite_index"] = res.ovalWallClockPartialSpriteIndex;
		r["oval_wall_clock_partial_brightness"] = res.ovalWallClockPartialBrightness;
		r["oval_wall_clock_next_sprite_index"] = res.ovalWallClockNextSpriteIndex;
		r["oval_wall_clock_next_brightness"] = res.ovalWallClockNextBrightness;
		r["compact_width"] = res.compactWidth;
		r["compact_height"] = res.compactHeight;
		r["chrome_auto_width"] = res.chromeAutoWidth;
		r["chrome_auto_height"] = res.chromeAutoHeight;
		r["text_compact_width"] = res.textCompactWidth;
		r["text_compact_height"] = res.textCompactHeight;
		r["text_compact_text_x_offset"] = res.textCompactTextXOffset;
		r["text_compact_text_width"] = res.textCompactTextWidth;
		r["text_compact_text_y_offset"] = res.textCompactTextYOffset;
		r["auto_short_width"] = res.autoShortWidth;
		r["auto_long_width"] = res.autoLongWidth;
		r["auto_multiline_height"] = res.autoMultilineHeight;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
	if(cmd.op == "clay_toggle_check"){
		silencer::clay_bridge::ToggleCheckResult res{};
		bool ok = silencer::clay_bridge::RunToggleCheck(game, res);
		if(!ok){
			cmd.reply->set_value(Err(cmd.id, "INTERNAL",
				"toggle check failed"));
			return;
		}
		nlohmann::json r;
		r["clicks_toggle_0"] = res.clicksToggle0;
		r["clicks_toggle_1"] = res.clicksToggle1;
		r["clicks_toggle_2"] = res.clicksToggle2;
		r["selected_brightness"] = res.selectedBrightness;
		r["unselected_brightness"] = res.unselectedBrightness;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
	if(cmd.op == "clay_scroll_list_check"){
		silencer::clay_bridge::ScrollListCheckResult res{};
		bool ok = silencer::clay_bridge::RunScrollListCheck(game, res);
		if(!ok){
			cmd.reply->set_value(Err(cmd.id, "INTERNAL",
				"scroll_list check failed"));
			return;
		}
		nlohmann::json r;
		r["select_actions"] = res.selectActions;
		r["last_selected_index"] = res.lastSelectedIndex;
		r["no_overflow_scrollbar_count"] = res.noOverflowScrollbarCount;
		r["overflow_scrollbar_count"] = res.overflowScrollbarCount;
		r["overflow_scrollbar_bbox_x"] = res.overflowScrollbarBboxX;
		r["overflow_scrollbar_bbox_y"] = res.overflowScrollbarBboxY;
		r["overflow_scrollbar_bbox_w"] = res.overflowScrollbarBboxW;
		r["overflow_scrollbar_bbox_h"] = res.overflowScrollbarBboxH;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
	if(cmd.op == "clay_scroll_text_box_check"){
		silencer::clay_bridge::ScrollTextBoxCheckResult res{};
		bool ok = silencer::clay_bridge::RunScrollTextBoxCheck(game, res);
		if(!ok){
			cmd.reply->set_value(Err(cmd.id, "INTERNAL",
				"scroll_text_box check failed"));
			return;
		}
		nlohmann::json r;
		r["at_bottom_prev_pos"] = res.atBottom_prevPos;
		r["not_at_bottom_prev_pos"] = res.notAtBottom_prevPos;
		r["at_bottom_overflow_prev_pos"] = res.atBottomOverflow_prevPos;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
	if(cmd.op == "clay_text_input_check"){
		silencer::clay_bridge::TextInputCheckResult res{};
		bool ok = silencer::clay_bridge::RunTextInputCheck(game, res);
		if(!ok){
			cmd.reply->set_value(Err(cmd.id, "INTERNAL",
				"text_input check failed"));
			return;
		}
		nlohmann::json r;
		r["submit_actions_for_enter"] = res.submitActionsForEnter;
		r["submit_actions_for_text"] = res.submitActionsForText;
		r["password_mask_applied_len"] = res.passwordMaskAppliedLen;
		r["overflow_tail_applied_len"] = res.overflowTailAppliedLen;
		r["overflow_tail_matches"] = res.overflowTailMatches;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
	if(cmd.op == "state"){
		nlohmann::json r;
		r["state"] = Game::StateName(game.GetState());
		r["frame"] = game.GetFrameCount();
		r["paused"] = game.paused;
		r["ui_width"] = game.CurrentUiInput().width;
		r["ui_height"] = game.CurrentUiInput().height;
		r["ui_scale"] = game.CurrentUiInput().uiScale;
		// Expose the lobby connection sub-state so test scripts can wait for
		// AUTHENTICATING before dispatching a Login click — the LobbyConnect
		// state machine progresses asynchronously through Connect/version
		// check, and a click before AUTHENTICATING is silently consumed.
		static const char * lobbyStateNames[] = {
			"IDLE","WAITING","CONNECTING","RESOLVING","WAITINGFORRESOLVER",
			"RESOLVED","RESOLVEFAILED","CONNECTIONFAILED","CONNECTED",
			"CHECKINGVERSION","AUTHENTICATING","AUTHSENT","AUTHENTICATED",
			"AUTHFAILED","DISCONNECTED"
		};
		int ls = (int)game.GetWorld().lobby.state;
		if(ls >= 0 && ls < (int)(sizeof(lobbyStateNames)/sizeof(lobbyStateNames[0]))){
			r["lobby_state"] = lobbyStateNames[ls];
		}else{
			r["lobby_state"] = "UNKNOWN";
		}
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
	if(cmd.op == "ui_editor_preview"){
		nlohmann::json documentJson;
		if(!cmd.args.contains("document")){
			cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST",
				"ui_editor_preview requires args.document"));
			return;
		}
		try{
			const nlohmann::json& raw = cmd.args["document"];
			if(raw.is_string()){
				documentJson = nlohmann::json::parse(raw.get<std::string>());
			}else{
				documentJson = raw;
			}
		}catch(const std::exception& ex){
			cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST",
				std::string("invalid ui editor document json: ") + ex.what()));
			return;
		}

		silencer::ui::UiEditorPreviewDocument document;
		std::string error;
		if(!silencer::net::ParseUiEditorPreviewDocument(documentJson, document, error)){
			cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST", error));
			return;
		}
		if(!game.ResizeRenderSurface(document.viewportWidth, document.viewportHeight)){
			cmd.reply->set_value(Err(cmd.id, "INTERNAL", "resize failed for ui editor preview"));
			return;
		}

		Screen * top = game.GetTopScreen();
		auto * preview =
			dynamic_cast<silencer::client_ui::UiEditorPreviewScreen *>(top);
		const std::string surface = document.surface;
		const int width = document.viewportWidth;
		const int height = document.viewportHeight;
		if(preview){
			preview->SetDocument(std::move(document));
		}else{
			game.ReplaceScreen(std::make_unique<silencer::client_ui::UiEditorPreviewScreen>(
				std::move(document)));
		}

		nlohmann::json r;
		r["surface"] = surface;
		r["width"] = width;
		r["height"] = height;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
	if(cmd.op == "inspect"){
		nlohmann::json widgets = nlohmann::json::array();
		for(const auto & cw : game.UiInteractions().Interactables()){
			nlohmann::json w;
			w["source"] = "clay";
			if(!cw.id.empty()) w["id"] = cw.id;
			w["x"] = cw.x; w["y"] = cw.y;
			w["w"] = cw.w; w["h"] = cw.h;
			if(silencer::ui::UiInteractableLabel(cw))
				w["label"] = silencer::ui::UiInteractableLabel(cw);
			if(cw.uid >= 0) w["uid"] = cw.uid;
			using K = silencer::ui::UiInteractableKind;
			switch(cw.kind){
				case K::Button:    w["kind"] = "button"; break;
				case K::Toggle:
					w["kind"] = "toggle";
					w["selected"] = cw.selected;
					break;
				case K::TextInput:
					w["kind"] = "textinput";
					w["password"] = cw.isPassword;
					w["text"] = cw.isPassword
						? std::string(cw.value.size(), '*')
						: cw.value;
					w["maxchars"] = cw.maxLength;
					break;
				case K::ListRow:
					w["kind"] = "listrow";
					w["row_index"] = cw.index;
					w["selected"] = cw.selected;
					break;
			}
			widgets.push_back(std::move(w));
		}
		nlohmann::json elements = nlohmann::json::array();
		for(const auto & element : game.UiInteractions().Elements()){
			if(!element.id.empty() &&
			   game.UiInteractions().FindInteractableById(element.id)){
				continue;
			}
			nlohmann::json e;
			e["source"] = "clay";
			if(!element.id.empty()) e["id"] = element.id;
			if(!element.label.empty()) e["label"] = element.label;
			if(!element.value.empty()) e["value"] = element.value;
			e["x"] = element.bounds.x;
			e["y"] = element.bounds.y;
			e["w"] = element.bounds.width;
			e["h"] = element.bounds.height;
			e["enabled"] = element.enabled;
			e["focused"] = element.focused;
			e["selected"] = element.selected;
			using EK = silencer::ui::UiElementKind;
			switch(element.kind){
				case EK::Container: e["kind"] = "container"; break;
				case EK::Button: e["kind"] = "button"; break;
				case EK::Text: e["kind"] = "text"; break;
				case EK::TextField: e["kind"] = "textfield"; break;
				case EK::ListItem: e["kind"] = "listitem"; break;
				case EK::Tab: e["kind"] = "tab"; break;
				case EK::Slider: e["kind"] = "slider"; break;
				case EK::Progress: e["kind"] = "progress"; break;
			}
			elements.push_back(std::move(e));
		}
		if(widgets.empty() && elements.empty()){
			cmd.reply->set_value(Err(cmd.id, "WRONG_STATE", "no clay widgets"));
			return;
		}
		nlohmann::json r;
		r["widgets"] = widgets;
		r["elements"] = elements;
		r["interface_id"] = 0;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
	if(cmd.op == "world_state"){
		cmd.reply->set_value(OkResult(cmd.id, WorldSummaryToJson(game.GetWorldSummary())));
		return;
	}
	if(cmd.op == "show_password_modal"){
		g_controlPasswordModalValue.clear();
		g_controlPasswordModalSubmitted = false;
		game.PushScreen(std::make_unique<PasswordModal>([](const char * password) {
			g_controlPasswordModalValue = password ? password : "";
			g_controlPasswordModalSubmitted = true;
		}));
		cmd.reply->set_value(OkResult(cmd.id, nlohmann::json::object()));
		return;
	}
	if(cmd.op == "password_modal_result"){
		nlohmann::json r;
		r["submitted"] = g_controlPasswordModalSubmitted;
		r["value"] = g_controlPasswordModalValue;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
	if(cmd.op == "ingame_ui_mode"){
		std::string mode = cmd.args.value("mode", std::string());
		silencer::client_ui::InGameUiControlMode controlMode;
		if(mode.empty() || !ParseMode(mode, controlMode)){
			cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST",
				"ingame_ui_mode needs --mode clear|status|chat|buy|tech|playerlist|all"));
			return;
		}
		silencer::client_ui::InGameUiControlResult result =
			game.InGameUi().ConfigureForControl(controlMode);
		if(!result.available){
			cmd.reply->set_value(Err(cmd.id, "WRONG_STATE",
				result.error.empty()
					? std::string("in-game UI mode unavailable")
					: result.error));
			return;
		}
		cmd.reply->set_value(OkResult(cmd.id, InGameUiControlResultToJson(result)));
		return;
	}
	if(cmd.op == "click"){
		std::string target;
		if(cmd.args.contains("label")) target = cmd.args["label"].get<std::string>();
		else if(cmd.args.contains("id")) target = std::to_string(cmd.args["id"].get<int>());
		if(target.empty()){
			cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST", "click needs label or id"));
			return;
		}
		const auto * cw = FindInteractableTarget(game.UiInteractions(), target);
		if(cw){
			using K = silencer::ui::UiInteractableKind;
			if(cw->kind == K::Button || cw->kind == K::Toggle){
				const char * label = silencer::ui::UiInteractableLabel(*cw);
				QueueInteractableAction(game, *cw, silencer::ui::UiActionKind::Activate,
				                  label ? label : "");
				nlohmann::json r;
				r["source"] = "clay";
				cmd.reply->set_value(OkResult(cmd.id, r));
				return;
			}
			if(cw->kind == K::ListRow){
				const char * label = silencer::ui::UiInteractableLabel(*cw);
				QueueInteractableAction(game, *cw, silencer::ui::UiActionKind::Select,
				                  label ? label : "");
				nlohmann::json r;
				r["source"] = "clay";
				r["row_index"] = cw->index;
				cmd.reply->set_value(OkResult(cmd.id, r));
				return;
			}
			cmd.reply->set_value(Err(cmd.id, "WRONG_TYPE",
				"clay widget \"" + target + "\" is not clickable"));
			return;
		}
		cmd.reply->set_value(Err(cmd.id, "WIDGET_NOT_FOUND",
			"no widget matches \"" + target + "\""));
		return;
	}
	if(cmd.op == "click_at"){
		if(!cmd.args.contains("x") || !cmd.args.contains("y")){
			cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST", "click_at needs x and y"));
			return;
		}
		int x = cmd.args["x"].get<int>();
		int y = cmd.args["y"].get<int>();
		if(!FindInteractableAt(game.UiInteractions(), x, y)){
			cmd.reply->set_value(Err(cmd.id, "WIDGET_NOT_FOUND",
				"no clickable clay widget at point"));
			return;
		}
		game.UiInput().QueueControlPointerPress(x, y);
		nlohmann::json r;
		r["source"] = "clay";
		r["x"] = x;
		r["y"] = y;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
	if(cmd.op == "set_text"){
		std::string text = cmd.args.value("text", std::string());
		const auto * cw = static_cast<const silencer::ui::UiInteractable *>(nullptr);
		int count = 0;
		if(cmd.args.contains("uid")){
			int uid = cmd.args["uid"].get<int>();
			for(const auto & candidate : game.UiInteractions().Interactables()){
				if(candidate.uid == uid
				   && candidate.kind == silencer::ui::UiInteractableKind::TextInput){
					cw = &candidate;
					count++;
				}
			}
			if(count != 1) cw = nullptr;
		}else if(cmd.args.contains("label")){
			std::string label = cmd.args["label"].get<std::string>();
			for(const auto & candidate : game.UiInteractions().Interactables()){
				if(candidate.kind == silencer::ui::UiInteractableKind::TextInput
				   && silencer::ui::UiInteractableMatchesLabel(candidate, label.c_str())){
					cw = &candidate;
					count++;
				}
			}
			if(count != 1) cw = nullptr;
		}
		if(cw){
			if(cw->kind == silencer::ui::UiInteractableKind::TextInput){
				int cap = cw->maxLength;
				int n = (int)text.size();
				if(cap > 0 && n > cap) n = cap;
				QueueInteractableAction(game, *cw, silencer::ui::UiActionKind::SetText,
				                  text.substr(0, static_cast<size_t>(n)));
				nlohmann::json r;
				r["source"] = "clay";
				cmd.reply->set_value(OkResult(cmd.id, r));
				return;
			}
			cmd.reply->set_value(Err(cmd.id, "WRONG_TYPE",
				"clay widget is not a textinput"));
			return;
		}
		if(cmd.args.contains("label")){
			std::string label = cmd.args["label"].get<std::string>();
			if(count > 1){
				cmd.reply->set_value(Err(cmd.id, "AMBIGUOUS_TARGET", label));
				return;
			}
			cmd.reply->set_value(Err(cmd.id, "WIDGET_NOT_FOUND", label));
			return;
		}
		if(cmd.args.contains("uid")){
			int uid = cmd.args["uid"].get<int>();
			cmd.reply->set_value(Err(cmd.id, "WIDGET_NOT_FOUND",
				"no clay widget with uid " + std::to_string(uid)));
			return;
		}
		cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST",
			"set_text needs label or uid"));
		return;
	}

	if(cmd.op == "select"){
		const auto & widgets = game.UiInteractions().Interactables();
		const auto * hit = static_cast<const silencer::ui::UiInteractable *>(nullptr);
		int count = 0;
		if(cmd.args.contains("index")){
			int index = cmd.args["index"].get<int>();
			for(const auto & candidate : widgets){
				if(candidate.kind == silencer::ui::UiInteractableKind::ListRow
				   && candidate.index == index){
					hit = &candidate;
					count++;
				}
			}
		}else if(cmd.args.contains("label")){
			std::string label = cmd.args["label"].get<std::string>();
			for(const auto & candidate : widgets){
				if(candidate.kind == silencer::ui::UiInteractableKind::ListRow
				   && silencer::ui::UiInteractableMatchesLabel(candidate, label.c_str())){
					hit = &candidate;
					count++;
				}
			}
		}else if(cmd.args.contains("text")){
			std::string text = cmd.args["text"].get<std::string>();
			for(const auto & candidate : widgets){
				if(candidate.kind == silencer::ui::UiInteractableKind::ListRow
				   && silencer::ui::UiInteractableMatchesLabel(candidate, text.c_str())){
					hit = &candidate;
					count++;
				}
			}
		}else{
			cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST",
				"select needs --index, --label, or --text"));
			return;
		}
		if(count != 1 || !hit){
			cmd.reply->set_value(Err(cmd.id,
				count > 1 ? "AMBIGUOUS_TARGET" : "WIDGET_NOT_FOUND",
				"no unique clay list row matches select target"));
			return;
		}
		const char * hitLabel = silencer::ui::UiInteractableLabel(*hit);
		QueueInteractableAction(game, *hit, silencer::ui::UiActionKind::Select,
		                  hitLabel ? hitLabel : "");
		nlohmann::json r;
		r["source"] = "clay";
		r["row_index"] = hit->index;
		if(hitLabel) r["label"] = hitLabel;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
	if(cmd.op == "scroll"){
		std::string label = cmd.args.value("label", std::string());
		int amount = cmd.args.value("amount", 1);
		if(label.empty()){
			cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST",
				"scroll needs --label for a Clay scroll control"));
			return;
		}
		if(amount < 1) amount = 1;
		const auto * element = game.UiInteractions().FindByLabel(label);
		if(element && element->value == "scroll"){
			silencer::ui::UiAction action;
			action.kind = silencer::ui::UiActionKind::Scroll;
			action.id = element->id;
			action.value = "control";
			action.amount = amount;
			game.UiInput().QueueControlAction(std::move(action));
			nlohmann::json r;
			r["source"] = "clay";
			r["target"] = "element";
			r["label"] = label;
			r["amount"] = amount;
			cmd.reply->set_value(OkResult(cmd.id, r));
			return;
		}
		const auto * cw = game.UiInteractions().FindInteractableByLabel(label.c_str());
		if(!cw){
			cmd.reply->set_value(Err(cmd.id, "WIDGET_NOT_FOUND", label));
			return;
		}
		if(cw->kind != silencer::ui::UiInteractableKind::Button){
			cmd.reply->set_value(Err(cmd.id, "WRONG_TYPE",
				"scroll target is not a Clay scroll button"));
			return;
		}
		const char * actionValue = silencer::ui::UiInteractableLabel(*cw);
		for(int i = 0; i < amount; ++i){
			QueueInteractableAction(game, *cw, silencer::ui::UiActionKind::Activate,
			                  actionValue ? actionValue : "");
		}
		nlohmann::json r;
		r["source"] = "clay";
		r["label"] = label;
		r["amount"] = amount;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}

	if(cmd.op == "back"){
		bool went = game.GoBack();
		nlohmann::json r; r["went_back"] = went;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
	if(cmd.op == "quit"){
		game.quitRequested = true;
		cmd.reply->set_value(OkResult(cmd.id, nlohmann::json::object()));
		return;
	}
	if(cmd.op == "pause"){
		if(game.IsLiveMultiplayer()){
			cmd.reply->set_value(Err(cmd.id, "WRONG_STATE", "pause not supported in live multiplayer"));
			return;
		}
		game.paused = true;
		cmd.reply->set_value(OkResult(cmd.id, nlohmann::json::object()));
		return;
	}
	if(cmd.op == "resume"){
		game.paused = false;
		game.stepFramesRemaining = 0;
		game.stepWallclockDeadlineMs = 0;
		cmd.reply->set_value(OkResult(cmd.id, nlohmann::json::object()));
		return;
	}
	if(cmd.op == "keybind"){
		HandleKeybind(game, cmd);
		return;
	}
	if(cmd.op == "gas"){
		HandleGas(game, cmd);
		return;
	}
#ifdef SILENCER_HAVE_LOBBY_UI
	if(cmd.op == "lobby_show_panel"){
		// Drive the lobby's right-side panel swap from a CLI test. The lobby
		// exposes these panels without world Interface objects, so
		// `click --label "Create Game"` can't reach them. This
		// op routes through `LobbyScreen::ShowGame*` directly.
		Screen * top = game.GetTopScreen();
		LobbyScreen * lobby = dynamic_cast<LobbyScreen *>(top);
		if(!lobby){
			cmd.reply->set_value(Err(cmd.id, "WRONG_STATE",
				"top screen is not a LobbyScreen"));
			return;
		}
		std::string which = cmd.args.value("panel", std::string());
		ScreenContext & ctx = game.GetScreenContext();
		if(which == "create")      lobby->ShowGameCreate(ctx);
		else if(which == "select") lobby->ShowGameSelect(ctx);
		else if(which == "join")   lobby->ShowGameJoin(ctx);
		else if(which == "tech")   lobby->ShowGameTech(ctx);
		else{
			cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST",
				"lobby_show_panel needs --panel select|create|join|tech"));
			return;
		}
		cmd.reply->set_value(OkResult(cmd.id, nlohmann::json::object()));
		return;
	}
#endif
	if(cmd.op == "key"){
		// Edge-triggered key event for the active UI. The external CLI keeps
		// its legacy ascii/name surface, but Game stores normalized text/nav
		// input for ClientUi to dispatch after layout.
		int ascii = -1;
		if(cmd.args.contains("ascii") && cmd.args["ascii"].is_number()){
			ascii = cmd.args["ascii"].get<int>();
		}else if(cmd.args.contains("key") && cmd.args["key"].is_string()){
			std::string k = cmd.args["key"].get<std::string>();
			if(k == "left")           ascii = 1;
			else if(k == "right")     ascii = 2;
			else if(k == "up")        ascii = 3;
			else if(k == "down")      ascii = 4;
			else if(k == "tab")       ascii = '\t';
			else if(k == "enter")     ascii = '\n';
			else if(k == "escape")    ascii = 0x1B;
			else if(k == "backspace") ascii = '\b';
			else if(k.size() == 1)    ascii = (unsigned char)k[0];
		}
		if(ascii < 0){
			cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST", "key needs 'ascii' int or 'key' name/char"));
			return;
		}
		if(game.HasUiInputTarget() && QueueControlKey(game, ascii)){
			cmd.reply->set_value(OkResult(cmd.id, nlohmann::json::object()));
			return;
		}
		cmd.reply->set_value(Err(cmd.id, "WRONG_STATE", "no normalized UI input target"));
		return;
	}
	cmd.reply->set_value(Err(cmd.id, "UNKNOWN_OP", "unknown op: " + cmd.op));
}

void EnqueueWait(Game& game, ControlCommand cmd){
	Game::PendingWait w;
	w.cmd = std::move(cmd);
	if(w.cmd.op == "wait_frames"){
		w.frames_left = w.cmd.args.value("n", 1);
	} else if(w.cmd.op == "wait_ms"){
		int ms = w.cmd.args.value("n", 0);
		w.deadline_ms = SDL_GetTicks() + (Uint64)ms;
	} else if(w.cmd.op == "wait_for_state"){
		w.wait_state = w.cmd.args.value("state", std::string());
		int t = w.cmd.args.value("timeout_ms", 5000);
		w.deadline_ms = SDL_GetTicks() + (Uint64)t;
	} else if(w.cmd.op == "step"){
		int frames = w.cmd.args.value("frames", 0);
		int ms     = w.cmd.args.value("ms", 0);
		if(frames > 0){
			game.stepFramesRemaining = frames;
			w.frames_left = frames;
		} else if(ms > 0){
			game.stepWallclockDeadlineMs = SDL_GetTicks() + (Uint64)ms;
			w.deadline_ms = game.stepWallclockDeadlineMs;
		} else {
			w.cmd.reply->set_value(Err(w.cmd.id, "BAD_REQUEST", "step needs frames>0 or ms>0"));
			return;
		}
		// step assumes the caller wanted the sim to advance and re-pause.
		game.paused = true;
	}
	game.pendingWaits.push_back(std::move(w));
}

void TickWaits(Game& game){
	Uint64 now = SDL_GetTicks();
	auto& v = game.pendingWaits;
	for(auto it = v.begin(); it != v.end();){
		bool done = false;
		auto& w = *it;
		if(w.cmd.op == "wait_frames"){
			if(w.frames_left > 0) --w.frames_left;
			if(w.frames_left == 0) done = true;
			if(w.deadline_ms > 0 && now >= w.deadline_ms) done = true;
		} else if(w.cmd.op == "step"){
			// Frame-based step: completion is when the sim has consumed all step
			// ticks. stepFramesRemaining is decremented per sim tick (which can
			// fire multiple times per Loop during catch-up), so it's the canonical
			// signal — using w.frames_left here would drift when catch-up runs.
			// w.frames_left > 0 just marks "this step is frame-based".
			if(w.frames_left > 0 && game.stepFramesRemaining == 0) done = true;
			if(w.deadline_ms > 0 && now >= w.deadline_ms) done = true;
		} else if(w.cmd.op == "wait_ms"){
			if(now >= w.deadline_ms) done = true;
		} else if(w.cmd.op == "wait_for_state"){
			if(w.wait_state == Game::StateName(game.GetState()) &&
			   (!StateNeedsScreen(w.wait_state) || game.GetTopScreen())){
				w.cmd.reply->set_value(OkResult(w.cmd.id, nlohmann::json::object()));
				it = v.erase(it); continue;
			}
			if(now >= w.deadline_ms){
				w.cmd.reply->set_value(Err(w.cmd.id, "TIMEOUT",
					"state did not become " + w.wait_state));
				it = v.erase(it); continue;
			}
		}
		if(done){
			if(w.cmd.op == "step"){
				game.paused = true;  // step span ended; re-pause
				game.stepFramesRemaining = 0;
				game.stepWallclockDeadlineMs = 0;
			}
			w.cmd.reply->set_value(OkResult(w.cmd.id, nlohmann::json::object()));
			it = v.erase(it);
		} else {
			++it;
		}
	}
}

void HandlePostRender(Game& game, ControlCommand& cmd) {
	if(cmd.op == "screenshot"){
		std::string out = cmd.args.value("out", std::string());
		if(out.empty()){
			char buf[256];
		#ifdef _WIN32
			const char* tmp = getenv("TEMP"); if(!tmp) tmp = ".";
			snprintf(buf, sizeof(buf), "%s\\silencer-%d.png", tmp, game.GetFrameCount());
		#else
			snprintf(buf, sizeof(buf), "/tmp/silencer-%d.png", game.GetFrameCount());
		#endif
			out = buf;
		}
		bool ok = game.GetRenderer().CapturePNG(game.GetScreenBuffer(),
			game.GetPaletteColors(), out.c_str());
		if(!ok){
			cmd.reply->set_value(Err(cmd.id, "INTERNAL", "stbi_write_png failed: " + out));
			return;
		}
		nlohmann::json r; r["path"] = out;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
	cmd.reply->set_value(Err(cmd.id, "UNKNOWN_OP", "unknown post-render op: " + cmd.op));
}

// ---------------------------------------------------------------------------
// keybind sub-dispatch (SSM-shaped: list / actions / get / put / unset / use /
// new / delete). All ops run on the game thread (IMMEDIATE phase) so they can
// mutate Game's live KeyMap without any locking — the per-frame poll reads
// the same KeyMap on the same thread.
// ---------------------------------------------------------------------------

namespace {

nlohmann::json BindingsToJson(const ActionBindings& ab) {
	nlohmann::json out = nlohmann::json::array();
	for (const auto& b : ab.bindings) {
		if (b.keys.size() == 1) {
			out.push_back(Stringify(b.keys[0]));
		} else {
			nlohmann::json arr = nlohmann::json::array();
			for (const auto& k : b.keys) arr.push_back(Stringify(k));
			out.push_back(arr);
		}
	}
	return out;
}

nlohmann::json ProfileToJson(const KeyMap& km) {
	nlohmann::json actions = nlohmann::json::object();
	for (int i = 0; i < (int)Action::Count; ++i) {
		const ActionInfo& info = ACTION_TABLE[i];
		nlohmann::json body;
		body["bindings"] = BindingsToJson(km.Get(info.action));
		actions[info.id] = body;
	}
	nlohmann::json out;
	out["name"]    = km.name;
	out["label"]   = km.label;
	out["actions"] = actions;
	return out;
}

// Read either a string or an array-of-strings from JSON into a Binding.
// Used by "put" arg parsing.
bool BindingFromJson(const nlohmann::json& j, Binding& out, std::string& err) {
	out.keys.clear();
	if (j.is_string()) {
		BindingKey k;
		if (!ParseBindingKey(j.get<std::string>(), k)) {
			err = "unrecognized binding: " + j.get<std::string>();
			return false;
		}
		out.keys.push_back(k);
		return true;
	}
	if (j.is_array()) {
		for (const auto& s : j) {
			if (!s.is_string()) { err = "chord entry must be string"; return false; }
			BindingKey k;
			if (!ParseBindingKey(s.get<std::string>(), k)) {
				err = "unrecognized binding: " + s.get<std::string>();
				return false;
			}
			out.keys.push_back(k);
		}
		if (out.keys.empty()) { err = "empty chord"; return false; }
		return true;
	}
	err = "binding must be string or array of strings";
	return false;
}

// Load a profile by name into a fresh KeyMap (not the live one).
bool LoadProfileByName(const std::string& name, KeyMap& out) {
	std::string path = ResolveProfilePath(name);
	if (path.empty()) return false;
	out.Clear();
	if (!out.LoadFile(path)) return false;
	if (out.name.empty()) out.name = name;
	return true;
}

bool BuiltinPathFor(const std::string& name, std::string& out) {
	std::string r = KeybindsResDir();
	if (r.empty()) return false;
	out = r + name + ".json";
	std::ifstream f(out);
	return f.is_open();
}

bool WritablePathExists(const std::string& name) {
	std::string p = WritableProfilePath(name);
	std::ifstream f(p);
	return f.is_open();
}

} // anonymous

static void HandleKeybind(Game& game, ControlCommand& cmd) {
	const std::string subop = cmd.args.value("subop", std::string());
	Config& cfg = Config::GetInstance();
	KeyMap& live = game.GetKeyMap();
	const std::string activeName = cfg.active_keybind_profile;

	if (subop.empty()) {
		cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST", "keybind requires args.subop"));
		return;
	}

	// Profile names flow into filesystem paths via WritableProfilePath /
	// ResolveProfilePath. Reject anything that isn't a plain identifier
	// before we touch disk — without this, a name like "../foo" can escape
	// the keybinds directory (and `delete`'s std::remove() could nuke
	// arbitrary user files). Skip subops that don't take a profile name.
	if (subop != "list" && subop != "actions" && cmd.args.contains("profile")) {
		const std::string profile = cmd.args.value("profile", std::string());
		if (!profile.empty() && !IsValidProfileName(profile)) {
			cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST",
				"invalid profile name (allowed: [A-Za-z0-9_-], 1-64 chars): " + profile));
			return;
		}
	}
	if (cmd.args.contains("from")) {
		const std::string from = cmd.args.value("from", std::string());
		if (!from.empty() && !IsValidProfileName(from)) {
			cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST",
				"invalid source profile name: " + from));
			return;
		}
	}

	// ---- list ---------------------------------------------------------
	if (subop == "list") {
		ProfileListing pl = ListProfiles();
		nlohmann::json r;
		r["active"]   = activeName;
		r["profiles"] = pl.all;
		r["builtins"] = pl.builtins;
		r["writable"] = pl.writable;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}

	// ---- actions ------------------------------------------------------
	if (subop == "actions") {
		// Defaults come from the built-in "default" profile if it exists.
		KeyMap def;
		LoadProfileByName("default", def);
		nlohmann::json arr = nlohmann::json::array();
		for (int i = 0; i < (int)Action::Count; ++i) {
			const ActionInfo& info = ACTION_TABLE[i];
			nlohmann::json e;
			e["id"]      = info.id;
			e["label"]   = info.label;
			e["default"] = BindingsToJson(def.Get(info.action));
			arr.push_back(e);
		}
		cmd.reply->set_value(OkResult(cmd.id, arr));
		return;
	}

	// ---- get ----------------------------------------------------------
	if (subop == "get") {
		std::string profile = cmd.args.value("profile", activeName);
		std::string actionId = cmd.args.value("action",  std::string());
		KeyMap tmp;
		const KeyMap* km = nullptr;
		if (profile == activeName) {
			km = &live;
		} else {
			if (!LoadProfileByName(profile, tmp)) {
				cmd.reply->set_value(Err(cmd.id, "NOT_FOUND", "no such profile: " + profile));
				return;
			}
			km = &tmp;
		}
		if (actionId.empty()) {
			cmd.reply->set_value(OkResult(cmd.id, ProfileToJson(*km)));
			return;
		}
		const ActionInfo* info = FindAction(actionId);
		if (!info) {
			cmd.reply->set_value(Err(cmd.id, "NOT_FOUND", "no such action: " + actionId));
			return;
		}
		nlohmann::json r;
		r["profile"]  = profile;
		r["action"]   = info->id;
		r["label"]    = info->label;
		r["bindings"] = BindingsToJson(km->Get(info->action));
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}

	// ---- put ----------------------------------------------------------
	if (subop == "put") {
		std::string profile  = cmd.args.value("profile", activeName);
		std::string actionId = cmd.args.value("action",  std::string());
		if (actionId.empty()) {
			cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST", "put requires --action"));
			return;
		}
		const ActionInfo* info = FindAction(actionId);
		if (!info) {
			cmd.reply->set_value(Err(cmd.id, "NOT_FOUND", "no such action: " + actionId));
			return;
		}
		if (!cmd.args.contains("bindings") || !cmd.args["bindings"].is_array()) {
			cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST", "put requires --bindings (array)"));
			return;
		}
		// Parse-validate everything BEFORE mutating, so a bad binding never
		// half-applies. Mirrors SSM's atomic put semantics.
		std::vector<Binding> parsed;
		for (const auto& je : cmd.args["bindings"]) {
			Binding b;
			std::string err;
			if (!BindingFromJson(je, b, err)) {
				cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST", err));
				return;
			}
			parsed.push_back(std::move(b));
		}

		// Load (or copy-on-write) the target profile.
		KeyMap edit;
		bool isActive = (profile == activeName);
		if (isActive) {
			edit = live;
		} else if (!LoadProfileByName(profile, edit)) {
			cmd.reply->set_value(Err(cmd.id, "NOT_FOUND", "no such profile: " + profile));
			return;
		}
		edit.Get(info->action).bindings = std::move(parsed);
		std::string path = WritableProfilePath(profile);
		if (!edit.SaveFile(path)) {
			cmd.reply->set_value(Err(cmd.id, "INTERNAL", "failed to save: " + path));
			return;
		}
		if (isActive) live = edit;

		nlohmann::json r;
		r["profile"]  = profile;
		r["action"]   = info->id;
		r["bindings"] = BindingsToJson(edit.Get(info->action));
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}

	// ---- unset --------------------------------------------------------
	// Replace the action's bindings in the writable copy with whatever the
	// built-in profile of the same name has. If no built-in exists, the
	// action becomes empty. Other actions in the writable copy are left alone.
	if (subop == "unset") {
		std::string profile  = cmd.args.value("profile", activeName);
		std::string actionId = cmd.args.value("action",  std::string());
		if (actionId.empty()) {
			cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST", "unset requires --action"));
			return;
		}
		const ActionInfo* info = FindAction(actionId);
		if (!info) {
			cmd.reply->set_value(Err(cmd.id, "NOT_FOUND", "no such action: " + actionId));
			return;
		}
		KeyMap edit;
		bool isActive = (profile == activeName);
		if (isActive) {
			edit = live;
		} else if (!LoadProfileByName(profile, edit)) {
			cmd.reply->set_value(Err(cmd.id, "NOT_FOUND", "no such profile: " + profile));
			return;
		}
		// Look up built-in's value.
		std::string builtinPath;
		if (BuiltinPathFor(profile, builtinPath)) {
			KeyMap builtin;
			if (builtin.LoadFile(builtinPath)) {
				edit.Get(info->action) = builtin.Get(info->action);
			} else {
				edit.Get(info->action).bindings.clear();
			}
		} else {
			edit.Get(info->action).bindings.clear();
		}
		std::string path = WritableProfilePath(profile);
		if (!edit.SaveFile(path)) {
			cmd.reply->set_value(Err(cmd.id, "INTERNAL", "failed to save: " + path));
			return;
		}
		if (isActive) live = edit;

		nlohmann::json r;
		r["profile"]  = profile;
		r["action"]   = info->id;
		r["bindings"] = BindingsToJson(edit.Get(info->action));
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}

	// ---- use ----------------------------------------------------------
	if (subop == "use") {
		std::string profile = cmd.args.value("profile", std::string());
		if (profile.empty()) {
			cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST", "use requires --profile"));
			return;
		}
		std::string path = ResolveProfilePath(profile);
		if (path.empty()) {
			cmd.reply->set_value(Err(cmd.id, "NOT_FOUND", "no such profile: " + profile));
			return;
		}
		std::strncpy(cfg.active_keybind_profile, profile.c_str(),
		             sizeof(cfg.active_keybind_profile) - 1);
		cfg.active_keybind_profile[sizeof(cfg.active_keybind_profile) - 1] = '\0';
		cfg.Save();
		LoadActiveKeymap(game.GetKeyMap());
		nlohmann::json r;
		r["active"] = profile;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}

	// ---- new ----------------------------------------------------------
	if (subop == "new") {
		std::string profile = cmd.args.value("profile", std::string());
		std::string from    = cmd.args.value("from",    std::string());
		if (profile.empty()) {
			cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST", "new requires --profile"));
			return;
		}
		if (WritablePathExists(profile)) {
			cmd.reply->set_value(Err(cmd.id, "ALREADY_EXISTS", profile));
			return;
		}
		KeyMap fresh;
		if (!from.empty()) {
			if (!LoadProfileByName(from, fresh)) {
				cmd.reply->set_value(Err(cmd.id, "NOT_FOUND", "no such source profile: " + from));
				return;
			}
		}
		fresh.name  = profile;
		fresh.label = profile;
		std::string path = WritableProfilePath(profile);
		if (!fresh.SaveFile(path)) {
			cmd.reply->set_value(Err(cmd.id, "INTERNAL", "failed to save: " + path));
			return;
		}
		nlohmann::json r;
		r["profile"] = profile;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}

	// ---- delete -------------------------------------------------------
	if (subop == "delete") {
		std::string profile = cmd.args.value("profile", std::string());
		if (profile.empty()) {
			cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST", "delete requires --profile"));
			return;
		}
		std::string p = WritableProfilePath(profile);
		std::ifstream f(p);
		if (!f.is_open()) {
			cmd.reply->set_value(Err(cmd.id, "READ_ONLY",
				"no writable copy of " + profile + " (built-ins can't be deleted)"));
			return;
		}
		f.close();
		if (std::remove(p.c_str()) != 0) {
			cmd.reply->set_value(Err(cmd.id, "INTERNAL", "could not remove: " + p));
			return;
		}
		// If the active profile lost its writable copy, fall back to whatever
		// resolves now (the built-in if any, else "default") and persist the
		// resolved name so `list` and the next restart agree on what's active.
		if (profile == activeName) {
			LoadActiveKeymap(game.GetKeyMap());
			const std::string& resolved = game.GetKeyMap().name;
			std::strncpy(cfg.active_keybind_profile, resolved.c_str(),
			             sizeof(cfg.active_keybind_profile) - 1);
			cfg.active_keybind_profile[sizeof(cfg.active_keybind_profile) - 1] = '\0';
			cfg.Save();
		}
		nlohmann::json r;
		r["profile"] = profile;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}

	cmd.reply->set_value(Err(cmd.id, "UNKNOWN_OP", "unknown keybind subop: " + subop));
}

// ---------------------------------------------------------------------------
// gas sub-dispatch
//
// `reload` is the only subop. It re-runs GASLoader::Load() against the
// shipped gas dir. State-gated: actor cache invalidation isn't worth the
// risk mid-match (per-instance state in robot.cpp/guard.cpp/civilian.cpp
// caches values from the def at construction), so reload only fires from
// non-INGAME states. Errors round-trip in the same {file, instancePath,
// code, message} shape as shared/gas-validation/errors.ts so the agent's
// remediation loop is platform-agnostic.
// ---------------------------------------------------------------------------

static void HandleGas(Game& game, ControlCommand& cmd) {
	const std::string subop = cmd.args.value("subop", std::string());
	if (subop.empty()) {
		cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST", "gas requires args.subop"));
		return;
	}

	if (subop == "reload") {
		// Hot-reload is unsafe mid-match: actors cached EnemyDef values at
		// construction. Restrict to quiescent states. Game's state enum is
		// private to the class, so compare via the StateName string keys
		// (same approach as wait_for_state).
		const std::string st = Game::StateName(game.GetState());
		const bool safe = (st == "NONE" || st == "MAINMENU" ||
		                   st == "LOBBY" || st == "MISSIONSUMMARY");
		if (!safe) {
			cmd.reply->set_value(Err(cmd.id, "WRONG_STATE",
				"gas reload not safe from state " + st +
				" (allowed: NONE, MAINMENU, LOBBY, MISSIONSUMMARY)"));
			return;
		}

		// On macOS GetResDir() returns "" and the bundle resources are reached
		// via a chdir set up by CDResDir(); other code paths (CDDataDir, replay
		// readers) may have moved the cwd since startup. Re-anchor before the
		// relative "gas" lookup, mirroring resources.cpp::Load.
		CDResDir();
		GASLoader& gas = GASLoader::Get();
		gas.Reload(GetResDir() + "gas");

		nlohmann::json errs = nlohmann::json::array();
		for (const auto& e : gas.lastLoadErrors) {
			errs.push_back({
				{"file",         e.file},
				{"instancePath", e.instancePath},
				{"code",         e.code},
				{"message",      e.message},
			});
		}

		nlohmann::json counts;
		counts["agencies"]    = gas.agencies.size();
		counts["weapons"]     = gas.weapons.size();
		counts["items"]       = gas.items.size();
		counts["enemies"]     = gas.enemies.size();
		counts["abilities"]   = gas.abilities.size();
		counts["gameObjects"] = gas.gameObjects.size();
		counts["terminals"]   = gas.terminals.size();

		nlohmann::json r;
		r["counts"] = counts;
		r["errors"] = errs;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}

	cmd.reply->set_value(Err(cmd.id, "UNKNOWN_OP", "unknown gas subop: " + subop));
}

} // namespace ControlDispatch
