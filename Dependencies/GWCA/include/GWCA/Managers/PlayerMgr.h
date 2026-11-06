#pragma once
#include <GWCA/Runtime.h>

#include <GWCA/Utilities/Export.h>

namespace GW {

    typedef uint32_t PlayerNumber;

    struct Player;
    struct Title;
    struct TitleClientData;
    typedef Array<Player> PlayerArray;

    namespace Constants {
        enum class TitleID : uint32_t;
        enum class Profession: uint32_t;
        enum class QuestID : uint32_t;
    }

    struct Module;
    extern Module PlayerModule;

    namespace PlayerMgr {

        bool SetActiveTitle(Constants::TitleID title_id);

        bool RemoveActiveTitle();

        uint32_t GetPlayerAgentId(uint32_t player_id);

        uint32_t GetAmountOfPlayersInInstance();

        PlayerArray* GetPlayerArray();

        // Get the player number of the currently logged in character
        PlayerNumber GetPlayerNumber();

        Player *GetPlayerByID(uint32_t player_id = 0);

        wchar_t *GetPlayerName(uint32_t player_id = 0);

        wchar_t* SetPlayerName(uint32_t player_id, const wchar_t *replace_name);

        bool ChangeSecondProfession(Constants::Profession prof, uint32_t hero_index = 0);

        Player *GetPlayerByName(const wchar_t *name);

        // Get the current progress of a title by id. If the title has no progress, this function will return nullptr
        Title* GetTitleTrack(Constants::TitleID title_id);

        Constants::TitleID GetActiveTitleId();

        Title* GetActiveTitle();

        TitleClientData* GetTitleData(Constants::TitleID title_id);
    };
}
// C Interop API

namespace GW::PlayerMgr {
    extern "C" GWCA_API bool __cdecl GW_PlayerMgr_SetActiveTitle(Constants::TitleID title_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_PlayerMgr_RemoveActiveTitle() noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_PlayerMgr_GetPlayerAgentId(uint32_t player_id) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_PlayerMgr_GetAmountOfPlayersInInstance() noexcept;
    extern "C" GWCA_API PlayerArray* __cdecl GW_PlayerMgr_GetPlayerArray() noexcept;
    extern "C" GWCA_API PlayerNumber __cdecl GW_PlayerMgr_GetPlayerNumber() noexcept;
    extern "C" GWCA_API Player* __cdecl GW_PlayerMgr_GetPlayerByID(uint32_t player_id) noexcept;
    extern "C" GWCA_API wchar_t* __cdecl GW_PlayerMgr_GetPlayerName(uint32_t player_id) noexcept;
    extern "C" GWCA_API wchar_t* __cdecl GW_PlayerMgr_SetPlayerName(uint32_t player_id, const wchar_t* replace_name) noexcept;
    extern "C" GWCA_API bool __cdecl GW_PlayerMgr_ChangeSecondProfession(Constants::Profession prof, uint32_t hero_index) noexcept;
    extern "C" GWCA_API Player* __cdecl GW_PlayerMgr_GetPlayerByName(const wchar_t* name) noexcept;
    extern "C" GWCA_API Title* __cdecl GW_PlayerMgr_GetTitleTrack(Constants::TitleID title_id) noexcept;
    extern "C" GWCA_API Constants::TitleID __cdecl GW_PlayerMgr_GetActiveTitleId() noexcept;
    extern "C" GWCA_API Title* __cdecl GW_PlayerMgr_GetActiveTitle() noexcept;
    extern "C" GWCA_API TitleClientData* __cdecl GW_PlayerMgr_GetTitleData(Constants::TitleID title_id) noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW::PlayerMgr {
    inline bool SetActiveTitle(Constants::TitleID title_id)
    {
        return GWCA_CALL_C(GW_PlayerMgr_SetActiveTitle, title_id);
    }

    inline bool RemoveActiveTitle()
    {
        return GWCA_CALL_C(GW_PlayerMgr_RemoveActiveTitle);
    }

    inline uint32_t GetPlayerAgentId(uint32_t player_id)
    {
        return GWCA_CALL_C(GW_PlayerMgr_GetPlayerAgentId, player_id);
    }

    inline uint32_t GetAmountOfPlayersInInstance()
    {
        return GWCA_CALL_C(GW_PlayerMgr_GetAmountOfPlayersInInstance);
    }

    inline PlayerArray* GetPlayerArray()
    {
        return GWCA_CALL_C(GW_PlayerMgr_GetPlayerArray);
    }

    inline PlayerNumber GetPlayerNumber()
    {
        return GWCA_CALL_C(GW_PlayerMgr_GetPlayerNumber);
    }

    inline Player* GetPlayerByID(uint32_t player_id)
    {
        return GWCA_CALL_C(GW_PlayerMgr_GetPlayerByID, player_id);
    }

    inline wchar_t* GetPlayerName(uint32_t player_id)
    {
        return GWCA_CALL_C(GW_PlayerMgr_GetPlayerName, player_id);
    }

    inline wchar_t* SetPlayerName(uint32_t player_id, const wchar_t* replace_name)
    {
        return GWCA_CALL_C(GW_PlayerMgr_SetPlayerName, player_id, replace_name);
    }

    inline bool ChangeSecondProfession(Constants::Profession prof, uint32_t hero_index)
    {
        return GWCA_CALL_C(GW_PlayerMgr_ChangeSecondProfession, prof, hero_index);
    }

    inline Player* GetPlayerByName(const wchar_t* name)
    {
        return GWCA_CALL_C(GW_PlayerMgr_GetPlayerByName, name);
    }

    inline Title* GetTitleTrack(Constants::TitleID title_id)
    {
        return GWCA_CALL_C(GW_PlayerMgr_GetTitleTrack, title_id);
    }

    inline Constants::TitleID GetActiveTitleId()
    {
        return GWCA_CALL_C(GW_PlayerMgr_GetActiveTitleId);
    }

    inline Title* GetActiveTitle()
    {
        return GWCA_CALL_C(GW_PlayerMgr_GetActiveTitle);
    }

    inline TitleClientData* GetTitleData(Constants::TitleID title_id)
    {
        return GWCA_CALL_C(GW_PlayerMgr_GetTitleData, title_id);
    }
}
#endif
