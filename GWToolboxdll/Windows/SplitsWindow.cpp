#include "stdafx.h"

#include <Windows/SplitsWindow.h>
#include <Windows/Splits/MapNames.h>
#include <Windows/Splits/Presets.h>

#include <Modules/Resources.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/Packets/StoC.h>
#include <GWCA/Packets/Opcodes.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameContainers/Array.h>

// ---------------------------------------------------------------------------
// JSON DTOs for resume.json / per-list run history (glaze reflection requires external linkage).
// ---------------------------------------------------------------------------
namespace SplitsWindowJson {
    struct SerializedSplit {
        double real_time    = 0.0;
        double game_time    = 0.0;
        double segment_real = 0.0;
        double segment_game = 0.0;
    };

    struct SerializedResume {
        std::string list_name;
        double      real_time = 0.0;
        double      game_time = 0.0;
        std::optional<double> total_paused;
        std::vector<std::optional<SerializedSplit>> goals;
    };

    struct SerializedRunSplit {
        double real_time = 0.0;
        double game_time = 0.0;
    };

    struct SerializedRunGoal {
        std::string label;
        std::string status; // "Completed" / "Failed" / "Started" / "NotStarted"
        double real_time = 0.0;
        double game_time = 0.0;
    };

    struct SerializedRun {
        double total_real = 0.0;
        std::vector<SerializedRunSplit> splits;
        std::optional<bool> failed;
        std::optional<int64_t> utc_start;
        std::optional<std::string> character_name;
        std::optional<std::vector<SerializedRunGoal>> goals;
        std::optional<double> total_paused;
    };
}
using namespace SplitsWindowJson;

// Shadow-step skills that should trigger auto-start the same as movement.
// Mirrors TimerLogic.cpp in GWChrono.
static const std::unordered_set<uint32_t> kShadowStepSkills = {
     769,  // Viper's Defense
     770,  // Return
     771,  // Aura of Displacement
     799,  // Beguiling Haze
     815,  // Scorpion Wire
     836,  // Ride the Lightning
     925,  // Recall
     952,  // Death's Charge
    1032,  // Heart of Shadow
    1040,  // Spirit Walk
    1044,  // Dark Prison
    1644,  // Wastrel's Collapse
    1646,  // Augury of Death
    1650,  // Shadow Walk
    1651,  // Death's Retreat
    1652,  // Shadow Prison
    1653,  // Swap
    1654,  // Shadow Meld
    2052,  // Shadow Fang
    2420,  // Ebon Escape
    3428,  // Shadow Theft
};

