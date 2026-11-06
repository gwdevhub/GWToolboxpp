#pragma once
#include <GWCA/Runtime.h>

#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Utilities/Export.h>
#include <GWCA/GameContainers/Array.h>

namespace GW {

    struct AreaInfo;
    struct PathingMap;
    struct MissionMapIcon;
    enum class Continent : uint32_t;

    typedef Array<PathingMap> PathingMapArray;
    typedef Array<MissionMapIcon> MissionMapIconArray;

    struct MapContext;

    namespace Constants {
        enum class MapID : uint32_t;
        enum class District;
        enum class InstanceType;
        enum class Language;
        enum class ServerRegion;
    }

    enum class RegionType : uint32_t;
    struct MapTypeInstanceInfo {
        uint32_t request_instance_map_type; // Used for auth server
        bool is_outpost;
        RegionType map_region_type;
    };
    struct Module;
    extern Module MapModule;
    struct MissionMapSubContext {
        uint32_t h0000[0xe]; // Could be 0x38, 0x3C or 0x68 depending on if observing etc.
    };
    struct MissionMapSubContext2 {
        uint32_t h0000;
        Vec2f player_mission_map_pos;
        uint32_t h000c;
        GW::Vec2f mission_map_size;
        float unk;
        GW::Vec2f mission_map_pan_offset;
        GW::Vec2f mission_map_pan_offset2;
        float unk2[2];
        uint32_t unk3[9];
    };
    static_assert(sizeof(MissionMapSubContext2) == 0x58);

    struct MissionMapContext {
        GW::Vec2f size; // Dimensions of the drawable area inside the mission map frame
        uint32_t h0008;
        GW::Vec2f last_mouse_location; // Percentage offset (-1.f to 1.f) relative to player_mission_map_pos
        uint32_t frame_id;
        GW::Vec2f player_mission_map_pos; // Position of player on the top down view of mission map (not in gwinches). Mission map centers on this point
        GW::Array<MissionMapSubContext*> h0020;
        uint32_t h0030;
        uint32_t h0034;
        uint32_t h0038;
        MissionMapSubContext2* h003c;
        uint32_t h0040;
        uint32_t h0044;
    };
    static_assert(sizeof(MissionMapContext) == 0x48);

    struct WorldMapContext {
        uint32_t frame_id;
        GW::Continent continent;
        uint32_t h0008;
        float h000c;
        float h0010;
        uint32_t h0014;
        float h0018;
        float h001c;
        float h0020;
        float h0024;
        float h0028;
        float h002c;
        float h0030;
        float h0034;
        float zoom; // 1.0f if zoomed in, 0.0f if zoomed out
        GW::Vec2f top_left; // Viewport position relative to world map, start
        GW::Vec2f bottom_right; // Viewport position relative to world map, end
        uint32_t h004c[7];
        float h0068;
        float h006c;
        uint32_t params[0x6d];
    };
    static_assert(sizeof(WorldMapContext) == 0x224);

    namespace Map {


        MissionMapContext* GetMissionMapContext();

        WorldMapContext* GetWorldMapContext();

        // Allows you to load map geometry/pathing/props from the DAT file if you know a map's file_id. Be sure to call DestroyMapContext later to avoid a mem leak
        MapContext* CreateMapContext(uint32_t map_file_id);

        // Call this to free a MapContext created via CreateMapContext
        bool DestroyMapContext(MapContext*);

        float QueryAltitude(const GamePos* pos, float radius = 5.f, GW::MapContext* context = 0);

        bool GetIsMapLoaded();

        // Get current map ID.
        Constants::MapID GetMapID();

        // Get current region you are in.
        bool GetIsMapUnlocked(Constants::MapID map_id);

        // Get current region you are in.
        GW::Constants::ServerRegion GetRegion();

        // Get current language you are in.
        GW::Constants::Language GetLanguage();

        // Get whether current character is observing a match
        bool GetIsObserving();

        // Get the district number you are in.
        int GetDistrict();

		size_t GetDistrictCount();

        // Get time, in ms, since the instance you are residing in has been created.
        uint32_t GetInstanceTime();

        // Get the instance type (Outpost, Explorable or Loading)
        Constants::InstanceType GetInstanceType();

