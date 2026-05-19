#ifndef TEAM_H
#define TEAM_H

#include "shared.h"
#include "object.h"

class Team : public Object
{
public:
	Team();
	void Serialize(bool write, Serializer & data, Serializer * old = 0);
	void Tick(World & world);
	void OnDestroy(World & world);
	bool AddPeer(Uint8 id);
	void RemovePeer(Uint8 id);
	Uint8 GetColor(void);
	const char * GetAgencyName(void);
	Uint32 GetAvailableTech(World & world);
	enum {NOXIS, LAZARUS, CALIBER, STATIC, BLACKROSE};
	Uint8 agency;
	Uint8 color;
	Uint8 numpeers;
	Uint8 peers[4];
	Uint8 number;
	Uint8 secrets;
	Uint16 secretdelivered;
	Uint8 secretprogress;
	Uint8 oldsecretprogress;
	Uint16 basedoorid;
	Uint16 beamingterminalid;
	int peerschecksum;
	Uint16 playerwithsecret;
	Uint32 disabledtech;
};

#endif
