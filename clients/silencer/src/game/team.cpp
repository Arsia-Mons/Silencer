#include "team.h"
#include "terminal.h"
#include "player.h"
#include "../gas/gasloader.h"

Team::Team() : Object(ObjectTypes::TEAM){
	agency = NOXIS;
	numpeers = 0;
	color = 0;
	requiresauthority = true;
	requiresmaptobeloaded = false;
	secrets = 0;
	secretdelivered = 0;
	secretprogress = 0;
	number = 0;
	basedoorid = 0;
	beamingterminalid = 0;
	for(int i = 0; i < 4; i++){
		peers[i] = 0;
	}
	peerschecksum = 0;
	oldsecretprogress = 0;
	issprite = false;
	playerwithsecret = 0;
	disabledtech = 0;
}

void Team::Serialize(bool write, Serializer & data, Serializer * old){
	Object::Serialize(write, data, old);
	data.Serialize(write, agency, old);
	data.Serialize(write, secrets, old);
	data.Serialize(write, secretprogress, old);
	data.Serialize(write, secretdelivered, old);
	data.Serialize(write, basedoorid, old);
	data.Serialize(write, beamingterminalid, old);
	data.Serialize(write, numpeers, old);
	data.Serialize(write, number, old);
	for(int i = 0; i < 4; i++){
		data.Serialize(write, peers[i], old);
	}
	data.Serialize(write, playerwithsecret, old);
	data.Serialize(write, disabledtech, old);
}

void Team::Tick(World & world){
	int newpeerschecksum = numpeers;
	for(int i = 0; i < numpeers; i++){
		if(world.peerlist[peers[i]]){
			newpeerschecksum += world.peerlist[peers[i]]->ip;
		}
	}
	if(secretprogress - oldsecretprogress >= GASLoader::Get().player.secretProgressSoundThresh){
		/*Player * localplayer = world.GetPeerPlayer(world.localpeerid);
		if(localplayer && this == localplayer->GetTeam(world)){
			Audio::GetInstance().Play(world.resources.soundbank["select2.wav"], 32);
		}*/
		for(int i = 0; i < numpeers; i++){
			Peer * peer = world.peerlist[peers[i]];
			if(peer){
				world.SendSound(GASLoader::Get().player.soundTeamJoin.c_str(), peer, 32);
			}
		}
		oldsecretprogress = secretprogress;
	}
	if(secretdelivered){
		secrets++;
		world.secretsBeamed++;
		world.SendSound(GASLoader::Get().player.soundTeamHQ.c_str());
		if(world.gameMode){
			world.gameMode->OnSecretDelivered(world, *this);
		}
		secretdelivered = 0;
	}
	if(secretprogress >= GASLoader::Get().player.secretProgressBeamThresh && oldsecretprogress > 0){
		secretprogress = 0;
		oldsecretprogress = 0;
		if(world.gameMode){
			world.gameMode->OnSecretBeamReady(world, *this);
		}
	}
	if(numpeers == 0){
		world.MarkDestroyObject(id);
	}
	peerschecksum = newpeerschecksum;
}

void Team::OnDestroy(World & world){
	(void)world;
}

bool Team::AddPeer(Uint8 id){
	const AgencyDef* ad = GASLoader::Get().GetAgencyDef(agency);
	Uint8 maxplayers = ad ? ad->maxPlayersPerTeam : 4;
	for(int i = 0; i < numpeers; i++){
		if(peers[i] == id){
			return false;
		}
	}
	if(numpeers < maxplayers){
		peers[numpeers] = id;
		numpeers++;
		return true;
	}
	return false;
}

void Team::RemovePeer(Uint8 id){
	for(int i = 0; i < numpeers; i++){
		if(peers[i] == id){
			for(int j = i; j < numpeers; j++){
				peers[j] = peers[j + 1];
			}
			numpeers--;
		}
	}
}

Uint8 Team::GetColor(void){
	if(color){
		return color;
	}
	// basecolor index maps to palette group (basecolor*16):
	//   8=white/silver, 9=yellow, 10=red, 11=tan, 12=orange, 13=blue, 14=green, 15=black
	// shade: 8=natural, 9=brighter, 7=darker
	Uint8 basecolor = 14;
	Uint8 shade = 9;
	switch(number){
		case 0: basecolor = 10; break; // red
		case 1: basecolor = 14; break; // green
		case 2: basecolor = 13; break; // blue
		case 3: basecolor = 9;  break; // yellow
		case 4: basecolor = 12; break; // orange
		case 5: basecolor = 8;  break; // silver/white
	}
	if(agency == BLACKROSE){
		basecolor = 15; // true black
		shade = 8;
	}
	return ((shade << 4) + basecolor);
}

const char * Team::GetAgencyName(void){
	switch(agency){
		default:
		case 0:
			return "Noxis";
		break;
		case 1:
			return "Lazarus";
		break;
		case 2:
			return "Caliber";
		break;
		case 3:
			return "Static";
		break;
		case 4:
			return "Black Rose";
		break;
	}
}

Uint32 Team::GetAvailableTech(World & world){
	Uint32 tech = 0;
	for(int i = 0; i < numpeers; i++){
		Peer * peer = world.peerlist[peers[i]];
		if(peer){
			tech |= peer->techchoices;
		}
	}
	return tech;
}
