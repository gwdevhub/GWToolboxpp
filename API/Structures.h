#pragma once

#include "APIMain.h"

namespace GWAPI{

	// General Structures for various objects in the game.
	struct Agent;
	struct Bag;
	struct ModValue;
	struct Mod;
	struct Item;
	struct Skill;
	struct Effect;
	struct Buff;
	struct Skillbar;

	// Base array class guild wars uses.
	template <typename T>
	class gw_array{
		T* m_array;
		DWORD m_sizeAllocated;
		DWORD m_sizeCurrent;
	public:
		T GetIndex(DWORD index);
		T operator[](DWORD index);
		DWORD size();
	};

	// Typedefs to make things more understandable
	typedef gw_array<Agent*> AgentArray;
	typedef gw_array<Bag*> BagArray;

	// Function call typedefs

	typedef void(__fastcall *SendCtoGSPacket_t)(DWORD packetObject, DWORD size, DWORD* packet);
}