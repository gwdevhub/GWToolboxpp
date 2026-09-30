#include "stdafx.h"

#include "SplitsWindow.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Title.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/QuestMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Packets/Opcodes.h>
#include <GWCA/Packets/StoC.h>

#include <ImGuiAddons.h>
#include <Modules/Resources.h>
#include <Utils/EncString.h>
#include <Utils/TextUtils.h>
#include <Utils/ToolboxUtils.h>
#include <Windows/Splits/GoalClock.h>
#include <Windows/Splits/GoalEngine.h>
#include <Windows/Splits/GoalList.h>
#include <Windows/Splits/LiveSplitServer.h>
#include <Windows/Splits/NuzlockeState.h>
#include <Windows/Splits/RunHistory.h>
#include <Windows/Splits/SCPresets.h>
#include <Windows/Splits/SplitsGoalListWindow.h>

#include <algorithm>
#include <cctype>
#include <cwchar>
#include <functional>
#include <tuple>

// glaze reflection need external linkage.
namespace SplitsWindowJson {
    // Registered with SettingsRegistry; per-profile settings live on SplitsProfile, Nuzlocke's on NuzlockeSettings.
    struct Settings {
        Colors::SettingColor    color_completed   = Colors::RGB(0, 255, 0);
        Colors::SettingColor    color_active      = Colors::RGB(255, 255, 255);
        Colors::SettingColor    color_real_time   = Colors::RGB(230, 230, 230);
        Colors::SettingColor    color_game_time   = Colors::RGB(153, 217, 255);
        Colors::SettingColor    color_pb_ahead    = Colors::RGB(255, 217, 0);
        Colors::SettingColor    color_pb_behind   = Colors::RGB(255, 102, 102);
        bool                    livesplit_enabled = false;
        int                     livesplit_port    = 9002;
        LiveSplitServer::Format livesplit_format  = LiveSplitServer::Format::LiveSplitOneJSON;
    };

    struct SerializedSplit {
        double real_time    = 0.0;
        double game_time    = 0.0;
        double segment_real = 0.0;
        double segment_game = 0.0;
        // Only used when status == "Started".
        double start_real_time  = 0.0;
        double start_game_time  = 0.0;
        int    trigger_progress = 0;
        std::string status; // NotStarted = nullopt
    };

    struct SerializedResume {
        std::string list_name;
        double      real_time = 0.0;
        double      game_time = 0.0;
        std::optional<double> total_paused;
        std::optional<int64_t>     start_unix;
        std::optional<std::string> char_name;
        std::vector<std::optional<SerializedSplit>> goals;
    };
}
using namespace SplitsWindowJson;

using SkillID = GW::Constants::SkillID;

// Shadow steps start Running like movement (same list as GWChrono).
constexpr SkillID kShadowStepSkills[] = {
    SkillID::Vipers_Defense,
    SkillID::Return,
    SkillID::Aura_of_Displacement,
    SkillID::Beguiling_Haze,
    SkillID::Scorpion_Wire,
    SkillID::Ride_the_Lightning,
    SkillID::Recall,
    SkillID::Deaths_Charge,
    SkillID::Heart_of_Shadow,
    SkillID::Spirit_Walk,
    SkillID::Dark_Prison,
    SkillID::Wastrels_Collapse,
    SkillID::Augury_of_Death,
    SkillID::Shadow_Walk,
    SkillID::Deaths_Retreat,
    SkillID::Shadow_Prison,
    SkillID::Swap,
    SkillID::Shadow_Meld,
    SkillID::Shadow_Fang,
    SkillID::Ebon_Escape,
    SkillID::Shadow_Theft,
};

// Ready-check wait pause game time. Vizunah/Unwaking lock party; Ascalon Academy solo, countdown packet only.
static bool IsMissionQueueMap(GW::Constants::MapID map)
{
    static constexpr GW::Constants::MapID kQueueMaps[] = {
        GW::Constants::MapID::Vizunah_Square_Local_Quarter_outpost,
        GW::Constants::MapID::Vizunah_Square_Foreign_Quarter_outpost,
        GW::Constants::MapID::Unwaking_Waters_Luxon_outpost,
        GW::Constants::MapID::Unwaking_Waters_Kurzick_outpost,
        GW::Constants::MapID::Ascalon_City_pre_searing,
    };
    return std::ranges::contains(kQueueMaps, map);
}

namespace {
    constexpr std::array<const char*, kProfileCount>    kProfileSections    = {"Splits.Manual", "Splits.Running", "Splits.SC"};
    constexpr std::array<const wchar_t*, kProfileCount> kProfileFolderNames = {L"manual", L"running", L"sc"};

    struct NuzlockeIntField {
        const char* label;
        int NuzlockeSettings::* member;
    };
    constexpr std::array<NuzlockeIntField, 3> kNuzlockeLivesFields = {{
        {"Hero lives", &NuzlockeSettings::hero_lives},
        {"Henchman lives", &NuzlockeSettings::hench_lives},
        {"Player lives", &NuzlockeSettings::player_lives},
    }};
    constexpr std::array<NuzlockeIntField, 8> kNuzlockePointFields = {{
        {"Manual points", &NuzlockeSettings::points_manual},
        {"Missions points", &NuzlockeSettings::points_missions},
        {"Explorables points", &NuzlockeSettings::points_explorables},
        {"Towns points", &NuzlockeSettings::points_towns},
        {"Titles points", &NuzlockeSettings::points_titles},
        {"Reach Level points", &NuzlockeSettings::points_reach_level},
        {"Quest points", &NuzlockeSettings::points_quest},
        {"Skill Learnt points", &NuzlockeSettings::points_skill_learnt},
    }};

    Settings             settings;
    GoalClock            run_clock;
    GoalEngine           engine;
    GoalList             active_list;
    SplitsGoalListWindow ui;
    RunHistory           run_history;
    NuzlockeState        nuzlocke;
    LiveSplitServer      livesplit;

    std::array<SplitsProfile, kProfileCount> profiles = {MakeManualProfile(), MakeRunningProfile(), MakeSCProfile()};
    int active_profile_idx = 0;

    // Dirty on SaveActiveList (only list writer) or folder change.
    std::vector<std::pair<std::string, std::wstring>> cached_saved_lists;
    std::wstring                                      cached_saved_lists_folder;
    bool                                              cached_saved_lists_dirty = true;
    // Anchor cache: not re-read file each rebuild.
    std::unordered_map<std::wstring, GW::Constants::MapID>                                      saved_list_anchor_cache;
    std::unordered_map<GW::Constants::MapID, std::vector<std::pair<std::string, std::wstring>>> cached_saved_lists_by_area;
    bool                                                                                        cached_saved_lists_by_area_dirty = true;

    std::filesystem::path splits_folder;
    std::filesystem::path runs_folder;

    GW::Constants::MapID last_map            = GW::Constants::MapID::None;
    bool                 last_was_explorable = false;
    // 0 = unknown. Reset on zone load so char switch re-seed.
    int player_level = 0;
    // Also "seen alive" for MobKill death check. Clear on zone load.
    std::unordered_map<uint32_t, std::unique_ptr<GuiUtils::EncString>> mob_name_cache;
    bool                                                                pending_map_enter            = false;
    bool                                                                pending_came_from_explorable = false;
    bool                                                                pending_party_defeated       = false;
    // Resign seen, wait kPartyDefeated confirm. Clear on zone load so stale resign not blame later wipe.
    bool pending_resign_seen = false;
    // Own latch: InstanceLoadFile may land other tick than InstanceLoadInfo.
    uint32_t  pending_doa_file_id = 0;
    GW::Vec2f pending_doa_spawn   = {};
    // Detected rotation (0-3 = Foundry/City/Veil/Gloom, -1 none); saved with the run for order analysis.
    int doa_start_zone = -1;
    // Applied after ApplyTimerPolicy: an auto-reset in the same tick would wipe a zone start set earlier.
    bool doa_zone_start_pending = false;
    bool startup_load_done      = false;

