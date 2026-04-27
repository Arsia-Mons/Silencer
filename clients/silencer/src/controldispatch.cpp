#include "controldispatch.h"
#include "game.h"
#include "interface.h"
#include "world.h"
#include "button.h"
#include "toggle.h"
#include "textbox.h"
#include "selectbox.h"
#include "objecttypes.h"
#include <cstring>
#include <cstdio>
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

} // namespace ControlDispatch
