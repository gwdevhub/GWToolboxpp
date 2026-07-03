#include "stdafx.h"

#include <Windows/SplitsWindow.h>
#include <Windows/Splits/MapNames.h>
#include <Modules/Resources.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>

#include <Modules/GWEventBus.h>
#include <Windows/ObjectiveTimerWindow.h>

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

    GWEventBus::Instance().Subscribe(this, [this](const GWEvent& e) {
        using T = GWEvent::Type;
        switch (e.type) {
            case T::MissionComplete:
                engine_.NotifyMissionComplete(static_cast<GW::Constants::MapID>(e.id1));
                break;
            case T::MissionBonus:
                engine_.NotifyMissionBonus(static_cast<GW::Constants::MapID>(e.id1));
                break;
            case T::VanquishComplete:
                engine_.NotifyVanquishComplete(static_cast<GW::Constants::MapID>(e.id1));
                break;
            case T::PartyDefeated:
                if (ActiveProfile().stop_on_party_defeated && clock_.IsRunning())
                    FailRun();
                break;
            case T::ObjectiveDone:
                engine_.NotifyEvent(GoalTrigger::Type::ObjectiveDone, e.id1);
                break;
            case T::ObjectiveStarted:
                engine_.NotifyEvent(GoalTrigger::Type::ObjectiveStarted, e.id1);
                break;
            case T::DoorOpen:
                engine_.NotifyEvent(GoalTrigger::Type::DoorOpen, e.id1);
                break;
            case T::DoorClose:
                engine_.NotifyEvent(GoalTrigger::Type::DoorClose, e.id1);
                break;
            case T::AgentUpdateAllegiance:
                engine_.NotifyEvent(GoalTrigger::Type::AgentUpdateAllegiance, e.id1, e.id2);
                break;
            case T::DoACompleteZone:
                engine_.NotifyEvent(GoalTrigger::Type::DoACompleteZone, e.id1);
                break;
            case T::DungeonReward:
                engine_.NotifyEvent(GoalTrigger::Type::DungeonReward);
                break;
            case T::ServerMessage:
                engine_.NotifyEvent(GoalTrigger::Type::ServerMessage, 0, 0, e.str, e.str_len);
                break;
            case T::DisplayDialogue:
                engine_.NotifyEvent(GoalTrigger::Type::DisplayDialogue, 0, 0, e.str, e.str_len);
                break;
            // ToPK/Ascalon Academy countdown — gated to mission queue maps so unrelated
            // countdowns don't pause Manual's game time.
            case T::CountdownStart:
                if (active_profile_idx_ == 0 && IsMissionQueueMap(static_cast<GW::Constants::MapID>(e.id1)))
                    in_mission_queue_ = true;
                engine_.NotifyEvent(GoalTrigger::Type::CountdownStart, e.id1);
                break;
            // Running: shadow-step auto-start — filter to local player only.
            case T::SkillActivate:
                if (e.id1 == GW::Agents::GetControlledCharacterId())
                    pending_skill_id_ = e.id2;
                break;
            // Manual: party lock signals the mission-start ready-check queue is up/down.
            // Gated to mission queue maps so unrelated party locks don't affect game time.
            case T::PartyLock:
                if (!e.id1) {
                    in_mission_queue_ = false;
                } else if (active_profile_idx_ == 0 && IsMissionQueueMap(last_map_)) {
                    in_mission_queue_ = true;
                }
                break;
            // Zone entry — fires for every new instance including same-map re-entry
            // (district change, new character into same starting map, etc.).
            case T::InstanceLoadInfo:
                pending_came_from_explorable_ = last_was_explorable_;
                last_was_explorable_          = (e.id2 != 0);
                last_map_                     = static_cast<GW::Constants::MapID>(e.id1);
                in_mission_queue_             = false;
                pending_map_enter_            = true;
                break;
            // Transfer packet fires before InstanceLoadInfo — clear queue state immediately
            // on any server transfer (covers district changes where the map doesn't change).
            case T::GameSrvTransfer:
                in_mission_queue_ = false;
                break;
            default:
                break;
        }
    });
}

void SplitsWindow::Terminate()
{
    GWEventBus::Instance().Unsubscribe(this);
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
    pending_map_enter_            = false;
    pending_came_from_explorable_ = false;
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
    pending_map_enter_            = false;
    pending_came_from_explorable_ = false;
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
    ObjectiveTimerWindow::BroadcastWebsocket("reset", "Splits: Reset - party defeated");
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
    if (active_profile_idx_ == 0) return splits_folder_ + L"manual\\";
    if (active_profile_idx_ == 1) return splits_folder_ + L"running\\";
    return splits_folder_;
}

