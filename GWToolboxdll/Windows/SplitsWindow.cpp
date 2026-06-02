#include "stdafx.h"

#include <Windows/SplitsWindow.h>
#include <Windows/Splits/MapNames.h>

#include <Modules/Resources.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>

using json = nlohmann::json;

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
}

void SplitsWindow::Terminate()
{
    GW::UI::RemoveUIMessageCallback(&on_mission_complete_hook_);
    GW::UI::RemoveUIMessageCallback(&on_objective_complete_hook_);
    engine_.Detach();
    ToolboxWindow::Terminate();
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------
void SplitsWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);

    // Establish data folders
    const auto splits_path = Resources::GetPath(L"splits");
    const auto runs_path   = Resources::GetPath(L"splits\\runs");
    Resources::EnsureFolderExists(splits_path);
    Resources::EnsureFolderExists(runs_path);
    splits_folder_ = splits_path.wstring() + L"\\";
    runs_folder_   = runs_path.wstring() + L"\\";

    key_start_ = ini->GetLongValue(Name(), "key_start", 0);
    key_reset_ = ini->GetLongValue(Name(), "key_reset", 0);
    key_split_ = ini->GetLongValue(Name(), "key_split", 0);

    engine_.Attach(&active_list_);

    // Crash-protection resume check
    const std::wstring resume_path = splits_folder_ + L"resume.json";
    std::ifstream rf(resume_path);
    if (rf.is_open()) {
        try {
            json j = json::parse(rf);
            rf.close();
            const std::string lname = j.value("list_name", "");
            if (!lname.empty()) {
                const std::wstring list_path = splits_folder_ +
                    std::wstring(lname.begin(), lname.end()) + L".json";
                if (std::filesystem::exists(list_path)) {
                    pending_resume_name_ = lname;
                    pending_resume_data_ = j.dump();
                    pending_resume_      = true;
                }
            }
        } catch (...) {}
    }
}

void SplitsWindow::SaveSettings(ToolboxIni* ini)
{
    ini->SetLongValue(Name(), "key_start", key_start_);
    ini->SetLongValue(Name(), "key_reset", key_reset_);
    ini->SetLongValue(Name(), "key_split", key_split_);
    ToolboxWindow::SaveSettings(ini);
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
    active_list_.SaveToFile(splits_folder_ + wname + L".json");
}

void SplitsWindow::LoadActiveList(const std::wstring& path)
{
    DeleteResumeState();
    engine_.Detach();
    active_list_.LoadFromFile(path);
    clock_.Reset();
    engine_.Attach(&active_list_);
    LoadPB();
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
    const std::wstring runs_path = runs_folder_ + safe_name + L".json";
    std::ifstream rf(runs_path);
    if (!rf.is_open()) return;

    json runs;
    try { runs = json::parse(rf); } catch (...) { return; }
    if (!runs.is_array()) return;

    const int goal_count = static_cast<int>(active_list_.goals.size());
    double best = std::numeric_limits<double>::infinity();
    const json* best_run = nullptr;
    for (const auto& run : runs) {
        if (!run.contains("total_real") || !run.contains("splits")) continue;
        if (static_cast<int>(run["splits"].size()) < goal_count)    continue;
        const double t = run["total_real"].get<double>();
        if (t < best) { best = t; best_run = &run; }
    }
    if (!best_run) return;

    pb_total_real_ = best;
    const auto& jsplits = (*best_run)["splits"];
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    pb_splits_.resize(static_cast<size_t>(goal_count), nan);
    pb_splits_game_.resize(static_cast<size_t>(goal_count), nan);
    for (int i = 0; i < goal_count && i < static_cast<int>(jsplits.size()); ++i) {
        pb_splits_[static_cast<size_t>(i)]      = jsplits[i].value("real_time", nan);
        pb_splits_game_[static_cast<size_t>(i)] = jsplits[i].value("game_time", nan);
    }
}

std::vector<std::pair<std::string, std::wstring>> SplitsWindow::GetSavedLists() const
{
    if (splits_folder_.empty()) return {};
    return GoalList::ListSaved(splits_folder_);
}

