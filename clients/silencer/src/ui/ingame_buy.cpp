#include "ingame_buy.h"

#include "audio.h"
#include "gasloader.h"
#include "player.h"
#include "team.h"
#include "world.h"

#include <SDL3/SDL_scancode.h>
#include <vector>

namespace ui { namespace v2 {

namespace {

// Mirrors Player::Tick's filter that legacy used when populating the buy
// SelectBox: world.buyableitems filtered by Player::BuyAvailable. Rebuilt
// each frame (renderer + dispatch). Cheap — buyableitems is small and the
// availability check is a single tech-bitmask test.
std::vector<int> BuildBuyableIndexList(World & world, Player & player){
	std::vector<int> out;
	out.reserve(world.buyableitems.size());
	for(size_t i = 0; i < world.buyableitems.size(); ++i){
		BuyableItem * item = world.buyableitems[i];
		if(!item) continue;
		// Player::BuyAvailable is private; replicate its logic here. The
		// active local player is what we care about, and the same access
		// gating applies (peer techchoices + team agency).
		Peer * peer = player.GetPeer(world);
		if(!peer) continue;
		if(!(peer->techchoices & item->techchoice)) continue;
		Team * team = player.GetTeam(world);
		if(item->agencyspecific != -1 && (!team || item->agencyspecific != team->agency)) continue;
		out.push_back((int)i);
	}
	return out;
}

}  // namespace

IngameBuy::IngameBuy(World & world)
	: world_(world) {}

bool IngameBuy::Active() const {
	Player * p = world_.GetPeerPlayer(world_.localpeerid);
	return p && p->isbuying;
}

void IngameBuy::Activate(){
	// Legacy plays this sound at the moment the buy Interface is created
	// (player.cpp Player::Tick). We fire it on the isbuying↑ edge.
	Audio::GetInstance().Play(world_.resources.soundbank[GASLoader::Get().player.soundMenuSelect], 96);
}

void IngameBuy::MoveSelection(int delta){
	Player * p = world_.GetPeerPlayer(world_.localpeerid);
	if(!p) return;
	std::vector<int> visible = BuildBuyableIndexList(world_, *p);
	if(visible.empty()) return;
	int sel = p->buyifacelastitem + delta;
	if(sel < 0) sel = 0;
	if(sel > (int)visible.size() - 1) sel = (int)visible.size() - 1;
	if(sel == p->buyifacelastitem) return;
	p->buyifacelastitem = sel;
	// Auto-scroll window (5 visible rows — legacy renderer caps `line` at 5).
	if(p->buyifacelastitem >= p->buyifacelastscrolled + 5){
		p->buyifacelastscrolled = p->buyifacelastitem - 4;
	}
	if(p->buyifacelastitem < p->buyifacelastscrolled){
		p->buyifacelastscrolled = p->buyifacelastitem;
	}
	// Selection-change audio.
	Audio::GetInstance().Play(world_.resources.soundbank[GASLoader::Get().player.soundRoundCountdown], 64);
}

void IngameBuy::Submit(){
	Player * p = world_.GetPeerPlayer(world_.localpeerid);
	if(!p) return;
	std::vector<int> visible = BuildBuyableIndexList(world_, *p);
	if(visible.empty()) return;
	int sel = p->buyifacelastitem;
	if(sel < 0 || sel >= (int)visible.size()) return;
	BuyableItem * item = world_.buyableitems[visible[sel]];
	if(!item) return;
	p->BuyItem(world_, item->id);
}

bool IngameBuy::DispatchKey(int sdl_scancode){
	if(!Active()) return false;
	switch(sdl_scancode){
		case SDL_SCANCODE_UP:     MoveSelection(-1); return true;
		case SDL_SCANCODE_DOWN:   MoveSelection(+1); return true;
		case SDL_SCANCODE_RETURN: Submit();          return true;
		default: break;
	}
	// Swallow everything else so movement/fire keys don't leak while the
	// menu is up.
	return true;
}

}}  // namespace ui::v2