        // Travel to specified outpost.
        bool TravelByRegion(Constants::MapID map_id, GW::Constants::ServerRegion region, int district_number = 0, GW::Constants::Language language = (GW::Constants::Language)0);
        inline bool Travel(Constants::MapID map_id, GW::Constants::ServerRegion region, int district_number = 0, GW::Constants::Language language = (GW::Constants::Language)0) { return TravelByRegion(map_id, region, district_number, language); }

        bool TravelByDistrict(Constants::MapID map_id, Constants::District district = (Constants::District)0, int district_number = 0);
        inline bool Travel(Constants::MapID map_id, Constants::District district = (Constants::District)0, int district_number = 0) { return TravelByDistrict(map_id, district, district_number); }

        Constants::ServerRegion RegionFromDistrict(const GW::Constants::District _district);

        Constants::Language LanguageFromDistrict(const GW::Constants::District _district);

        // Array of icons (res shrines, quarries, traders...) on the mission map; see MissionMapIcon.
        MissionMapIconArray* GetMissionMapIconArray();

        // Returns pointer of collision trapezoid array.
        PathingMapArray* GetPathingMap();

        uint32_t GetFoesKilled();
        uint32_t GetFoesToKill();

        AreaInfo *GetMapInfo(Constants::MapID map_id = (Constants::MapID)0);

        inline AreaInfo *GetCurrentMapInfo() {
            Constants::MapID map_id = GetMapID();
            return GetMapInfo(map_id);
        }

        bool GetIsInCinematic();
        bool SkipCinematic();

        bool EnterChallenge();
        bool CancelEnterChallenge();
    };
}
// C Interop API

