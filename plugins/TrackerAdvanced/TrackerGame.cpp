#include "TrackerGame.h"

#include <algorithm>
#include <limits>
#include <utility>

#include <Windows.h>

#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Title.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/UIMgr.h>

namespace {
    using GW::Constants::Campaign;
    using GW::Constants::MapID;
    using GW::Constants::TitleID;

    constexpr auto HookAltitude = 0x8000;

    struct AgentLevelUpdated {
        uint32_t agent_id;
        uint32_t level;
    };
    static_assert(sizeof(AgentLevelUpdated) == 8);

    bool ArrayBitAt(const GW::Array<uint32_t>& values, const uint32_t index)
    {
        const auto word = index / 32;
        if (!values.valid() || !values.m_buffer || word >= values.size()) {
            return false;
        }
        return (values[word] & (1u << (index % 32))) != 0;
    }

    std::string WideToUtf8(const wchar_t* value)
    {
        if (!(value && *value)) {
            return {};
        }
        const auto length = wcslen(value);
        if (length > static_cast<size_t>(std::numeric_limits<int>::max())) {
            return {};
        }
        const auto source_length = static_cast<int>(length);
        const auto result_length = WideCharToMultiByte(CP_UTF8, 0, value, source_length, nullptr, 0, nullptr, nullptr);
        if (result_length <= 0) {
            return {};
        }
        std::string result(static_cast<size_t>(result_length), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value, source_length, result.data(), result_length, nullptr, nullptr);
        return result;
    }

    bool IsMissionMap(const GW::AreaInfo& info)
    {
        switch (info.type) {
            case GW::RegionType::MissionOutpost:
            case GW::RegionType::CooperativeMission:
                return info.thumbnail_id != 0;
            case GW::RegionType::EotnMission: return true;
            default:
                return false;
        }
    }

    bool MapMatchesUsage(const TrackerAdvanced::MapCatalogEntry& entry, const TrackerAdvanced::MapUsage usage)
    {
        switch (usage) {
            case TrackerAdvanced::MapUsage::Any:
                return true;
            case TrackerAdvanced::MapUsage::Mission:
                return entry.is_mission;
            case TrackerAdvanced::MapUsage::Dungeon:
                return entry.is_dungeon;
            case TrackerAdvanced::MapUsage::Vanquish:
                return entry.is_vanquishable;
        }
        return false;
    }

    std::optional<GW::UI::UIMessage> MessageForCriterion(const TrackerAdvanced::CriterionType type)
    {
        using GW::UI::UIMessage;
        switch (type) {
            case TrackerAdvanced::CriterionType::player_level:
                return UIMessage::kMessage_0x10000014;
            case TrackerAdvanced::CriterionType::map_loaded:
                return UIMessage::kMapLoaded;
            case TrackerAdvanced::CriterionType::mission_complete:
                return UIMessage::kMissionComplete;
            case TrackerAdvanced::CriterionType::dungeon_complete:
                return UIMessage::kDungeonComplete;
            case TrackerAdvanced::CriterionType::vanquish_complete:
                return UIMessage::kVanquishComplete;
            case TrackerAdvanced::CriterionType::title_progress:
                return UIMessage::kTitleProgressUpdated;
            case TrackerAdvanced::CriterionType::invalid:
            case TrackerAdvanced::CriterionType::manual:
                return std::nullopt;
        }
        return std::nullopt;
    }
}

namespace TrackerAdvanced {
    struct TrackerGame::MapDecodeContext {
        TrackerGame* owner = nullptr;
        uint32_t map_id = 0;
        std::wstring encoded;
    };

    void TrackerGame::Initialize()
    {
        if (initialized_) {
            return;
        }
        initialized_ = true;
        BuildMapCatalog();
    }

    void TrackerGame::Terminate()
    {
        if (!initialized_) {
            return;
        }
        SetActiveCriterion(nullptr);
        SetLifecycleHooks(false, false);
        SetExperienceHook(false);
        AbandonMapDecodes();
        pending_events_ = {};
        initialized_ = false;
    }

    bool TrackerGame::IsSafeGameState() const
    {
        const auto game = GW::GetGameContext();
        const auto character = GW::GetCharContext();
        return GW::Map::GetIsMapLoaded()
            && GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading
            && GW::UI::GetFrameByLabel(L"Mission") == nullptr
            && game && game->world
            && character && character->player_name[0]
            && GW::Agents::GetControlledCharacter();
    }