// Manual: maps with a "Time until mission start" ready-check dialog whose wait should pause
// game time. Vizunah Square / Unwaking Waters lock the party (PartyLock) while waiting; the
// Ascalon Academy queue is solo (no party to lock) and only sends the generic countdown
// packet — both paths funnel into in_mission_queue_, cleared on the next real map change.
static bool IsMissionQueueMap(GW::Constants::MapID map)
{
    static constexpr GW::Constants::MapID kQueueMaps[] = {
        GW::Constants::MapID::Vizunah_Square_Local_Quarter_outpost,
        GW::Constants::MapID::Vizunah_Square_Foreign_Quarter_outpost,
        GW::Constants::MapID::Unwaking_Waters_Luxon_outpost,
        GW::Constants::MapID::Unwaking_Waters_Kurzick_outpost,
        GW::Constants::MapID::Ascalon_City_pre_searing,
    };
    for (const auto m : kQueueMaps) {
        if (map == m) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Initialize / Terminate
// ---------------------------------------------------------------------------
void SplitsWindow::Initialize()
{
    ToolboxWindow::Initialize();
    // MapNames::Init() deferred to first Update() — GW UI context may not be ready yet.

    GW::UI::RegisterUIMessageCallback(
        &on_mission_complete_hook_,
        GW::UI::UIMessage::kMissionComplete,
        [this](GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
            engine_.NotifyMissionComplete(GW::Map::GetMapID());
        });

    GW::UI::RegisterUIMessageCallback(
        &on_objective_complete_hook_,
        GW::UI::UIMessage::kObjectiveComplete,
        [this](GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
            engine_.NotifyMissionBonus(GW::Map::GetMapID());
        });

    GW::UI::RegisterUIMessageCallback(
        &on_vanquish_complete_hook_,
        GW::UI::UIMessage::kVanquishComplete,
        [this](GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
            engine_.NotifyVanquishComplete(GW::Map::GetMapID());
        });

    GW::UI::RegisterUIMessageCallback(
        &on_party_defeated_hook_,
        GW::UI::UIMessage::kPartyDefeated,
        [this](GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
            if (ActiveProfile().stop_on_party_defeated && clock_.IsRunning())
                FailRun();
        });

    // Preset-only StoC hooks for elite area / dungeon triggers
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveDone>(
        &on_objective_done_hook_,
        [this](GW::HookStatus*, const GW::Packet::StoC::ObjectiveDone* p) {
            engine_.NotifyEvent(GoalTrigger::Type::ObjectiveDone, p->objective_id);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveUpdateName>(
        &on_objective_started_hook_,
        [this](GW::HookStatus*, const GW::Packet::StoC::ObjectiveUpdateName* p) {
            engine_.NotifyEvent(GoalTrigger::Type::ObjectiveStarted, p->objective_id);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ManipulateMapObject>(
        &on_door_hook_,
        [this](GW::HookStatus*, const GW::Packet::StoC::ManipulateMapObject* p) {
            if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) return;
            if (p->animation_type == 16 && p->animation_stage == 2)
                engine_.NotifyEvent(GoalTrigger::Type::DoorOpen, p->object_id);
            else if (p->animation_type == 3 && p->animation_stage == 2)
                engine_.NotifyEvent(GoalTrigger::Type::DoorClose, p->object_id);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentUpdateAllegiance>(
        &on_agent_allegiance_hook_,
        [this](GW::HookStatus*, const GW::Packet::StoC::AgentUpdateAllegiance* p) {
            const GW::Agent* agent = GW::Agents::GetAgentByID(p->agent_id);
            if (!agent) return;
            const GW::AgentLiving* living = agent->GetAsAgentLiving();
            if (!living) return;
            engine_.NotifyEvent(GoalTrigger::Type::AgentUpdateAllegiance,
                                living->player_number, p->allegiance_bits);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DoACompleteZone>(
        &on_doa_zone_hook_,
        [this](GW::HookStatus*, const GW::Packet::StoC::DoACompleteZone* p) {
            if (p->message[0] == 0x8101)
                engine_.NotifyEvent(GoalTrigger::Type::DoACompleteZone, p->message[1]);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DungeonReward>(
        &on_dungeon_reward_hook_,
        [this](GW::HookStatus*, GW::Packet::StoC::DungeonReward*) {
            engine_.NotifyEvent(GoalTrigger::Type::DungeonReward);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageServer>(
        &on_server_message_hook_,
        [this](GW::HookStatus*, GW::Packet::StoC::MessageServer*) {
            const GW::Array<wchar_t>* buff = &GW::GetGameContext()->world->message_buff;
            if (!buff || !buff->valid() || !buff->size()) return;
            const wchar_t* msg = buff->begin();
            engine_.NotifyEvent(GoalTrigger::Type::ServerMessage, 0, 0, msg, wcslen(msg));
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DisplayDialogue>(
        &on_display_dialogue_hook_,
        [this](GW::HookStatus*, const GW::Packet::StoC::DisplayDialogue* p) {
            engine_.NotifyEvent(GoalTrigger::Type::DisplayDialogue, 0, 0,
                                p->message, wcslen(p->message));
        });

    // Cache DoA spawn point so the preset builder can determine starting area.
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadFile>(
        &on_instance_load_file_hook_,
        [this](GW::HookStatus*, const GW::Packet::StoC::InstanceLoadFile* p) {
            doa_spawn_point_ = p->spawn_point;
        });

    // ToPK arena countdown — fires when the GvG match countdown begins. Also doubles as the
    // Ascalon Academy queue's only signal (solo queue, no PartyLock); gated to our map list so
    // ToPK/other countdown uses elsewhere don't pause Manual's game time.
    GW::StoC::RegisterPacketCallback(
        &on_countdown_start_hook_, GAME_SMSG_INSTANCE_COUNTDOWN,
        [this](GW::HookStatus*, GW::Packet::StoC::PacketBase*) {
            const GW::Constants::MapID map = GW::Map::GetMapID();
            if (active_profile_idx_ == 1 && IsMissionQueueMap(map))
                in_mission_queue_ = true;
            engine_.NotifyEvent(GoalTrigger::Type::CountdownStart,
                                static_cast<uint32_t>(map));
        });

    // Running profile: capture local player skill use for shadow-step auto-start.
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::SkillActivate>(
        &on_skill_activate_hook_,
        [this](GW::HookStatus*, GW::Packet::StoC::SkillActivate* p) {
            if (p->agent_id == GW::Agents::GetControlledCharacterId())
                pending_skill_id_ = p->skill_id;
        });

    // Manual: "Time until mission start" ready-check dialog (Vizunah Square, Unwaking Waters)
    // — server toggles party lock while the queue is pending. Gated to our map list since
    // PartyLock also fires for unrelated party-window locks (e.g. mission entry elsewhere)
    // where we don't want to pause game time.
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyLock>(
        &on_mission_queue_hook_,
        [this](GW::HookStatus*, GW::Packet::StoC::PartyLock* p) {
            if (!p->unk2) {
                in_mission_queue_ = false;
                return;
            }
            if (active_profile_idx_ == 1 && IsMissionQueueMap(GW::Map::GetMapID()))
                in_mission_queue_ = true;
        });
}

void SplitsWindow::Terminate()
{
    GW::UI::RemoveUIMessageCallback(&on_mission_complete_hook_);
    GW::UI::RemoveUIMessageCallback(&on_objective_complete_hook_);
    GW::UI::RemoveUIMessageCallback(&on_vanquish_complete_hook_);
    GW::UI::RemoveUIMessageCallback(&on_party_defeated_hook_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::ObjectiveDone>(&on_objective_done_hook_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::ObjectiveUpdateName>(&on_objective_started_hook_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::ManipulateMapObject>(&on_door_hook_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentUpdateAllegiance>(&on_agent_allegiance_hook_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::DoACompleteZone>(&on_doa_zone_hook_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::DungeonReward>(&on_dungeon_reward_hook_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::MessageServer>(&on_server_message_hook_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::DisplayDialogue>(&on_display_dialogue_hook_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::InstanceLoadFile>(&on_instance_load_file_hook_);
    GW::StoC::RemoveCallback(GAME_SMSG_INSTANCE_COUNTDOWN, &on_countdown_start_hook_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::SkillActivate>(&on_skill_activate_hook_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::PartyLock>(&on_mission_queue_hook_);
    engine_.Detach();
    ToolboxWindow::Terminate();
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------
void SplitsWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWindow::LoadSettings(doc, legacy);

    // Establish data folders — profile subfolders created below
    const auto splits_path = Resources::GetPath(L"splits");
    const auto runs_path   = Resources::GetPath(L"splits\\runs");
    Resources::EnsureFolderExists(splits_path);
    Resources::EnsureFolderExists(runs_path);
    Resources::EnsureFolderExists(splits_path / L"manual");
    Resources::EnsureFolderExists(splits_path / L"running");
    Resources::EnsureFolderExists(runs_path   / L"manual");
    Resources::EnsureFolderExists(runs_path   / L"running");
    splits_folder_ = splits_path.wstring() + L"\\";
    runs_folder_   = runs_path.wstring() + L"\\";

    auto get_long = [&](const char* key, long def) -> long {
        long v = def;
        if (!doc.Get(Name(), key, v)) v = legacy->GetLongValue(Name(), key, def);
        return v;
    };
    key_start_ = get_long("key_start", 0);
    key_reset_ = get_long("key_reset", 0);
    key_split_ = get_long("key_split", 0);
    active_profile_idx_ = static_cast<int>(get_long("active_profile", 0));
    if (active_profile_idx_ < 0 || active_profile_idx_ >= kProfileCount) active_profile_idx_ = 0;
    for (int i = 0; i < kProfileCount; ++i)
        profiles_[i].LoadSettings(doc, legacy, kProfileSections[i]);

    engine_.Attach(&active_list_);

    // Crash-protection resume check
    const std::wstring resume_path = splits_folder_ + L"resume.json";
    std::ifstream rf(resume_path);
    if (rf.is_open()) {
        std::stringstream ss;
        ss << rf.rdbuf();
        rf.close();

        SerializedResume j;
        constexpr glz::opts opts{.error_on_unknown_keys = false};
        if (!glz::read<opts>(j, ss.str())) {
            if (!j.list_name.empty()) {
                // active_profile_idx_ is already restored from settings above, so
                // ActiveSplitsFolder() returns the correct profile subfolder.
                const std::wstring list_path = ActiveSplitsFolder() +
                    std::wstring(j.list_name.begin(), j.list_name.end()) + L".json";
                if (std::filesystem::exists(list_path)) {
                    pending_resume_name_ = j.list_name;
                    pending_resume_data_ = ss.str();
                    pending_resume_      = true;
                }
            }
        }
    }
}

void SplitsWindow::SaveSettings(SettingsDoc& doc)
{
    doc.Set(Name(), "key_start", key_start_);
    doc.Set(Name(), "key_reset", key_reset_);
    doc.Set(Name(), "key_split", key_split_);
    doc.Set(Name(), "active_profile", active_profile_idx_);
    for (int i = 0; i < kProfileCount; ++i)
        profiles_[i].SaveSettings(doc, kProfileSections[i]);
    ToolboxWindow::SaveSettings(doc);
}

// ---------------------------------------------------------------------------
// Goal list management
// ---------------------------------------------------------------------------
void SplitsWindow::NewActiveList(const char* name)
{
    engine_.Detach();
    active_list_ = GoalList{};
    active_list_.name = name ? name : "New List";
    clock_.Reset();
    engine_.Attach(&active_list_);
}

void SplitsWindow::SaveActiveList()
{
    if (splits_folder_.empty() || active_list_.name.empty()) return;
    const std::wstring wname(active_list_.name.begin(), active_list_.name.end());
    active_list_.SaveToFile(ActiveSplitsFolder() + wname + L".json");
}

void SplitsWindow::LoadActiveList(const std::wstring& path)
{
    DeleteResumeState();
    engine_.Detach();
    active_list_.LoadFromFile(path);
    clock_.Reset();
    engine_.Attach(&active_list_);
    LoadPB();

    run_complete_              = false;
    run_failed_                = false;
    running_awaiting_movement_ = false;
    running_load_paused_       = false;
    pending_skill_id_          = 0;
    in_mission_queue_          = false;
    manually_paused_           = false;
    manual_pause_accum_        = 0.0;
    total_paused_real_         = 0.0;
    last_map_                  = GW::Constants::MapID::None;
    last_instance_time_        = 0;
}

void SplitsWindow::LoadPB()
{
    pb_splits_.clear();
    pb_splits_game_.clear();
    pb_total_real_ = std::numeric_limits<double>::quiet_NaN();

    if (runs_folder_.empty() || active_list_.name.empty()) return;

    std::wstring safe_name(active_list_.name.begin(), active_list_.name.end());
    for (auto& c : safe_name) {
        if (c == L' ') c = L'_';
        else if (c == L'/' || c == L'\\' || c == L':' || c == L'*' ||
                 c == L'?' || c == L'"'  || c == L'<' || c == L'>' || c == L'|')
            c = L'-';
    }
    const std::wstring runs_path = ActiveRunsFolder() + safe_name + L".json";
    std::ifstream rf(runs_path);
    if (!rf.is_open()) return;

    std::stringstream ss;
    ss << rf.rdbuf();

    std::vector<SerializedRun> runs;
    constexpr glz::opts opts{.error_on_unknown_keys = false};
    if (glz::read<opts>(runs, ss.str())) return;

    // PB arrays are indexed by non-header goals only (headers carry no split data).
    int goal_count = 0;
    for (const auto& g : active_list_.goals) if (!g.is_header) ++goal_count;

    double best = std::numeric_limits<double>::infinity();
    const SerializedRun* best_run = nullptr;
    for (const auto& run : runs) {
        if (run.failed.value_or(false)) continue;
        if (static_cast<int>(run.splits.size()) < goal_count) continue;
        if (run.total_real < best) { best = run.total_real; best_run = &run; }
    }
    if (!best_run) return;

    pb_total_real_ = best;
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    pb_splits_.resize(static_cast<size_t>(goal_count), nan);
    pb_splits_game_.resize(static_cast<size_t>(goal_count), nan);
    for (int i = 0; i < goal_count && i < static_cast<int>(best_run->splits.size()); ++i) {
        pb_splits_[static_cast<size_t>(i)]      = best_run->splits[static_cast<size_t>(i)].real_time;
        pb_splits_game_[static_cast<size_t>(i)] = best_run->splits[static_cast<size_t>(i)].game_time;
    }
}

std::vector<std::pair<std::string, std::wstring>> SplitsWindow::GetSavedLists() const
{
    if (splits_folder_.empty()) return {};
    return GoalList::ListSaved(ActiveSplitsFolder());
}

// ---------------------------------------------------------------------------
// Crash protection
// ---------------------------------------------------------------------------
void SplitsWindow::SaveResumeState()
{
    if (splits_folder_.empty() || !clock_.IsRunning() || active_list_.name.empty()) return;

    SerializedResume j;
    j.list_name    = active_list_.name;
    j.real_time    = clock_.RealTime();
    j.game_time    = clock_.GameTime();
    j.total_paused = total_paused_real_;

    j.goals.reserve(active_list_.goals.size());
    for (const auto& g : active_list_.goals) {
        if (g.status != GoalStatus::Completed) {
            j.goals.emplace_back(std::nullopt);
        } else {
            j.goals.push_back(SerializedSplit{
                g.split.real_time, g.split.game_time, g.split.segment_real, g.split.segment_game});
        }
    }

    std::ofstream f(splits_folder_ + L"resume.json");
    if (f.is_open()) f << glz::write<glz::opts{.prettify = true}>(j).value_or(std::string{});
}

void SplitsWindow::DeleteResumeState()
{
    pending_resume_ = false;
    pending_resume_name_.clear();
    pending_resume_data_.clear();
    if (splits_folder_.empty()) return;
    std::error_code ec;
    std::filesystem::remove(splits_folder_ + L"resume.json", ec);
}

void SplitsWindow::ApplyResume()
{
    if (!pending_resume_) return;
    pending_resume_ = false;

    SerializedResume j;
    constexpr glz::opts opts{.error_on_unknown_keys = false};
    const bool parse_failed = static_cast<bool>(glz::read<opts>(j, pending_resume_data_));
    pending_resume_data_.clear();
    if (parse_failed) return;

    if (j.list_name.empty()) return;
    const double real_time = j.real_time;
    const double game_time = j.game_time;

    const std::wstring list_path = ActiveSplitsFolder() +
        std::wstring(j.list_name.begin(), j.list_name.end()) + L".json";

    engine_.Detach();
    active_list_.LoadFromFile(list_path);
    engine_.Attach(&active_list_);

    for (size_t i = 0; i < j.goals.size() && i < active_list_.goals.size(); ++i) {
        const auto& jg = j.goals[i];
        if (!jg.has_value()) continue;
        auto& g              = active_list_.goals[i];
        g.status             = GoalStatus::Completed;
        g.split.real_time    = jg->real_time;
        g.split.game_time    = jg->game_time;
        g.split.segment_real = jg->segment_real;
        g.split.segment_game = jg->segment_game;
    }

    clock_.Restore(real_time, game_time);
    engine_.ForceStarted();
    total_paused_real_   = j.total_paused.value_or(0.0);
    last_map_            = GW::Constants::MapID::None;
    last_instance_time_  = 0;
}

void SplitsWindow::DiscardResume()
{
    pending_resume_ = false;
    pending_resume_data_.clear();
    DeleteResumeState();
}

// ---------------------------------------------------------------------------
// SaveCompletedRun / FailRun / SaveRunToHistory
// ---------------------------------------------------------------------------
void SplitsWindow::SaveCompletedRun()
{
    run_complete_ = true;
    clock_.Pause();
    SaveRunToHistory(/*failed=*/false);
    LoadPB();
    UpdateReferenceIfPB();
    DeleteResumeState();
    if (ActiveProfile().auto_send_age)
        GW::Chat::SendChat('/', L"age");
}

void SplitsWindow::FailRun()
{
    if (run_complete_ || run_failed_ || !clock_.IsRunning()) return;
    engine_.FailRun(clock_);
    clock_.Pause();
    run_failed_ = true;
    SaveRunToHistory(/*failed=*/true);
    DeleteResumeState();
}

void SplitsWindow::SaveRunToHistory(bool failed)
{
    if (runs_folder_.empty() || active_list_.name.empty()) return;

    std::wstring safe_name(active_list_.name.begin(), active_list_.name.end());
    for (auto& c : safe_name) {
        if (c == L' ') c = L'_';
        else if (c == L'/' || c == L'\\' || c == L':' || c == L'*' ||
                 c == L'?' || c == L'"'  || c == L'<' || c == L'>' || c == L'|')
            c = L'-';
    }
    const std::wstring runs_path = ActiveRunsFolder() + safe_name + L".json";

    std::vector<SerializedRun> runs;
    {
        std::ifstream rf(runs_path);
        if (rf.is_open()) {
            std::stringstream ss;
            ss << rf.rdbuf();
            constexpr glz::opts opts{.error_on_unknown_keys = false};
            if (glz::read<opts>(runs, ss.str())) runs.clear();
        }
    }

    auto status_name = [](GoalStatus s) -> std::string {
        switch (s) {
            case GoalStatus::Started:   return "Started";
            case GoalStatus::Completed: return "Completed";
            case GoalStatus::Failed:    return "Failed";
            default:                    return "NotStarted";
        }
    };

    SerializedRun run;
    run.total_real      = clock_.RealTime();
    run.failed          = failed;
    run.utc_start       = run_start_unix_;
    run.character_name  = run_char_name_;
    run.total_paused    = total_paused_real_;

    // Headers carry no split data — PB/history arrays index only non-header goals.
    std::vector<SerializedRunGoal> rgoals;
    for (const auto& g : active_list_.goals) {
        if (g.is_header) continue;
        run.splits.push_back(SerializedRunSplit{g.split.real_time, g.split.game_time});
        rgoals.push_back(SerializedRunGoal{g.label, status_name(g.status),
                                            g.split.real_time, g.split.game_time});
    }
    run.goals = std::move(rgoals);

    runs.push_back(std::move(run));
    constexpr size_t kMaxRuns = 200;
    if (runs.size() > kMaxRuns)
        runs.erase(runs.begin(), runs.end() - static_cast<long>(kMaxRuns));

    std::ofstream f(runs_path);
    if (f.is_open())
        f << glz::write<glz::opts{.prettify = true}>(runs).value_or(std::string{});
}

// ---------------------------------------------------------------------------
// Profile folder routing
// ---------------------------------------------------------------------------
std::wstring SplitsWindow::ActiveSplitsFolder() const
{
    if (active_profile_idx_ == 1) return splits_folder_ + L"manual\\";
    if (active_profile_idx_ == 2) return splits_folder_ + L"running\\";
    return splits_folder_;
}

std::wstring SplitsWindow::ActiveRunsFolder() const
{
    if (active_profile_idx_ == 1) return runs_folder_ + L"manual\\";
    if (active_profile_idx_ == 2) return runs_folder_ + L"running\\";
    return runs_folder_;
}

void SplitsWindow::UpdateReferenceIfPB()
{
    if (std::isnan(pb_total_real_) || pb_splits_.empty()) return;

    double ref_total = std::numeric_limits<double>::infinity();
    if (active_list_.reference.has_value() && !active_list_.reference->splits.empty())
        ref_total = active_list_.reference->splits.back();

    if (pb_total_real_ >= ref_total) return;

    GoalReference& ref = active_list_.reference.emplace();
    ref.name   = "Personal Best";
    ref.splits = pb_splits_;

    tm tm_buf{};
    const time_t t = time(nullptr);
    localtime_s(&tm_buf, &t);
    char date_buf[16];
    snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d",
             tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday);
    ref.date = date_buf;

    SaveActiveList();
}

// ---------------------------------------------------------------------------
// Update — called every frame
// ---------------------------------------------------------------------------
void SplitsWindow::Update(float delta)
{
    MapNames::Init(); // no-op after first successful call; deferred here so GW UI is ready

    const auto instance_type   = GW::Map::GetInstanceType();
    const bool is_explorable   = (instance_type == GW::Constants::InstanceType::Explorable);
    const bool is_loading      = (instance_type == GW::Constants::InstanceType::Loading);
    const bool in_outpost      = (instance_type == GW::Constants::InstanceType::Outpost);
    const bool in_cinematic    = GW::Map::GetIsInCinematic();
    const GW::Constants::MapID current_map = GW::Map::GetMapID();

    const uint32_t instance_time     = GW::Map::GetInstanceTime();
    const bool map_changed           = (current_map != last_map_) && current_map != GW::Constants::MapID::None;
    if (map_changed)
        in_mission_queue_ = false; // queue resolved into a real zone change; stop pausing for it

    // Accumulate wall-clock time while manually paused (clock_.RealTime() is frozen during
    // a manual pause, so it can't be used to measure how long the pause lasted).
    if (manually_paused_)
        manual_pause_accum_ += static_cast<double>(delta);
    // Detect entering a new instance of the same map (e.g. creating a new character into the same
    // starting map as a previous session): instance_time resets to near-0 for every new instance.
    // We only update last_instance_time_ when not loading and the map is valid, so the large
    // instance_time from the previous session is preserved across the character-select screen
    // (which has no active instance), making the reset detectable on entry.
    // Manual-only: this exists solely for the character-creation case (new char loads into the
    // same map a previous Manual session ended in). Scoped to idx==1 so it cannot affect Running/SC.
    const bool instance_reset        = active_profile_idx_ == 1 && !clock_.IsRunning() && !is_loading
                                      && !map_changed
                                      && current_map != GW::Constants::MapID::None
                                      && instance_time < last_instance_time_;
    // Manual: the player going through "no active character" (character select) and coming back
    // valid, while landing in the same map, also means a new session loaded in — independent of
    // whether the new character happens to share the old one's name (belt-and-suspenders
    // alongside instance_reset above, in case instance_time doesn't behave as expected across
    // the character-select screen).
    const wchar_t* current_player_name = GW::PlayerMgr::GetPlayerName();
    const bool     has_player_name     = current_player_name && *current_player_name;
    const bool character_changed     = active_profile_idx_ == 1 && !clock_.IsRunning() && !is_loading
                                      && !map_changed
                                      && current_map != GW::Constants::MapID::None
                                      && has_player_name && !had_player_name_;
    const bool just_entered_map      = map_changed || instance_reset || character_changed;
    const bool came_from_explorable  = map_changed && last_was_explorable_;
    // SC (idx 0): count loading time to match OT and /age; pause only in outpost.
    // Other profiles: pause during loading and cinematics; optionally pause in outpost.
    // Manual (idx 1) additionally pauses while a mission-start ready-check queue is up.
    const bool time_paused  = (active_profile_idx_ == 0)
        ? in_outpost
        : (is_loading || in_cinematic || (ActiveProfile().game_time_explorable_only && !is_explorable)
           || (active_profile_idx_ == 1 && in_mission_queue_));

    // Real: raw wall-clock, never paused.
    clock_.AddRealTime(static_cast<double>(delta));

    // Game: pause on loading, cinematics, and (in SC mode) outposts.
    if (!time_paused)
        clock_.AddGameTime(static_cast<double>(delta));

    const GW::Agent* controlled = GW::Agents::GetControlledCharacter();
    const GW::AgentLiving* controlled_living = controlled ? controlled->GetAsAgentLiving() : nullptr;
    int player_level = 0;
    if (controlled_living)
        player_level = static_cast<int>(controlled_living->level);

    // Running: pause clock at load-start, resume only when the player struct is available.
    // Avoids "is_loading" flickering causing the timer to flap start/stop.
    if (active_profile_idx_ == 2) {
        if (is_loading && clock_.IsRunning() && !running_load_paused_) {
            clock_.Pause();
            running_load_paused_ = true;
        } else if (running_load_paused_ && !is_loading && controlled) {
            clock_.Resume();
            running_load_paused_ = false;
        }
    }

    // SC: zone to outpost mid-run → fail (mirrors OT's StopObjectives on zone-out).
    // FailRun() pauses the clock, so this is naturally one-shot.
    if (active_profile_idx_ == 0 && in_outpost && clock_.IsRunning() && !run_complete_)
        FailRun();

    // Auto-load preset when entering a supported elite area or dungeon (SC profile).
    // Always resets and starts a fresh run, mirroring OT's "new ObjectiveSet on every instance entry".
    if (just_entered_map && active_profile_idx_ == 0) {
        if (auto preset = Presets::GetPresetForMap(current_map, doa_spawn_point_)) {
            DeleteResumeState();
            engine_.Detach();
            active_list_ = std::move(*preset);
            run_complete_ = false;
            run_failed_   = false;
            clock_.Reset();
            engine_.Attach(&active_list_);
            LoadPB();
            run_char_name_.clear();
            run_char_level_ = 0;
            run_start_unix_ = static_cast<int64_t>(time(nullptr));
            if (const wchar_t* wname = GW::PlayerMgr::GetPlayerName()) {
                const int sz = WideCharToMultiByte(CP_UTF8, 0, wname, -1, nullptr, 0, nullptr, nullptr);
                if (sz > 1) {
                    run_char_name_.resize(static_cast<size_t>(sz) - 1);
                    WideCharToMultiByte(CP_UTF8, 0, wname, -1, run_char_name_.data(), sz, nullptr, nullptr);
                }
            }
            if (controlled_living)
                run_char_level_ = static_cast<int>(controlled_living->level);
            clock_.Start();
        }
    }

    // Running: two-stage auto-start.
    // Stage 1 (map entry): arm the movement detector when entering the first goal's map.
    // Stage 2 (movement/shadow step): start the clock the moment the runner actually moves.
    if (active_profile_idx_ == 2 && !clock_.IsRunning() && !run_complete_ && !run_failed_) {
        // Find the first non-header goal with a MapEnter start trigger.
        const GoalEntry* first_goal = nullptr;
        for (const auto& g : active_list_.goals) {
            if (g.is_header) continue;
            first_goal = &g;
            break;
        }
        const bool has_start_map = first_goal &&
            first_goal->start_trigger.has_value() &&
            first_goal->start_trigger->type == GoalTrigger::Type::MapEnter;

        // Stage 1: entering the first goal's map arms the detector.
        if (just_entered_map) {
            running_awaiting_movement_ =
                has_start_map && (current_map == first_goal->start_trigger->map_id);
        }

        // Stage 2: movement or shadow step fires the clock.
        // Gated on !is_loading: stale move_x/y values from the previous map linger
        // during loading and would otherwise trigger a spurious start.
        if (running_awaiting_movement_ && !is_loading) {
            const uint32_t skill = pending_skill_id_;
            pending_skill_id_    = 0;

            bool triggered = skill != 0 && kShadowStepSkills.count(skill) != 0;
            if (!triggered) {
                triggered = controlled_living &&
                    (controlled_living->GetIsMoving() ||
                     controlled_living->move_x != 0.f ||
                     controlled_living->move_y != 0.f);
            }

            if (triggered) {
                running_awaiting_movement_ = false;
                run_complete_   = false;
                run_failed_     = false;
                run_char_name_.clear();
                run_char_level_ = player_level;
                run_start_unix_ = static_cast<int64_t>(time(nullptr));
                if (const wchar_t* wname = GW::PlayerMgr::GetPlayerName()) {
                    const int sz = WideCharToMultiByte(CP_UTF8, 0, wname, -1, nullptr, 0, nullptr, nullptr);
                    if (sz > 1) {
                        run_char_name_.resize(static_cast<size_t>(sz) - 1);
                        WideCharToMultiByte(CP_UTF8, 0, wname, -1, run_char_name_.data(), sz, nullptr, nullptr);
                    }
                }
                clock_.Start();
                engine_.ForceStarted();
            }
        } else {
            pending_skill_id_ = 0; // drain bus when not armed
        }
    }

    const int fired = engine_.Update(clock_, current_map, just_entered_map,
                                     came_from_explorable, instance_type,
                                     player_level);

    if (clock_.IsRunning()) {
        resume_save_timer_ += static_cast<float>(delta);
        if (fired >= 0 || resume_save_timer_ >= 1.0f) {
            SaveResumeState();
            resume_save_timer_ = 0.f;
        }
    }

    if (!run_complete_ && !run_failed_ && clock_.IsRunning() && !active_list_.goals.empty()) {
        bool all_done = true;
        for (const auto& g : active_list_.goals) {
            if (g.is_header) continue;
            if (g.status != GoalStatus::Completed) { all_done = false; break; }
        }
        if (all_done) SaveCompletedRun();
    }

    last_map_ = current_map;
    if (!is_loading && current_map != GW::Constants::MapID::None)
        last_instance_time_ = instance_time;
    if (!is_loading)
        last_was_explorable_ = is_explorable;
    had_player_name_ = has_player_name;

    // Keybind edge detection
    auto poll_key = [](int vk, bool& prev) -> bool {
        if (vk <= 0) { prev = false; return false; }
        const bool held  = (GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool fired2 = held && !prev;
        prev = held;
        return fired2;
    };
    if (poll_key(key_start_, key_start_prev_)) StartRun();
    if (poll_key(key_reset_, key_reset_prev_)) ResetRun();
    if (poll_key(key_split_, key_split_prev_)) TriggerManualSplit();

    // Manual: auto-start clock when the first goal fires (any trigger, including a manual Split).
    // Checked here so it catches both engine_.Update() fires and TriggerManualSplit() fires.
    // Excludes manually_paused_ so a user-initiated pause doesn't get immediately undone by
    // the first goal's already-fired status (it was fired before the pause, not by it).
    if (active_profile_idx_ == 1 && !clock_.IsRunning() && !run_complete_ && !run_failed_ &&
        !manually_paused_) {
        for (const auto& g : active_list_.goals) {
            if (g.is_header) continue;
            if (g.status == GoalStatus::Started || g.status == GoalStatus::Completed) {
                run_char_name_.clear();
                run_char_level_ = player_level;
                run_start_unix_ = static_cast<int64_t>(time(nullptr));
                if (const wchar_t* wname = GW::PlayerMgr::GetPlayerName()) {
                    const int sz = WideCharToMultiByte(CP_UTF8, 0, wname, -1, nullptr, 0, nullptr, nullptr);
                    if (sz > 1) {
                        run_char_name_.resize(static_cast<size_t>(sz) - 1);
                        WideCharToMultiByte(CP_UTF8, 0, wname, -1, run_char_name_.data(), sz, nullptr, nullptr);
                    }
                }
                clock_.Start();
            }
            break; // only check the first non-header goal
        }
    }
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------
void SplitsWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) return;
    window_.Draw(*this);
}

void SplitsWindow::DrawSettingsInternal()
{
    window_.DrawSettings(*this);
}

// ---------------------------------------------------------------------------
// Controls
// ---------------------------------------------------------------------------
void SplitsWindow::StartRun()
{
    if (clock_.IsRunning()) {
        // Pause — leave all run/goal state untouched.
        clock_.Pause();
        manually_paused_    = true;
        manual_pause_accum_ = 0.0;
        return;
    }

    if (manually_paused_) {
        // Resume — keep all progress; just fold the pause into the running total and continue.
        total_paused_real_ += manual_pause_accum_;
        manually_paused_ = false;
        clock_.Resume();
        return;
    }

    // Fresh start.
    engine_.Attach(&active_list_);
    auto wide_to_utf8 = [](const wchar_t* ws) -> std::string {
        if (!ws || !*ws) return {};
        const int sz = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
        if (sz <= 1) return {};
        std::string s(static_cast<size_t>(sz) - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws, -1, s.data(), sz, nullptr, nullptr);
        return s;
    };
    run_char_name_.clear();
    run_char_level_ = 0;
    if (const wchar_t* wname = GW::PlayerMgr::GetPlayerName())
        run_char_name_ = wide_to_utf8(wname);
    if (const auto* agent = GW::Agents::GetControlledCharacter())
        run_char_level_ = static_cast<int>(agent->level);
    run_start_unix_ = static_cast<int64_t>(time(nullptr));
    clock_.Start();
    engine_.ForceStarted();
    last_map_           = GW::Constants::MapID::None;
    last_instance_time_ = 0;
}

void SplitsWindow::ResetRun()
{
    DeleteResumeState();
    run_complete_              = false;
    run_failed_                = false;
    running_awaiting_movement_ = false;
    running_load_paused_       = false;
    pending_skill_id_          = 0;
    in_mission_queue_          = false;
    manually_paused_           = false;
    manual_pause_accum_        = 0.0;
    total_paused_real_         = 0.0;
    engine_.Reset();
    clock_.Reset();
    // last_map_/last_instance_time_ deliberately left untouched: forcing them to a "just
    // entered" sentinel would re-fire the first goal's MapEnter trigger immediately if the
    // player is still standing in that zone. They should have to actually leave and re-enter.
}

void SplitsWindow::TriggerManualSplit()
{
    engine_.TriggerManual(clock_);
}

void SplitsWindow::SwitchProfile(int idx)
{
    if (idx < 0 || idx >= kProfileCount || idx == active_profile_idx_) return;

    // Save the outgoing profile's list name so we can restore it later.
    // SC's list is always a preset (not file-backed), so don't save it.
    if (active_profile_idx_ != 0 && !active_list_.name.empty())
        profiles_[active_profile_idx_].last_list_name = active_list_.name;

    active_profile_idx_ = idx;

    // Reset run state without clearing last_map_ — resetting it to None would trigger
    // a spurious just_entered_map on the next Update tick, immediately re-loading presets.
    DeleteResumeState();
    run_complete_       = false;
    run_failed_         = false;
    manually_paused_    = false;
    manual_pause_accum_ = 0.0;
    total_paused_real_  = 0.0;
    engine_.Detach();
    engine_.Reset();
    clock_.Reset();

    active_list_ = GoalList{};

    // SC always starts empty — presets auto-load on map entry. Never restore from file.
    if (active_profile_idx_ != 0) {
        const SplitsProfile& p = ActiveProfile();
        if (!p.last_list_name.empty() && !splits_folder_.empty()) {
            const std::wstring path = ActiveSplitsFolder() +
                std::wstring(p.last_list_name.begin(), p.last_list_name.end()) + L".json";
            if (std::filesystem::exists(path))
                LoadActiveList(path);
        }
    }
}
