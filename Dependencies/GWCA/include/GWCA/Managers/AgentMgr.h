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


    namespace Agents {
        // === Dialogs ===
        // Same as pressing button (id) while talking to an NPC.
        GWCA_API bool SendDialog(uint32_t dialog_id);

        // Returns last dialog id sent to the server. Requires the hook.
        GWCA_API bool GetIsAgentTargettable(const GW::Agent* agent);

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

        // Whether we're observing someone else
        GWCA_API bool IsObserving();

        // Get AgentLiving of current target
        GWCA_API AgentLiving    *GetTargetAsAgentLiving();

        GWCA_API uint32_t GetAmountOfPlayersInInstance();

        // Returns array of alternate agent array that can be read beyond compass range.
        // Holds limited info and needs to be explored more.
        GWCA_API MapAgentArray* GetMapAgentArray();
        GWCA_API uint32_t CountAllegianceInRange(GW::Constants::Allegiance allegiance, float sqr_range);

        GWCA_API MapAgent* GetMapAgentByID(uint32_t agent_id);

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
