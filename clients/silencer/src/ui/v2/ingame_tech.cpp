#include "ingame_tech.h"

#include "audio.h"
#include "gasloader.h"
#include "player.h"
#include "team.h"
#include "world.h"

#include <SDL3/SDL_scancode.h>
#include <vector>

namespace ui { namespace v2 {

namespace {

// Mirrors Player::Tick's filter that legacy used when populating the tech
// SelectBox (player.cpp:541-549): when InOwnBase, items whose techchoice
// overlaps team->disabledtech (downed/can-repair); else, items whose
// techchoice overlaps the other team's GetAvailableTech (can-virus).
// Rebuilt each frame — buyableitems is small, the bitmask test is one op.
std::vector<int> BuildTechIndexList(World & world, Player & player){
	std::vector<int> out;
	out.reserve(world.buyableitems.size());
	const bool in_own_base = player.InOwnBase(world);
	Team * own_team = player.GetTeam(world);
	Team * other_team = in_own_base ? nullptr : player.TeamOfCurrentBase(world);
	for(size_t i = 0; i < world.buyableitems.size(); ++i){
		BuyableItem * item = world.buyableitems[i];
		if(!item) continue;
		if(in_own_base){
			if(own_team && (item->techchoice & own_team->disabledtech)){
				out.push_back((int)i);
			}
		}else{
			if(other_team && (item->techchoice & other_team->GetAvailableTech(world))){
				out.push_back((int)i);
			}
		}
	}
	return out;
}

}  // namespace

IngameTech::IngameTech(World & world)
	: world_(world) {}

bool IngameTech::Active() const {
	Player * p = world_.GetPeerPlayer(world_.localpeerid);
	return p && p->techstationactive;
}

void IngameTech::Activate(){
	// Legacy plays soundMenuSelect at the moment the tech Interface is
	// created (player.cpp Player::Tick). Fire it on the techstationactive↑
	// edge. Mirror legacy's `selectbox->selecteditem = 0` by resetting the
	// cursor — tech menu doesn't preserve position across opens.
	Player * p = world_.GetPeerPlayer(world_.localpeerid);
	if(p){
		p->techlastitem = 0;
		p->techlastscrolled = 0;
	}
	Audio::GetInstance().Play(world_.resources.soundbank[GASLoader::Get().player.soundMenuSelect], 96);
}

void IngameTech::MoveSelection(int delta){
	Player * p = world_.GetPeerPlayer(world_.localpeerid);
	if(!p) return;
	std::vector<int> visible = BuildTechIndexList(world_, *p);
	if(visible.empty()) return;
	int sel = p->techlastitem + delta;
	if(sel < 0) sel = 0;
	if(sel > (int)visible.size() - 1) sel = (int)visible.size() - 1;
	if(sel == p->techlastitem) return;
	p->techlastitem = sel;
	// Auto-scroll window (5 visible rows — same as buy menu).
	if(p->techlastitem >= p->techlastscrolled + 5){
		p->techlastscrolled = p->techlastitem - 4;
	}
	if(p->techlastitem < p->techlastscrolled){
		p->techlastscrolled = p->techlastitem;
	}
	// Selection-change audio (legacy ProcessInGameInterfaces).
	Audio::GetInstance().Play(world_.resources.soundbank[GASLoader::Get().player.soundRoundCountdown], 64);
}

void IngameTech::Submit(){
	Player * p = world_.GetPeerPlayer(world_.localpeerid);
	if(!p) return;
	std::vector<int> visible = BuildTechIndexList(world_, *p);
	if(visible.empty()) return;
	int sel = p->techlastitem;
	if(sel < 0 || sel >= (int)visible.size()) return;
	BuyableItem * item = world_.buyableitems[visible[sel]];
	if(!item) return;
	// Mirrors ingame.cpp:142-148 enterpressed branch: own base → repair,
	// enemy base → virus.
	if(p->InOwnBase(world_)){
		p->RepairItem(world_, item->id);
	}else{
		p->VirusItem(world_, item->id);
	}
}

bool IngameTech::DispatchKey(int sdl_scancode){
	if(!Active()) return false;
	switch(sdl_scancode){
		case SDL_SCANCODE_UP:     MoveSelection(-1); return true;
		case SDL_SCANCODE_DOWN:   MoveSelection(+1); return true;
		case SDL_SCANCODE_RETURN: Submit();          return true;
		default: break;
	}
	// Swallow everything else (weapon/movement keys) while the menu is up.
	return true;
}

}}  // namespace ui::v2
