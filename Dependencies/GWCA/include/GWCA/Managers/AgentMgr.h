#pragma once
#include <GWCA/Runtime.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Constants/Constants.h>

#include <GWCA/Utilities/Export.h>
#include <GWCA/Utilities/Hook.h>

namespace GW {
    typedef uint32_t AgentID;
    struct GamePos;

    struct NPC;
    struct Agent;
    struct Player;
    struct MapAgent;
    struct AgentLiving;
    struct AgentCharData;

    typedef Array<NPC> NPCArray;
    typedef Array<Agent *> AgentArray;
    typedef Array<Player> PlayerArray;
    typedef Array<MapAgent> MapAgentArray;

    struct Module;
    extern Module AgentModule;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4200) // nonstandard extension: zero-sized array in struct
#endif
    struct AgentEffect {
        uint32_t effect_type;
        uint32_t data_size; // usually 0x34, detemines length of data in bytes
        uint32_t data[];
    };
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    enum class WorldActionId : uint32_t {
        InteractEnemy,
        InteractPlayerOrOther,
        InteractNPC,
        InteractItem,
        InteractTrade,
        InteractGadget
    };

    // NB: Theres more target types, and they're in the code, but not used for our context
    enum class CallTargetType : uint32_t {
        Following = 0x3,
        Morale = 0x7,
        AttackingOrTargetting = 0xA,
        None = 0xFF
    };

    enum AgentTargetFlags : uint32_t {
        Include_Ally = 0x000001,
        Include_Neutral = 0x000002,
        Accept_HasQuest = 0x000004,
        Include_Enemy = 0x000008,
        Include_SpiritPet = 0x000010,
        Accept_ActiveState = 0x000020,
		Include_Minion = 0x000040,
			Include_NPCMinipet = 0x000080,
			Accept_Player = 0x000100,
			Type_Gadget = 0x000200,
			Type_Item = 0x000400,
			Exclude_DeadAlly = 0x000800,
			Exclude_DeadNeutral = 0x001000,
			Exclude_DeadEnemy = 0x002000,
			Exclude_DeadSpiritPet = 0x004000,
			Exclude_DeadMinion = 0x008000,
			Exclude_DeadNPCMinipet = 0x010000,
			Exclude_UsedCorpse = 0x020000,
			Exclude_AliveAlly = 0x040000,
			Exclude_AliveNeutral = 0x080000,
			Exclude_AliveEnemy = 0x100000,
			Exclude_AliveSpiritPet = 0x200000,
			Exclude_AliveMinion = 0x400000,
			Exclude_AliveNPCMinipet = 0x800000,
			Exclude_BeingObserved = 0x2000000,
			ZeroPriority = 0x4000000,
			NPCMinipet_ZeroPriority = 0x8000000,
	};

	inline AgentTargetFlags operator|(AgentTargetFlags a, AgentTargetFlags b) {
		return static_cast<AgentTargetFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
	}
	inline AgentTargetFlags operator&(AgentTargetFlags a, AgentTargetFlags b) {
		return static_cast<AgentTargetFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
	}
	inline AgentTargetFlags operator~(AgentTargetFlags a) {
		return static_cast<AgentTargetFlags>(~static_cast<uint32_t>(a));
	}

	namespace TargetFilter {
		const AgentTargetFlags Enemies = static_cast<AgentTargetFlags>(
			Include_Enemy | Exclude_DeadEnemy);

		const AgentTargetFlags Allies = static_cast<AgentTargetFlags>(
			Include_Ally | Accept_Player | Exclude_DeadAlly);

		const AgentTargetFlags Corpses = static_cast<AgentTargetFlags>(
			Include_Enemy | Exclude_AliveEnemy | Exclude_UsedCorpse);

		const AgentTargetFlags Items = static_cast<AgentTargetFlags>(
			Type_Item);

		const AgentTargetFlags Gadgets = static_cast<AgentTargetFlags>(
			Type_Gadget);

		const AgentTargetFlags AnyLiving = static_cast<AgentTargetFlags>(
			Include_Ally | Include_Neutral | Include_Enemy |
			Include_SpiritPet | Include_Minion | Include_NPCMinipet |
			Accept_Player |
			Exclude_DeadAlly | Exclude_DeadNeutral | Exclude_DeadEnemy |
			Exclude_DeadSpiritPet | Exclude_DeadMinion | Exclude_DeadNPCMinipet);

		const AgentTargetFlags Any = static_cast<AgentTargetFlags>(
			Include_Ally | Include_Neutral | Include_Enemy |
			Include_SpiritPet | Include_Minion | Include_NPCMinipet |
			Accept_Player | Accept_HasQuest |
			Type_Gadget | Type_Item);
	}


	namespace Agents {

		// Dialogs -- same as pressing button (id) while talking to an NPC.
		bool SendDialog(uint32_t dialog_id);

		// === Agent Array ===

		// Get Agent ID of currently observed agent
		uint32_t GetObservingId();
		// Get Agent ID of client's logged in player
		uint32_t GetControlledCharacterId();
		// Get Agent ID of current target
		uint32_t GetTargetId();
		// Get Agent ID of current evaluated target - either auto target or actual target
		uint32_t GetEvaluatedTargetId();

		// Returns Agentstruct Array of agents in compass range, full structs.
		AgentArray* GetAgentArray();

		// Get AgentArray Structures of player or target.
		Agent* GetAgentByID(uint32_t id);
		// Get agent that we're currently observing
		inline Agent* GetObservingAgent() { return GetAgentByID(GetObservingId()); }
		// Get Agent of current target
		inline Agent* GetTarget() { return GetAgentByID(GetTargetId()); }
		// Get Agent of current evaluated target - either auto target or actual target
		inline Agent* GetEvaluatedTarget() { return GetAgentByID(GetEvaluatedTargetId()); }

		Agent* GetPlayerByID(uint32_t player_id);

		// Get Agent of current logged in character
		AgentLiving* GetControlledCharacter();

		bool GetAgentMatchesFlags(const Agent*, AgentTargetFlags flags = TargetFilter::Any);

		bool RefreshAgentNameTag(const Agent*);

        // Whether we're observing someone else
        bool IsObserving();

        // Get AgentLiving of current target
        AgentLiving    *GetTargetAsAgentLiving();

        uint32_t GetAmountOfPlayersInInstance();

        // Alternate agent array readable beyond compass range; holds limited info and needs more exploration.
        MapAgentArray* GetMapAgentArray();
        uint32_t CountAllegianceInRange(GW::Constants::Allegiance allegiance, float sqr_range);

        AgentCharData* GetAgentCharData(uint32_t agent_id);
        MapAgent* GetMapAgentByID(uint32_t agent_id);

        GW::Constants::ProfessionByte GetAgentPrimary(uint32_t agent_id);
        GW::Constants::ProfessionByte GetAgentSecondary(uint32_t agent_id);
        // === Other Arrays ===
        PlayerArray* GetPlayerArray();

        NPCArray* GetNPCArray();
        NPC *GetNPCByID(uint32_t npc_id);

        // Change targeted agent to (Agent)
        bool ChangeTarget(AgentID agent_id);
        inline bool ChangeTarget(const Agent* agent) { return agent ? ChangeTarget(agent->agent_id) : false; }

        // Move to specified coordinates. Does nothing if the coordinate is outside the map.
		bool Move(GamePos pos);
        inline bool Move(float x, float y, uint32_t zplane = 0) { return Move(GamePos(x, y, zplane)); }

        bool InteractAgent(const Agent* agent, bool call_target = false);

        // Returns name of player with selected login_number.
        wchar_t *GetPlayerNameByLoginNumber(uint32_t login_number);

        // Returns AgentID of player with selected login_number.
        uint32_t GetAgentIdByLoginNumber(uint32_t login_number);

        AgentID GetHeroAgentID(uint32_t hero_index);

        // Might be bugged, avoid to use.
		wchar_t* GetAgentEncNameByAgent(const Agent* agent);
		wchar_t* GetAgentEncNameById(uint32_t agent_id);
        inline wchar_t* GetAgentEncName(const Agent* agent) { return GetAgentEncNameByAgent(agent); }
        inline wchar_t* GetAgentEncName(uint32_t agent_id) { return GetAgentEncNameById(agent_id); }
    };
}
// C Interop API