    bool livesplit_applied_enabled = false;
    int  livesplit_applied_port    = 0;
    int  livesplit_splits_sent     = 0;
    bool livesplit_paused          = false;
    // Set by ApplyResume: LiveSplit lost the run with the restart, so the unpause re-sends start plus the completed splits.
    bool livesplit_resync_pending = false;
    // Port field edits a copy, committed when the field loses focus, so typing doesn't restart the server per digit.
    int  livesplit_port_edit    = 0;
    bool livesplit_port_editing = false;

    uint32_t pending_skill_id          = 0;
    bool     running_awaiting_movement = false;
    bool     running_load_paused       = false;
    // Mission ready-check up; game time not count.
    bool in_mission_queue = false;
    // Manual pause freeze real time too: track apart.
    bool    manually_paused    = false;
    double  manual_pause_accum = 0.0;
    double  total_paused_real  = 0.0;
    int64_t title_start_points = -1; // -1 = not read

    bool        run_complete   = false;
    bool        run_failed     = false;
    std::string run_char_name;
    int64_t     run_start_unix = 0;

    bool        pending_resume = false;
    std::string pending_resume_data;

    GW::HookEntry ui_hooks;
    GW::HookEntry stoc_hooks;
    GW::HookEntry chat_cmd_hook;
}

// =============================================================================
// ACCESSORS
// =============================================================================

Color& SplitsWindow::ColorCompleted() { return settings.color_completed; }
Color& SplitsWindow::ColorActive() { return settings.color_active; }
Color& SplitsWindow::ColorRealTime() { return settings.color_real_time; }
Color& SplitsWindow::ColorGameTime() { return settings.color_game_time; }
Color& SplitsWindow::ColorPbAhead() { return settings.color_pb_ahead; }
Color& SplitsWindow::ColorPbBehind() { return settings.color_pb_behind; }

SplitsProfile& SplitsWindow::ActiveProfile() { return profiles[active_profile_idx]; }
const SplitsProfile& SplitsWindow::ActiveProfile() const { return profiles[active_profile_idx]; }
int SplitsWindow::ActiveProfileIdx() const { return active_profile_idx; }
std::array<SplitsProfile, kProfileCount>& SplitsWindow::Profiles() { return profiles; }

const GoalClock& SplitsWindow::Clock() const { return run_clock; }
GoalList* SplitsWindow::List() { return &active_list; }
bool SplitsWindow::RunComplete() const { return run_complete; }
bool SplitsWindow::RunFailed() const { return run_failed; }
double SplitsWindow::TotalPausedReal() const { return total_paused_real + (manually_paused ? manual_pause_accum : 0.0); }

const ComparisonSplits& SplitsWindow::ActiveComparison() const { return run_history.Comparison(ActiveProfile().comparison_mode); }
const ComparisonSplits& SplitsWindow::Comparison(const SplitsProfile::ComparisonMode mode) const { return run_history.Comparison(mode); }
const std::vector<RecentRun>& SplitsWindow::RecentRuns() const { return run_history.RecentRuns(); }
int SplitsWindow::PBAttemptNumber() const { return run_history.PBAttemptNumber(); }
double SplitsWindow::PBTotalReal() const { return run_history.PBTotalReal(); }
const std::vector<int>& SplitsWindow::BestSegRealAttempt() const { return run_history.BestSegRealAttempt(); }
const std::vector<int>& SplitsWindow::BestSegGameAttempt() const { return run_history.BestSegGameAttempt(); }
std::vector<RecentRun> SplitsWindow::LoadFullRunHistory() const { return run_history.LoadFull(RunHistoryFilePath()); }

bool SplitsWindow::NuzlockeDeathTrackerEnabled() const { return nuzlocke.settings.death_tracker_enabled && active_profile_idx == kProfileManual; }
bool SplitsWindow::NuzlockePointsEnabled() const { return nuzlocke.settings.points_enabled && active_profile_idx == kProfileManual; }

void SplitsWindow::SendLiveSplit(const char* command) { livesplit.Send(command, settings.livesplit_format); }

// =============================================================================
// LIFECYCLE
// =============================================================================