    std::wstring TrackerGame::CurrentCharacterName() const
    {
        const auto character = GW::GetCharContext();
        return character && character->player_name[0] ? character->player_name : L"";
    }

    std::optional<uint32_t> TrackerGame::CurrentMapId() const
    {
        if (!IsSafeGameState()) {
            return std::nullopt;
        }
        return static_cast<uint32_t>(GW::Map::GetMapID());
    }

    std::string TrackerGame::CurrentMapName() const
    {
        const auto current = CurrentMapId();
        if (!current) {
            return {};
        }
        const auto found = std::ranges::find(map_catalog_, *current, &MapCatalogEntry::map_id);
        return found == map_catalog_.end() ? std::string{} : found->name;
    }

    std::optional<uint32_t> TrackerGame::CurrentExperience() const
    {
        if (!IsSafeGameState()) {
            return std::nullopt;
        }
        const auto world = GW::GetWorldContext();
        return world ? std::optional{world->experience} : std::nullopt;
    }

    uint64_t TrackerGame::MapLoadedSequence() const noexcept
    {
        return map_loaded_sequence_;
    }

    bool TrackerGame::EvaluateCriterion(const CriterionDefinition& criterion) const
    {
        if (!IsSafeGameState()) {
            return false;
        }
        switch (criterion.type) {
            case CriterionType::manual:
            case CriterionType::invalid:
                return false;
            case CriterionType::player_level: {
                const auto player = GW::Agents::GetControlledCharacter();
                return player && criterion.level && player->level >= *criterion.level;
            }
            case CriterionType::map_loaded: {
                if (criterion.map) {
                    const auto current_name = CurrentMapName();
                    return !current_name.empty() && current_name == *criterion.map;
                }
                return false;
            }
            case CriterionType::mission_complete: {
                if (!criterion.mission) {
                    return false;
                }
                const auto targets = ResolveMapIds(*criterion.mission, MapUsage::Mission);
                return std::ranges::any_of(targets, [this, &criterion](const uint32_t target) {
                    return IsMissionComplete(target, criterion.is_hard_mode.value_or(false));
                });
            }
            case CriterionType::dungeon_complete: {
                if (!criterion.dungeon) {
                    return false;
                }
                const auto targets = ResolveMapIds(*criterion.dungeon, MapUsage::Dungeon);
                return std::ranges::any_of(targets, [this, &criterion](const uint32_t target) {
                    return IsDungeonComplete(target, criterion.is_hard_mode.value_or(false));
                });
            }
            case CriterionType::vanquish_complete: {
                if (!criterion.map) {
                    return false;
                }
                const auto targets = ResolveMapIds(*criterion.map, MapUsage::Vanquish);
                return std::ranges::any_of(targets, [this](const uint32_t target) {
                    return IsVanquishComplete(target);
                });
            }
            case CriterionType::title_progress: {
                if (!(criterion.title && criterion.required_progress)) {
                    return false;
                }
                const auto supported = FindTitle(*criterion.title);
                const auto title = supported ? GW::PlayerMgr::GetTitleTrack(supported->id) : nullptr;
                return title && title->current_points >= *criterion.required_progress;
            }
        }
        return false;
    }

    void TrackerGame::SetActiveCriterion(const CriterionDefinition* criterion)
    {
        if (active_criterion_ == criterion) {
            return;
        }
        RemoveActiveCriterionHook();
        pending_events_.criterion = {};
        active_criterion_ = criterion;
        active_required_level_ = 0;
        active_title_id_.reset();
        if (!criterion) {
            return;
        }
        const auto message = MessageForCriterion(criterion->type);
        if (!message) {
            return;
        }
        if (criterion->type == CriterionType::player_level) {
            if (!criterion->level) {
                return;
            }
            active_required_level_ = *criterion->level;
        }
        if (criterion->type == CriterionType::title_progress) {
            if (!criterion->title) {
                return;
            }
            const auto supported = FindTitle(*criterion->title);
            if (!supported) {
                return;
            }
            active_title_id_ = supported->id;
        }
        RegisterActiveCriterionHook(*message);
    }

