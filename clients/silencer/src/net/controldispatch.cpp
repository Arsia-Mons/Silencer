#include "controldispatch.h"
#include "game.h"
#include "world.h"
#include "keybinds.h"
#include "config.h"
#include "gasloader.h"
#include "os.h"
#include "shared.h"
#include "client/ui/app_shell/client_ui.h"
#include "ui/game_ui_pipeline.h"
#include "ui/world_session_model.h"
#include "ui/input.h"
#include "ui/runtime/focus.h"
#include "ui/runtime/tree.h"
#include <SDL3/SDL_keyboard.h>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <string>
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
// A handful of list-oriented UI ops (select/scroll/hover_at) are bound to
// screens that land in later slices (Options/lobby lists, SIL-19/20). They
// return a structured UNSUPPORTED error until then. inspect/click/set_text/key
// and the modal ops are live against the retained cppx tree (SIL-18).
const char * const kUiUnsupportedMsg =
	"UI op unavailable on cppx path yet (pending later screen slices)";

// --- cppx UI introspection + automation (SIL-18) -------------------------
// Read-only walks of the retained UiTree (client::ui::ClientUi) + a small
// injection seam through GameUiPipeline. No friend grant, no handle leak: the
// control socket sees node snapshots and pushes UiInputFrame edges only.

const char * NodeRoleName(::ui::NodeRole role){
	switch(role){
		case ::ui::NodeRole::Box: return "box";
		case ::ui::NodeRole::Text: return "text";
		case ::ui::NodeRole::Button: return "button";
		case ::ui::NodeRole::Input: return "input";
		case ::ui::NodeRole::Checkbox: return "checkbox";
		case ::ui::NodeRole::Dialog: return "dialog";
		case ::ui::NodeRole::Generic: return "generic";
	}
	return "generic";
}

// A node matches a name if its control id OR accessibility label OR text value
// equals it (control id first — it's the stable automation handle).
bool NodeMatchesName(const ::ui::NodeSnapshot& s, const std::string& name){
	if(s.control_id && name == s.control_id) return true;
	if(s.accessibility_label && name == s.accessibility_label) return true;
	if(s.value && name == s.value) return true;
	return false;
}

// First node (pre-order) matching `name`. When focusableOnly, a non-focusable
// match (e.g. a label text node) never shadows the focusable control carrying
// the same label.
::ui::NodeId FindUiNode(const ::ui::UiTree& tree, ::ui::NodeId id,
                        const std::string& name, bool focusableOnly,
                        ::ui::NodeSnapshot* out){
	::ui::NodeSnapshot s = {};
	if(tree.snapshot(id, &s)){
		if((!focusableOnly || s.interaction.focusable) && NodeMatchesName(s, name)){
			if(out) *out = s;
			return id;
		}
	}
	for(int i = 0; i < tree.child_count(id); ++i){
		::ui::NodeId found = FindUiNode(tree, tree.child_at(id, i), name,
		                                focusableOnly, out);
		if(found) return found;
	}
	return 0;
}

void CollectUiNodes(const ::ui::UiTree& tree, ::ui::NodeId id,
                    ::ui::NodeId focused, ::ui::NodeId hovered,
                    nlohmann::json& out){
	::ui::NodeSnapshot s = {};
	if(tree.snapshot(id, &s)){
		nlohmann::json n;
		n["id"] = static_cast<uint64_t>(id);
		n["type"] = s.type ? s.type : "";
		n["role"] = NodeRoleName(s.role);
		if(s.control_id && s.control_id[0]) n["control_id"] = s.control_id;
		if(s.accessibility_label && s.accessibility_label[0]) n["label"] = s.accessibility_label;
		if(s.value && s.value[0]) n["value"] = s.value;
		n["focusable"] = s.interaction.focusable;
		n["disabled"] = s.interaction.disabled;
		n["checked"] = s.interaction.checked;
		n["focused"] = (focused != 0 && id == focused);
		n["hovered"] = (hovered != 0 && id == hovered);
		n["x"] = s.layout.x;
		n["y"] = s.layout.y;
		n["w"] = s.layout.width;
		n["h"] = s.layout.height;
		out.push_back(std::move(n));
	}
	for(int i = 0; i < tree.child_count(id); ++i){
		CollectUiNodes(tree, tree.child_at(id, i), focused, hovered, out);
	}
}

