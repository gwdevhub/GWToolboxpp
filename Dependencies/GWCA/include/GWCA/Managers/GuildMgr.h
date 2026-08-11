#pragma once
#include <GWCA/Utilities/Export.h>
namespace GW {
    struct Guild;
    struct GuildContext;
    typedef Array<Guild*> GuildArray;
    struct GHKey;
    struct Module;
    extern Module GuildModule;
    namespace GuildMgr {
        GWCA_API GuildArray* GetGuildArray();
        GWCA_API Guild* GetPlayerGuild();
        GWCA_API Guild* GetCurrentGH();
        GWCA_API Guild* GetGuildInfo(uint32_t guild_id);
        GWCA_API uint32_t GetPlayerGuildIndex();
        GWCA_API wchar_t* GetPlayerGuildAnnouncement();
        GWCA_API wchar_t* GetPlayerGuildAnnouncer();
        GWCA_API bool TravelGH();
        GWCA_API bool TravelGH(GHKey key);
        GWCA_API bool LeaveGH();
    };
}

// C Interop API
extern "C" {
    GWCA_API void* GetPlayerGuild();
    GWCA_API void* GetCurrentGH();
    GWCA_API void* GetGuildInfo(uint32_t guild_id);
    GWCA_API uint32_t       GetPlayerGuildIndex();
    GWCA_API const wchar_t* GetPlayerGuildAnnouncement();
    GWCA_API const wchar_t* GetPlayerGuildAnnouncer();
    GWCA_API bool           TravelGH();
    GWCA_API bool           LeaveGH();
}