std::wstring SplitsWindow::ActiveRunsFolder() const
{
    if (active_profile_idx_ == 0) return runs_folder_ + L"manual\\";
    if (active_profile_idx_ == 1) return runs_folder_ + L"running\\";
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
    const bool in_cinematic    = GW::Map::GetIsInCinematic();

    // Consume bus-sourced map-entry flags (set by InstanceLoadInfo / GameSrvTransfer callbacks).
    const bool just_entered_map     = pending_map_enter_;
    const bool came_from_explorable = pending_came_from_explorable_;
    pending_map_enter_            = false;
    pending_came_from_explorable_ = false;

    // Accumulate wall-clock time while manually paused (clock_.RealTime() is frozen during
    // a manual pause, so it can't be used to measure how long the pause lasted).
    if (manually_paused_)
        manual_pause_accum_ += static_cast<double>(delta);
    // Manual (idx 0) also pauses while a mission-start ready-check queue is up.
    const bool time_paused = is_loading || in_cinematic
        || (ActiveProfile().game_time_explorable_only && !is_explorable)
        || (active_profile_idx_ == 0 && in_mission_queue_);

    // Real: raw wall-clock, never paused.
    clock_.AddRealTime(static_cast<double>(delta));

    if (!time_paused)
        clock_.AddGameTime(static_cast<double>(delta));

    const GW::Agent* controlled = GW::Agents::GetControlledCharacter();
    const GW::AgentLiving* controlled_living = controlled ? controlled->GetAsAgentLiving() : nullptr;
    int player_level = 0;
    if (controlled_living)
        player_level = static_cast<int>(controlled_living->level);

    // Running: pause clock at load-start, resume only when the player struct is available.
    // Avoids "is_loading" flickering causing the timer to flap start/stop.
    if (active_profile_idx_ == 1) {
        if (is_loading && clock_.IsRunning() && !running_load_paused_) {
            clock_.Pause();
            running_load_paused_ = true;
        } else if (running_load_paused_ && !is_loading && controlled) {
            clock_.Resume();
            running_load_paused_ = false;
        }
    }

    // Running: two-stage auto-start.
    // Stage 1 (map entry): arm the movement detector when entering the first goal's map.
    // Stage 2 (movement/shadow step): start the clock the moment the runner actually moves.
    if (active_profile_idx_ == 1 && !clock_.IsRunning() && !run_complete_ && !run_failed_) {
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
                has_start_map && (last_map_ == first_goal->start_trigger->map_id);
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
                ObjectiveTimerWindow::BroadcastWebsocket("reset", "Splits: Reset - run starting");
                ObjectiveTimerWindow::BroadcastWebsocket("start", "Splits: Start - movement detected");
                clock_.Start();
                engine_.ForceStarted();
            }
        } else {
            pending_skill_id_ = 0; // drain bus when not armed
        }
    }

    const int fired = engine_.Update(clock_, last_map_, just_entered_map,
                                     came_from_explorable, instance_type,
                                     player_level, ActiveProfile().simple_order);

    if (clock_.IsRunning()) {
        resume_save_timer_ += static_cast<float>(delta);
        if (fired >= 0 || resume_save_timer_ >= 1.0f) {
            SaveResumeState();
            resume_save_timer_ = 0.f;
        }
        if (fired >= 0)
            ObjectiveTimerWindow::BroadcastWebsocket("split", "Splits: Split - goal complete");
    }

    if (!run_complete_ && !run_failed_ && clock_.IsRunning() && !active_list_.goals.empty()) {
        bool all_done = true;
        for (const auto& g : active_list_.goals) {
            if (g.is_header) continue;
            if (g.status != GoalStatus::Completed) { all_done = false; break; }
        }
        if (all_done) SaveCompletedRun();
    }

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
    if (active_profile_idx_ == 0 && !clock_.IsRunning() && !run_complete_ && !run_failed_ &&
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
                ObjectiveTimerWindow::BroadcastWebsocket("reset", "Splits: Reset - run starting");
                ObjectiveTimerWindow::BroadcastWebsocket("start", "Splits: Start - first goal fired");
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
    ObjectiveTimerWindow::BroadcastWebsocket("reset", "Splits: Reset - run starting");
    ObjectiveTimerWindow::BroadcastWebsocket("start", "Splits: Start - manually started");
    clock_.Start();
    engine_.ForceStarted();
}

void SplitsWindow::ResetRun()
{
    if (clock_.IsRunning() || run_complete_ || run_failed_)
        ObjectiveTimerWindow::BroadcastWebsocket("reset", "Splits: Reset - run reset");
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
    pending_map_enter_            = false;
    pending_came_from_explorable_ = false;
    // last_map_ deliberately left untouched: re-entry only fires when InstanceLoadInfo arrives,
    // so the player must leave and re-enter the zone — no spurious MapEnter re-trigger on reset.
}

void SplitsWindow::TriggerManualSplit()
{
    engine_.TriggerManual(clock_);
}

void SplitsWindow::SwitchProfile(int idx)
{
    if (idx < 0 || idx >= kProfileCount || idx == active_profile_idx_) return;

    if (!active_list_.name.empty())
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

    const SplitsProfile& p = ActiveProfile();
    if (!p.last_list_name.empty() && !splits_folder_.empty()) {
        const std::wstring path = ActiveSplitsFolder() +
            std::wstring(p.last_list_name.begin(), p.last_list_name.end()) + L".json";
        if (std::filesystem::exists(path))
            LoadActiveList(path);
    }
}