// ---------------------------------------------------------------------------
// Crash protection
// ---------------------------------------------------------------------------
void SplitsWindow::SaveResumeState()
{
    if (splits_folder_.empty() || !clock_.IsRunning() || active_list_.name.empty()) return;

    json j;
    j["list_name"] = active_list_.name;
    j["real_time"] = clock_.RealTime();
    j["game_time"] = clock_.GameTime();

    json jgoals = json::array();
    for (const auto& g : active_list_.goals) {
        if (!g.completed) {
            jgoals.push_back(nullptr);
        } else {
            json js;
            js["real_time"]    = g.split.real_time;
            js["game_time"]    = g.split.game_time;
            js["segment_real"] = g.split.segment_real;
            js["segment_game"] = g.split.segment_game;
            jgoals.push_back(std::move(js));
        }
    }
    j["goals"] = std::move(jgoals);

    std::ofstream f(splits_folder_ + L"resume.json");
    if (f.is_open()) f << j.dump(2);
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

    json j;
    try { j = json::parse(pending_resume_data_); } catch (...) { return; }
    pending_resume_data_.clear();

    const std::string lname = j.value("list_name", "");
    if (lname.empty()) return;
    const double real_time = j.value("real_time", 0.0);
    const double game_time = j.value("game_time", 0.0);

    const std::wstring list_path = splits_folder_ +
        std::wstring(lname.begin(), lname.end()) + L".json";

    engine_.Detach();
    active_list_.LoadFromFile(list_path);
    engine_.Attach(&active_list_);

    const auto& jgoals = j.value("goals", json::array());
    for (int i = 0; i < static_cast<int>(jgoals.size()) &&
                    i < static_cast<int>(active_list_.goals.size()); ++i) {
        const auto& jg = jgoals[i];
        if (jg.is_null()) continue;
        auto& g              = active_list_.goals[i];
        g.completed          = true;
        g.split.real_time    = jg.value("real_time",    0.0);
        g.split.game_time    = jg.value("game_time",    0.0);
        g.split.segment_real = jg.value("segment_real", 0.0);
        g.split.segment_game = jg.value("segment_game", 0.0);
    }

    clock_.Restore(real_time, game_time);
    engine_.ForceStarted();
    last_map_           = GW::Constants::MapID::None;
    last_instance_time_ = 0;
}

void SplitsWindow::DiscardResume()
{
    pending_resume_ = false;
    pending_resume_data_.clear();
    DeleteResumeState();
}

// ---------------------------------------------------------------------------
// SaveCompletedRun
// ---------------------------------------------------------------------------
void SplitsWindow::SaveCompletedRun()
{
    run_complete_ = true;
    clock_.Pause();
    DeleteResumeState();

    if (runs_folder_.empty()) return;

    const std::time_t now = std::time(nullptr);
    struct tm tm_local{};
    localtime_s(&tm_local, &now);
    char ts_display[32];
    std::strftime(ts_display, sizeof(ts_display), "%Y-%m-%d %H:%M:%S", &tm_local);

    json attempt;
    attempt["date"]       = ts_display;
    attempt["character"]  = run_char_name_;
    attempt["level"]      = run_char_level_;
    attempt["total_real"] = clock_.RealTime();
    attempt["total_game"] = clock_.GameTime();

    json jsplits = json::array();
    for (const auto& g : active_list_.goals) {
        json js;
        js["label"]        = g.label;
        js["real_time"]    = g.split.real_time;
        js["game_time"]    = g.split.game_time;
        js["segment_real"] = g.split.segment_real;
        js["segment_game"] = g.split.segment_game;
        jsplits.push_back(std::move(js));
    }
    attempt["splits"] = std::move(jsplits);

    std::wstring safe_name(active_list_.name.begin(), active_list_.name.end());
    for (auto& c : safe_name) {
        if (c == L' ') c = L'_';
        else if (c == L'/' || c == L'\\' || c == L':' || c == L'*' ||
                 c == L'?' || c == L'"'  || c == L'<' || c == L'>' || c == L'|')
            c = L'-';
    }

    std::error_code ec;
    std::filesystem::create_directories(runs_folder_, ec);
    const std::wstring out_path = runs_folder_ + safe_name + L".json";

    json runs = json::array();
    {
        std::ifstream rf(out_path);
        if (rf.is_open()) {
            try { runs = json::parse(rf); } catch (...) { runs = json::array(); }
            if (!runs.is_array()) runs = json::array();
        }
    }
    runs.push_back(std::move(attempt));

    std::ofstream f(out_path);
    if (f.is_open()) f << runs.dump(2);

    // Update in-memory PB if this run beats it
    const double total = clock_.RealTime();
    if (std::isnan(pb_total_real_) || total < pb_total_real_) {
        pb_total_real_ = total;
        const int n = static_cast<int>(active_list_.goals.size());
        pb_splits_.resize(static_cast<size_t>(n));
        pb_splits_game_.resize(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            pb_splits_[static_cast<size_t>(i)]      = active_list_.goals[static_cast<size_t>(i)].split.real_time;
            pb_splits_game_[static_cast<size_t>(i)] = active_list_.goals[static_cast<size_t>(i)].split.game_time;
        }
    }
}

