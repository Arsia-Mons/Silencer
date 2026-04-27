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
					break;
				}
				case ObjectTypes::SELECTBOX: {
					SelectBox* sb = (SelectBox*)o;
					w["kind"] = "selectbox";
					w["w"] = sb->width; w["h"] = sb->height;
					w["selected_index"] = sb->selecteditem;
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
	cmd.reply->set_value(Err(cmd.id, "UNKNOWN_OP", "unknown op: " + cmd.op));
}

void HandlePostRender(Game& game, ControlCommand& cmd) {
	cmd.reply->set_value(Err(cmd.id, "UNKNOWN_OP", "unknown post-render op: " + cmd.op));
}

} // namespace ControlDispatch
