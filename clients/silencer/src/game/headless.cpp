#include "game.h"
#include "controldispatch.h"
#include "world.h"

void Game::DrainControlQueue(){
	if(controlPort <= 0) return;
	auto cmds = controlserver.DrainImmediate();
	for(auto& c : cmds){
		if(c.phase == ControlCommand::MULTI_FRAME){
			ControlDispatch::EnqueueWait(*this, std::move(c));
		} else {
			ControlDispatch::HandleImmediate(*this, c);
		}
	}
	// TickWaits intentionally NOT called here — it runs after the sim while-loop
	// in Loop() so the very first tick after enqueue counts.
	// Dedicated server has no rendering and never calls PostFrameReplies(), so
	// any POST_RENDER op (e.g. screenshot) would block its handler thread
	// forever. Fail them at receive time.
	if(world.dedicatedserver.active){
		auto pr = controlserver.DrainPostRender();
		for(auto& c : pr){
			if(!c.reply) continue;
			ControlReply rpl;
			rpl.id = c.id;
			rpl.ok = false;
			rpl.code = "WRONG_STATE";
			rpl.error = "post-render ops not supported in dedicated server mode";
			c.reply->set_value(rpl);
		}
	}
}

void Game::PostFrameReplies(){
	if(controlPort <= 0) return;
	auto cmds = controlserver.DrainPostRender();
	for(auto& c : cmds){
		ControlDispatch::HandlePostRender(*this, c);
	}
}