// Translate a `key` op value into per-frame UiInputFrame edges. Single-character
// values are typed text (routed to the focused field); named keys map to nav /
// confirm / cancel / editing keys.
void InjectKeyOp(::ui::UiInputFrame& ui, const std::string& key){
	ui.source = ::ui::UiFocusSource::Keyboard;
	if(key.size() == 1){
		char buf[2] = {key[0], '\0'};
		::ui::ui_input_push_text(ui, buf);
		return;
	}
	if(key == "enter" || key == "return"){
		ui.confirm_pressed = true;
		ui.confirm_down = true;
	}else if(key == "space"){
		::ui::ui_input_push_text(ui, " ");
	}else if(key == "escape" || key == "esc"){
		ui.cancel_pressed = true;
		ui.cancel_down = true;
	}else if(key == "up"){ ui.nav_up = true;
	}else if(key == "down" || key == "tab"){ ui.nav_down = true;
	}else if(key == "left"){ ui.nav_left = true;
	}else if(key == "right"){ ui.nav_right = true;
	}else if(key == "backspace"){
		::ui::ui_input_push_key(ui, ::ui::UiKey::Backspace);
	}else if(key == "delete"){
		::ui::ui_input_push_key(ui, ::ui::UiKey::DeleteForward);
	}
}
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

