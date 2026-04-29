#include "controldispatch.h"
#include "game.h"
#include "interface.h"
#include "world.h"
#include "button.h"
#include "toggle.h"
#include "textbox.h"
#include "selectbox.h"
#include "objecttypes.h"
#include "keybinds.h"
#include "config.h"
#include <cstring>
#include <cstdio>
#include <fstream>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir((p), 0755)
#endif

namespace ControlDispatch {

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

static bool IEq(const char* a, const char* b){
	if(!a || !b) return false;
	while(*a && *b){
		if(std::tolower((unsigned char)*a) != std::tolower((unsigned char)*b)) return false;
		++a; ++b;
	}
	return *a == 0 && *b == 0;
}

static ControlReply Err(int id, const char* code, const std::string& msg){
	ControlReply rpl;
	rpl.id = id;
	rpl.ok = false;
	rpl.code = code;
	rpl.error = msg;
	return rpl;
}

// Forward decl for the keybind sub-dispatcher implemented at the bottom of
// this file. Lives in the same TU because it only ever reads/mutates Game's
// KeyMap and Config; no other consumers.
static void HandleKeybind(Game& game, ControlCommand& cmd);

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
	if(cmd.op == "state"){
		nlohmann::json r;
		r["state"] = Game::StateName(game.GetState());
		r["current_interface_id"] = game.GetCurrentInterfaceId();
		r["frame"] = game.GetFrameCount();
		r["paused"] = game.paused;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
	if(cmd.op == "inspect"){
		Uint16 ifid = cmd.args.value("interface_id", 0);
		if(ifid == 0) ifid = game.GetCurrentInterfaceId();
		Interface* iface = (Interface*)game.GetWorld().GetObjectFromId(ifid);
		if(!iface || iface->type != ObjectTypes::INTERFACE){
			cmd.reply->set_value(Err(cmd.id, "WRONG_STATE", "no current interface"));
			return;
		}
		nlohmann::json widgets = nlohmann::json::array();
		for(Uint16 oid : iface->objects){
			Object* o = game.GetWorld().GetObjectFromId(oid);
			if(!o) continue;
			nlohmann::json w;
			w["id"] = oid;
			w["x"] = o->x; w["y"] = o->y;
			switch(o->type){
				case ObjectTypes::BUTTON: {
					Button* b = (Button*)o;
					w["kind"] = "button";
					w["label"] = b->text;
					w["w"] = b->width; w["h"] = b->height;
					w["enabled"] = !iface->disabled;
					break;
				}
				case ObjectTypes::TOGGLE: {
					Toggle* t = (Toggle*)o;
					w["kind"] = "toggle";
					w["label"] = t->text;
					w["w"] = t->width; w["h"] = t->height;
					w["enabled"] = !iface->disabled;
					w["selected"] = t->selected;
					break;
				}
				case ObjectTypes::TEXTBOX: {
					TextBox* tb = (TextBox*)o;
					w["kind"] = "textbox";
					w["w"] = tb->width; w["h"] = tb->height;
					// uid is the developer-assigned identifier; expose it so
					// agents can disambiguate textboxes (which have no label).
					w["uid"] = tb->uid;
					break;
				}
				case ObjectTypes::SELECTBOX: {
					SelectBox* sb = (SelectBox*)o;
					w["kind"] = "selectbox";
					w["w"] = sb->width; w["h"] = sb->height;
					w["selected_index"] = sb->selecteditem;
					w["uid"] = sb->uid;
					break;
				}
				default:
					w["kind"] = "other";
					w["object_type"] = o->type;
					break;
			}
			widgets.push_back(std::move(w));
		}
		nlohmann::json r;
		r["widgets"] = widgets;
		r["interface_id"] = ifid;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
	if(cmd.op == "world_state"){
		cmd.reply->set_value(OkResult(cmd.id, game.GetWorldSummary()));
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
		Uint16 ifid = game.GetCurrentInterfaceId();
		Interface* iface = (Interface*)game.GetWorld().GetObjectFromId(ifid);
		if(!iface){
			cmd.reply->set_value(Err(cmd.id, "WRONG_STATE", "no current interface"));
			return;
		}
		Uint64 mask = (1ULL << ObjectTypes::BUTTON) | (1ULL << ObjectTypes::TOGGLE);
		Uint16 wid = 0;
		auto m = iface->FindWidgetByLabel(game.GetWorld(), target.c_str(), mask, &wid);
		if(m == Interface::MATCH_NOT_FOUND){
			cmd.reply->set_value(Err(cmd.id, "WIDGET_NOT_FOUND",
				"no widget matches \"" + target + "\""));
			return;
		}
		if(m == Interface::MATCH_AMBIGUOUS){
			cmd.reply->set_value(Err(cmd.id, "WIDGET_AMBIGUOUS",
				"multiple widgets match \"" + target + "\""));
			return;
		}
		Object* o = game.GetWorld().GetObjectFromId(wid);
		if(o->type == ObjectTypes::BUTTON){
			Button* b = (Button*)o;
			b->Activate(); // existing behavior used by the main menu
			b->clicked = true;
		} else if(o->type == ObjectTypes::TOGGLE){
			Toggle* t = (Toggle*)o;
			t->selected = !t->selected;
		}
		nlohmann::json r;
		r["widget_id"] = wid;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
	if(cmd.op == "set_text"){
		std::string target = cmd.args.value("label", std::string());
		std::string text   = cmd.args.value("text", std::string());
		Uint16 ifid = game.GetCurrentInterfaceId();
		Interface* iface = (Interface*)game.GetWorld().GetObjectFromId(ifid);
		if(!iface){ cmd.reply->set_value(Err(cmd.id, "WRONG_STATE", "no interface")); return; }
		Uint64 mask = (1ULL << ObjectTypes::TEXTBOX);
		Uint16 wid = 0;
		auto m = iface->FindWidgetByLabel(game.GetWorld(), target.c_str(), mask, &wid);
		if(m != Interface::MATCH_OK){
			cmd.reply->set_value(Err(cmd.id, m == Interface::MATCH_NOT_FOUND
				? "WIDGET_NOT_FOUND" : "WIDGET_AMBIGUOUS", target));
			return;
		}
		TextBox* tb = (TextBox*)game.GetWorld().GetObjectFromId(wid);
		tb->text.clear();
		tb->AddText(text.c_str());
		cmd.reply->set_value(OkResult(cmd.id, nlohmann::json::object()));
		return;
	}

	if(cmd.op == "select"){
		std::string target = cmd.args.value("label", std::string());
		int idx = cmd.args.value("index", -1);
		std::string text = cmd.args.value("text", std::string());
		Uint16 ifid = game.GetCurrentInterfaceId();
		Interface* iface = (Interface*)game.GetWorld().GetObjectFromId(ifid);
		if(!iface){ cmd.reply->set_value(Err(cmd.id, "WRONG_STATE", "no interface")); return; }
		Uint64 mask = (1ULL << ObjectTypes::SELECTBOX);
		Uint16 wid = 0;
		auto m = iface->FindWidgetByLabel(game.GetWorld(), target.c_str(), mask, &wid);
		if(m == Interface::MATCH_NOT_FOUND){
			cmd.reply->set_value(Err(cmd.id, "WIDGET_NOT_FOUND",
				"no selectbox matches \"" + target + "\""));
			return;
		}
		if(m == Interface::MATCH_AMBIGUOUS){
			cmd.reply->set_value(Err(cmd.id, "WIDGET_AMBIGUOUS",
				"multiple selectboxes match \"" + target + "\""));
			return;
		}
		SelectBox* sb = (SelectBox*)game.GetWorld().GetObjectFromId(wid);
		if(idx < 0 && !text.empty()){
			for(unsigned int i = 0; i < sb->items.size(); ++i){
				if(sb->items[i] && IEq(sb->items[i], text.c_str())){ idx = (int)i; break; }
			}
		}
		if(idx < 0 || (unsigned)idx >= sb->items.size()){
			cmd.reply->set_value(Err(cmd.id, "WIDGET_NOT_FOUND", "no such item"));
			return;
		}
		sb->selecteditem = idx;
		cmd.reply->set_value(OkResult(cmd.id, nlohmann::json::object()));
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
		game.LoadActiveKeymap();
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
			game.LoadActiveKeymap();
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

} // namespace ControlDispatch