    void TrackerGame::SetLifecycleHooks(const bool map_loaded, const bool logout)
    {
        if (map_loaded != map_loaded_hooked_) {
            if (map_loaded) {
                GW::UI::RegisterUIMessageCallback(
                    &map_loaded_hook_,
                    GW::UI::UIMessage::kMapLoaded,
                    [this](GW::HookStatus*, GW::UI::UIMessage, void*, void*) { OnMapLoaded(); },
                    HookAltitude);
            }
            else {
                GW::UI::RemoveUIMessageCallback(&map_loaded_hook_);
            }
            map_loaded_hooked_ = map_loaded;
        }
        if (logout != logout_hooked_) {
            if (logout) {
                GW::UI::RegisterUIMessageCallback(
                    &logout_hook_,
                    GW::UI::UIMessage::kLogout,
                    [this](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) { OnLogout(wparam); },
                    HookAltitude);
            }
            else {
                GW::UI::RemoveUIMessageCallback(&logout_hook_);
            }
            logout_hooked_ = logout;
        }
    }

    void TrackerGame::SetExperienceHook(const bool enabled)
    {
        if (enabled == experience_hooked_) {
            return;
        }
        if (enabled) {
            GW::UI::RegisterUIMessageCallback(
                &experience_hook_,
                GW::UI::UIMessage::kExperienceGained,
                [this](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) { OnExperienceGained(wparam); },
                HookAltitude);
        }
        else {
            GW::UI::RemoveUIMessageCallback(&experience_hook_);
        }
        experience_hooked_ = enabled;
    }

    PendingGameEvents TrackerGame::DrainPendingEvents()
    {
        auto events = pending_events_;
        pending_events_ = {};
        return events;
    }

    bool TrackerGame::IsMapCatalogReady() const
    {
        return pending_map_decodes_.empty();
    }

    const std::vector<MapCatalogEntry>& TrackerGame::MapCatalog() const
    {
        return map_catalog_;
    }

    std::optional<uint32_t> TrackerGame::ResolveMapId(const std::string_view name, const MapUsage usage) const
    {
        const auto found = std::ranges::find_if(map_catalog_, [name, usage](const MapCatalogEntry& entry) {
            return entry.name == name && MapMatchesUsage(entry, usage);
        });
        return found == map_catalog_.end() ? std::nullopt : std::optional{found->map_id};
    }

    std::vector<uint32_t> TrackerGame::ResolveMapIds(
        const std::string_view name,
        const MapUsage usage) const
    {
        std::vector<uint32_t> matches;
        for (const auto& entry : map_catalog_) {
            if (entry.name == name && MapMatchesUsage(entry, usage)) {
                matches.push_back(entry.map_id);
            }
        }
        return matches;
    }

    void TrackerGame::BuildMapCatalog()
    {
        map_catalog_.clear();
        AbandonMapDecodes();
        map_catalog_.reserve(static_cast<size_t>(MapID::Count));
        for (uint32_t value = 1; value < static_cast<uint32_t>(MapID::Count); ++value) {
            const auto map_id = static_cast<MapID>(value);
            const auto info = GW::Map::GetMapInfo(map_id);
            if (!(info && info->name_id)) {
                continue;
            }
            map_catalog_.push_back({
                .map_id = value,
                .is_mission = IsMissionMap(*info),
                .is_dungeon = info->type == GW::RegionType::Dungeon,
                .is_vanquishable = info->GetIsVanquishableArea(),
            });
        }
        for (const auto& entry : map_catalog_) {
            const auto info = GW::Map::GetMapInfo(static_cast<MapID>(entry.map_id));
            wchar_t encoded[8]{};
            if (!(info && GW::UI::UInt32ToEncStr(info->name_id, encoded, std::size(encoded)))) {
                continue;
            }
            auto context = new MapDecodeContext{
                .owner = this,
                .map_id = entry.map_id,
                .encoded = encoded,
            };
            pending_map_decodes_.push_back(context);
            GW::UI::AsyncDecodeStr(
                context->encoded.c_str(),
                OnMapNameDecodedStatic,
                context,
                GW::Constants::Language::English);
        }
    }

    void TrackerGame::AbandonMapDecodes()
    {
        for (const auto context : pending_map_decodes_) {
            context->owner = nullptr;
        }
        pending_map_decodes_.clear();
    }

    void TrackerGame::OnMapNameDecoded(MapDecodeContext* context, const wchar_t* decoded)
    {
        const auto pending = std::ranges::find(pending_map_decodes_, context);
        if (pending != pending_map_decodes_.end()) {
            pending_map_decodes_.erase(pending);
        }
        const auto entry = std::ranges::find(map_catalog_, context->map_id, &MapCatalogEntry::map_id);
        if (entry != map_catalog_.end()) {
            entry->name = WideToUtf8(decoded);
        }
    }

