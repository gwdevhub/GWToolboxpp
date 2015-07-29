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
	class SkillbarMgr;
	class EffectMgr;

	class DirectXMgr;
}

#include "MemoryMgr.h"
#include "GameThreadMgr.h"

#include "CtoSMgr.h"
#include "AgentMgr.h"

#include "StoCMgr.h"
#include "DirectXMgr.h"

#include "SkillbarMgr.h"
#include "EffectMgr.h"

#include "GWAPIMgr.h"

#endif