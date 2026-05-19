#include "world_replication.h"
#include "world.h"
#include "serializer.h"
#include "player.h"
#include "civilian.h"
#include "robot.h"
#include "fixedcannon.h"
#include "walldefense.h"
#include "techstation.h"
#include "surveillancemonitor.h"
#include "team.h"
#include "objecttypes.h"
#include "terminal.h"
#include "basedoor.h"
#include "bodypart.h"
#include "gasloader.h"
#include "gamestateobject.h"
#include "text_wrap.h"
#include <algorithm>

#define DELTAENABLED 1

WorldReplication::WorldReplication(World & world) : world(world){
	memset(oldsnapshots, 0, sizeof(oldsnapshots));
	totalsnapshots = 0;
	totalinputpackets = 0;
	snapshotqueueminsize = GASLoader::Get().gameengine.snapshotQueueMinSize;
	snapshotqueuemaxsize = GASLoader::Get().gameengine.snapshotQueueInitMaxSize;
	lastsnapshotqueueadjust = 0;
}

bool WorldReplication::ProcessInputQueue(Peer & peer){
	if(inputqueue[peer.id].size() > 0){
		Serializer * data = inputqueue[peer.id].front();
		Uint32 theirlasttick;
		data->Get(theirlasttick);
		Uint32 lasttick;
		data->Get(lasttick);
		if(lasttick >= peer.lasttick){
			peer.theirlasttick = theirlasttick;
			peer.lasttick = lasttick;
			//peer.port = ntohs(senderaddr.sin_port);
			peer.input.Serialize(Serializer::READ, *data);
			for(std::list<Uint16>::iterator i = peer.controlledlist.begin(); i != peer.controlledlist.end(); i++){
				Object * object = world.GetObjectFromId((*i));
				if(object){
					//if(!world.replaying){
						object->oldx = object->x;
						object->oldy = object->y;
					//}
					if (!world.input_locked) object->HandleInput(peer.input);
					object->Tick(world);
					object->lasttick = world.tickcount;
					//printf("Processed input for peer %d at tick %d\n", peer.id, world.tickcount);
				}
			}
		}
		delete data;
		inputqueue[peer.id].pop_front();
		return true;
	}
	return false;
}

void WorldReplication::ProcessSnapshotQueue(void){
	if(!world.peerlist[world.localpeerid]){
		return;
	}
	int maxruns = 1;
	if(snapshotqueue.size() > snapshotqueuemaxsize){
		maxruns = 2;
	}
	if(snapshotqueue.size() == 0){
		if(snapshotqueuemaxsize < GASLoader::Get().gameengine.snapshotQueueMaxCap){
			snapshotqueuemaxsize++;
		}
	}
	if(world.tickcount - lastsnapshotqueueadjust > (Uint32)GASLoader::Get().gameengine.snapshotQueueShrinkTicks){
		if(snapshotqueuemaxsize - snapshotqueueminsize > 1){
			snapshotqueuemaxsize--;
			lastsnapshotqueueadjust = world.tickcount;
		}
	}
	if(snapshotqueue.size() < snapshotqueueminsize){
		return;
	}
	snapshotqueue.sort(CompareSnapshot);
	for(int k = 0; k < maxruns; k++){
		if(snapshotqueue.size() > 0){
			Serializer * data = snapshotqueue.front();
			unsigned int oldreadoffset = data->readoffset;
			Uint32 tick;
			data->Get(tick);
			Uint32 ourtick;
			data->Get(ourtick);
			unsigned int readoffsetbeforedelta = data->readoffset;
			Uint32 deltatick;
			data->Get(deltatick);
			data->readoffset = readoffsetbeforedelta;
			Serializer ** deltasnapshotptr = &oldsnapshots[world.localpeerid][deltatick % World::maxoldsnapshots];
			
			if(DELTAENABLED && *deltasnapshotptr && tick - deltatick < World::maxoldsnapshots){
				LoadSnapshot(*data, true, *deltasnapshotptr);
			}else{
				LoadSnapshot(*data, true);
			}
			
			Serializer ** newsnapshotptr = &oldsnapshots[world.localpeerid][tick % World::maxoldsnapshots];
			if(!*newsnapshotptr){
				*newsnapshotptr = new Serializer;
			}
			(*newsnapshotptr)->offset = 0;
			SaveSnapshot(**newsnapshotptr, world.localpeerid);
			
			world.TickObjects();
			
			if(tick > world.peerlist[world.localpeerid]->lasttick){
				world.peerlist[world.localpeerid]->lasttick = tick;
				ClientSidePredict(ourtick);
			}

			data->readoffset = oldreadoffset;
			delete data;
			snapshotqueue.pop_front();
		}
	}
}