namespace GW::Map {
    extern "C" GWCA_API MissionMapContext* __cdecl GW_Map_GetMissionMapContext() noexcept;
    extern "C" GWCA_API WorldMapContext* __cdecl GW_Map_GetWorldMapContext() noexcept;
    extern "C" GWCA_API MapContext* __cdecl GW_Map_CreateMapContext(uint32_t map_file_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Map_DestroyMapContext(MapContext* gwca_arg0) noexcept;
    extern "C" GWCA_API float __cdecl GW_Map_QueryAltitude(const GamePos* pos, float radius, GW::MapContext* context) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Map_GetIsMapLoaded() noexcept;
    extern "C" GWCA_API Constants::MapID __cdecl GW_Map_GetMapID() noexcept;
    extern "C" GWCA_API bool __cdecl GW_Map_GetIsMapUnlocked(Constants::MapID map_id) noexcept;
    extern "C" GWCA_API GW::Constants::ServerRegion __cdecl GW_Map_GetRegion() noexcept;
    extern "C" GWCA_API GW::Constants::Language __cdecl GW_Map_GetLanguage() noexcept;
    extern "C" GWCA_API bool __cdecl GW_Map_GetIsObserving() noexcept;
    extern "C" GWCA_API int __cdecl GW_Map_GetDistrict() noexcept;
    extern "C" GWCA_API size_t __cdecl GW_Map_GetDistrictCount() noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Map_GetInstanceTime() noexcept;
    extern "C" GWCA_API Constants::InstanceType __cdecl GW_Map_GetInstanceType() noexcept;
    extern "C" GWCA_API bool __cdecl GW_Map_TravelByRegion(Constants::MapID map_id, GW::Constants::ServerRegion region, int district_number, GW::Constants::Language language) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Map_TravelByDistrict(Constants::MapID map_id, Constants::District district, int district_number) noexcept;
    extern "C" GWCA_API Constants::ServerRegion __cdecl GW_Map_RegionFromDistrict(const GW::Constants::District _district) noexcept;
    extern "C" GWCA_API Constants::Language __cdecl GW_Map_LanguageFromDistrict(const GW::Constants::District _district) noexcept;
    extern "C" GWCA_API MissionMapIconArray* __cdecl GW_Map_GetMissionMapIconArray() noexcept;
    extern "C" GWCA_API PathingMapArray* __cdecl GW_Map_GetPathingMap() noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Map_GetFoesKilled() noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Map_GetFoesToKill() noexcept;
    extern "C" GWCA_API AreaInfo* __cdecl GW_Map_GetMapInfo(Constants::MapID map_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Map_GetIsInCinematic() noexcept;
    extern "C" GWCA_API bool __cdecl GW_Map_SkipCinematic() noexcept;
    extern "C" GWCA_API bool __cdecl GW_Map_EnterChallenge() noexcept;
    extern "C" GWCA_API bool __cdecl GW_Map_CancelEnterChallenge() noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW::Map {
    inline MissionMapContext* GetMissionMapContext()
    {
        return GWCA_CALL_C(GW_Map_GetMissionMapContext);
    }

    inline WorldMapContext* GetWorldMapContext()
    {
        return GWCA_CALL_C(GW_Map_GetWorldMapContext);
    }

    inline MapContext* CreateMapContext(uint32_t map_file_id)
    {
        return GWCA_CALL_C(GW_Map_CreateMapContext, map_file_id);
    }

    inline bool DestroyMapContext(MapContext* gwca_arg0)
    {
        return GWCA_CALL_C(GW_Map_DestroyMapContext, gwca_arg0);
    }

    inline float QueryAltitude(const GamePos* pos, float radius, GW::MapContext* context)
    {
        return GWCA_CALL_C(GW_Map_QueryAltitude, pos, radius, context);
    }

    inline bool GetIsMapLoaded()
    {
        return GWCA_CALL_C(GW_Map_GetIsMapLoaded);
    }

    inline Constants::MapID GetMapID()
    {
        return GWCA_CALL_C(GW_Map_GetMapID);
    }

    inline bool GetIsMapUnlocked(Constants::MapID map_id)
    {
        return GWCA_CALL_C(GW_Map_GetIsMapUnlocked, map_id);
    }

    inline GW::Constants::ServerRegion GetRegion()
    {
        return GWCA_CALL_C(GW_Map_GetRegion);
    }

    inline GW::Constants::Language GetLanguage()
    {
        return GWCA_CALL_C(GW_Map_GetLanguage);
    }

    inline bool GetIsObserving()
    {
        return GWCA_CALL_C(GW_Map_GetIsObserving);
    }

    inline int GetDistrict()
    {
        return GWCA_CALL_C(GW_Map_GetDistrict);
    }

    inline size_t GetDistrictCount()
    {
        return GWCA_CALL_C(GW_Map_GetDistrictCount);
    }

    inline uint32_t GetInstanceTime()
    {
        return GWCA_CALL_C(GW_Map_GetInstanceTime);
    }

    inline Constants::InstanceType GetInstanceType()
    {
        return GWCA_CALL_C(GW_Map_GetInstanceType);
    }

    inline bool TravelByRegion(Constants::MapID map_id, GW::Constants::ServerRegion region, int district_number, GW::Constants::Language language)
    {
        return GWCA_CALL_C(GW_Map_TravelByRegion, map_id, region, district_number, language);
    }

    inline bool TravelByDistrict(Constants::MapID map_id, Constants::District district, int district_number)
    {
        return GWCA_CALL_C(GW_Map_TravelByDistrict, map_id, district, district_number);
    }

    inline Constants::ServerRegion RegionFromDistrict(const GW::Constants::District _district)
    {
        return GWCA_CALL_C(GW_Map_RegionFromDistrict, _district);
    }

    inline Constants::Language LanguageFromDistrict(const GW::Constants::District _district)
    {
        return GWCA_CALL_C(GW_Map_LanguageFromDistrict, _district);
    }

    inline MissionMapIconArray* GetMissionMapIconArray()
    {
        return GWCA_CALL_C(GW_Map_GetMissionMapIconArray);
    }

    inline PathingMapArray* GetPathingMap()
    {
        return GWCA_CALL_C(GW_Map_GetPathingMap);
    }

    inline uint32_t GetFoesKilled()
    {
        return GWCA_CALL_C(GW_Map_GetFoesKilled);
    }

    inline uint32_t GetFoesToKill()
    {
        return GWCA_CALL_C(GW_Map_GetFoesToKill);
    }

    inline AreaInfo* GetMapInfo(Constants::MapID map_id)
    {
        return GWCA_CALL_C(GW_Map_GetMapInfo, map_id);
    }

    inline bool GetIsInCinematic()
    {
        return GWCA_CALL_C(GW_Map_GetIsInCinematic);
    }

    inline bool SkipCinematic()
    {
        return GWCA_CALL_C(GW_Map_SkipCinematic);
    }

    inline bool EnterChallenge()
    {
        return GWCA_CALL_C(GW_Map_EnterChallenge);
    }

    inline bool CancelEnterChallenge()
    {
        return GWCA_CALL_C(GW_Map_CancelEnterChallenge);
    }
}
#endif
