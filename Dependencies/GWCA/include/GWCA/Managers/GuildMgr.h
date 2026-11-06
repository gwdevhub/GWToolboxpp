#pragma once
#include <GWCA/Runtime.h>
#include <GWCA/Utilities/Export.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameEntities/Guild.h>
namespace GW {
    struct Guild;
    struct GuildContext;
    typedef Array<Guild*> GuildArray;
    struct GHKey;
    struct Module;
    extern Module GuildModule;
    namespace GuildMgr {
        GuildArray* GetGuildArray();
        Guild* GetPlayerGuild();
        Guild* GetCurrentGH();
        Guild* GetGuildInfo(uint32_t guild_id);
        uint32_t GetPlayerGuildIndex();
        wchar_t* GetPlayerGuildAnnouncement();
        wchar_t* GetPlayerGuildAnnouncer();
        bool TravelGHForPlayer();
        bool TravelGHByKey(GHKey key);
        inline bool TravelGH() { return TravelGHForPlayer(); }
        inline bool TravelGH(GHKey key) { return TravelGHByKey(key); }
        bool LeaveGH();
    };
}

// C Interop API

namespace GW::GuildMgr {
    extern "C" GWCA_API GuildArray* __cdecl GW_GuildMgr_GetGuildArray() noexcept;
    extern "C" GWCA_API Guild* __cdecl GW_GuildMgr_GetPlayerGuild() noexcept;
    extern "C" GWCA_API Guild* __cdecl GW_GuildMgr_GetCurrentGH() noexcept;
    extern "C" GWCA_API Guild* __cdecl GW_GuildMgr_GetGuildInfo(uint32_t guild_id) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_GuildMgr_GetPlayerGuildIndex() noexcept;
    extern "C" GWCA_API wchar_t* __cdecl GW_GuildMgr_GetPlayerGuildAnnouncement() noexcept;
    extern "C" GWCA_API wchar_t* __cdecl GW_GuildMgr_GetPlayerGuildAnnouncer() noexcept;
    extern "C" GWCA_API bool __cdecl GW_GuildMgr_TravelGHForPlayer() noexcept;
    extern "C" GWCA_API bool __cdecl GW_GuildMgr_TravelGHByKey(GHKey key) noexcept;
    extern "C" GWCA_API bool __cdecl GW_GuildMgr_LeaveGH() noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW::GuildMgr {
    inline GuildArray* GetGuildArray()
    {
        return GWCA_CALL_C(GW_GuildMgr_GetGuildArray);
    }

    inline Guild* GetPlayerGuild()
    {
        return GWCA_CALL_C(GW_GuildMgr_GetPlayerGuild);
    }

    inline Guild* GetCurrentGH()
    {
        return GWCA_CALL_C(GW_GuildMgr_GetCurrentGH);
    }

    inline Guild* GetGuildInfo(uint32_t guild_id)
    {
        return GWCA_CALL_C(GW_GuildMgr_GetGuildInfo, guild_id);
    }

    inline uint32_t GetPlayerGuildIndex()
    {
        return GWCA_CALL_C(GW_GuildMgr_GetPlayerGuildIndex);
    }

    inline wchar_t* GetPlayerGuildAnnouncement()
    {
        return GWCA_CALL_C(GW_GuildMgr_GetPlayerGuildAnnouncement);
    }

    inline wchar_t* GetPlayerGuildAnnouncer()
    {
        return GWCA_CALL_C(GW_GuildMgr_GetPlayerGuildAnnouncer);
    }

    inline bool TravelGHForPlayer()
    {
        return GWCA_CALL_C(GW_GuildMgr_TravelGHForPlayer);
    }

    inline bool TravelGHByKey(GHKey key)
    {
        return GWCA_CALL_C(GW_GuildMgr_TravelGHByKey, key);
    }

    inline bool LeaveGH()
    {
        return GWCA_CALL_C(GW_GuildMgr_LeaveGH);
    }
}
#endif
