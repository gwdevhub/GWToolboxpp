#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <Windows.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/UIMgr.h>

#include "TrackerModel.h"

namespace TrackerAdvanced {
    enum class MapUsage : uint8_t {
        Any,
        Mission,
        Dungeon,
        Vanquish
    };

    struct MapCatalogEntry {
        uint32_t map_id = 0;
        std::string name;
        bool is_mission = false;
        bool is_dungeon = false;
        bool is_vanquishable = false;
    };

    struct PendingCriterionEvent {
        bool pending = false;
        GW::UI::UIMessage message = GW::UI::UIMessage::kNone;
        uint32_t value0 = 0;
        uint32_t value1 = 0;
    };

    struct PendingGameEvents {
        PendingCriterionEvent criterion;
        bool map_loaded = false;
        uint64_t map_loaded_sequence = 0;
        bool logout = false;
        bool returning_to_character_select = false;
        uint32_t logout_unknown = 0;
        uint32_t logout_character_select = 0;
        uint32_t experience_event_count = 0;
        uint32_t last_experience_amount = 0;
        uint64_t experience_amount_total = 0;
    };

    class TrackerGame final {
    public:
        void Initialize();
        void Terminate();

        [[nodiscard]] bool IsSafeGameState() const;
        [[nodiscard]] std::wstring CurrentCharacterName() const;
        [[nodiscard]] std::optional<uint32_t> CurrentMapId() const;
        [[nodiscard]] std::string CurrentMapName() const;
        [[nodiscard]] std::optional<uint32_t> CurrentExperience() const;
        [[nodiscard]] uint64_t MapLoadedSequence() const noexcept;

        [[nodiscard]] bool EvaluateCriterion(const CriterionDefinition& criterion) const;

        void SetActiveCriterion(const CriterionDefinition* criterion);
        void SetLifecycleHooks(bool map_loaded, bool logout);
        void SetExperienceHook(bool enabled);
        [[nodiscard]] PendingGameEvents DrainPendingEvents();

        [[nodiscard]] bool IsMapCatalogReady() const;
        [[nodiscard]] const std::vector<MapCatalogEntry>& MapCatalog() const;
        [[nodiscard]] std::optional<uint32_t> ResolveMapId(std::string_view name, MapUsage usage = MapUsage::Any) const;
        [[nodiscard]] std::vector<uint32_t> ResolveMapIds(
            std::string_view name,
            MapUsage usage = MapUsage::Any) const;

    private:
        struct MapDecodeContext;

        void BuildMapCatalog();
        void AbandonMapDecodes();
        void OnMapNameDecoded(MapDecodeContext* context, const wchar_t* decoded);
        static void OnMapNameDecodedStatic(void* context, const wchar_t* decoded);

        void RegisterActiveCriterionHook(GW::UI::UIMessage message);
        void RemoveActiveCriterionHook();
        void OnActiveCriterionMessage(GW::UI::UIMessage message, void* wparam);
        void OnMapLoaded();
        void OnLogout(void* wparam);
        void OnExperienceGained(void* wparam);

        [[nodiscard]] bool IsMissionComplete(uint32_t map_id, bool hard_mode) const;
        [[nodiscard]] bool IsDungeonComplete(uint32_t map_id, bool hard_mode) const;
        [[nodiscard]] bool IsVanquishComplete(uint32_t map_id) const;

        bool initialized_ = false;
        const CriterionDefinition* active_criterion_ = nullptr;
        bool active_criterion_hooked_ = false;
        bool map_loaded_hooked_ = false;
        bool logout_hooked_ = false;
        bool experience_hooked_ = false;
        GW::UI::UIMessage active_criterion_message_ = GW::UI::UIMessage::kNone;
        uint32_t active_required_level_ = 0;
        std::optional<GW::Constants::TitleID> active_title_id_;

        GW::HookEntry active_criterion_hook_;
        GW::HookEntry map_loaded_hook_;
        GW::HookEntry logout_hook_;
        GW::HookEntry experience_hook_;

        PendingGameEvents pending_events_;
        uint64_t map_loaded_sequence_ = 0;
        std::vector<MapCatalogEntry> map_catalog_;
        std::vector<MapDecodeContext*> pending_map_decodes_;
    };
}
