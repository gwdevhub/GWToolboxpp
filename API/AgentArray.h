#pragma once

#include <Windows.h>

namespace GWCA{

class AgentArray{

public:
	struct Agent {
		DWORD* vtable;
		BYTE unknown1[24];
		BYTE unknown2[4]; //This actually points to the agent before but with a small offset
		Agent* NextAgent; //Pointer to the next agent (by id)
		BYTE unknown3[8];
		long Id; //AgentId
		float Z; //Z coord in float
		BYTE unknown4[8];
		float BoxHoverWidth; //Width of the model's box
		float BoxHoverHeight; //Height of the model's box
		BYTE unknown5[8];
		float Rotation; //Rotation in radians from East (-pi to pi)
		BYTE unknown6[8];
		long NameProperties; //Bitmap basically telling what the agent is
		BYTE unknown7[24];
		float X; //X coord in float
		float Y; //Y coord in float
		DWORD Ground;
		BYTE unknown8[4];
		float NameTagX; //Exactly the same as X above
		float NameTagY; //Exactly the same aswell
		float NameTagZ; //Z coord in float (actually negated)
		BYTE unknown9[12];
		long Type; //0xDB = players, npc's, monsters etc. 0x200 = signpost/chest/object (unclickable). 0x400 = item to pick up
		float MoveX; //If moving, how much on the X axis per second
		float MoveY; //If moving, how much on the Y axis per second
		//BYTE unknown10[68];
		BYTE unknown10[28];
		long Owner;
		BYTE unknown24[8];
		long ExtraType;
		BYTE unknown11[24];
		float WeaponAttackSpeed; //The base attack speed in float of last attacks weapon. 1.33 = axe, sWORD, daggers etc.
		float AttackSpeedModifier; //Attack speed modifier of the last attack. 0.67 = 33% increase (1-.33)
		WORD PlayerNumber; //Selfexplanatory. All non-players have identifiers for their type. Two of the same mob = same number
		BYTE unknown12[6];
		DWORD** Equip;
		BYTE unknown13[10];
		BYTE Primary; //Primary profession 0-10 (None,W,R,Mo,N,Me,E,A,Rt,P,D)
		BYTE Secondary; //Secondary profession 0-10 (None,W,R,Mo,N,Me,E,A,Rt,P,D)
		BYTE Level; //Duh!
		BYTE TeamId; //0=None, 1=Blue, 2=Red, 3=Yellow
		BYTE unknown14[14];
		float Energy; //Only works for yourself
		long MaxEnergy; //Only works for yourself
		BYTE unknown15[4];
		float HPPips; //Regen/degen as float
		BYTE unknown16[4];
		//Offset +0x130
		float HP; //Health in % where 1=100% and 0=0%
		long MaxHP; //Only works for yourself
		long Effects; //Bitmap for effects to display when targetted. DOES include hexes
		BYTE unknown17[4];
		BYTE Hex; //Bitmap for the hex effect when targetted (apparently obsolete!)
		BYTE unknown18[18];
		long ModelState; //Different values for different states of the model.
		long TypeMap; //Odd variable! 0x08 = dead, 0xC00 = boss, 0x40000 = spirit, 0x400000 = player
		BYTE unknown19[16];
		long InSpiritRange; //Tells if agent is within spirit range of you. Doesn't work anymore?
		BYTE unknown20[16];
		long LoginNumber; //Unique number in instance that only works for players
		float ModelMode; //Float for the current mode the agent is in. Varies a lot
		BYTE unknown21[4];
		long ModelAnimation; //Id of the current animation
		BYTE unknown22[32];
		WORD Allegiance; //0x100 = ally/non-attackable, 0x300 = enemy, 0x400 = spirit/pet, 0x500 = minion, 0x600 = npc/minipet
		WORD WeaponType; //1=bow, 2=axe, 3=hammer, 4=daggers, 5=scythe, 6=spear, 7=sWORD, 10=wand, 12=staff, 14=staff
		//Offset +0x1B4
		WORD Skill; //0 = not using a skill. Anything else is the Id of that skill
		BYTE unknown23[4];
		WORD WeaponItemId;
		WORD OffhandItemId;

