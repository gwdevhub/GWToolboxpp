#pragma once
#include <GWCA/Runtime.h>

#include <GWCA/Utilities/Export.h>
#include <GWCA/GameContainers/GamePos.h>

namespace GW {
    struct PartyInfo;
    struct PartySearch;
    struct PetInfo;
    struct HeroInfo;
    struct HeroConstData;

    struct Attribute;
    enum class HeroBehavior : uint32_t;

    typedef uint32_t AgentID;

    struct Module;
    extern Module PartyModule;
    namespace Constants {
        enum HeroID : uint32_t;
    }

    namespace PartyMgr {

        // Set or unset whether ticking works as a toggle instead of showing a drop-down menu.

        void SetTickToggle(bool enable);

        // Set party ready status.
        bool Tick(bool flag = true);

        Attribute* GetAgentAttributes(uint32_t agent_id);

        PartySearch* GetPartySearch(uint32_t party_search_id = 0);

        PartyInfo *GetPartyInfo(uint32_t party_id = 0);

        uint32_t GetPartySize();
        uint32_t GetPartyPlayerCount();
        uint32_t GetPartyHeroCount();
        uint32_t GetPartyHenchmanCount();

        bool GetIsPartyDefeated();

        bool SetHardMode(bool flag);

        // When defeated, tell the client to return to outpost.
        bool ReturnToOutpost();

        bool GetIsPartyInHardMode();

        bool GetIsHardModeUnlocked();

        // check if the whole party is ticked
        bool GetIsPartyTicked();

        // check if selected party member is ticked
        bool GetIsPlayerTicked(uint32_t player_index = -1);

        // check if the whole party is loaded
        bool GetIsPartyLoaded();

        // returns if the player agent is leader
        bool GetIsLeader();

        // Accept or reject a party invitation via party window
        bool RespondToPartyRequest(uint32_t party_id, bool accept);

        bool LeaveParty();

        // hero managment
        bool AddHero(GW::Constants::HeroID heroid);
        bool KickHero(GW::Constants::HeroID heroid);
        bool KickAllHeroes();
        bool AddHenchman(uint32_t agent_id);
        bool KickHenchman(uint32_t agent_id);
        bool InvitePlayerById(uint32_t player_id);
        bool InvitePlayerByName(const wchar_t* player_name);
        inline bool InvitePlayer(uint32_t player_id) { return InvitePlayerById(player_id); }
        inline bool InvitePlayer(const wchar_t* player_name) { return InvitePlayerByName(player_name); }
        bool KickPlayer(uint32_t player_id);

        // hero flagging
        bool FlagHero(uint32_t hero_index, GamePos pos);
        bool UnflagHero(uint32_t hero_index);

        bool FlagAll(GamePos pos);
        bool UnflagAll();
        bool SetHeroTarget(uint32_t hero_agent_id, uint32_t target_agent_id = 0);
        bool SetHeroBehavior(uint32_t hero_agent_id, HeroBehavior behavior);
        bool SetPetBehavior(uint32_t owner_agent_id, HeroBehavior behavior);
        bool SetHeroSkillDisabled(uint32_t hero_agent_id, uint32_t slot_index, bool disabled = true);

        PetInfo* GetPetInfo(uint32_t owner_agent_id = 0);

        uint32_t GetHeroAgentID(uint32_t hero_index);

        HeroInfo* GetHeroInfo(GW::Constants::HeroID hero_id);

        HeroConstData* GetHeroConstData(GW::Constants::HeroID hero_id);

        // Advertise your party in party search window
        bool SearchParty(uint32_t search_type, const wchar_t* advertisement = nullptr);
        // Cancel party advertisement
        bool SearchPartyCancel();
        // Accept or reject a party invitation via party search window
        bool SearchPartyReply(uint32_t party_search_id, bool accept);
    };
}