static ControlReply Err(int id, const char* code, const std::string& msg){
	ControlReply rpl;
	rpl.id = id;
	rpl.ok = false;
	rpl.code = code;
	rpl.error = msg;
	return rpl;
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
	r["quit_state"] = summary.quitState;
	r["show_team_colors"] = summary.showTeamColors;
	return r;
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
	// SIL-23: the legacy clay_*_check probes are retired — the cppx-primitive
	// ops (inspect / click / set_text over the retained UiTree) are the
	// supported automation checks. Removed ops fall through to UNKNOWN_OP.
	if(cmd.op == "state"){
		nlohmann::json r;
		r["state"] = Game::StateName(game.GetState());
		r["frame"] = game.GetFrameCount();
		r["paused"] = game.paused;
		// Expose the lobby connection sub-state so test scripts can wait for
		// AUTHENTICATING before dispatching a Login/Create click — the LobbyConnect
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
	if(cmd.op == "inspect"){
		client::ui::ClientUi * ui = game.GetUiPipeline().TryClientUi();
		if(!ui){
			cmd.reply->set_value(Err(cmd.id, "WRONG_STATE",
				"cppx UI has not rendered a frame yet"));
			return;
		}
		const ::ui::UiTree & tree = ui->retained_tree();
		::ui::NodeId focused = ::ui::focus_focused_id(ui->retained_focus());
		::ui::NodeId hovered = ::ui::focus_hovered_id(ui->retained_focus());
		nlohmann::json nodes = nlohmann::json::array();
		if(tree.contains(tree.root_id())){
			CollectUiNodes(tree, tree.root_id(), focused, hovered, nodes);
		}
		nlohmann::json r;
		r["nodes"] = std::move(nodes);
		r["focused_id"] = static_cast<uint64_t>(focused);
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
	if(cmd.op == "world_state"){
		cmd.reply->set_value(OkResult(cmd.id, WorldSummaryToJson(game.GetWorldSummary())));
		return;
	}
	if(cmd.op == "show_password_modal"){
		if(!game.GetUiPipeline().TryClientUi()){
			cmd.reply->set_value(Err(cmd.id, "WRONG_STATE",
				"cppx UI has not rendered a frame yet"));
			return;
		}
		std::string title = cmd.args.value("title", std::string("Password"));
		game.GetUiPipeline().ShowPasswordModal(title.c_str());
		cmd.reply->set_value(OkResult(cmd.id, nlohmann::json::object()));
		return;
	}
	if(cmd.op == "password_modal_result"){
		const GameUiPipeline::PasswordModalResult & res = game.GetUiPipeline().PasswordModal();
		nlohmann::json r;
		r["open"] = res.open;
		r["submitted"] = res.submitted;
		r["value"] = res.value;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
	if(cmd.op == "show_message_modal"){
		if(!game.GetUiPipeline().TryClientUi()){
			cmd.reply->set_value(Err(cmd.id, "WRONG_STATE",
				"cppx UI has not rendered a frame yet"));
			return;
		}
		std::string title = cmd.args.value("title", std::string("Message"));
		std::string message = cmd.args.value("message", std::string());
		game.GetUiPipeline().ShowMessageModal(title.c_str(), message.c_str());
		cmd.reply->set_value(OkResult(cmd.id, nlohmann::json::object()));
		return;
	}
	if(cmd.op == "ui_gallery"){
		if(!game.GetUiPipeline().TryClientUi()){
			cmd.reply->set_value(Err(cmd.id, "WRONG_STATE",
				"cppx UI has not rendered a frame yet"));
			return;
		}
		// SIL-24: push the design-system gallery overlay so the visual-regression
		// suite can golden every component variant in isolation. Pop via `back`.
		game.GetUiPipeline().ShowGallery();
		cmd.reply->set_value(OkResult(cmd.id, nlohmann::json::object()));
		return;
	}
	if(cmd.op == "ui_audio"){
		// Interaction-sound edge counter (hover-enter/activate/nav on audible
		// buttons) — observable headless where Audio itself is disabled.
		nlohmann::json j;
		j["clicks"] = game.GetUiPipeline().UiClickCount();
		cmd.reply->set_value(OkResult(cmd.id, j));
		return;
	}
	if(cmd.op == "rain"){
		// Capture plumbing: disable the (rand-driven, wall-clock) rain layer so
		// in-game renders are deterministic against the origin goldens.
		game.GetRenderer().rainDisabled = !cmd.args.value("enabled", 0);
		nlohmann::json j;
		j["enabled"] = !game.GetRenderer().rainDisabled;
		cmd.reply->set_value(OkResult(cmd.id, j));
		return;
	}
	if(cmd.op == "camera"){
		// Test/capture plumbing: read or pin the world camera. The in-game
		// camera's follow window has a 100px y-hysteresis (Camera::Follow
		// h=100/yoffset=30), so its rest position depends on render cadence
		// during the spawn fall — capture scripts pin it to the golden's.
		Camera & cam = game.GetRenderer().camera;
		if(cmd.args.contains("x") && cmd.args.contains("y")){
			cam.SetPosition((Sint16)cmd.args.value("x", 0),
			                (Sint16)cmd.args.value("y", 0));
		}
		nlohmann::json j;
		j["x"] = cam.x;
		j["y"] = cam.y;
		cmd.reply->set_value(OkResult(cmd.id, j));
		return;
	}
	if(cmd.op == "ingame_ui_mode"){
		std::string mode = cmd.args.value("mode", std::string("status"));
		namespace gu = silencer::game_ui;
		gu::InGameUiMode m;
		if(mode == "clear")            m = gu::InGameUiMode::Clear;
		else if(mode == "chat")        m = gu::InGameUiMode::Chat;
		else if(mode == "buy")         m = gu::InGameUiMode::Buy;
		else if(mode == "tech")        m = gu::InGameUiMode::Tech;
		else if(mode == "playerlist")  m = gu::InGameUiMode::PlayerList;
		else if(mode == "all")         m = gu::InGameUiMode::All;
		else                           m = gu::InGameUiMode::Status;
		gu::InGameUiChatSeed seed;
		seed.text = cmd.args.value("chat_text", std::string());
		seed.line = cmd.args.value("chat_line", std::string());
		gu::InGameUiControlResult r = gu::ConfigureInGameUi(game, m, seed);
		if(!r.available){
			cmd.reply->set_value(Err(cmd.id, "WRONG_STATE",
				r.error.empty() ? std::string("not in a match") : r.error));
			return;
		}
		nlohmann::json j;
		j["ok"] = r.available;
		j["mode"] = mode;
		j["chat_active"] = r.chat_active;
		j["chat_with_team"] = r.chat_with_team;
		j["chat_line_count"] = r.chat_line_count;
		j["chat_text_len"] = r.chat_text_len;
		j["buy_active"] = r.buy_active;
		j["tech_active"] = r.tech_active;
		j["show_chat_ticks"] = r.show_chat_ticks;
		j["show_player_list"] = r.show_player_list;
		j["buy_item_count"] = r.buy_item_count;
		j["tech_item_count"] = r.tech_item_count;
		j["buy_selected_index"] = r.buy_selected_index;
		j["tech_selected_index"] = r.tech_selected_index;
		cmd.reply->set_value(OkResult(cmd.id, j));
		return;
	}
	if(cmd.op == "click"){
		client::ui::ClientUi * ui = game.GetUiPipeline().TryClientUi();
		if(!ui){
			cmd.reply->set_value(Err(cmd.id, "WRONG_STATE",
				"cppx UI has not rendered a frame yet"));
			return;
		}
		std::string label = cmd.args.value("label", std::string());
		if(label.empty()){
			cmd.reply->set_value(Err(cmd.id, "BAD_ARGS", "click requires --label"));
			return;
		}
		const ::ui::UiTree & tree = ui->retained_tree();
		::ui::NodeSnapshot s = {};
		::ui::NodeId node = FindUiNode(tree, tree.root_id(), label, true, &s);
		if(!node){
			cmd.reply->set_value(Err(cmd.id, "NOT_FOUND", "no focusable widget: " + label));
			return;
		}
		// Activate by location: a single-frame pointer press+release at the
		// node center drives the real focus/hit-test path next render.
		game.GetUiPipeline().InjectPointerClick(s.layout.x + s.layout.width * 0.5f,
		                                        s.layout.y + s.layout.height * 0.5f);
		nlohmann::json r;
		r["widget_id"] = static_cast<uint64_t>(node);
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
	if(cmd.op == "click_at"){
		if(!game.GetUiPipeline().TryClientUi()){
			cmd.reply->set_value(Err(cmd.id, "WRONG_STATE",
				"cppx UI has not rendered a frame yet"));
			return;
		}
		float x = cmd.args.value("x", -1.0f);
		float y = cmd.args.value("y", -1.0f);
		if(x < 0.0f || y < 0.0f){
			cmd.reply->set_value(Err(cmd.id, "BAD_ARGS", "click_at requires --x --y"));
			return;
		}
		game.GetUiPipeline().InjectPointerClick(x, y);
		cmd.reply->set_value(OkResult(cmd.id, nlohmann::json::object()));
		return;
	}
	if(cmd.op == "set_text"){
		client::ui::ClientUi * ui = game.GetUiPipeline().TryClientUi();
		if(!ui){
			cmd.reply->set_value(Err(cmd.id, "WRONG_STATE",
				"cppx UI has not rendered a frame yet"));
			return;
		}
		std::string label = cmd.args.value("label", std::string());
		std::string text = cmd.args.value("text", std::string());
		if(label.empty()){
			cmd.reply->set_value(Err(cmd.id, "BAD_ARGS", "set_text requires --label"));
			return;
		}
		const ::ui::UiTree & tree = ui->retained_tree();
		::ui::NodeSnapshot s = {};
		::ui::NodeId node = FindUiNode(tree, tree.root_id(), label, true, &s);
		if(!node){
			cmd.reply->set_value(Err(cmd.id, "NOT_FOUND", "no text field: " + label));
			return;
		}
		// Focus the field (click) and deliver the text in the same frame: the
		// focus pass runs before text dispatch, so the field receives the insert.
		game.GetUiPipeline().InjectPointerClick(s.layout.x + s.layout.width * 0.5f,
		                                        s.layout.y + s.layout.height * 0.5f);
		::ui::ui_input_push_text(game.GetUiPipeline().UiInput(), text.c_str());
		cmd.reply->set_value(OkResult(cmd.id, nlohmann::json::object()));
		return;
	}
	if(cmd.op == "hover_at"){
		if(!game.GetUiPipeline().TryClientUi()){
			cmd.reply->set_value(Err(cmd.id, "WRONG_STATE",
				"cppx UI has not rendered a frame yet"));
			return;
		}
		float x = cmd.args.value("x", -1.0f);
		float y = cmd.args.value("y", -1.0f);
		if(x < 0.0f || y < 0.0f){
			cmd.reply->set_value(Err(cmd.id, "BAD_ARGS", "hover_at requires --x --y"));
			return;
		}
		// Park a sticky pointer at (x,y); focus-follows-hover tracks it next render.
		game.GetUiPipeline().InjectPointerMove(x, y);
		cmd.reply->set_value(OkResult(cmd.id, nlohmann::json::object()));
		return;
	}
	if(cmd.op == "scroll"){
		// SIL-111: inject a scroll-wheel delta. Optionally park the pointer at
		// (x,y) first so the runtime routes the wheel to that scrollable (wheel
		// goes to the hovered node, mirroring a real mouse). +dy = wheel up.
		if(!game.GetUiPipeline().TryClientUi()){
			cmd.reply->set_value(Err(cmd.id, "WRONG_STATE",
				"cppx UI has not rendered a frame yet"));
			return;
		}
		if(cmd.args.contains("x") && cmd.args.contains("y")){
			float x = cmd.args.value("x", 0.0f);
			float y = cmd.args.value("y", 0.0f);
			game.GetUiPipeline().InjectPointerMove(x, y);
		}
		::ui::UiInputFrame& ui = game.GetUiPipeline().UiInput();
		ui.wheel_x += cmd.args.value("dx", 0.0f);
		ui.wheel_y += cmd.args.value("dy", 0.0f);
		ui.source = ::ui::UiFocusSource::Mouse;
		cmd.reply->set_value(OkResult(cmd.id, nlohmann::json::object()));
		return;
	}
	if(cmd.op == "select"){
		cmd.reply->set_value(Err(cmd.id, "UNSUPPORTED", kUiUnsupportedMsg));
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
	if(cmd.op == "key"){
		if(!game.GetUiPipeline().TryClientUi()){
			cmd.reply->set_value(Err(cmd.id, "WRONG_STATE",
				"cppx UI has not rendered a frame yet"));
			return;
		}
		std::string key = cmd.args.value("key", std::string());
		if(key.empty()){
			cmd.reply->set_value(Err(cmd.id, "BAD_ARGS", "key requires --key"));
			return;
		}
		InjectKeyOp(game.GetUiPipeline().UiInput(), key);
		cmd.reply->set_value(OkResult(cmd.id, nlohmann::json::object()));
		return;
	}
	if(cmd.op == "lobby_show_panel"){
		cmd.reply->set_value(Err(cmd.id, "UNSUPPORTED", kUiUnsupportedMsg));
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
			if(w.wait_state == Game::StateName(game.GetState())){
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
		bool ok = game.CaptureCompositedFrame(out.c_str());
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
		if ((int)out.keys.size() > CHORD_CAP) {
			err = "chord exceeds " + std::to_string(CHORD_CAP) + " keys";
			return false;
		}
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
		if ((int)parsed.size() > COMBO_CAP) {
			cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST",
				"too many combos for one action (max " + std::to_string(COMBO_CAP) + ")"));
			return;
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

	// ---- capture ------------------------------------------------------
	// Drives the multi-device keybind-capture state machine (SIL-19 §7b) without
	// SDL: begin → feed (one device edge per call, as a "KEY:..|MOUSE:..|PAD:.."
	// string) → confirm/cancel. The same state machine the windowed event path
	// feeds, so this exercises the real capture → use_key_map commit path.
	if (subop == "capture") {
		const std::string op = cmd.args.value("op", std::string());
		GameUiPipeline& pipe = game.GetUiPipeline();
		if (op == "begin") {
			const std::string actionId = cmd.args.value("action", std::string());
			const ActionInfo* info = FindAction(actionId);
			if (!info) {
				cmd.reply->set_value(Err(cmd.id, "NOT_FOUND", "no such action: " + actionId));
				return;
			}
			int combo = cmd.args.value("combo", -1);
			pipe.BeginKeybindCapture(info->action, combo);
			nlohmann::json r;
			r["action"] = info->id;
			cmd.reply->set_value(OkResult(cmd.id, r));
			return;
		}
		if (op == "feed") {
			const std::string binding = cmd.args.value("binding", std::string());
			BindingKey bk;
			if (!ParseBindingKey(binding, bk)) {
				cmd.reply->set_value(Err(cmd.id, "BAD_ARGS", "unrecognized binding: " + binding));
				return;
			}
			bool added = pipe.FeedKeybindEdge(bk);
			nlohmann::json r;
			r["added"] = added;
			cmd.reply->set_value(OkResult(cmd.id, r));
			return;
		}
		if (op == "confirm") {
			pipe.ConfirmKeybindChord();
			cmd.reply->set_value(OkResult(cmd.id, nlohmann::json::object()));
			return;
		}
		if (op == "cancel") {
			pipe.CancelKeybindCapture();
			cmd.reply->set_value(OkResult(cmd.id, nlohmann::json::object()));
			return;
		}
		if (op == "status") {
			nlohmann::json pend = nlohmann::json::array();
			for (const BindingKey& k : pipe.KeybindCapturePending()) pend.push_back(Stringify(k));
			nlohmann::json r;
			r["capturing"] = pipe.IsCapturingKeybind();
			r["pending"] = pend;
			cmd.reply->set_value(OkResult(cmd.id, r));
			return;
		}
		cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST",
			"keybind capture requires --op begin|feed|confirm|cancel|status"));
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