void WorldReplication::ClientSidePredict(Uint32 ourtick){
	// Perform client side predication
	int replayticks = world.tickcount - ourtick - 1;
	if(replayticks < World::maxlocalinputhistory - 1){
		for(int i = replayticks; i > 0; i--){
			for(std::list<Uint16>::iterator it = world.peerlist[world.localpeerid]->controlledlist.begin(); it != world.peerlist[world.localpeerid]->controlledlist.end(); it++){
				Object * object = world.GetObjectFromId((*it));
				if(object){
					// set the oldinput
					if(object->type == ObjectTypes::PLAYER){
						Player * player = (Player *)object;
						player->oldinput = localinputhistory[(world.tickcount - i - 1) % World::maxlocalinputhistory];
					}
					//
					object->HandleInput(localinputhistory[(world.tickcount - i) % World::maxlocalinputhistory]);
					object->oldx = object->x;
					object->oldy = object->y;
					world.replaying = true;
					world.audio.enabled = false;
					object->Tick(world);
					object->lasttick = world.tickcount;
					world.audio.enabled = true;
					world.replaying = false;
				}
			}
		}
	}
	//
}

void WorldReplication::CheckExists(void){
	Serializer data;
	Uint8 code = World::MSG_EXISTS;
	data.Put(code);
	int count = 0;
	Uint8 types[] = {ObjectTypes::PICKUP, ObjectTypes::FIXEDCANNON, ObjectTypes::DETONATOR};
	// check for these objects, because they can get destroyed while we are not there, and they wont get deleted
	for(int i = 0; i < sizeof(types) / sizeof(Uint8); i++){
		for(std::vector<Uint16>::iterator it = world.objectsbytype[types[i]].begin(); it != world.objectsbytype[types[i]].end(); it++){
			if(count >= GASLoader::Get().gameengine.maxStaleSnapshots){
				break;
			}
			Object * object = world.GetObjectFromId(*it);
			if(object && world.tickcount - object->lastsnapshottick > World::maxoldsnapshots){
				data.Put((*it));
				count++;
			}
		}
	}
	world.SendPacket(world.GetAuthorityPeer(), data.data, data.BitsToBytes(data.offset));
}

void WorldReplication::ClearSnapshotQueue(void){
	for(std::list<Serializer *>::iterator it = snapshotqueue.begin(); it != snapshotqueue.end(); it++){
		Serializer * data = *it;
		delete data;
	}
	snapshotqueue.clear();
}

void WorldReplication::DeleteOldSnapshots(Uint8 peerid){
	for(unsigned int i = 0; i < World::maxoldsnapshots; i++){
		if(oldsnapshots[peerid][i]){
			delete oldsnapshots[peerid][i];
			oldsnapshots[peerid][i] = 0;
		}
	}
}

bool WorldReplication::CompareSnapshot(Serializer * snapshot1, Serializer * snapshot2){
	unsigned int oldreadoffset = snapshot1->readoffset;
	Uint32 tick1;
	snapshot1->Get(tick1);
	snapshot1->readoffset = oldreadoffset;
	oldreadoffset = snapshot2->readoffset;
	Uint32 tick2;
	snapshot2->Get(tick2);
	snapshot2->readoffset = oldreadoffset;
	return(tick1 < tick2);
}

void WorldReplication::SendInput(void){
	Peer * peer = 0;
	localinputhistory[world.tickcount % World::maxlocalinputhistory] = world.localinput;
	if(world.mode == World::REPLICA && world.state == World::CONNECTED && world.gameplaystate == World::INGAME){
		peer = world.peerlist[world.localpeerid];
		if(peer && peer->controlledlist.size() > 0){
			peer->input = world.localinput;
			peer->input.mousex = 0xFFFF;
			peer->input.mousey = 0xFFFF;
			Serializer data;
			Uint8 code = World::MSG_INPUT;
			data.Put(code);
			data.Put(world.tickcount);
			data.Put(world.peerlist[world.localpeerid]->lasttick);
			peer->input.Serialize(Serializer::WRITE, data);
			world.SendPacket(world.GetAuthorityPeer(), data.data, data.BitsToBytes(data.offset));
		}
	}else
	if(world.mode == World::AUTHORITY){
		peer = world.GetAuthorityPeer();
		peer->input = world.localinput;
	}
	if(peer){
		for(std::list<Uint16>::iterator i = peer->controlledlist.begin(); i != peer->controlledlist.end(); i++){
			Object * object = world.GetObjectFromId((*i));
			if(object){
				//if(!world.replaying){
					object->oldx = object->x;
					object->oldy = object->y;
				//}
				if (!world.input_locked) object->HandleInput(peer->input);
				object->Tick(world);
				object->lasttick = world.tickcount;
			}
		}
	}
}