// ---------------------------------------------------------------------------
// Update — called every frame
// ---------------------------------------------------------------------------
void SplitsWindow::Update(float delta)
{
    MapNames::Init(); // no-op after first successful call; deferred here so GW UI is ready

    const auto instance_type   = GW::Map::GetInstanceType();
    const bool is_explorable   = (instance_type == GW::Constants::InstanceType::Explorable);
    const GW::Constants::MapID current_map = GW::Map::GetMapID();

    const bool map_changed          = (current_map != last_map_) && current_map != GW::Constants::MapID::None;
    const bool just_entered_map     = map_changed;
    const bool came_from_explorable = map_changed && last_was_explorable_;

    const bool is_loading   = (instance_type == GW::Constants::InstanceType::Loading);
    const bool in_cinematic = GW::Map::GetIsInCinematic();
    const bool time_paused  = is_loading || in_cinematic;

    if (!time_paused)
        clock_.AddRealTime(static_cast<double>(delta));

    if (is_explorable && !time_paused) {
        const uint32_t inst = GW::Map::GetInstanceTime();
        if (last_instance_time_ > 0 && inst > last_instance_time_)
            clock_.AddGameTime((inst - last_instance_time_) / 1000.0);
        last_instance_time_ = inst;
    } else {
        last_instance_time_ = 0;
    }

    vq_complete_ = false;
    if (is_explorable) {
        const uint32_t foes   = GW::Map::GetFoesToKill();
        const uint32_t killed = GW::Map::GetFoesKilled();
        vq_complete_ = (foes > 0 || killed > 0) && foes == 0;
    }

    int player_level = 0;
    if (const auto* player = GW::Agents::GetControlledCharacter())
        player_level = static_cast<int>(player->level);

    const int fired = engine_.Update(clock_, current_map, just_entered_map,
                                     came_from_explorable, instance_type,
                                     vq_complete_, player_level);

    if (clock_.IsRunning()) {
        resume_save_timer_ += static_cast<float>(delta);
        if (fired >= 0 || resume_save_timer_ >= 1.0f) {
            SaveResumeState();
            resume_save_timer_ = 0.f;
        }
    }

    if (!run_complete_ && clock_.IsRunning() && !active_list_.goals.empty()) {
        bool all_done = true;
        for (const auto& g : active_list_.goals) {
            if (!g.completed) { all_done = false; break; }
        }
        if (all_done) SaveCompletedRun();
    }

    last_map_ = current_map;
    if (!is_loading)
        last_was_explorable_ = is_explorable;

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
    engine_.Attach(&active_list_);
    if (clock_.IsRunning()) {
        clock_.Pause();
    } else {
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
        clock_.Start();
    }
    last_map_           = GW::Constants::MapID::None;
    last_instance_time_ = 0;
}

void SplitsWindow::ResetRun()
{
    DeleteResumeState();
    run_complete_ = false;
    engine_.Reset();
    clock_.Reset();
    last_map_           = GW::Constants::MapID::None;
    last_instance_time_ = 0;
}

void SplitsWindow::TriggerManualSplit()
{
    engine_.TriggerManual(clock_);
}