namespace GW::Agents {
    extern "C" GWCA_API bool __cdecl GW_Agents_SendDialog(uint32_t dialog_id) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Agents_GetObservingId() noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Agents_GetControlledCharacterId() noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Agents_GetTargetId() noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Agents_GetEvaluatedTargetId() noexcept;
    extern "C" GWCA_API AgentArray* __cdecl GW_Agents_GetAgentArray() noexcept;
    extern "C" GWCA_API Agent* __cdecl GW_Agents_GetAgentByID(uint32_t id) noexcept;
    extern "C" GWCA_API Agent* __cdecl GW_Agents_GetPlayerByID(uint32_t player_id) noexcept;
    extern "C" GWCA_API AgentLiving* __cdecl GW_Agents_GetControlledCharacter() noexcept;
    extern "C" GWCA_API bool __cdecl GW_Agents_GetAgentMatchesFlags(const Agent* gwca_arg0, AgentTargetFlags flags) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Agents_RefreshAgentNameTag(const Agent* gwca_arg0) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Agents_IsObserving() noexcept;
    extern "C" GWCA_API AgentLiving* __cdecl GW_Agents_GetTargetAsAgentLiving() noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Agents_GetAmountOfPlayersInInstance() noexcept;
    extern "C" GWCA_API MapAgentArray* __cdecl GW_Agents_GetMapAgentArray() noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Agents_CountAllegianceInRange(GW::Constants::Allegiance allegiance, float sqr_range) noexcept;
    extern "C" GWCA_API AgentCharData* __cdecl GW_Agents_GetAgentCharData(uint32_t agent_id) noexcept;
    extern "C" GWCA_API MapAgent* __cdecl GW_Agents_GetMapAgentByID(uint32_t agent_id) noexcept;
    extern "C" GWCA_API GW::Constants::ProfessionByte __cdecl GW_Agents_GetAgentPrimary(uint32_t agent_id) noexcept;
    extern "C" GWCA_API GW::Constants::ProfessionByte __cdecl GW_Agents_GetAgentSecondary(uint32_t agent_id) noexcept;
    extern "C" GWCA_API PlayerArray* __cdecl GW_Agents_GetPlayerArray() noexcept;
    extern "C" GWCA_API NPCArray* __cdecl GW_Agents_GetNPCArray() noexcept;
    extern "C" GWCA_API NPC* __cdecl GW_Agents_GetNPCByID(uint32_t npc_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Agents_ChangeTarget(AgentID agent_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Agents_Move(GamePos pos) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Agents_InteractAgent(const Agent* agent, bool call_target) noexcept;
    extern "C" GWCA_API wchar_t* __cdecl GW_Agents_GetPlayerNameByLoginNumber(uint32_t login_number) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Agents_GetAgentIdByLoginNumber(uint32_t login_number) noexcept;
    extern "C" GWCA_API AgentID __cdecl GW_Agents_GetHeroAgentID(uint32_t hero_index) noexcept;
    extern "C" GWCA_API wchar_t* __cdecl GW_Agents_GetAgentEncNameByAgent(const Agent* agent) noexcept;
    extern "C" GWCA_API wchar_t* __cdecl GW_Agents_GetAgentEncNameById(uint32_t agent_id) noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW::Agents {
    inline bool SendDialog(uint32_t dialog_id)
    {
        return GWCA_CALL_C(GW_Agents_SendDialog, dialog_id);
    }

    inline uint32_t GetObservingId()
    {
        return GWCA_CALL_C(GW_Agents_GetObservingId);
    }

    inline uint32_t GetControlledCharacterId()
    {
        return GWCA_CALL_C(GW_Agents_GetControlledCharacterId);
    }

    inline uint32_t GetTargetId()
    {
        return GWCA_CALL_C(GW_Agents_GetTargetId);
    }

    inline uint32_t GetEvaluatedTargetId()
    {
        return GWCA_CALL_C(GW_Agents_GetEvaluatedTargetId);
    }

    inline AgentArray* GetAgentArray()
    {
        return GWCA_CALL_C(GW_Agents_GetAgentArray);
    }

    inline Agent* GetAgentByID(uint32_t id)
    {
        return GWCA_CALL_C(GW_Agents_GetAgentByID, id);
    }

    inline Agent* GetPlayerByID(uint32_t player_id)
    {
        return GWCA_CALL_C(GW_Agents_GetPlayerByID, player_id);
    }

    inline AgentLiving* GetControlledCharacter()
    {
        return GWCA_CALL_C(GW_Agents_GetControlledCharacter);
    }

    inline bool GetAgentMatchesFlags(const Agent* gwca_arg0, AgentTargetFlags flags)
    {
        return GWCA_CALL_C(GW_Agents_GetAgentMatchesFlags, gwca_arg0, flags);
    }

    inline bool RefreshAgentNameTag(const Agent* gwca_arg0)
    {
        return GWCA_CALL_C(GW_Agents_RefreshAgentNameTag, gwca_arg0);
    }

    inline bool IsObserving()
    {
        return GWCA_CALL_C(GW_Agents_IsObserving);
    }

    inline AgentLiving* GetTargetAsAgentLiving()
    {
        return GWCA_CALL_C(GW_Agents_GetTargetAsAgentLiving);
    }

    inline uint32_t GetAmountOfPlayersInInstance()
    {
        return GWCA_CALL_C(GW_Agents_GetAmountOfPlayersInInstance);
    }

    inline MapAgentArray* GetMapAgentArray()
    {
        return GWCA_CALL_C(GW_Agents_GetMapAgentArray);
    }

    inline uint32_t CountAllegianceInRange(GW::Constants::Allegiance allegiance, float sqr_range)
    {
        return GWCA_CALL_C(GW_Agents_CountAllegianceInRange, allegiance, sqr_range);
    }

    inline AgentCharData* GetAgentCharData(uint32_t agent_id)
    {
        return GWCA_CALL_C(GW_Agents_GetAgentCharData, agent_id);
    }

    inline MapAgent* GetMapAgentByID(uint32_t agent_id)
    {
        return GWCA_CALL_C(GW_Agents_GetMapAgentByID, agent_id);
    }

    inline GW::Constants::ProfessionByte GetAgentPrimary(uint32_t agent_id)
    {
        return GWCA_CALL_C(GW_Agents_GetAgentPrimary, agent_id);
    }

    inline GW::Constants::ProfessionByte GetAgentSecondary(uint32_t agent_id)
    {
        return GWCA_CALL_C(GW_Agents_GetAgentSecondary, agent_id);
    }

    inline PlayerArray* GetPlayerArray()
    {
        return GWCA_CALL_C(GW_Agents_GetPlayerArray);
    }

    inline NPCArray* GetNPCArray()
    {
        return GWCA_CALL_C(GW_Agents_GetNPCArray);
    }

    inline NPC* GetNPCByID(uint32_t npc_id)
    {
        return GWCA_CALL_C(GW_Agents_GetNPCByID, npc_id);
    }

    inline bool ChangeTarget(AgentID agent_id)
    {
        return GWCA_CALL_C(GW_Agents_ChangeTarget, agent_id);
    }

    inline bool Move(GamePos pos)
    {
        return GWCA_CALL_C(GW_Agents_Move, pos);
    }

    inline bool InteractAgent(const Agent* agent, bool call_target)
    {
        return GWCA_CALL_C(GW_Agents_InteractAgent, agent, call_target);
    }

    inline wchar_t* GetPlayerNameByLoginNumber(uint32_t login_number)
    {
        return GWCA_CALL_C(GW_Agents_GetPlayerNameByLoginNumber, login_number);
    }

    inline uint32_t GetAgentIdByLoginNumber(uint32_t login_number)
    {
        return GWCA_CALL_C(GW_Agents_GetAgentIdByLoginNumber, login_number);
    }

    inline AgentID GetHeroAgentID(uint32_t hero_index)
    {
        return GWCA_CALL_C(GW_Agents_GetHeroAgentID, hero_index);
    }

    inline wchar_t* GetAgentEncNameByAgent(const Agent* agent)
    {
        return GWCA_CALL_C(GW_Agents_GetAgentEncNameByAgent, agent);
    }

    inline wchar_t* GetAgentEncNameById(uint32_t agent_id)
    {
        return GWCA_CALL_C(GW_Agents_GetAgentEncNameById, agent_id);
    }
}
#endif