    void TrackerGame::OnMapNameDecodedStatic(void* context, const wchar_t* decoded)
    {
        auto decode = static_cast<MapDecodeContext*>(context);
        if (decode->owner) {
            decode->owner->OnMapNameDecoded(decode, decoded);
        }
        delete decode;
    }

    void TrackerGame::RegisterActiveCriterionHook(const GW::UI::UIMessage message)
    {
        active_criterion_message_ = message;
        GW::UI::RegisterUIMessageCallback(
            &active_criterion_hook_,
            message,
            [this](GW::HookStatus*, const GW::UI::UIMessage received, void* wparam, void*) {
                OnActiveCriterionMessage(received, wparam);
            },
            HookAltitude);
        active_criterion_hooked_ = true;
    }

    void TrackerGame::RemoveActiveCriterionHook()
    {
        if (active_criterion_hooked_) {
            GW::UI::RemoveUIMessageCallback(&active_criterion_hook_);
        }
        active_criterion_hooked_ = false;
        active_criterion_message_ = GW::UI::UIMessage::kNone;
        active_criterion_ = nullptr;
    }

    void TrackerGame::OnActiveCriterionMessage(const GW::UI::UIMessage message, void* wparam)
    {
        auto value0 = 0u;
        auto value1 = 0u;
        if (message == GW::UI::UIMessage::kMessage_0x10000014) {
            const auto packet = static_cast<const AgentLevelUpdated*>(wparam);
            if (
                !(packet
                    && packet->agent_id == GW::Agents::GetControlledCharacterId()
                    && packet->level >= active_required_level_)) {
                return;
            }
            value0 = packet->agent_id;
            value1 = packet->level;
        }
        else if (message == GW::UI::UIMessage::kTitleProgressUpdated) {
            value0 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(wparam));
            if (!active_title_id_ || value0 != static_cast<uint32_t>(*active_title_id_)) {
                return;
            }
        }
        else if (message == GW::UI::UIMessage::kMapLoaded) {
            value0 = static_cast<uint32_t>(GW::Map::GetMapID());
        }
        pending_events_.criterion = {
            .pending = true,
            .message = message,
            .value0 = value0,
            .value1 = value1,
        };
    }

    void TrackerGame::OnMapLoaded()
    {
        ++map_loaded_sequence_;
        pending_events_.map_loaded = true;
        pending_events_.map_loaded_sequence = map_loaded_sequence_;
    }

    void TrackerGame::OnLogout(void* wparam)
    {
        const auto packet = static_cast<const GW::UI::UIPacket::kLogout*>(wparam);
        if (!packet) {
            return;
        }
        pending_events_.logout = true;
        pending_events_.returning_to_character_select |= packet->character_select == 1;
        pending_events_.logout_unknown = packet->unknown;
        pending_events_.logout_character_select = packet->character_select;
    }

    void TrackerGame::OnExperienceGained(void* wparam)
    {
        ++pending_events_.experience_event_count;
        const auto amount =
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(wparam));
        pending_events_.last_experience_amount = amount;
        const auto remaining =
            std::numeric_limits<uint64_t>::max()
            - pending_events_.experience_amount_total;
        pending_events_.experience_amount_total +=
            std::min(remaining, static_cast<uint64_t>(amount));
    }

    bool TrackerGame::IsMissionComplete(const uint32_t map_id, const bool hard_mode) const
    {
        const auto world = GW::GetWorldContext();
        const auto info = GW::Map::GetMapInfo(static_cast<MapID>(map_id));
        if (!(world && info)) {
            return false;
        }
        const auto& completed = hard_mode ? world->missions_completed_hm : world->missions_completed;
        if (!ArrayBitAt(completed, map_id)) {
            return false;
        }
        if (info->campaign == Campaign::EyeOfTheNorth) {
            return true;
        }
        const auto& bonus = hard_mode ? world->missions_bonus_hm : world->missions_bonus;
        return ArrayBitAt(bonus, map_id);
    }

    bool TrackerGame::IsDungeonComplete(const uint32_t map_id, const bool hard_mode) const
    {
        const auto world = GW::GetWorldContext();
        if (!world) {
            return false;
        }
        const auto& completed = hard_mode ? world->missions_completed_hm : world->missions_completed;
        return ArrayBitAt(completed, map_id);
    }

    bool TrackerGame::IsVanquishComplete(const uint32_t map_id) const
    {
        const auto world = GW::GetWorldContext();
        return world && ArrayBitAt(world->vanquished_areas, map_id);
    }
}