void WorldReplication::SendSnapshots(void){
	for(unsigned int i = 0; i < World::maxpeers; i++){
		Peer * peer = world.peerlist[i];
		if(peer && i != world.localpeerid && !peer->isbot && !peer->disconnected){
			Serializer data;
			Uint8 code = World::MSG_SNAPSHOT;
			data.Put(code);
			data.Put(world.tickcount);
			data.Put(peer->theirlasttick);
			SaveSnapshot(data, i);
			world.SendPacket(peer, data.data, data.BitsToBytes(data.offset));
		}
	}
}

void WorldReplication::SendGameInfo(Uint8 peerid){
	Peer * peer = world.peerlist[peerid];
	if(peer){
		Serializer data;
		Uint8 code = World::MSG_GAMEINFO;
		data.Put(code);
		world.gameinfo.Serialize(Serializer::WRITE, data);
		world.SendPacket(peer, data.data, data.BitsToBytes(data.offset));
	}
}

void WorldReplication::SendGameInfoLoaded(void){
	char data[1];
	data[0] = World::MSG_GAMEINFO;
	world.SendPacket(world.GetAuthorityPeer(), data, sizeof(data));
}

void WorldReplication::SendReady(void){
	Serializer data;
	Uint8 code = World::MSG_READY;
	data.Put(code);
	world.SendPacket(world.GetAuthorityPeer(), data.data, data.BitsToBytes(data.offset));
}

bool WorldReplication::AllPeersReady(Uint8 except){
	bool allready = true;
	for(int i = 0; i < World::maxpeers; i++){
		Peer * peer = world.peerlist[i];
		if(peer){
			if(!peer->isready && peer->id != except){
				allready = false;
				break;
			}
		}
	}
	return allready;
}

bool WorldReplication::AllPeersLoadedGameInfo(void){
	bool allloaded = true;
	for(int i = 0; i < World::maxpeers; i++){
		Peer * peer = world.peerlist[i];
		if(peer){
			if(!peer->gameinfoloaded){
				allloaded = false;
				break;
			}
		}
	}
	return allloaded;
}

bool WorldReplication::AllPeersDownloadedMap(void){
	bool allloaded = true;
	for(int i = 0; i < World::maxpeers; i++){
		if(i == world.authoritypeer) continue; // dedicated server doesn't download maps
		Peer * peer = world.peerlist[i];
		if(peer){
			if(!peer->mapdownloaded){
				allloaded = false;
				break;
			}
		}
	}
	return allloaded;
}

