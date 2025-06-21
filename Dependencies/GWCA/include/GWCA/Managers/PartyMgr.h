#pragma once

#include <GWCA/Utilities/Export.h>

namespace GW {
    struct GamePos;
    struct PartyInfo;
    struct PartySearch;
    struct PetInfo;
    struct HeroInfo;

    struct Attribute;
    enum class HeroBehavior : uint32_t;

    typedef uint32_t AgentID;

    struct Module;
    extern Module PartyModule;

    namespace PartyMgr {

        // set or unset the fact that ticking will work as a toggle instead
        // of showing a drop-down menu

        GWCA_API void SetTickToggle(bool enable);

        // Set party ready status.
        GWCA_API bool Tick(bool flag = true);

        GWCA_API Attribute* GetAgentAttributes(uint32_t agent_id);

        GWCA_API PartySearch* GetPartySearch(uint32_t party_search_id = 0);

        GWCA_API PartyInfo *GetPartyInfo(uint32_t party_id = 0);

        GWCA_API uint32_t GetPartySize();
        GWCA_API uint32_t GetPartyPlayerCount();
        GWCA_API uint32_t GetPartyHeroCount();
        GWCA_API uint32_t GetPartyHenchmanCount();

        GWCA_API bool GetIsPartyDefeated();

        GWCA_API bool SetHardMode(bool flag);

        // When defeated, tell the client to return to outpost.
        GWCA_API bool ReturnToOutpost();

        GWCA_API bool GetIsPartyInHardMode();

        GWCA_API bool GetIsHardModeUnlocked();

        // check if the whole party is ticked
        GWCA_API bool GetIsPartyTicked();

        // check if selected party member is ticked
        GWCA_API bool GetIsPlayerTicked(uint32_t player_index = -1);

        // check if the whole party is loaded
        GWCA_API bool GetIsPartyLoaded();

        // returns if the player agent is leader
        GWCA_API bool GetIsLeader();

        // Accept or reject a party invitation via party window
        GWCA_API bool RespondToPartyRequest(uint32_t party_id, bool accept);

        GWCA_API bool LeaveParty();

        // hero managment
        GWCA_API bool AddHero(uint32_t heroid);
        GWCA_API bool KickHero(uint32_t heroid);
        GWCA_API bool KickAllHeroes();
        GWCA_API bool AddHenchman(uint32_t agent_id);
        GWCA_API bool KickHenchman(uint32_t agent_id);
        GWCA_API bool InvitePlayer(uint32_t player_id);
        GWCA_API bool InvitePlayer(const wchar_t* player_name);
        GWCA_API bool KickPlayer(uint32_t player_id);

        // hero flagging
        GWCA_API bool FlagHero(uint32_t hero_index, GamePos pos);
        GWCA_API bool UnflagHero(uint32_t hero_index);

        GWCA_API bool FlagAll(GamePos pos);
        GWCA_API bool UnflagAll();
        GWCA_API bool SetHeroTarget(uint32_t hero_agent_id, uint32_t target_agent_id = 0);
        GWCA_API bool SetHeroBehavior(uint32_t hero_agent_id, HeroBehavior behavior);
        GWCA_API bool SetPetBehavior(uint32_t owner_agent_id, HeroBehavior behavior);

        GWCA_API PetInfo* GetPetInfo(uint32_t owner_agent_id = 0);

        GWCA_API uint32_t GetHeroAgentID(uint32_t hero_index);

        GWCA_API HeroInfo* GetHeroInfo(uint32_t hero_id);

        // Advertise your party in party search window
        GWCA_API bool SearchParty(uint32_t search_type, const wchar_t* advertisement = nullptr);
        // Cancel party advertisement
        GWCA_API bool SearchPartyCancel();
        // Accept or reject a party invitation via party search window
        GWCA_API bool SearchPartyReply(bool accept);
    };
}