		// Health Bar Effect Bitmasks.
		inline bool GetIsBleeding() { return Effects & 1; }
		inline bool GetIsConditioned() { return Effects & 2; }
		inline bool GetIsDead() { return Effects & 16; }
		inline bool GetIsDeepWounded() { return Effects & 32; }
		inline bool GetIsPoisoned() { return Effects & 64; }
		inline bool GetIsEnchanted() { return Effects & 128; }
		inline bool GetIsDegenHexed() { return Effects & 1024; }
		inline bool GetIsHexed() { return Effects & 2048; }
		inline bool GetIsWeaponSpelled() { return Effects & 32768; }

		// Agent Type Bitmasks.
		inline bool GetIsLivingType() { return Type & 0xDB; }
		inline bool GetIsSignpostType() { return Type & 0x200; }
		inline bool GetIsItemType() { return Type & 0x400; }

		// Agent TypeMap Bitmasks.
		inline bool GetInCombatStance() { return TypeMap & 1; }
		inline bool GetHasQuest() { return TypeMap & 2; }
		inline bool GetIsDeadByTypeMap() { return TypeMap & 8; }
		inline bool GetIsFemale() { return TypeMap & 512; }
		inline bool GetHasBossGlow() { return TypeMap & 1024; }
		inline bool GetIsHidingCape() { return TypeMap & 4096; }
		inline bool GetCanBeViewedInPartyWindow() { return TypeMap & 131072; }
		inline bool GetIsSpawned() { return TypeMap & 262144; }
		inline bool GetIsBeingObserved() { return TypeMap & 4194304; }

		// Modelstates.
		inline bool GetIsKnockedDown() { return ModelState == 1104; }
		inline bool GetIsMoving() { return ModelState == 12 || ModelState == 76 || ModelState == 204; }
		inline bool GetIsAttacking() { return ModelState == 96 || ModelState == 1088 || ModelState == 1120; }
	};

private:

	typedef Agent*(__fastcall *GetAgent_t)(DWORD);

	GetAgent_t m_GetAgent = NULL;
	DWORD* m_MaxAgents = NULL;
	DWORD* m_PlayerID = NULL;
	DWORD* m_TargetID = NULL; 

public:

	AgentArray();
	DWORD GetMaxAgents();
	Agent* GetAgent(DWORD ID);
	Agent* operator[](DWORD ID);
	DWORD GetPlayerID();
	DWORD GetTargetID();
	Agent* GetPlayer();
	Agent* GetTarget();	
};

}


// cpp

GWCA::AgentArray::Agent* GWCA::AgentArray::GetTarget()
{
	return GetAgent(*m_TargetID);
}

GWCA::AgentArray::Agent* GWCA::AgentArray::GetPlayer()
{
	return GetAgent(*m_PlayerID);
}

DWORD GWCA::AgentArray::GetTargetID()
{
	return *m_TargetID;
}

DWORD GWCA::AgentArray::GetPlayerID()
{
	return *m_PlayerID;
}

GWCA::AgentArray::Agent* GWCA::AgentArray::operator[](DWORD ID)
{
	return GetAgent(ID);
}

GWCA::AgentArray::Agent* GWCA::AgentArray::GetAgent(DWORD ID)
{
	Agent* ag = m_GetAgent(ID);
	return ag ? ag : NULL;
}

DWORD GWCA::AgentArray::GetMaxAgents()
{
	return *m_MaxAgents;
}

GWCA::AgentArray::AgentArray()
{
	BYTE* start = (BYTE*)0x401000;
	const BYTE* end = (BYTE*)0x900000;

	const BYTE AgentBaseCode[] = { 0x56, 0x8B, 0xF1, 0x3B, 0xF0, 0x72, 0x04 };

	while (start++ != end)
	{
		if (!memcmp(start, AgentBaseCode, sizeof(AgentBaseCode)))
		{
			m_GetAgent = (GetAgent_t)(start - 5);
			m_MaxAgents = *(DWORD**)(start - 4);
			m_PlayerID = (DWORD*)((*(DWORD*)(start + 0xC)) - 84);
			m_TargetID = (DWORD*)((*(DWORD*)(start + 0xC)) - 0x500);
			return;
		}
	}
	throw 0;
}
