#include "controldispatch.h"
#include "game.h"
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
	cmd.reply->set_value(Err(cmd.id, "UNKNOWN_OP", "unknown op: " + cmd.op));
}

void HandlePostRender(Game& game, ControlCommand& cmd) {
	cmd.reply->set_value(Err(cmd.id, "UNKNOWN_OP", "unknown post-render op: " + cmd.op));
}

} // namespace ControlDispatch