void SplitsWindow::Initialize()
{
    ToolboxWindow::Initialize();
    SettingsRegistry::Register(this, settings);
    SettingsRegistry::Register(this, nuzlocke.settings);
    // Settings search highlights the widget whose text equals the label exactly, so labels are the on-screen text.
    static constexpr std::tuple<const char*, const char*, const char*> kSearchLabels[] = {
        {"color_completed", "Completed color", ""}, {"color_active", "Current color", ""},
        {"color_real_time", "Real time color", ""}, {"color_game_time", "Game time color", ""},
        {"color_pb_ahead", "Ahead color", ""},      {"color_pb_behind", "Behind color", ""},
        {"livesplit_enabled", "Enable LiveSplit websocket server", "LiveSplit"},
        {"livesplit_port", "Websocket server port", "LiveSplit"},
        {"livesplit_format", "LiveSplit One JSON Format", "LiveSplit format"},
        {"death_tracker_enabled", "Death Tracker", "Nuzlocke"},
        {"hero_lives", "Hero lives", "Nuzlocke"}, {"hench_lives", "Henchman lives", "Nuzlocke"},
        {"player_lives", "Player lives", "Nuzlocke"},
        {"merge_hench_by_name", "Merge same-named henchmen across campaigns", "Nuzlocke"},
        {"points_enabled", "Points", "Nuzlocke"},
        {"points_manual", "Manual points", "Nuzlocke"},           {"points_missions", "Missions points", "Nuzlocke"},
        {"points_explorables", "Explorables points", "Nuzlocke"}, {"points_towns", "Towns points", "Nuzlocke"},
        {"points_titles", "Titles points", "Nuzlocke"},           {"points_reach_level", "Reach Level points", "Nuzlocke"},
        {"points_quest", "Quest points", "Nuzlocke"},             {"points_skill_learnt", "Skill Learnt points", "Nuzlocke"},
    };
    for (const auto& [key, label, description] : kSearchLabels)
        SettingsRegistry::Describe(this, key, label, description);

    GW::Chat::CreateCommand(&chat_cmd_hook, L"splits", &CmdSplits);

    GW::UI::RegisterUIMessageCallback(
        &ui_hooks,
        GW::UI::UIMessage::kMissionComplete,
        [this](GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
            engine.NotifyMissionComplete(GW::Map::GetMapID());
        });

    GW::UI::RegisterUIMessageCallback(
        &ui_hooks,
        GW::UI::UIMessage::kVanquishComplete,
        [this](GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
            engine.NotifyVanquishComplete(GW::Map::GetMapID());
        });

    GW::UI::RegisterUIMessageCallback(
        &ui_hooks,
        GW::UI::UIMessage::kPartyDefeated,
        [this](GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
            // Latch only: ApplyTimerPolicy own all fail/start decisions.
            pending_party_defeated = true;
            // Fires only when WHOLE party resigned/wiped: this confirm resign, not chat line.
            if (pending_resign_seen) {
                pending_resign_seen = false;
                if (NuzlockeDeathTrackerEnabled()) nuzlocke.OnPartyResigned();
            }
        });

    // type_flags 0x1 = bullet; 0x0 = primary. Primary done give reliable map_id.
    GW::UI::RegisterUIMessageCallback(
        &ui_hooks,
        GW::UI::UIMessage::kObjectiveAdd,
        [this](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
            if (!wparam) return;
            const auto* p = static_cast<GW::UI::UIPacket::kObjectiveAdd*>(wparam);
            engine.NotifyObjectiveAdd(p->objective_id, p->type);
        });

    GW::UI::RegisterUIMessageCallback(
        &ui_hooks,
        GW::UI::UIMessage::kObjectiveComplete,
        [this](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
            if (!wparam) return;
            const auto* p = static_cast<GW::UI::UIPacket::kObjectiveComplete*>(wparam);
            const auto map_id = static_cast<uint32_t>(GW::Map::GetMapID());
            engine.NotifyEvent(GoalTrigger::Type::ObjectiveDone, p->objective_id, map_id);
        });

    GW::UI::RegisterUIMessageCallback(
        &ui_hooks,
        GW::UI::UIMessage::kObjectiveUpdated,
        [this](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
            if (!wparam) return;
            const auto* p = static_cast<GW::UI::UIPacket::kObjectiveUpdated*>(wparam);
            engine.NotifyEvent(GoalTrigger::Type::ObjectiveStarted, p->objective_id);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ManipulateMapObject>(
        &stoc_hooks,
        [this](GW::HookStatus*, const GW::Packet::StoC::ManipulateMapObject* p) {
            if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) return;
            if (p->animation_type == 16 && p->animation_stage == 2) {
                engine.NotifyEvent(GoalTrigger::Type::DoorOpen, p->object_id);
            } else if (p->animation_type == 3 && p->animation_stage == 2) {
                engine.NotifyEvent(GoalTrigger::Type::DoorClose, p->object_id);
            }
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentUpdateAllegiance>(
        &stoc_hooks,
        [this](GW::HookStatus*, const GW::Packet::StoC::AgentUpdateAllegiance* p) {
            const auto* agent = GW::Agents::GetAgentByID(p->agent_id);
            if (!agent) return;
            const auto* living = agent->GetAsAgentLiving();
            if (!living) return;
            engine.NotifyEvent(GoalTrigger::Type::AgentUpdateAllegiance, living->player_number, p->allegiance_bits);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DoACompleteZone>(
        &stoc_hooks,
        [this](GW::HookStatus*, const GW::Packet::StoC::DoACompleteZone* p) {
            if (p->message[0] != 0x8101) return;
            engine.NotifyEvent(GoalTrigger::Type::DoACompleteZone, p->message[1]);
        });

    GW::UI::RegisterUIMessageCallback(
        &ui_hooks,
        GW::UI::UIMessage::kDungeonComplete,
        [this](GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
            const auto map_id = static_cast<uint32_t>(GW::Map::GetMapID());
            engine.NotifyEvent(GoalTrigger::Type::DungeonReward, map_id);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageServer>(
        &stoc_hooks,
        [this](GW::HookStatus*, GW::Packet::StoC::MessageServer*) {
            const wchar_t* msg = ToolboxUtils::GetMessageCore();
            if (!msg || !*msg) return;
            engine.NotifyEvent(GoalTrigger::Type::ServerMessage, 0, 0, msg, wcslen(msg));
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DisplayDialogue>(
        &stoc_hooks,
        [this](GW::HookStatus*, const GW::Packet::StoC::DisplayDialogue* p) {
            const auto len = wcslen(p->message);
            engine.NotifyEvent(GoalTrigger::Type::DisplayDialogue, 0, 0, p->message, len);
        });

    // Gate to queue maps so other countdowns not pause game time.
    GW::StoC::RegisterPacketCallback(
        &stoc_hooks, GAME_SMSG_INSTANCE_COUNTDOWN,
        [this](GW::HookStatus*, GW::Packet::StoC::PacketBase*) {
            const auto map_id = static_cast<uint32_t>(GW::Map::GetMapID());
            if (IsMissionQueueMap(static_cast<GW::Constants::MapID>(map_id)))
                in_mission_queue = true;
            engine.NotifyEvent(GoalTrigger::Type::CountdownStart, map_id);
        });

    GW::UI::RegisterUIMessageCallback(
        &ui_hooks,
        GW::UI::UIMessage::kSkillActivated,
        [this](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
            if (!wparam) return;
            const auto* p = static_cast<GW::UI::UIPacket::kAgentSkillPacket*>(wparam);
            if (p->agent_id == GW::Agents::GetControlledCharacterId())
                pending_skill_id = static_cast<uint32_t>(p->skill_id);
        });

    // Gate to queue maps so other party locks not touch game time.
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyLock>(
        &stoc_hooks,
        [this](GW::HookStatus*, const GW::Packet::StoC::PartyLock* p) {
            if (!p->unk2) {
                in_mission_queue = false;
            } else if (IsMissionQueueMap(last_map)) {
                in_mission_queue = true;
            }
        });

    // Fires every new instance, same-map too (district change, char switch).
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadInfo>(
        &stoc_hooks,
        [this](GW::HookStatus*, const GW::Packet::StoC::InstanceLoadInfo* p) {
            pending_came_from_explorable = last_was_explorable;
            last_was_explorable          = (p->is_explorable != 0);
            last_map                     = static_cast<GW::Constants::MapID>(p->map_id);
            in_mission_queue             = false;
            pending_map_enter            = true;
            // Reset so char switch re-seed level.
            player_level                 = 0;
            // Old zone mobs gone: clear so cache not grow forever.
            mob_name_cache.clear();
            // Resign never confirmed must not blame later wipe.
            pending_resign_seen = false;
            if (NuzlockeDeathTrackerEnabled()) nuzlocke.OnInstanceLoad();
        }, 0x8000);

    // Before InstanceLoadInfo: covers district change, same map.
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(
        &stoc_hooks,
        [this](GW::HookStatus*, const GW::Packet::StoC::GameSrvTransfer*) {
            in_mission_queue = false;
        });

    // DoA rotation depend on spawn, not map_id.
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadFile>(
        &stoc_hooks,
        [this](GW::HookStatus*, const GW::Packet::StoC::InstanceLoadFile* p) {
            pending_doa_file_id = p->map_fileID;
            pending_doa_spawn   = p->spawn_point;
        }, 0x8000);

    // Resign kills party, same as wipe. Chat line only latch: partial resign fire it too.
    // Own check, not ResignLogModule: that module can be off.
    GW::UI::RegisterUIMessageCallback(
        &ui_hooks,
        GW::UI::UIMessage::kWriteToChatLog,
        [this](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
            if (!wparam) return;
            const auto* p = static_cast<GW::UI::UIPacket::kWriteToChatLog*>(wparam);
            const wchar_t* message = p->message;
            if (!message || wmemcmp(message, L"\x7BFF\xC9C4\xAEAA\x1B9B\x107", 5) != 0) return;
            pending_resign_seen = true;
        });

    // No GWCA name yet: AgentLevelChanged {agent_id, level}.
    GW::UI::RegisterUIMessageCallback(
        &ui_hooks,
        GW::UI::UIMessage::kMessage_0x10000014,
        [this](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
            if (!wparam) return;
            struct AgentLevelChanged { uint32_t agent_id; uint32_t level; };
            const auto* p = static_cast<AgentLevelChanged*>(wparam);
            if (p->agent_id == GW::Agents::GetControlledCharacterId())
                player_level = static_cast<int>(p->level);
        });

    // Also re-fires on zone resync. IsCompleted() = fallback; kQuestDetailsChanged is live signal.
    GW::UI::RegisterUIMessageCallback(
        &ui_hooks,
        GW::UI::UIMessage::kQuestAdded,
        [this](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
            if (!wparam) return;
            const auto quest_id = *static_cast<GW::Constants::QuestID*>(wparam);
            auto* quest = GW::QuestMgr::GetQuest(quest_id);
            engine.NotifyEvent(GoalTrigger::Type::QuestPickup, static_cast<uint32_t>(quest_id));
            if (quest && quest->IsCompleted())
                engine.NotifyEvent(GoalTrigger::Type::QuestComplete, static_cast<uint32_t>(quest_id));
        });

    GW::UI::RegisterUIMessageCallback(
        &ui_hooks,
        GW::UI::UIMessage::kQuestDetailsChanged,
        [this](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
            if (!wparam) return;
            const auto quest_id = *static_cast<GW::Constants::QuestID*>(wparam);
            auto* quest = GW::QuestMgr::GetQuest(quest_id);
            if (quest && quest->IsCompleted())
                engine.NotifyEvent(GoalTrigger::Type::QuestComplete, static_cast<uint32_t>(quest_id));
        });

    // Fires on turn-in AND abandon: abandoning splits early. Accepted trade-off.
    GW::UI::RegisterUIMessageCallback(
        &ui_hooks,
        GW::UI::UIMessage::kQuestRemoved,
        [this](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
            // Null seen: pre-Searing ending clears whole quest log, crashed here.
            if (!wparam) return;
            const auto quest_id = *static_cast<GW::Constants::QuestID*>(wparam);
            engine.NotifyEvent(GoalTrigger::Type::QuestComplete, static_cast<uint32_t>(quest_id));
        });
}




void SplitsWindow::Terminate()
{
    GW::UI::RemoveUIMessageCallback(&ui_hooks);
    GW::StoC::RemoveCallbacks(&stoc_hooks);
    GW::Chat::DeleteCommand(&chat_cmd_hook);
    livesplit.Stop();
    engine.Detach();
    ToolboxWindow::Terminate();
}




// =============================================================================
// SETTINGS
// =============================================================================

void SplitsWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWindow::LoadSettings(doc, legacy);

    const auto splits_path = Resources::GetPath(L"splits");
    const auto runs_path   = splits_path / L"runs";
    for (const auto& root : {splits_path, runs_path})
        for (const wchar_t* leaf : {L"manual", L"running", L"sc\\Defaults"})
            Resources::EnsureFolderExists(root / leaf);
    splits_folder = splits_path;
    runs_folder   = runs_path;

    doc.GetStruct(Name(), settings);
    doc.GetStruct(Name(), nuzlocke.settings);
    for (int i = 0; i < kProfileCount; ++i)
        doc.GetStruct(kProfileSections[i], profiles[i]);

    // Startup only: a settings reload mid-session must not switch profile, swap the list, reset a run or re-ask to resume.
    if (startup_load_done) return;
    startup_load_done = true;
    doc.Get(Name(), "active_profile", active_profile_idx);
    if (active_profile_idx < 0 || active_profile_idx >= kProfileCount) active_profile_idx = 0;
    LoadProfileLastList();

    std::string content;
    if (!Resources::ReadFile(splits_folder / L"resume.json", content)) return;
    SerializedResume j;
    constexpr glz::opts opts{.error_on_unknown_keys = false};
    if (glz::read<opts>(j, content)) {
        Log::Error("Splits: failed to parse resume.json");
        return;
    }
    // active_profile_idx already loaded, so folder right.
    if (j.list_name.empty() || !std::filesystem::exists(ActiveSplitsFolder() / (GoalList::FileStem(j.list_name) + L".json"))) return;
    pending_resume_data = std::move(content);
    pending_resume      = true;
    const std::string msg = std::format("Run '{}' was paused last session.\n"
                                        "Resume where you left off? It stays paused until you press Start.\n"
                                        "No discards it.", j.list_name);
    ImGui::ConfirmDialog(msg.c_str(), [this](const bool resume, void*) {
        resume ? ApplyResume() : DeleteResumeState();
    });
}


void SplitsWindow::SaveSettings(SettingsDoc& doc)
{
    doc.SetStruct(Name(), settings);
    doc.SetStruct(Name(), nuzlocke.settings);
    doc.Set(Name(), "active_profile", active_profile_idx);
    profiles[active_profile_idx].last_list_name = active_list.name;
    for (int i = 0; i < kProfileCount; ++i)
        doc.SetStruct(kProfileSections[i], profiles[i]);

    ToolboxWindow::SaveSettings(doc);
}


// =============================================================================
// GOAL LIST & SAVED-LIST MANAGEMENT
// =============================================================================

void SplitsWindow::NewActiveList(const char* name)
{
    engine.Detach();
    active_list = GoalList{};
    active_list.name = name ? name : "New List";
    run_clock.Reset();
    ResetRunFlags();
    DeleteResumeState();
    engine.Attach(&active_list);
    LoadPB();
}

void SplitsWindow::SaveActiveList(bool clear_preset)
{
    if (splits_folder.empty() || active_list.name.empty()) return;
    // Presets own subfolder: preset and user list can share name.
    if (clear_preset) active_list.is_preset = false;
    const auto folder = active_list.is_preset ? ActiveSplitsFolder() / L"Defaults" : ActiveSplitsFolder();
    const std::wstring path = (folder / (GoalList::FileStem(active_list.name) + L".json")).wstring();
    if (!active_list.SaveToFile(path)) Log::Error("Splits: failed to save list '%s'", active_list.name.c_str());
    cached_saved_lists_dirty = true;
    cached_saved_lists_by_area_dirty = true;
    // Already in memory: no re-read for anchor.
    saved_list_anchor_cache[path] = SCPresets::AnchorMapId(active_list);
}


void SplitsWindow::ReplaceActiveList(const std::function<void()>& populate)
{
    engine.Detach();
    populate();
    engine.Attach(&active_list);
    LoadPB();
}


void SplitsWindow::LoadActiveList(const std::wstring& path)
{
    DeleteResumeState();
    ReplaceActiveList([&] { active_list.LoadFromFile(path); });
    run_clock.Reset();
    ResetRunFlags();
}


void SplitsWindow::SetActiveList(GoalList list, const bool keep_preset)
{
    // Not NewActiveList() + add goals: Attach setup (starts_immediately) need goals present.
    DeleteResumeState();
    ReplaceActiveList([&] { active_list = std::move(list); active_list.is_preset = keep_preset; });
    run_clock.Reset();
    ResetRunFlags();
}


const std::vector<std::pair<std::string, std::wstring>>& SplitsWindow::GetSavedLists() const
{
    static const std::vector<std::pair<std::string, std::wstring>> kEmpty;
    if (splits_folder.empty()) return kEmpty;
    const std::wstring folder = ActiveSplitsFolder().wstring();
    if (cached_saved_lists_dirty || folder != cached_saved_lists_folder) {
        // Presets in Defaults\: non-recursive scan skip them.
        cached_saved_lists = GoalList::ListSaved(folder);
        cached_saved_lists_folder = folder;
        cached_saved_lists_dirty  = false;
    }
    return cached_saved_lists;
}


const std::unordered_map<GW::Constants::MapID, std::vector<std::pair<std::string, std::wstring>>>&
SplitsWindow::GetSavedListsByArea() const
{
    const auto& saved = GetSavedLists();
    if (!cached_saved_lists_by_area_dirty) return cached_saved_lists_by_area;

    cached_saved_lists_by_area.clear();
    for (const auto& [name, path] : saved) {
        auto it = saved_list_anchor_cache.find(path);
        if (it == saved_list_anchor_cache.end()) {
            GoalList tmp;
            tmp.LoadFromFile(path);
            it = saved_list_anchor_cache.emplace(path, SCPresets::AnchorMapId(tmp)).first;
        }
        if (it->second != GW::Constants::MapID::None)
            cached_saved_lists_by_area[it->second].push_back({name, path});
    }
    cached_saved_lists_by_area_dirty = false;
    return cached_saved_lists_by_area;
}


std::filesystem::path SplitsWindow::RunHistoryFilePath() const
{
    std::wstring safe_name = GoalList::FileStem(active_list.name);
    std::ranges::replace(safe_name, L' ', L'_'); // history files always used underscores
    // Defaults\ split so preset and same-name user list not share history.
    const auto folder = active_list.is_preset ? ActiveRunsFolder() / L"Defaults" : ActiveRunsFolder();
    return folder / (safe_name + L".json");
}


std::filesystem::path SplitsWindow::ActiveSplitsFolder() const
{
    return splits_folder / kProfileFolderNames[static_cast<size_t>(active_profile_idx)];
}

std::filesystem::path SplitsWindow::ActiveRunsFolder() const
{
    return runs_folder / kProfileFolderNames[static_cast<size_t>(active_profile_idx)];
}


// =============================================================================
// PB / COMPARISON / RUN HISTORY
// =============================================================================

int SplitsWindow::ActiveGoalCount() const
{
    return static_cast<int>(std::ranges::count_if(active_list.goals, [](const GoalEntry& g) { return !g.is_header; }));
}

int SplitsWindow::CompletedGoalCount() const
{
    return static_cast<int>(std::ranges::count_if(active_list.goals, [](const GoalEntry& g) {
        return !g.is_header && g.status == GoalStatus::Completed;
    }));
}

void SplitsWindow::LoadPB(bool refresh_comparisons)
{
    const bool has_file = !runs_folder.empty() && !active_list.name.empty();
    run_history.Load(has_file ? RunHistoryFilePath() : std::filesystem::path{}, has_file ? ActiveGoalCount() : 0, refresh_comparisons);
}

void SplitsWindow::DeleteRunsFromHistory(const std::vector<int>& attempt_numbers)
{
    if (runs_folder.empty() || active_list.name.empty() || attempt_numbers.empty()) return;
    run_history.DeleteRuns(RunHistoryFilePath(), attempt_numbers, ActiveGoalCount());
}

void SplitsWindow::ClearRunHistory()
{
    if (runs_folder.empty() || active_list.name.empty()) return;
    run_history.Clear(RunHistoryFilePath(), ActiveGoalCount());
}


void SplitsWindow::UpdateReferenceIfPB()
{
    const double pb_total = run_history.PBTotalReal();
    const auto& pb_splits = run_history.Comparison(SplitsProfile::ComparisonMode::PB).real;
    if (std::isnan(pb_total) || pb_splits.empty()) return;

    double ref_total = std::numeric_limits<double>::infinity();
    if (active_list.reference.has_value() && !active_list.reference->splits.empty())
        ref_total = active_list.reference->splits.back();

    if (pb_total >= ref_total) return;

    GoalReference& ref = active_list.reference.emplace();
    ref.splits = pb_splits;

    SaveActiveList(/*clear_preset=*/false);
}


void SplitsWindow::SaveCompletedRun()
{
    run_complete = true;
    run_clock.Pause();
    SaveRunToHistory(/*failed=*/false);
    LoadPB(/*refresh_comparisons=*/false);
    UpdateReferenceIfPB();
    DeleteResumeState();
    if (ActiveProfile().auto_send_age)
        GW::Chat::SendChat('/', L"age");
}


void SplitsWindow::FailRun()
{
    // RealTime()>0 too: Running auto-pause can fire earlier same tick.
    if (run_complete || run_failed || (!run_clock.IsRunning() && run_clock.RealTime() <= 0.0)) return;
    engine.FailRun(run_clock);
    run_clock.Pause();
    run_failed = true;
    SendLiveSplit("reset");
    SaveRunToHistory(/*failed=*/true);
    // PB-only reload, same as SaveCompletedRun: refreshes Recent Runs without folding this run into Avg/SoB yet.
    LoadPB(/*refresh_comparisons=*/false);
    DeleteResumeState();
}


void SplitsWindow::SaveRunToHistory(bool failed)
{
    if (runs_folder.empty() || active_list.name.empty()) return;
    const bool doa_list = SCPresets::AnchorMapId(active_list) == GW::Constants::MapID::Domain_of_Anguish;
    run_history.Save(RunHistoryFilePath(), active_list, failed, run_clock.RealTime(),
                       run_start_unix, run_char_name, total_paused_real,
                       doa_list ? SCPresets::DoAZoneName(doa_start_zone) : "");
}


// =============================================================================
// PAUSE / RESUME
// =============================================================================

// Resume = deliberate pause checkpoint, not crash recovery: after crash, game state can't be trusted.
void SplitsWindow::SaveResumeState()
{
    if (splits_folder.empty() || !manually_paused || active_list.name.empty()) return;

    SerializedResume j;
    j.list_name    = active_list.name;
    j.real_time    = run_clock.RealTime();
    j.game_time    = run_clock.GameTime();
    j.total_paused = total_paused_real;
    j.start_unix   = run_start_unix;
    j.char_name    = run_char_name;

    j.goals.reserve(active_list.goals.size());
    for (const auto& g : active_list.goals) {
        // Save Started too, else partial MobKill count lost.
        if (g.status != GoalStatus::Completed && g.status != GoalStatus::Started) {
            j.goals.emplace_back(std::nullopt);
        } else {
            SerializedSplit js{
                g.split.real_time, g.split.game_time, g.split.segment_real, g.split.segment_game,
                g.start_real_time, g.start_game_time, g.trigger_progress,
                g.status == GoalStatus::Completed ? "Completed" : "Started"};
            j.goals.push_back(std::move(js));
        }
    }

    if (!Resources::WriteFile(splits_folder / L"resume.json", glz::write<glz::opts{.prettify = true}>(j).value_or(std::string{})))
        Log::Error("Splits: failed to write resume.json");
}

void SplitsWindow::DeleteResumeState()
{
    pending_resume = false;
    pending_resume_data.clear();
    if (splits_folder.empty()) return;
    std::error_code ec;
    std::filesystem::remove(splits_folder / L"resume.json", ec);
}


void SplitsWindow::ApplyResume()
{
    if (!pending_resume) return;
    pending_resume = false;

    SerializedResume j;
    constexpr glz::opts opts{.error_on_unknown_keys = false};
    const bool parse_failed = static_cast<bool>(glz::read<opts>(j, pending_resume_data));
    pending_resume_data.clear();
    if (parse_failed) return;

    if (j.list_name.empty()) return;
    const double real_time = j.real_time;
    const double game_time = j.game_time;

    // Never preset: resume never written in SC.
    const auto list_path = ActiveSplitsFolder() / (GoalList::FileStem(j.list_name) + L".json");
    ReplaceActiveList([&] { active_list.LoadFromFile(list_path); });

    double last_split_real = 0.0, last_split_game = 0.0;
    for (size_t i = 0; i < j.goals.size() && i < active_list.goals.size(); ++i) {
        const auto& jg = j.goals[i];
        if (!jg.has_value()) continue;
        auto& g              = active_list.goals[i];
        g.trigger_progress   = jg->trigger_progress;
        if (jg->status == "Started") {
            g.status          = GoalStatus::Started;
            g.start_real_time = jg->start_real_time;
            g.start_game_time = jg->start_game_time;
        } else {
            g.status             = GoalStatus::Completed;
            g.split.real_time    = jg->real_time;
            g.split.game_time    = jg->game_time;
            g.split.segment_real = jg->segment_real;
            g.split.segment_game = jg->segment_game;
            last_split_real = std::max(last_split_real, jg->real_time);
            last_split_game = std::max(last_split_game, jg->game_time);
        }
    }

    // Stay paused: time between sessions never counted.
    run_clock.Restore(real_time, game_time);
    manually_paused    = true;
    manual_pause_accum = 0.0;
    engine.ForceStarted();
    // Attach zeroed baseline; else first split measure from 0:00.
    engine.RestoreLastSplit(last_split_real, last_split_game);
    total_paused_real   = j.total_paused.value_or(0.0);
    livesplit_splits_sent    = CompletedGoalCount();
    livesplit_paused         = true; // comes back paused
    livesplit_resync_pending = true;
    run_start_unix      = j.start_unix.value_or(0);
    run_char_name       = j.char_name.value_or("");
    pending_map_enter            = false;
    pending_came_from_explorable = false;
}

// =============================================================================
// RUN LIFECYCLE CONTROLS
// =============================================================================

void SplitsWindow::ResetRunFlags()
{
    run_complete              = false;
    run_failed                = false;
    running_awaiting_movement = false;
    running_load_paused       = false;
    pending_skill_id          = 0;
    in_mission_queue          = false;
    manually_paused           = false;
    manual_pause_accum        = 0.0;
    total_paused_real         = 0.0;
    livesplit_splits_sent     = 0;
    livesplit_paused          = false;
    livesplit_resync_pending  = false;
    title_start_points        = -1;
    pending_map_enter            = false;
    pending_came_from_explorable = false;
}


void SplitsWindow::BeginRun()
{
    run_char_name.clear();
    if (const wchar_t* wname = GW::PlayerMgr::GetPlayerName())
        run_char_name = TextUtils::WStringToString(wname);
    run_start_unix = static_cast<int64_t>(time(nullptr));
    SendLiveSplit("reset");
    SendLiveSplit("start");
    livesplit_splits_sent = 0;
    livesplit_paused      = false;
    run_clock.Start();
}


void SplitsWindow::StartRun()
{
    if (run_clock.IsRunning()) {
        run_clock.Pause();
        SetLiveSplitPaused(true);
        manually_paused    = true;
        manual_pause_accum = 0.0;
        // SC instances not survive restart: no checkpoint.
        if (active_profile_idx != kProfileSC) SaveResumeState();
        return;
    }

    if (manually_paused) {
        total_paused_real += manual_pause_accum;
        manually_paused = false;
        run_clock.Start();
        if (livesplit_resync_pending) {
            // Sent now, not in ApplyResume: at startup LiveSplit usually hasn't reconnected yet and would miss it.
            livesplit_resync_pending = false;
            SendLiveSplit("reset");
            SendLiveSplit("start");
            livesplit_splits_sent = 0;
            livesplit_paused      = false;
            SyncLiveSplitSplits();
        }
        else {
            SetLiveSplitPaused(false);
        }
        // Stale snapshot would offer rewind next launch.
        DeleteResumeState();
        return;
    }

    if (running_load_paused || running_awaiting_movement) {
        // Mid-run (auto-paused or awaiting move): not fresh start, don't wipe progress.
        return;
    }

    // Finished run's clock/flags still set until Reset.
    if (run_complete || run_failed) {
        run_clock.Reset();
        ResetRunFlags();
    }
    engine.Attach(&active_list);
    // Full refresh so Avg/SoB include run just finished.
    LoadPB();
    BeginRun();
    engine.ForceStarted();
}

void SplitsWindow::ResetRun()
{
    if (run_clock.IsRunning() || run_complete || run_failed)
        SendLiveSplit("reset");
    DeleteResumeState();
    ResetRunFlags();
    engine.Reset();
    run_clock.Reset();
    // last_map kept: no fake MapEnter on reset.
    LoadPB();
    if (NuzlockeDeathTrackerEnabled()) nuzlocke.ResetProgress();
}

void CHAT_CMD_FUNC(SplitsWindow::CmdSplits)
{
    const std::wstring arg = argc > 1 ? TextUtils::ToLower(argv[1]) : L"";
    auto& splits = Instance();
    if (arg == L"start")      splits.StartRun();
    else if (arg == L"split") splits.TriggerManualSplit();
    else if (arg == L"reset") splits.ResetRun();
    else Log::Error("Usage: /splits start|split|reset (start also pauses/unpauses)");
}

void SplitsWindow::TriggerManualSplit()
{
    engine.TriggerManual(run_clock);
    SyncLiveSplitSplits();
}

void SplitsWindow::SwitchProfile(int idx)
{
    if (idx < 0 || idx >= kProfileCount || idx == active_profile_idx) return;

    profiles[active_profile_idx].last_list_name = active_list.name;
    active_profile_idx = idx;

    // last_map kept: None would fake just_entered_map and reload presets.
    DeleteResumeState();
    ResetRunFlags();
    run_clock.Reset();
    LoadProfileLastList();
}

void SplitsWindow::LoadProfileLastList()
{
    // No DeleteResumeState: at startup a resume prompt may still be pending.
    ReplaceActiveList([&] {
        active_list = GoalList{};
        const std::string& name = ActiveProfile().last_list_name;
        if (name.empty() || splits_folder.empty()) return;
        const auto path = ActiveSplitsFolder() / (GoalList::FileStem(name) + L".json");
        if (std::filesystem::exists(path)) active_list.LoadFromFile(path);
    });
}


// =============================================================================
// TICK LOOP & TIMER POLICY
// =============================================================================

void SplitsWindow::Update(float delta)
{
    ApplyLiveSplitSettings();
    if (NuzlockeDeathTrackerEnabled()) nuzlocke.Update(last_was_explorable);

    const auto instance_type   = GW::Map::GetInstanceType();
    const bool is_explorable   = (instance_type == GW::Constants::InstanceType::Explorable);
    const bool is_loading      = (instance_type == GW::Constants::InstanceType::Loading);
    const bool in_cinematic    = GW::Map::GetIsInCinematic();
    const bool is_running      = ActiveProfile().sequential_route;

    const bool just_entered_map     = pending_map_enter;
    const bool came_from_explorable = pending_came_from_explorable;
    pending_map_enter            = false;
    pending_came_from_explorable = false;
    // Re-seed title baseline on load: char switch always a map load.
    if (just_entered_map && !run_clock.IsRunning()) title_start_points = -1;

    // Before engine.Update() so swapped preset attached this tick.
    ApplySCAutoLoadPreset(just_entered_map);

    // RealTime frozen in manual pause, can't measure it.
    if (manually_paused)
        manual_pause_accum += static_cast<double>(delta);
    const bool time_paused = is_loading || in_cinematic
        || in_mission_queue
        || InDoAHub();

    run_clock.AddRealTime(static_cast<double>(delta));

    if (!time_paused)
        run_clock.AddGameTime(static_cast<double>(delta));

    const GW::Agent* controlled = GW::Agents::GetControlledCharacter();
    const GW::AgentLiving* controlled_living = controlled ? controlled->GetAsAgentLiving() : nullptr;
    // Poll only until level event seeds it (min level 1).
    if (player_level == 0 && controlled_living)
        player_level = static_cast<int>(controlled_living->level);

    if (is_explorable)
        RefreshMobNameCache();

    if (is_running) {
        if (!is_explorable && run_clock.IsRunning() && !running_load_paused) {
            run_clock.Pause();
            SetLiveSplitPaused(true);
            running_load_paused = true;
        }
        // Arm continuously, like GWChrono, so no re-zone needed.
        if (is_explorable && !run_clock.IsRunning() && !run_complete && !run_failed)
            running_awaiting_movement = true;
    }

    if (is_running && !run_complete && !run_failed) {
        if (running_awaiting_movement && is_explorable) {
            const uint32_t skill = pending_skill_id;
            pending_skill_id    = 0;

            bool triggered = skill != 0 && std::ranges::contains(kShadowStepSkills, static_cast<SkillID>(skill));
            if (!triggered) {
                triggered = controlled_living &&
                    (controlled_living->GetIsMoving() ||
                     controlled_living->model_state == 204 ||
                     controlled_living->move_x != 0.f ||
                     controlled_living->move_y != 0.f);
            }

            if (triggered) {
                running_awaiting_movement = false;
                if (running_load_paused) {
                    run_clock.Start();
                    SetLiveSplitPaused(false);
                    running_load_paused = false;
                } else {
                    BeginRun();
                    engine.ForceStarted();
                }
            }
        } else {
            pending_skill_id = 0;
        }
    }

    // IsRunning() catch Start tick; RealTime()>0 catch Pause-on-leave tick.
    const bool fire_map_enter = !is_running
        ? just_entered_map
        : (just_entered_map && (run_clock.IsRunning() || run_clock.RealTime() > 0.0));

    // Synchronous last_was_explorable, not polled is_explorable.
    const int fired = engine.Update(run_clock, last_map, fire_map_enter,
                                     came_from_explorable, last_was_explorable,
                                     player_level, delta);

    SyncLiveSplitSplits();

    ApplyTimerPolicy(just_entered_map);
    StartDoAZone();

    // Running: final outpost split fire with clock paused.
    if (!run_complete && !run_failed && ActiveGoalCount() > 0 &&
        (run_clock.IsRunning() || (is_running && fired > 0))) {
        if (CompletedGoalCount() == ActiveGoalCount()) SaveCompletedRun();
    }
}


void SplitsWindow::ApplyTimerPolicy(const bool just_entered_map)
{
    const bool party_defeated = pending_party_defeated;
    pending_party_defeated    = false;
    if (party_defeated && ActiveProfile().stop_on_party_defeated && run_clock.IsRunning())
        FailRun();

    // Always drained so flag not go stale when setting off.
    if (engine.ConsumeIncompleteRezone() && ActiveProfile().auto_fail_on_rezone && run_clock.IsRunning())
        FailRun();

    const bool is_running = ActiveProfile().sequential_route;

    // RealTime()>0 too: same auto-pause race as FailRun.
    if (engine.ConsumeWrongMapEntered() && is_running && (run_clock.IsRunning() || run_clock.RealTime() > 0.0))
        FailRun();

    if (!is_running && (run_complete || run_failed) && ActiveProfile().auto_reset_on_complete && just_entered_map)
        ResetRun();

    // Start on first sign of attempt, not completion. Separate from GoalEngine on purpose.
    // New trigger with start->done gap need rule here too.
    if (!is_running &&
        !run_clock.IsRunning() && !run_complete && !run_failed && !manually_paused) {
        bool should_start = false;
        // Elite checkpoints have no map_id: use header's.
        GW::Constants::MapID owning_header_map = GW::Constants::MapID::None;
        for (const auto& g : active_list.goals) {
            if (g.is_header) {
                owning_header_map = g.trigger.map_id;
                continue;
            }
            if (g.status == GoalStatus::Started || g.status == GoalStatus::Completed) {
                should_start = true;
            } else {
                // Dispatch on type first so kill/title on map-entry tick still checked.
                using TT = GoalTrigger::Type;
                const auto tt = g.trigger.type;
                if (tt == TT::MissionComplete || tt == TT::MissionBonus || tt == TT::VanquishComplete ||
                    tt == TT::DungeonReward) {
                    // last_was_explorable not polled type: poll lag one frame at transition. Also skip outpost entry.
                    if (just_entered_map && last_was_explorable && last_map == g.trigger.map_id) should_start = true;
                } else if (owning_header_map != GW::Constants::MapID::None) {
                    if (just_entered_map && last_was_explorable && last_map == owning_header_map) should_start = true;
                } else if (tt == TT::ReachTitleRank) {
                    // Points above armed baseline, not any points. Null = 0 points, unless loading (no data).
                    const GW::Title* title = GW::PlayerMgr::GetTitleTrack(g.trigger.title_id);
                    const bool readable = title || GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading;
                    const int64_t points = title ? title->current_points : 0;
                    if (readable) {
                        if (title_start_points < 0) title_start_points = points;
                        else if (points > title_start_points) should_start = true;
                    }
                } else if (tt == TT::MobKill) {
                    if (g.trigger_progress > 0) should_start = true;
                }
            }
            break;
        }
        if (should_start) BeginRun();
    }
}


void SplitsWindow::RefreshMobNameCache()
{
    const bool has_mobkill_goal = std::ranges::any_of(active_list.goals, [](const GoalEntry& g) {
        return g.trigger.type == GoalTrigger::Type::MobKill;
    });
    if (!has_mobkill_goal) return;

    const GW::AgentArray* agents = GW::Agents::GetAgentArray();
    if (!agents) return;
    for (const auto* agent : *agents) {
        const GW::AgentLiving* living = agent ? agent->GetAsAgentLiving() : nullptr;
        if (!living || living->IsPlayer()) continue; // not Allegiance::Enemy, matches MobKill

        if (living->GetIsAlive()) {
            if (mob_name_cache.contains(living->agent_id)) continue;
            auto entry = std::make_unique<GuiUtils::EncString>();
            entry->reset(GW::Agents::GetAgentEncName(living->agent_id));
            mob_name_cache[living->agent_id] = std::move(entry);
            continue;
        }

        // Dead now, seen alive before: fire once, drop. Never seen alive = skip.
        const auto it = mob_name_cache.find(living->agent_id);
        if (it == mob_name_cache.end()) continue;
        const std::wstring* name = it->second ? &it->second->wstring() : nullptr;
        engine.NotifyEvent(GoalTrigger::Type::MobKill, living->player_number, 0,
                            name ? name->c_str() : nullptr, name ? name->size() : 0);
        mob_name_cache.erase(it);
    }
}

bool SplitsWindow::InDoAHub() const
{
    if (!run_clock.IsRunning() || SCPresets::AnchorMapId(active_list) != GW::Constants::MapID::Domain_of_Anguish) return false;
    bool any_done = false, any_left = false;
    for (size_t i = 0; i < active_list.goals.size(); ++i) {
        if (!active_list.goals[i].is_header) continue;
        int total = 0, completed = 0;
        bool started = false;
        for (size_t j = i + 1; j < active_list.goals.size() && !active_list.goals[j].is_header; ++j) {
            const auto status = active_list.goals[j].status;
            ++total;
            if (status == GoalStatus::Completed) ++completed;
            if (status == GoalStatus::Started) started = true;
        }
        if (total == 0) continue; // the root header: zones are its sub-headers
        if (started || (completed > 0 && completed < total)) return false; // a zone is in progress
        if (completed == total) any_done = true;
        else any_left = true;
    }
    return any_done && any_left;
}

void SplitsWindow::ApplyLiveSplitSettings()
{
    const auto& s = settings;
    if (s.livesplit_enabled == livesplit_applied_enabled && s.livesplit_port == livesplit_applied_port) return;
    livesplit_applied_enabled = s.livesplit_enabled;
    livesplit_applied_port    = s.livesplit_port;
    if (s.livesplit_enabled) livesplit.Start(s.livesplit_port);
    else livesplit.Stop();
}

void SplitsWindow::SetLiveSplitPaused(const bool paused)
{
    if (paused == livesplit_paused) return;
    livesplit_paused = paused;
    SendLiveSplit(paused ? "pause" : "resume");
}

void SplitsWindow::SyncLiveSplitSplits()
{
    // Before the run starts there's nothing to split; BeginRun zeroes the count, so a goal from the start tick still goes out next frame.
    if (!run_clock.IsRunning() && run_clock.RealTime() <= 0.0) return;
    const int completed = CompletedGoalCount();
    if (completed <= livesplit_splits_sent) {
        livesplit_splits_sent = completed;
        return;
    }
    const bool was_paused = livesplit_paused;
    SetLiveSplitPaused(false);
    for (; livesplit_splits_sent < completed; ++livesplit_splits_sent)
        SendLiveSplit("split");
    SetLiveSplitPaused(was_paused);
}

void SplitsWindow::StartDoAZone()
{
    if (!doa_zone_start_pending) return;
    doa_zone_start_pending = false;
    // A run already past its first objective is mid-clear, not a fresh entry.
    if (CompletedGoalCount() > 0) return;
    const int first = SCPresets::DoAZoneFirstGoal(active_list, doa_start_zone);
    if (first < 0) return;
    // Entrance DoorClose can fire before we know the rotation, so the starting zone is started here instead.
    GoalEntry& g = active_list.goals[static_cast<size_t>(first)];
    if (g.status == GoalStatus::NotStarted) {
        g.status          = GoalStatus::Started;
        g.start_real_time = run_clock.RealTime();
        g.start_game_time = run_clock.GameTime();
    }
    engine.ForceStarted();
}

void SplitsWindow::ApplySCAutoLoadPreset(const bool just_entered_map)
{
    if (active_profile_idx != kProfileSC) return;

    // DoA: file_id 219215 (same as OT), rotation from spawn.
    if (pending_doa_file_id == 219215) {
        const GW::Vec2f spawn = pending_doa_spawn;
        pending_doa_file_id  = 0; // consume even if no swap
        const int starting_zone = SCPresets::DetectDoAStartingZone(spawn);
        if (starting_zone == -1) return; // Mallyx
        // Any DoA list (incl. renamed variants) already fits every rotation; only a different area's list is swapped.
        if (SCPresets::AnchorMapId(active_list) != GW::Constants::MapID::Domain_of_Anguish && !run_clock.IsRunning())
            SetActiveList(SCPresets::BuildDoAPresetList(), /*keep_preset=*/true);
        doa_start_zone         = starting_zone;
        doa_zone_start_pending = true;
        return;
    }

    // Explorable only: ToPK's entry map id is also a town (The_Underworld_PvP).
    if (!just_entered_map || !last_was_explorable || run_clock.IsRunning()) return;

    auto preset = SCPresets::BuildPresetForMap(last_map);
    if (!preset) return;

    // By anchor map_id: renamed list for same dungeon still counts.
    if (SCPresets::AnchorMapId(*preset) != GW::Constants::MapID::None &&
        SCPresets::AnchorMapId(active_list) == SCPresets::AnchorMapId(*preset))
        return;

    SetActiveList(std::move(*preset), /*keep_preset=*/true);
}


// =============================================================================
// DRAW
// =============================================================================

void SplitsWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) return;
    ui.Draw(*this);
}


void SplitsWindow::DrawHelp()
{
    if (!ImGui::TreeNodeEx("Splits Chat Commands", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        return;
    }
    ImGui::Bullet();
    ImGui::Text("'/splits start' starts the run, or pauses and unpauses it once running.");
    ImGui::Bullet();
    ImGui::Text("'/splits split' completes the next Manual goal.");
    ImGui::Bullet();
    ImGui::Text("'/splits reset' resets the run.");
    ImGui::Bullet();
    ImGui::Text("Bind any of these as a Send Chat hotkey in the Hotkeys window.");
    ImGui::TreePop();
}

void SplitsWindow::DrawSettingsInternal()
{
    ui.DrawSettings(*this);

    // Hidden outside Manual, not disabled, so no confusion.
    if (active_profile_idx == kProfileManual) {
        ImGui::Separator();
        ImGui::TextUnformatted("Nuzlocke");
        ImGui::Indent();

        auto& ns = nuzlocke.settings;
        ImGui::Checkbox("Death Tracker", &ns.death_tracker_enabled);
        if (ns.death_tracker_enabled) {
            ImGui::Indent();
            for (const auto& f : kNuzlockeLivesFields) {
                ImGui::SetNextItemWidth(120.f * ImGui::FontScale());
                ImGui::InputInt(f.label, &(ns.*f.member));
                ns.*f.member = std::max(ns.*f.member, 1);
            }

            ImGui::CheckboxWithHelp("Merge same-named henchmen across campaigns", &ns.merge_hench_by_name,
                "Off: a henchman name reused by a different NPC/build in another campaign\n"
                "or outpost (e.g. two different \"Eve\"s) tracks as a separate entry.\n"
                "On: any henchman sharing that display name is folded into one entry,\n"
                "sharing the same life count.\n"
                "Only affects henchmen tracked from here on, not ones already seen this session.");
            ImGui::Unindent();
        }

        ImGui::Checkbox("Points", &ns.points_enabled);
        if (ns.points_enabled) {
            ImGui::Indent();
            ImGui::TextDisabled("Leave at 0 for goal types you don't want scored.");
            for (const auto& f : kNuzlockePointFields) {
                ImGui::SetNextItemWidth(100.f * ImGui::FontScale());
                ImGui::InputInt(f.label, &(ns.*f.member));
            }
            ImGui::Unindent();
        }

        ImGui::Unindent();
    }

    ImGui::Separator();
    auto& s = settings;
    ImGui::CheckboxWithHelp("Enable LiveSplit websocket server", &s.livesplit_enabled,
                            "Sends Start/Split/Reset/Pause to a connected LiveSplit.\n"
                            "Own port, separate from the Objective Timer's server, so both can run.");
    if (s.livesplit_enabled) {
        ImGui::Indent();
        ImGui::SetNextItemWidth(120.f * ImGui::FontScale());
        if (!livesplit_port_editing) livesplit_port_edit = s.livesplit_port;
        ImGui::InputInt("Websocket server port", &livesplit_port_edit, 0);
        livesplit_port_editing = ImGui::IsItemActive();
        if (ImGui::IsItemDeactivatedAfterEdit()) s.livesplit_port = std::clamp(livesplit_port_edit, 1, 65535);
        ImGui::Text("Status: %s", livesplit.IsRunning() ? "Running" : "Stopped");
        ImGui::SameLine();
        if (ImGui::SmallButton("Restart")) livesplit.Start(s.livesplit_port);
        using LF = LiveSplitServer::Format;
        if (ImGui::RadioButton("LiveSplit One JSON Format", s.livesplit_format == LF::LiveSplitOneJSON)) s.livesplit_format = LF::LiveSplitOneJSON;
        if (ImGui::RadioButton("LiveSplit Server Command Format", s.livesplit_format == LF::LiveSplitServerCommand)) s.livesplit_format = LF::LiveSplitServerCommand;
        ImGui::Unindent();
    }
}


void SplitsWindow::DrawNuzlockeSection()
{
    if (NuzlockeDeathTrackerEnabled()) nuzlocke.Draw();
}

int SplitsWindow::NuzlockeTotalPoints() const
{
    return nuzlocke.TotalPoints(active_list);
}
