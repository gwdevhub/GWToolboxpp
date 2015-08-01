#ifndef TOOLBOX_API_H // Did a classic guard here so we can detect if the api is included, might be useful i dunno.
#define TOOLBOX_API_H

#include <Windows.h>


namespace GWAPI{
	class GWAPIMgr;

	struct MemoryMgr;
	class GameThreadMgr;

	class CtoSMgr;
	class StoCMgr;

	class AgentMgr;
	class ItemMgr;
	class SkillbarMgr;
	class EffectMgr;
	class MapMgr;

#ifdef GWAPI_USEDIRECTX
	class DirectXMgr;
#endif

}

#include "MemoryMgr.h"
#include "GameThreadMgr.h"

#include "CtoSMgr.h"
#include "AgentMgr.h"
#include "ItemMgr.h"

#include "StoCMgr.h"

//#ifdef GWAPI_USEDIRECTX
#include "DirectXMgr.h"
//#endif

#include "SkillbarMgr.h"
#include "EffectMgr.h"

#include "MapMgr.h"

#include "GWAPIMgr.h"

#endif