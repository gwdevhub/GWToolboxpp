#pragma once

#include <GWCA/GameContainers/Array.h>
#include <GWCA/Constants/Constants.h>

#include <GWCA/Utilities/Export.h>

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
        // === Dialogs ===
        // Same as pressing button (id) while talking to an NPC.
        GWCA_API bool SendDialog(uint32_t dialog_id);

        // === Agent Array ===

        // Get Agent ID of currently observed agent
        GWCA_API uint32_t GetObservingId();
        // Get Agent ID of client's logged in player
        GWCA_API uint32_t GetControlledCharacterId();
        // Get Agent ID of current target
        GWCA_API uint32_t GetTargetId();
        // Get Agent ID of current evaluated target - either auto target or actual target
        GWCA_API uint32_t GetEvaluatedTargetId();

        // Returns Agentstruct Array of agents in compass range, full structs.
        GWCA_API AgentArray* GetAgentArray();

        // Get AgentArray Structures of player or target.
        GWCA_API Agent *GetAgentByID(uint32_t id);
        // Get agent that we're currently observing
        inline Agent   *GetObservingAgent() { return GetAgentByID(GetObservingId()); }
        // Get Agent of current target
        inline Agent   *GetTarget() { return GetAgentByID(GetTargetId()); }
        // Get Agent of current evaluated target - either auto target or actual target
        inline Agent   *GetEvaluatedTarget() { return GetAgentByID(GetEvaluatedTargetId()); }

        GWCA_API Agent *GetPlayerByID(uint32_t player_id);

        // Get Agent of current logged in character
        GWCA_API AgentLiving* GetControlledCharacter();

        GWCA_API bool GetAgentMatchesFlags(const Agent*, AgentTargetFlags flags = TargetFilter::Any);

        // Whether we're observing someone else
        GWCA_API bool IsObserving();

        // Get AgentLiving of current target
        GWCA_API AgentLiving    *GetTargetAsAgentLiving();

        GWCA_API uint32_t GetAmountOfPlayersInInstance();

        // Returns array of alternate agent array that can be read beyond compass range.
        // Holds limited info and needs to be explored more.
        GWCA_API MapAgentArray* GetMapAgentArray();
        GWCA_API uint32_t CountAllegianceInRange(GW::Constants::Allegiance allegiance, float sqr_range);

        GWCA_API AgentCharData* GetAgentCharData(uint32_t agent_id);
        GWCA_API MapAgent* GetMapAgentByID(uint32_t agent_id);

        GWCA_API GW::Constants::ProfessionByte GetAgentPrimary(uint32_t agent_id);
        GWCA_API GW::Constants::ProfessionByte GetAgentSecondary(uint32_t agent_id);
        // === Other Arrays ===
        GWCA_API PlayerArray* GetPlayerArray();

        GWCA_API NPCArray* GetNPCArray();
        GWCA_API NPC *GetNPCByID(uint32_t npc_id);

        // Change targeted agent to (Agent)
        GWCA_API bool ChangeTarget(const Agent *agent);
        GWCA_API bool ChangeTarget(AgentID agent_id);

        // Move to specified coordinates.
        // Note: will do nothing if coordinate is outside the map!
        GWCA_API bool Move(float x, float y, uint32_t zplane = 0);
        GWCA_API bool Move(GamePos pos);

        GWCA_API bool InteractAgent(const Agent* agent, bool call_target = false);

        // Returns name of player with selected login_number.
        GWCA_API wchar_t *GetPlayerNameByLoginNumber(uint32_t login_number);

        // Returns AgentID of player with selected login_number.
        GWCA_API uint32_t GetAgentIdByLoginNumber(uint32_t login_number);

        GWCA_API AgentID GetHeroAgentID(uint32_t hero_index);

        // Might be bugged, avoid to use.
        GWCA_API wchar_t* GetAgentEncName(const Agent* agent);
        GWCA_API wchar_t* GetAgentEncName(uint32_t agent_id);
    };
}
// ============================================================
// C Interop API
// ============================================================
extern "C" {
    GWCA_API bool     SendDialog(uint32_t dialog_id);

    GWCA_API uint32_t GetObservingId();
    GWCA_API uint32_t GetControlledCharacterId();
    GWCA_API uint32_t GetTargetId();
    GWCA_API uint32_t GetEvaluatedTargetId();

    GWCA_API void* GetAgentByID(uint32_t agent_id);
    GWCA_API void* GetPlayerByID(uint32_t player_id);
    GWCA_API void* GetControlledCharacter();
    GWCA_API void* GetTargetAsAgentLiving();
    GWCA_API void* GetMapAgentByID(uint32_t agent_id);
    GWCA_API void* GetNPCByID(uint32_t npc_id);

    GWCA_API bool     IsObserving();
    GWCA_API uint32_t GetAmountOfPlayersInInstance();
    GWCA_API uint32_t CountAllegianceInRange(uint32_t allegiance, float sqr_range);

    GWCA_API bool     ChangeTargetByAgent(const void* agent);
    GWCA_API bool     ChangeTargetById(uint32_t agent_id);

    GWCA_API bool     Move(float x, float y, uint32_t zplane);
    GWCA_API bool     InteractAgent(const void* agent, bool call_target);

    GWCA_API const wchar_t* GetPlayerNameByLoginNumber(uint32_t login_number);
    GWCA_API uint32_t GetAgentIdByLoginNumber(uint32_t login_number);
    GWCA_API uint32_t GetHeroAgentID(uint32_t hero_index);

    GWCA_API const wchar_t* GetAgentEncNameByAgent(const void* agent);
    GWCA_API const wchar_t* GetAgentEncNameById(uint32_t agent_id);
}