namespace GW::PartyMgr {
    extern "C" GWCA_API void __cdecl GW_PartyMgr_SetTickToggle(bool enable) noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_Tick(bool flag) noexcept;
    extern "C" GWCA_API Attribute* __cdecl GW_PartyMgr_GetAgentAttributes(uint32_t agent_id) noexcept;
    extern "C" GWCA_API PartySearch* __cdecl GW_PartyMgr_GetPartySearch(uint32_t party_search_id) noexcept;
    extern "C" GWCA_API PartyInfo* __cdecl GW_PartyMgr_GetPartyInfo(uint32_t party_id) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_PartyMgr_GetPartySize() noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_PartyMgr_GetPartyPlayerCount() noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_PartyMgr_GetPartyHeroCount() noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_PartyMgr_GetPartyHenchmanCount() noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_GetIsPartyDefeated() noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_SetHardMode(bool flag) noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_ReturnToOutpost() noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_GetIsPartyInHardMode() noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_GetIsHardModeUnlocked() noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_GetIsPartyTicked() noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_GetIsPlayerTicked(uint32_t player_index) noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_GetIsPartyLoaded() noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_GetIsLeader() noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_RespondToPartyRequest(uint32_t party_id, bool accept) noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_LeaveParty() noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_AddHero(GW::Constants::HeroID heroid) noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_KickHero(GW::Constants::HeroID heroid) noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_KickAllHeroes() noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_AddHenchman(uint32_t agent_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_KickHenchman(uint32_t agent_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_InvitePlayerById(uint32_t player_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_InvitePlayerByName(const wchar_t* player_name) noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_KickPlayer(uint32_t player_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_FlagHero(uint32_t hero_index, GamePos pos) noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_UnflagHero(uint32_t hero_index) noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_FlagAll(GamePos pos) noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_UnflagAll() noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_SetHeroTarget(uint32_t hero_agent_id, uint32_t target_agent_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_SetHeroBehavior(uint32_t hero_agent_id, HeroBehavior behavior) noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_SetPetBehavior(uint32_t owner_agent_id, HeroBehavior behavior) noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_SetHeroSkillDisabled(uint32_t hero_agent_id, uint32_t slot_index, bool disabled) noexcept;
    extern "C" GWCA_API PetInfo* __cdecl GW_PartyMgr_GetPetInfo(uint32_t owner_agent_id) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_PartyMgr_GetHeroAgentID(uint32_t hero_index) noexcept;
    extern "C" GWCA_API HeroInfo* __cdecl GW_PartyMgr_GetHeroInfo(GW::Constants::HeroID hero_id) noexcept;
    extern "C" GWCA_API HeroConstData* __cdecl GW_PartyMgr_GetHeroConstData(GW::Constants::HeroID hero_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_SearchParty(uint32_t search_type, const wchar_t* advertisement) noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_SearchPartyCancel() noexcept;
    extern "C" GWCA_API bool __cdecl GW_PartyMgr_SearchPartyReply(uint32_t party_search_id, bool accept) noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW::PartyMgr {
    inline void SetTickToggle(bool enable)
    {
        return GWCA_CALL_C(GW_PartyMgr_SetTickToggle, enable);
    }

    inline bool Tick(bool flag)
    {
        return GWCA_CALL_C(GW_PartyMgr_Tick, flag);
    }

    inline Attribute* GetAgentAttributes(uint32_t agent_id)
    {
        return GWCA_CALL_C(GW_PartyMgr_GetAgentAttributes, agent_id);
    }

    inline PartySearch* GetPartySearch(uint32_t party_search_id)
    {
        return GWCA_CALL_C(GW_PartyMgr_GetPartySearch, party_search_id);
    }

    inline PartyInfo* GetPartyInfo(uint32_t party_id)
    {
        return GWCA_CALL_C(GW_PartyMgr_GetPartyInfo, party_id);
    }

    inline uint32_t GetPartySize()
    {
        return GWCA_CALL_C(GW_PartyMgr_GetPartySize);
    }

    inline uint32_t GetPartyPlayerCount()
    {
        return GWCA_CALL_C(GW_PartyMgr_GetPartyPlayerCount);
    }

    inline uint32_t GetPartyHeroCount()
    {
        return GWCA_CALL_C(GW_PartyMgr_GetPartyHeroCount);
    }

    inline uint32_t GetPartyHenchmanCount()
    {
        return GWCA_CALL_C(GW_PartyMgr_GetPartyHenchmanCount);
    }

    inline bool GetIsPartyDefeated()
    {
        return GWCA_CALL_C(GW_PartyMgr_GetIsPartyDefeated);
    }

    inline bool SetHardMode(bool flag)
    {
        return GWCA_CALL_C(GW_PartyMgr_SetHardMode, flag);
    }

    inline bool ReturnToOutpost()
    {
        return GWCA_CALL_C(GW_PartyMgr_ReturnToOutpost);
    }

    inline bool GetIsPartyInHardMode()
    {
        return GWCA_CALL_C(GW_PartyMgr_GetIsPartyInHardMode);
    }

    inline bool GetIsHardModeUnlocked()
    {
        return GWCA_CALL_C(GW_PartyMgr_GetIsHardModeUnlocked);
    }

    inline bool GetIsPartyTicked()
    {
        return GWCA_CALL_C(GW_PartyMgr_GetIsPartyTicked);
    }

    inline bool GetIsPlayerTicked(uint32_t player_index)
    {
        return GWCA_CALL_C(GW_PartyMgr_GetIsPlayerTicked, player_index);
    }

    inline bool GetIsPartyLoaded()
    {
        return GWCA_CALL_C(GW_PartyMgr_GetIsPartyLoaded);
    }

    inline bool GetIsLeader()
    {
        return GWCA_CALL_C(GW_PartyMgr_GetIsLeader);
    }

    inline bool RespondToPartyRequest(uint32_t party_id, bool accept)
    {
        return GWCA_CALL_C(GW_PartyMgr_RespondToPartyRequest, party_id, accept);
    }

    inline bool LeaveParty()
    {
        return GWCA_CALL_C(GW_PartyMgr_LeaveParty);
    }

    inline bool AddHero(GW::Constants::HeroID heroid)
    {
        return GWCA_CALL_C(GW_PartyMgr_AddHero, heroid);
    }

    inline bool KickHero(GW::Constants::HeroID heroid)
    {
        return GWCA_CALL_C(GW_PartyMgr_KickHero, heroid);
    }

    inline bool KickAllHeroes()
    {
        return GWCA_CALL_C(GW_PartyMgr_KickAllHeroes);
    }

    inline bool AddHenchman(uint32_t agent_id)
    {
        return GWCA_CALL_C(GW_PartyMgr_AddHenchman, agent_id);
    }

    inline bool KickHenchman(uint32_t agent_id)
    {
        return GWCA_CALL_C(GW_PartyMgr_KickHenchman, agent_id);
    }

    inline bool InvitePlayerById(uint32_t player_id)
    {
        return GWCA_CALL_C(GW_PartyMgr_InvitePlayerById, player_id);
    }

    inline bool InvitePlayerByName(const wchar_t* player_name)
    {
        return GWCA_CALL_C(GW_PartyMgr_InvitePlayerByName, player_name);
    }

    inline bool KickPlayer(uint32_t player_id)
    {
        return GWCA_CALL_C(GW_PartyMgr_KickPlayer, player_id);
    }

    inline bool FlagHero(uint32_t hero_index, GamePos pos)
    {
        return GWCA_CALL_C(GW_PartyMgr_FlagHero, hero_index, pos);
    }

    inline bool UnflagHero(uint32_t hero_index)
    {
        return GWCA_CALL_C(GW_PartyMgr_UnflagHero, hero_index);
    }

    inline bool FlagAll(GamePos pos)
    {
        return GWCA_CALL_C(GW_PartyMgr_FlagAll, pos);
    }

    inline bool UnflagAll()
    {
        return GWCA_CALL_C(GW_PartyMgr_UnflagAll);
    }

    inline bool SetHeroTarget(uint32_t hero_agent_id, uint32_t target_agent_id)
    {
        return GWCA_CALL_C(GW_PartyMgr_SetHeroTarget, hero_agent_id, target_agent_id);
    }

    inline bool SetHeroBehavior(uint32_t hero_agent_id, HeroBehavior behavior)
    {
        return GWCA_CALL_C(GW_PartyMgr_SetHeroBehavior, hero_agent_id, behavior);
    }

    inline bool SetPetBehavior(uint32_t owner_agent_id, HeroBehavior behavior)
    {
        return GWCA_CALL_C(GW_PartyMgr_SetPetBehavior, owner_agent_id, behavior);
    }

    inline bool SetHeroSkillDisabled(uint32_t hero_agent_id, uint32_t slot_index, bool disabled)
    {
        return GWCA_CALL_C(GW_PartyMgr_SetHeroSkillDisabled, hero_agent_id, slot_index, disabled);
    }

    inline PetInfo* GetPetInfo(uint32_t owner_agent_id)
    {
        return GWCA_CALL_C(GW_PartyMgr_GetPetInfo, owner_agent_id);
    }

    inline uint32_t GetHeroAgentID(uint32_t hero_index)
    {
        return GWCA_CALL_C(GW_PartyMgr_GetHeroAgentID, hero_index);
    }

    inline HeroInfo* GetHeroInfo(GW::Constants::HeroID hero_id)
    {
        return GWCA_CALL_C(GW_PartyMgr_GetHeroInfo, hero_id);
    }

    inline HeroConstData* GetHeroConstData(GW::Constants::HeroID hero_id)
    {
        return GWCA_CALL_C(GW_PartyMgr_GetHeroConstData, hero_id);
    }

    inline bool SearchParty(uint32_t search_type, const wchar_t* advertisement)
    {
        return GWCA_CALL_C(GW_PartyMgr_SearchParty, search_type, advertisement);
    }

    inline bool SearchPartyCancel()
    {
        return GWCA_CALL_C(GW_PartyMgr_SearchPartyCancel);
    }

    inline bool SearchPartyReply(uint32_t party_search_id, bool accept)
    {
        return GWCA_CALL_C(GW_PartyMgr_SearchPartyReply, party_search_id, accept);
    }
}
#endif