void WorldReplication::SaveSnapshot(Serializer & data, Uint8 peerid){
	if(world.mode == World::AUTHORITY){
		Player * player = world.GetPeerPlayer(peerid);
		bool isobserver = world.peerlist[peerid] && world.peerlist[peerid]->observer;
		Serializer ** oldsnapshotptr = &oldsnapshots[peerid][world.tickcount % World::maxoldsnapshots];
		Serializer ** deltasnapshotptr = &oldsnapshots[peerid][world.peerlist[peerid]->lasttick % World::maxoldsnapshots];
		if(world.tickcount - world.peerlist[peerid]->lasttick >= World::maxoldsnapshots){
			*deltasnapshotptr = 0;
		}
		if(!(*oldsnapshotptr)){
			*oldsnapshotptr = new Serializer;
		}
		
		// Find deleted objects
		std::vector<Uint16> deletedobjects;
		for(int i = world.peerlist[peerid]->lasttick; i < world.tickcount; i++){
			// Go through all the snapshots sent to the peer since the last acknowledged one
			// and find objects that no longer exist.  Alternative is to create a reliable
			// packet and just send them when objects are deleted - TODO
			Serializer ** tempdeltasnapshotptr = &oldsnapshots[peerid][(world.tickcount - i) % World::maxoldsnapshots];
			if(*tempdeltasnapshotptr){
				(*tempdeltasnapshotptr)->readoffset = 0;
				while((*tempdeltasnapshotptr)->MoreBytesToRead()){
					Uint8 type;
					Uint16 id;
					int oldreadoffset = (*tempdeltasnapshotptr)->readoffset;
					(*tempdeltasnapshotptr)->Get(type);
					(*tempdeltasnapshotptr)->Get(id);
					(*tempdeltasnapshotptr)->readoffset = oldreadoffset;
					Object * object = world.GetObjectFromId(id);
					if(!object && std::find(deletedobjects.begin(), deletedobjects.end(), id) == deletedobjects.end()){
						deletedobjects.push_back(id);
					}
					(*tempdeltasnapshotptr)->readoffset += world.objecttypes.SerializedSize(type);
				}
			}
			if(i > World::maxoldsnapshots){
				break;
			}
		}
		Uint16 deletedobjectscount = deletedobjects.size();
		//
		
		data.Put(world.peerlist[peerid]->lasttick);
		data.Put(deletedobjectscount);
		for(std::vector<Uint16>::iterator it = deletedobjects.begin(); it != deletedobjects.end(); it++){
			data.Put(*it);
		}
		(*oldsnapshotptr)->offset = 0;
		
		if(DELTAENABLED && *deltasnapshotptr){
			// Write delta'ed objects in snapshot
			(*deltasnapshotptr)->readoffset = 0;
			std::map<Uint16, Object *> oldobjects;
			while((*deltasnapshotptr)->MoreBytesToRead()){
				Uint8 type;
				Uint16 id;
				int oldreadoffset = (*deltasnapshotptr)->readoffset;
				(*deltasnapshotptr)->Get(type);
				(*deltasnapshotptr)->Get(id);
				(*deltasnapshotptr)->readoffset = oldreadoffset;
				Object * object = world.GetObjectFromId(id);
				if(object){
					oldobjects[id] = object;
					data.PutBit(1);
					object->Serialize(Serializer::WRITE, data, *deltasnapshotptr);
				}else{
					(*deltasnapshotptr)->readoffset += world.objecttypes.SerializedSize(type);
				}
			}
			//
			
			// Write all new objects
			for(std::list<Object *>::iterator i = world.objectlist.begin(); i != world.objectlist.end(); i++){
				if((*i)->RequiresAuthority() && (isobserver || world.RelevantToPlayer(player, (*i)))){
					(*i)->Serialize(Serializer::WRITE, **oldsnapshotptr);
					if(oldobjects.find((*i)->id) == oldobjects.end()){
						data.PutBit(0);
						(*i)->Serialize(Serializer::WRITE, data);
					}
				}
			}
			//
		}else{
			// Write all relevant objects, no delta compression
			for(std::list<Object *>::iterator i = world.objectlist.begin(); i != world.objectlist.end(); i++){
				if((*i)->RequiresAuthority() && (isobserver || world.RelevantToPlayer(player, (*i)))){
					(*i)->Serialize(Serializer::WRITE, **oldsnapshotptr);
					data.PutBit(0);
					(*i)->Serialize(Serializer::WRITE, data);
				}
			}
			//
		}
	}else
	if(world.mode == World::REPLICA){
		Uint32 nulltickcount = 0;
		data.Put(nulltickcount);
		Uint16 nulldeletedobjectscount = 0;
		data.Put(nulldeletedobjectscount);
		for(std::list<Object *>::iterator i = world.objectlist.begin(); i != world.objectlist.end(); i++){
			if((*i)->RequiresAuthority()){
				data.PutBit(0);
				(*i)->Serialize(Serializer::WRITE, data);
			}
		}
	}
}

void WorldReplication::LoadSnapshot(Serializer & data, bool create, Serializer * delta, Uint16 objectid){
	Uint32 deltatick;
	data.Get(deltatick);
	Uint16 deletedobjectscount;
	data.Get(deletedobjectscount);
	for(int i = 0; i < deletedobjectscount; i++){
		Uint16 objectid;
		data.Get(objectid);
		if(world.GetObjectFromId(objectid)){
			//printf("deleted %d\n", objectid);
			world.MarkDestroyObject(objectid);
		}
	}
	while(data.MoreBytesToRead()){
		bool isdeltacompressed = data.GetBit();
		Uint8 type;
		Uint16 id;
		int readoffset = data.readoffset;
		data.Get(type);
		data.Get(id);
		data.readoffset = readoffset;
		if(objectid && id != objectid){
			data.readoffset += world.objecttypes.SerializedSize(type);
			continue;
		}
		Object * object = world.GetObjectFromId(id);
		if(!object && create){
			object = world.CreateObject(type, id);
		}
		if(object){
			object->lastsnapshottick = world.tickcount;
			if(object->type != type){
				//printf("OBJECT TYPE DOES NOT MATCH IN SNAPSHOT\n");
				world.MarkDestroyObject(object->id);
				data.readoffset += world.objecttypes.SerializedSize(type);
			}else{
				if(object->iscontrollable){
					//if(!world.replaying){
						object->oldx = object->x;
						object->oldy = object->y;
					//}
				}
				if(delta){
					delta->readoffset = 0;
					LoadSnapshot(*delta, false, 0, id);
				}
				if(isdeltacompressed){
					object->Serialize(Serializer::READ, data, (Serializer *)true);
				}else{
					object->Serialize(Serializer::READ, data);
				}
			}
		}else{
			data.readoffset += world.objecttypes.SerializedSize(type);
		}
	}
}

