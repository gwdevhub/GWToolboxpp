#include "TrackerAdvancedPlugin.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <limits>
#include <ranges>
#include <set>

namespace {
    using namespace TrackerAdvanced;

    int ResizeString(ImGuiInputTextCallbackData* data)
    {
        if (data->EventFlag != ImGuiInputTextFlags_CallbackResize) {
            return 0;
        }
        auto value = static_cast<std::string*>(data->UserData);
        value->resize(static_cast<size_t>(data->BufTextLen));
        data->Buf = value->data();
        return 0;
    }

    bool InputString(
        const char* label,
        std::string& value,
        const ImGuiInputTextFlags flags = 0)
    {
        if (value.capacity() < 64) {
            value.reserve(64);
        }
        const auto changed = ImGui::InputText(
            label,
            value.data(),
            value.capacity() + 1,
            flags | ImGuiInputTextFlags_CallbackResize,
            ResizeString,
            &value);
        value.resize(std::strlen(value.c_str()));
        return changed;
    }

    bool InputStringMultiline(
        const char* label,
        std::string& value,
        const ImVec2 size)
    {
        if (value.capacity() < 256) {
            value.reserve(256);
        }
        const auto changed = ImGui::InputTextMultiline(
            label,
            value.data(),
            value.capacity() + 1,
            size,
            ImGuiInputTextFlags_CallbackResize,
            ResizeString,
            &value);
        value.resize(std::strlen(value.c_str()));
        return changed;
    }

    void CenteredProgressBar(
        const float fraction,
        const ImVec2 size,
        const std::string& overlay)
    {
        ImGui::ProgressBar(fraction, size, "");
        if (overlay.empty()) {
            return;
        }
        const auto minimum = ImGui::GetItemRectMin();
        const auto maximum = ImGui::GetItemRectMax();
        const auto text_size = ImGui::CalcTextSize(overlay.c_str());
        const ImVec2 position{
            minimum.x + std::max(0.f, (maximum.x - minimum.x - text_size.x) * 0.5f),
            minimum.y + std::max(0.f, (maximum.y - minimum.y - text_size.y) * 0.5f),
        };
        const auto draw_list = ImGui::GetWindowDrawList();
        draw_list->PushClipRect(minimum, maximum, true);
        draw_list->AddText(
            position,
            ImGui::GetColorU32(ImGuiCol_Text),
            overlay.c_str());
        draw_list->PopClipRect();
    }

    bool IsModifierKey(const size_t key)
    {
        switch (key) {
            case VK_CONTROL:
            case VK_LCONTROL:
            case VK_RCONTROL:
            case VK_MENU:
            case VK_LMENU:
            case VK_RMENU:
            case VK_SHIFT:
            case VK_LSHIFT:
            case VK_RSHIFT:
                return true;
            default:
                return false;
        }
    }

    std::optional<HotkeyBinding> BindingFromKeys(const std::bitset<256>& keys)
    {
        HotkeyBinding binding{
            .control = keys.test(VK_CONTROL)
                || keys.test(VK_LCONTROL)
                || keys.test(VK_RCONTROL),
            .alt = keys.test(VK_MENU)
                || keys.test(VK_LMENU)
                || keys.test(VK_RMENU),
            .shift = keys.test(VK_SHIFT)
                || keys.test(VK_LSHIFT)
                || keys.test(VK_RSHIFT),
        };
        for (size_t key = 1; key < keys.size(); ++key) {
            if (!keys.test(key) || IsModifierKey(key)) {
                continue;
            }
            if (binding.key) {
                return std::nullopt;
            }
            binding.key = static_cast<uint32_t>(key);
        }
        if (!binding.key && (binding.control || binding.alt || binding.shift)) {
            return std::nullopt;
        }
        return binding;
    }

    ImVec4 StatusColor(const RuntimeTaskStatus status)
    {
        switch (status) {
            case RuntimeTaskStatus::active: return {1.f, 1.f, 1.f, 1.f};
            case RuntimeTaskStatus::completed: return {0.25f, 1.f, 0.35f, 1.f};
            case RuntimeTaskStatus::skipped: return {1.f, 0.8f, 0.25f, 1.f};
            case RuntimeTaskStatus::pending:
                return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        }
        return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    }

    void BeginTransparent(const bool transparent)
    {
        if (transparent) {
            ImGui::SetNextWindowBgAlpha(0.08f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        }
    }

    void EndTransparent(const bool transparent)
    {
        if (transparent) {
            ImGui::PopStyleVar();
        }
    }

    std::string OptionalDuration(const std::optional<uint64_t>& milliseconds)
    {
        return milliseconds
            ? TrackerEngine::FormatDuration(*milliseconds)
            : std::string("—");
    }

    std::string FullDuration(const uint64_t milliseconds)
    {
        const auto hours = milliseconds / 3'600'000;
        const auto minutes = milliseconds / 60'000 % 60;
        const auto seconds = milliseconds / 1'000 % 60;
        const auto tenths = milliseconds / 100 % 10;
        return std::format(
            "{:02}:{:02}:{:02}.{}",
            hours,
            minutes,
            seconds,
            tenths);
    }

    uint64_t ExpectedTimeFrom(const PublishedProfile& profile, const size_t first_task)
    {
        uint64_t total = 0;
        for (auto index = first_task; index < profile.tasks.size(); ++index) {
            total += profile.tasks[index].task->expected_time_ms;
        }
        return total;
    }

    bool HasActiveTask(const PluginSnapshot& snapshot)
    {
        const auto index = snapshot.runtime.active_task_index;
        return snapshot.profile
            && index < snapshot.profile->tasks.size()
            && index < snapshot.runtime.tasks.size()
            && snapshot.runtime.tasks[index].status == RuntimeTaskStatus::active;
    }

    std::string HistoryDuration(const double milliseconds, const bool show_sign)
    {
        const auto negative = milliseconds < 0.0;
        const auto magnitude = static_cast<uint64_t>(
            std::llround(std::abs(milliseconds)));
        auto text = TrackerEngine::FormatDuration(magnitude);
        if (show_sign) {
            text.insert(text.begin(), negative ? '-' : '+');
        }
        return text;
    }

    uint32_t NextObjectiveId(const ProfileDefinition& profile)
    {
        uint32_t maximum = 0;
        for (const auto& objective : profile.objectives) {
            maximum = std::max(maximum, objective.id);
        }
        return maximum + 1;
    }

    uint32_t NextTaskId(const ProfileDefinition& profile)
    {
        uint32_t maximum = 0;
        for (const auto& objective : profile.objectives) {
            for (const auto& task : objective.tasks) {
                maximum = std::max(maximum, task.id);
            }
        }
        return maximum + 1;
    }

    uint32_t NextActionId(const ProfileDefinition& profile)
    {
        uint32_t maximum = 0;
        for (const auto& action : profile.actions) {
            maximum = std::max(maximum, action.id);
        }
        return maximum + 1;
    }

    bool MapMatchesUsage(const MapCatalogEntry& entry, const MapUsage usage)
    {
        switch (usage) {
            case MapUsage::Any: return true;
            case MapUsage::Mission: return entry.is_mission;
            case MapUsage::Dungeon: return entry.is_dungeon;
            case MapUsage::Vanquish: return entry.is_vanquishable;
        }
        return false;
    }

    bool EditMap(
        const char* label,
        std::optional<std::string>& value,
        const MapUsage usage,
        const std::shared_ptr<const std::vector<MapCatalogEntry>>& catalog)
    {
        auto changed = false;
        if (!value) {
            value.emplace();
        }
        changed |= InputString(label, *value);
        ImGui::SameLine();
        const auto picker = std::format("Choose##{}", label);
        if (ImGui::BeginCombo(picker.c_str(), "Map list")) {
            std::set<std::string> shown;
            for (const auto& entry : catalog ? *catalog : std::vector<MapCatalogEntry>{}) {
                if (
                    entry.name.empty()
                    || !MapMatchesUsage(entry, usage)
                    || !shown.insert(entry.name).second) {
                    continue;
                }
                if (ImGui::Selectable(entry.name.c_str(), *value == entry.name)) {
                    *value = entry.name;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    bool EditCriterion(
        CriterionDefinition& criterion,
        const std::shared_ptr<const std::vector<MapCatalogEntry>>& catalog)
    {
        static constexpr std::array Types{
            CriterionType::player_level,
            CriterionType::map_loaded,
            CriterionType::mission_complete,
            CriterionType::dungeon_complete,
            CriterionType::vanquish_complete,
            CriterionType::title_progress,
            CriterionType::manual,
        };
        static constexpr std::array Labels{
            "Player Level",
            "Map Loaded",
            "Mission Complete",
            "Dungeon Complete",
            "Vanquish Complete",
            "Title Progress",
            "Manual",
        };
        auto selected = 0;
        if (const auto found = std::ranges::find(Types, criterion.type); found != Types.end()) {
            selected = static_cast<int>(std::distance(Types.begin(), found));
        }
        auto changed = false;
        if (ImGui::Combo(
                "Criterion type",
                &selected,
                [](void*, const int index) {
                    return Labels[static_cast<size_t>(index)];
                },
                nullptr,
                static_cast<int>(Labels.size()))) {
            criterion = {.type = Types[static_cast<size_t>(selected)]};
            if (
                criterion.type == CriterionType::mission_complete
                || criterion.type == CriterionType::dungeon_complete) {
                criterion.is_hard_mode = false;
            }
            changed = true;
        }
        switch (criterion.type) {
            case CriterionType::player_level: {
                auto level = static_cast<int>(criterion.level.value_or(20));
                if (ImGui::InputInt("Required level", &level)) {
                    criterion.level = static_cast<uint32_t>(std::clamp(level, 1, 20));
                    changed = true;
                }
                break;
            }
            case CriterionType::map_loaded:
                changed |= EditMap("Map", criterion.map, MapUsage::Any, catalog);
                break;
            case CriterionType::mission_complete:
                changed |= EditMap("Mission", criterion.mission, MapUsage::Mission, catalog);
                if (!criterion.is_hard_mode) criterion.is_hard_mode = false;
                changed |= ImGui::Checkbox("Hard mode", &*criterion.is_hard_mode);
                break;
            case CriterionType::dungeon_complete:
                changed |= EditMap("Dungeon", criterion.dungeon, MapUsage::Dungeon, catalog);
                if (!criterion.is_hard_mode) criterion.is_hard_mode = false;
                changed |= ImGui::Checkbox("Hard mode", &*criterion.is_hard_mode);
                break;
            case CriterionType::vanquish_complete:
                changed |= EditMap("Map", criterion.map, MapUsage::Vanquish, catalog);
                break;
            case CriterionType::title_progress: {
                if (!criterion.title) criterion.title = "KoaBD";
                if (ImGui::BeginCombo("Title", criterion.title->c_str())) {
                    for (const auto& title : SupportedTitles()) {
                        if (ImGui::Selectable(
                                std::string(title.label).c_str(),
                                *criterion.title == title.token)) {
                            criterion.title = title.token;
                            changed = true;
                        }
                    }
                    ImGui::EndCombo();
                }
                auto progress = static_cast<int>(criterion.required_progress.value_or(1));
                if (ImGui::InputInt("Required progress", &progress)) {
                    criterion.required_progress =
                        static_cast<uint32_t>(std::max(1, progress));
                    changed = true;
                }
                break;
            }
            case CriterionType::manual:
            case CriterionType::invalid:
                break;
        }
        return changed;
    }

    bool EditTitleTrackers(
        std::vector<TitleTrackerDefinition>& trackers,
        const char* id)
    {
        auto changed = false;
        ImGui::PushID(id);
        for (size_t i = 0; i < trackers.size();) {
            ImGui::PushID(static_cast<int>(i));
            auto& tracker = trackers[i];
            const auto title = FindTitle(tracker.title);
            if (ImGui::BeginCombo(
                    "##title", title ? std::string(title->label).c_str() : tracker.title.c_str())) {
                for (const auto& choice : SupportedTitles()) {
                    if (ImGui::Selectable(
                            std::string(choice.label).c_str(),
                            tracker.title == choice.token)) {
                        tracker.title = choice.token;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            changed |= ImGui::Checkbox("Hide when complete", &tracker.hide_when_complete);
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) {
                trackers.erase(trackers.begin() + static_cast<ptrdiff_t>(i));
                changed = true;
                ImGui::PopID();
                continue;
            }
            ImGui::PopID();
            ++i;
        }
        if (ImGui::SmallButton("Add title tracker")) {
            trackers.push_back({.title = "KoaBD"});
            changed = true;
        }
        ImGui::PopID();
        return changed;
    }

    const TaskRecord* FindRecord(const HistoryAttempt& attempt, const uint32_t task_id)
    {
        const auto found = std::ranges::find(attempt.tasks, task_id, &TaskRecord::id);
        return found == attempt.tasks.end() ? nullptr : &*found;
    }

    bool IsFullPbEligible(
        const HistoryAttempt& attempt,
        const PublishedProfile& profile)
    {
        if (
            attempt.result != AttemptResult::completed
            || attempt.tasks.size() != profile.tasks.size()) {
            return false;
        }
        for (size_t i = 0; i < attempt.tasks.size(); ++i) {
            const auto& record = attempt.tasks[i];
            if (
                record.id != profile.tasks[i].task->id
                || record.result != TaskResult::completed
                || !record.split_ms
                || !record.segment_ms) {
                return false;
            }
        }
        return true;
    }

    const HistoryAttempt* PersonalBest(
        const HistoryFile* history,
        const PublishedProfile& profile)
    {
        if (!history) {
            return nullptr;
        }
        const auto route_hash = ComputeRouteHash(profile.definition);
        const HistoryAttempt* best = nullptr;
        for (const auto& attempt : history->attempts) {
            if (
                attempt.route_hash != route_hash
                || !IsFullPbEligible(attempt, profile)
                || best && best->elapsed_ms <= attempt.elapsed_ms) {
                continue;
            }
            best = &attempt;
        }
        return best;
    }

    ImU32 RunColor(const size_t index)
    {
        static constexpr std::array Colors{
            IM_COL32(64, 180, 255, 255),
            IM_COL32(255, 130, 80, 255),
            IM_COL32(120, 220, 120, 255),
            IM_COL32(210, 120, 255, 255),
            IM_COL32(255, 210, 80, 255),
            IM_COL32(80, 220, 210, 255),
        };
        return Colors[index % Colors.size()];
    }
}

namespace TrackerAdvanced {
    bool TrackerAdvancedPlugin::DrawTabButton(
        const bool show_icon, const bool show_text, const bool center_align_text)
    {
        std::scoped_lock lock(settings_mutex_);
        const auto any_window = show_objectives_ || show_stats_ || show_notes_ || show_actions_
            || show_profile_editor_ || show_history_;
        const auto visible = ui_enabled_ && any_window;
        ImGui::PushStyleColor(
            ImGuiCol_Button,
            visible ? ImGui::GetStyle().Colors[ImGuiCol_Button] : ImVec4(0, 0, 0, 0));
        const auto position = ImGui::GetCursorScreenPos();
        const auto text_size = ImGui::CalcTextSize(Name());
        const auto width = ImGui::GetContentRegionAvail().x;
        const auto icon_size = show_icon && Icon() ? ImGui::GetTextLineHeightWithSpacing() : 0.f;
        const auto text_x = center_align_text
            ? position.x + icon_size + (width - icon_size - text_size.x) / 2.f
            : position.x + icon_size + ImGui::GetStyle().ItemSpacing.x;
        const auto clicked = ImGui::Button("", {width, ImGui::GetTextLineHeightWithSpacing()});
        if (show_icon && Icon()) {
            ImGui::GetWindowDrawList()->AddText(
                {position.x, position.y + ImGui::GetStyle().ItemSpacing.y / 2.f},
                ImColor(ImGui::GetStyle().Colors[ImGuiCol_Text]),
                Icon());
        }
        if (show_text) {
            ImGui::GetWindowDrawList()->AddText(
                {text_x, position.y + ImGui::GetStyle().ItemSpacing.y / 2.f},
                ImColor(ImGui::GetStyle().Colors[ImGuiCol_Text]),
                Name());
        }
        if (clicked) {
            ui_enabled_ = !visible;
            if (ui_enabled_ && !any_window) show_objectives_ = true;
        }
        ImGui::PopStyleColor();
        return clicked;
    }

    void TrackerAdvancedPlugin::Draw(IDirect3DDevice9*)
    {
        std::scoped_lock lock(settings_mutex_);
        const auto snapshot = Snapshot();
        if (!snapshot) {
            return;
        }
        if (show_objectives_) DrawObjectivesWindow(*snapshot);
        if (show_stats_) DrawStatsWindow(*snapshot);
        if (show_notes_) DrawNotesWindow(*snapshot);
        if (show_actions_) DrawActionsWindow(*snapshot);
        if (show_profile_editor_) DrawProfileEditorWindow(*snapshot);
        if (show_history_) DrawHistoryWindow(*snapshot);
        DrawResetModal(*snapshot);
    }

    void TrackerAdvancedPlugin::DrawSettings()
    {
        std::scoped_lock lock(settings_mutex_);
        ImGui::Checkbox("Show in main window", &show_in_main_window_);
        ImGui::TextWrapped(
            "Run settings stay outside profiles so the same route can be used by another character.");
        const auto snapshot = Snapshot();
        const auto run_active =
            snapshot
            && (snapshot->runtime.state == RunState::running
                || snapshot->runtime.state == RunState::paused);
        ImGui::BeginDisabled(run_active);
        InputString("Run character", character_name_);
        ImGui::Checkbox("Automatically start for the run character", &auto_start_);
        ImGui::Checkbox("Automatically resume after logout or recovery", &auto_resume_);
        ImGui::EndDisabled();
        if (run_active) {
            ImGui::TextDisabled("Character and automation settings are frozen during an active run.");
        }
        auto checkpoint_seconds = static_cast<int>(checkpoint_seconds_);
        if (ImGui::SliderInt(
                "Checkpoint interval",
                &checkpoint_seconds,
                1,
                60,
                "%d seconds")) {
            checkpoint_seconds_ = static_cast<uint32_t>(checkpoint_seconds);
        }

        if (ImGui::CollapsingHeader("Run profile", ImGuiTreeNodeFlags_DefaultOpen)) {
            const auto profile_change_blocked =
                run_active
                || (snapshot
                    && (snapshot->profile_mutation_pending || snapshot->persistence_pending));
            ImGui::TextUnformatted("Available profiles");
            ImGui::BeginDisabled(profile_change_blocked);
            ImGui::SetNextItemWidth(std::min(
                260.f,
                std::max(120.f, ImGui::GetContentRegionAvail().x - 130.f)));
            if (ImGui::BeginCombo("##available-profiles", profile_loader_selection_.c_str())) {
                if (snapshot) {
                    for (const auto& path : snapshot->profile_files) {
                        const auto filename = Utf8(path.filename().wstring());
                        const auto selected = profile_loader_selection_ == filename;
                        if (ImGui::Selectable(filename.c_str(), selected)) {
                            profile_loader_selection_ = filename;
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            if (ImGui::Button("Load") && !profile_loader_selection_.empty()) {
                QueueProfileLoad(Wide(profile_loader_selection_));
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Refresh")) {
                QueueProfileRefresh();
            }
            ImGui::TextDisabled(
                "Loaded profile: %s",
                snapshot && snapshot->profile
                    ? selected_profile_file_.c_str()
                    : "(none)");
            if (profile_change_blocked) {
                ImGui::TextDisabled(
                    "Profile loading is disabled while a run or profile transaction is active.");
            }
        }

        if (ImGui::CollapsingHeader("Windows", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Tracker Objectives", &show_objectives_);
            ImGui::Checkbox("Tracker Stats", &show_stats_);
            ImGui::Checkbox("Tracker Notes", &show_notes_);
            ImGui::Checkbox("Tracker Actions", &show_actions_);
            ImGui::Checkbox("Run Profile Editor", &show_profile_editor_);
            ImGui::Checkbox("Run History", &show_history_);
            ImGui::Separator();
            ImGui::Checkbox("Transparent Objectives", &transparent_objectives_);
            ImGui::Checkbox("Transparent Stats", &transparent_stats_);
            ImGui::Checkbox("Transparent Notes", &transparent_notes_);
            ImGui::Checkbox("Transparent Actions", &transparent_actions_);
        }

        if (ImGui::CollapsingHeader("Reset and comparison", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Save run data on reset", &save_run_data_on_reset_);
            ImGui::Checkbox(
                "Prompt before discarding otherwise-unsaved run data",
                &prompt_before_discarding_reset_);
            ImGui::Checkbox(
                "Allow saved reset segments to update best segments",
                &include_reset_segments_in_best_);
            ImGui::TextDisabled("Completed personal bests always require a full run with no skipped Tasks.");
        }

        if (ImGui::CollapsingHeader("Hotkeys", ImGuiTreeNodeFlags_DefaultOpen)) {
            static constexpr std::array Labels{
                "Start / Resume",
                "Pause",
                "Split",
                "Undo",
                "Skip",
                "Reset",
            };
            for (size_t i = 0; i < Labels.size(); ++i) {
                DrawHotkeyEditor(Labels[i], i);
            }
            for (size_t left = 0; left < hotkeys_.size(); ++left) {
                if (!hotkeys_[left].key) continue;
                for (size_t right = left + 1; right < hotkeys_.size(); ++right) {
                    if (
                        hotkeys_[left].key == hotkeys_[right].key
                        && hotkeys_[left].control == hotkeys_[right].control
                        && hotkeys_[left].alt == hotkeys_[right].alt
                        && hotkeys_[left].shift == hotkeys_[right].shift) {
                        ImGui::TextColored(
                            {1.f, 0.65f, 0.2f, 1.f},
                            "Two plugin controls use %s.",
                            HotkeyText(hotkeys_[left]).c_str());
                    }
                }
            }
        }
        DrawHotkeyCapturePopup();

        if (ImGui::CollapsingHeader("Test diagnostics")) {
            ImGui::Checkbox("Lifecycle", &debug_lifecycle_);
            ImGui::SameLine();
            ImGui::Checkbox("Criteria and hooks", &debug_criteria_);
            ImGui::SameLine();
            ImGui::Checkbox("Components", &debug_components_);
            ImGui::TextDisabled("Detailed diagnostics are retained only while Test Mode is selected.");
        }
    }

    void TrackerAdvancedPlugin::DrawObjectivesWindow(const PluginSnapshot& snapshot)
    {
        ImGui::SetNextWindowSize({620.f, 500.f}, ImGuiCond_FirstUseEver);
        BeginTransparent(transparent_objectives_);
        if (ImGui::Begin("Tracker Objectives", &show_objectives_)) {
            const auto state = snapshot.runtime.state;
            if (snapshot.recovery_pending) {
                ImGui::TextDisabled("Checking this profile for recoverable run state...");
            }
            else if (snapshot.persistence_pending) {
                ImGui::TextDisabled("Finishing a persistence transaction...");
            }
            if (snapshot.persistence_retry_available) {
                if (ImGui::Button("Retry Persistence")) {
                    std::scoped_lock request_lock(ui_requests_mutex_);
                    ui_requests_.push_back({
                        .type = UiRequestType::retry_persistence,
                    });
                }
            }
            ImGui::BeginDisabled(snapshot.persistence_pending);
            if (state == RunState::ready) {
                if (ImGui::Button("Start")) QueueCommand(UserCommand::start_resume);
            }
            else if (state == RunState::paused) {
                if (ImGui::Button("Resume")) QueueCommand(UserCommand::start_resume);
            }
            else {
                ImGui::BeginDisabled(state != RunState::running);
                if (ImGui::Button("Pause")) QueueCommand(UserCommand::pause);
                ImGui::EndDisabled();
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(state != RunState::running && state != RunState::paused);
            if (ImGui::Button("Split")) QueueCommand(UserCommand::split);
            ImGui::SameLine();
            if (ImGui::Button("Undo")) QueueCommand(UserCommand::undo);
            ImGui::SameLine();
            if (ImGui::Button("Skip")) QueueCommand(UserCommand::skip);
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Reset")) QueueCommand(UserCommand::reset);
            ImGui::SameLine();
            auto mode = snapshot.runtime.mode == RunMode::test ? 1 : 0;
            ImGui::SetNextItemWidth(90.f);
            if (ImGui::Combo("##mode", &mode, "Live\0Test\0")) {
                QueueMode(mode ? RunMode::test : RunMode::live);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Windows")) {
                ImGui::OpenPopup("TrackerWindows");
            }
            if (ImGui::BeginPopup("TrackerWindows")) {
                ImGui::Checkbox("Stats", &show_stats_);
                ImGui::Checkbox("Notes", &show_notes_);
                ImGui::Checkbox("Actions", &show_actions_);
                ImGui::Checkbox("Profile Editor", &show_profile_editor_);
                ImGui::Checkbox("History", &show_history_);
                ImGui::EndPopup();
            }

            ImGui::TextDisabled(
                "State: %s  Mode: %s  Character: %s",
                TrackerEngine::StateLabel(state).c_str(),
                TrackerEngine::ModeLabel(snapshot.runtime.mode).c_str(),
                snapshot.runtime.current_character.empty()
                    ? "(none)"
                    : snapshot.runtime.current_character.c_str());
            if (!snapshot.runtime.status_message.empty()) {
                ImGui::TextWrapped("%s", snapshot.runtime.status_message.c_str());
            }
            if (!snapshot.runtime.map_catalog_ready) {
                ImGui::TextDisabled("Decoding the English map catalog...");
            }
            else if (!snapshot.runtime.safe_game_state) {
                ImGui::TextDisabled("Tracker transitions are paused until a map is safely loaded.");
            }

            const auto profile = snapshot.profile;
            if (!profile) {
                ImGui::Separator();
                ImGui::TextWrapped("No valid run profile is loaded.");
            }
            else {
                ImGui::Separator();
                const auto footer_height =
                    128.f + 2.f * ImGui::GetTextLineHeightWithSpacing();
                const auto table_height =
                    std::max(120.f, ImGui::GetContentRegionAvail().y - footer_height);
                constexpr auto flags =
                    ImGuiTableFlags_BordersInnerV
                    | ImGuiTableFlags_RowBg
                    | ImGuiTableFlags_Resizable
                    | ImGuiTableFlags_ScrollY;
                if (ImGui::BeginTable("Objectives", 3, flags, {0.f, table_height})) {
                    ImGui::TableSetupColumn("Task", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Expected", ImGuiTableColumnFlags_WidthFixed, 96.f);
                    ImGui::TableSetupColumn("Actual", ImGuiTableColumnFlags_WidthFixed, 96.f);
                    ImGui::TableHeadersRow();
                    size_t flat = 0;
                    uint64_t cumulative_expected = 0;
                    for (const auto& objective : profile->definition.objectives) {
                        const auto first = flat;
                        const auto last = first + objective.tasks.size();
                        auto objective_status = RuntimeTaskStatus::pending;
                        auto all_resolved = first != last;
                        for (auto index = first; index < last; ++index) {
                            const auto status = index < snapshot.runtime.tasks.size()
                                ? snapshot.runtime.tasks[index].status
                                : RuntimeTaskStatus::pending;
                            if (status == RuntimeTaskStatus::active) {
                                objective_status = RuntimeTaskStatus::active;
                                break;
                            }
                            if (
                                status != RuntimeTaskStatus::completed
                                && status != RuntimeTaskStatus::skipped) {
                                all_resolved = false;
                            }
                        }
                        if (
                            objective_status != RuntimeTaskStatus::active
                            && all_resolved) {
                            objective_status = RuntimeTaskStatus::completed;
                        }
                        ImGui::PushID(static_cast<int>(objective.id));
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::PushStyleColor(ImGuiCol_Text, StatusColor(objective_status));
                        const auto open = ImGui::TreeNodeEx(
                            objective.name.c_str(),
                            objective_status == RuntimeTaskStatus::active
                                ? ImGuiTreeNodeFlags_DefaultOpen
                                : ImGuiTreeNodeFlags_None);
                        ImGui::PopStyleColor();
                        auto objective_expected = cumulative_expected;
                        for (size_t i = first; i < last; ++i) {
                            objective_expected += profile->tasks[i].task->expected_time_ms;
                        }
                        ImGui::TableNextColumn();
                        ImGui::TextDisabled(
                            "%s",
                            TrackerEngine::FormatDuration(objective_expected).c_str());
                        ImGui::TableNextColumn();
                        const auto objective_actual =
                            last && last - 1 < snapshot.runtime.tasks.size()
                            ? snapshot.runtime.tasks[last - 1].split_ms
                            : std::nullopt;
                        ImGui::TextDisabled("%s", OptionalDuration(objective_actual).c_str());
                        if (open) {
                            for (; flat < last; ++flat) {
                                const auto& task = *profile->tasks[flat].task;
                                const auto runtime = flat < snapshot.runtime.tasks.size()
                                    ? snapshot.runtime.tasks[flat]
                                    : RuntimeTask{};
                                cumulative_expected += task.expected_time_ms;
                                ImGui::PushID(static_cast<int>(task.id));
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::PushStyleColor(ImGuiCol_Text, StatusColor(runtime.status));
                                ImGui::BulletText("%s", task.name.c_str());
                                ImGui::PopStyleColor();
                                ImGui::TableNextColumn();
                                ImGui::TextDisabled(
                                    "%s",
                                    TrackerEngine::FormatDuration(cumulative_expected).c_str());
                                ImGui::TableNextColumn();
                                if (runtime.status == RuntimeTaskStatus::skipped) {
                                    ImGui::TextDisabled("—");
                                }
                                else if (runtime.status == RuntimeTaskStatus::active) {
                                    ImGui::TextDisabled(
                                        "%s",
                                        TrackerEngine::FormatDuration(
                                            snapshot.runtime.elapsed_ms).c_str());
                                }
                                else {
                                    ImGui::TextDisabled(
                                        "%s", OptionalDuration(runtime.split_ms).c_str());
                                }
                                ImGui::PopID();
                            }
                            ImGui::TreePop();
                        }
                        else {
                            flat = last;
                            cumulative_expected = objective_expected;
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }

                const auto index = snapshot.runtime.active_task_index;
                const auto test_without_active_task =
                    snapshot.runtime.mode == RunMode::test && !HasActiveTask(snapshot);
                const auto active =
                    index < profile->tasks.size() && !test_without_active_task
                    ? &profile->tasks[index]
                    : nullptr;
                ImGui::Separator();
                const auto test_complete =
                    snapshot.runtime.mode == RunMode::test
                    && snapshot.runtime.state == RunState::completed;
                ImGui::Text(
                    "Objective: %s",
                    active
                        ? active->objective->name.c_str()
                        : snapshot.runtime.mode == RunMode::test
                            ? "(no active test)"
                            : "(complete)");
                ImGui::Text(
                    "Task: %s",
                    active
                        ? active->task->name.c_str()
                        : test_complete
                            ? "(test complete)"
                            : snapshot.runtime.mode == RunMode::test
                                ? "(no active test)"
                                : "(complete)");
                const auto expected_total = ExpectedTimeFrom(*profile, 0);
                ImGui::Text(
                    "Expected Total Time: %s",
                    FullDuration(expected_total).c_str());
                if (snapshot.runtime.mode == RunMode::test) {
                    ImGui::TextDisabled("Best Possible Time: —");
                }
                else {
                    const auto best_possible =
                        snapshot.runtime.last_resolved_elapsed_ms
                        + ExpectedTimeFrom(*profile, snapshot.runtime.active_task_index);
                    ImGui::Text(
                        "Best Possible Time: %s",
                        FullDuration(best_possible).c_str());
                }
                if (const auto pb = PersonalBest(snapshot.history.get(), *profile)) {
                    ImGui::Text(
                        "Personal Best: %s",
                        TrackerEngine::FormatDuration(pb->elapsed_ms).c_str());
                }
                const auto timer = TrackerEngine::FormatDuration(snapshot.runtime.elapsed_ms);
                ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.65f);
                ImGui::TextUnformatted(timer.c_str());
                ImGui::PopFont();
            }
            if (snapshot.runtime.mode == RunMode::test) {
                DrawDiagnostics(snapshot);
            }
        }
        ImGui::End();
        EndTransparent(transparent_objectives_);
    }

    void TrackerAdvancedPlugin::DrawStatsWindow(const PluginSnapshot& snapshot)
    {
        ImGui::SetNextWindowSize({390.f, 320.f}, ImGuiCond_FirstUseEver);
        BeginTransparent(transparent_stats_);
        if (ImGui::Begin("Tracker Stats", &show_stats_)) {
            if (snapshot.runtime.titles.empty()) {
                ImGui::TextDisabled("No title trackers are active.");
            }
            for (const auto& title : snapshot.runtime.titles) {
                ImGui::PushID(title.token.c_str());
                ImGui::TextUnformatted(title.label.c_str());
                const auto overlay = title.maximum
                    ? std::format(
                        "{}/{} ({:.1f}%)",
                        title.current,
                        title.maximum,
                        title.fraction * 100.f)
                    : std::to_string(title.current);
                CenteredProgressBar(title.fraction, {-1.f, 0.f}, overlay);
                ImGui::PopID();
            }

            if (snapshot.runtime.experience && snapshot.profile
                && snapshot.runtime.active_task_index < snapshot.profile->tasks.size()) {
                const auto& task = *snapshot.profile->tasks[snapshot.runtime.active_task_index].task;
                if (task.experience_tracker) {
                    const auto& runtime = *snapshot.runtime.experience;
                    const auto& definition = *task.experience_tracker;
                    ImGui::Separator();
                    ImGui::Text("%s%s", definition.label.c_str(), runtime.armed ? "" : " (waiting to arm)");
                    ImGui::Text(
                        "Time Elapsed: %s",
                        TrackerEngine::FormatDuration(
                            snapshot.runtime.experience_elapsed_ms).c_str());
                    const auto current = runtime.baseline_experience;
                    const auto remaining =
                        definition.goal_experience > current
                        ? definition.goal_experience - current
                        : 0u;
                    if (runtime.armed) {
                        ImGui::Text(
                            "Experience: %u / %u",
                            current,
                            definition.goal_experience);
                    }
                    else {
                        ImGui::Text(
                            "Experience: — / %u",
                            definition.goal_experience);
                    }
                    const auto progress = definition.goal_experience
                        ? std::clamp(
                            static_cast<float>(current)
                                / static_cast<float>(definition.goal_experience),
                            0.f,
                            1.f)
                        : 0.f;
                    const auto progress_text = std::format("{:.1f}%", progress * 100.f);
                    if (ImGui::BeginTable(
                            "ExperienceProgress",
                            2,
                            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings)) {
                        ImGui::TableSetupColumn(
                            "Experience",
                            ImGuiTableColumnFlags_WidthFixed,
                            220.f);
                        ImGui::TableSetupColumn(
                            "Progress",
                            ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::AlignTextToFramePadding();
                        ImGui::Text(
                            "XP Gained: %llu (%u left)",
                            static_cast<unsigned long long>(runtime.experience_gained),
                            remaining);
                        ImGui::TableSetColumnIndex(1);
                        CenteredProgressBar(progress, {-1.f, 0.f}, progress_text);
                        ImGui::EndTable();
                    }
                    ImGui::Text("Runs: %u", runtime.runs);
                    if (snapshot.runtime.experience_elapsed_ms) {
                        const auto runs_per_hour = static_cast<uint64_t>(std::llround(
                            static_cast<double>(runtime.runs) * 3'600'000.0
                            / static_cast<double>(snapshot.runtime.experience_elapsed_ms)));
                        ImGui::Text(
                            "Runs/Hour: %llu",
                            static_cast<unsigned long long>(runs_per_hour));
                    }
                    else {
                        ImGui::TextUnformatted("Runs/Hour: —");
                    }
                    if (runtime.runs) {
                        const auto time_per_run =
                            snapshot.runtime.experience_elapsed_ms / runtime.runs;
                        const auto xp_per_run = runtime.experience_gained / runtime.runs;
                        const auto runs_left = runtime.experience_gained
                            ? static_cast<uint64_t>(std::ceil(
                                static_cast<double>(remaining) * runtime.runs
                                / static_cast<double>(runtime.experience_gained)))
                            : 0;
                        ImGui::Text(
                            "Time/Run: %s",
                            TrackerEngine::FormatDuration(time_per_run).c_str());
                        ImGui::Text(
                            "XP/Run: %llu",
                            static_cast<unsigned long long>(xp_per_run));
                        if (!remaining || runtime.experience_gained) {
                            ImGui::Text(
                                "Runs Remaining: %llu",
                                static_cast<unsigned long long>(runs_left));
                        }
                        else {
                            ImGui::TextUnformatted("Runs Remaining: —");
                        }
                    }
                    else {
                        ImGui::TextUnformatted("Time/Run: —");
                        ImGui::TextUnformatted("XP/Run: —");
                        ImGui::TextUnformatted(
                            remaining ? "Runs Remaining: —" : "Runs Remaining: 0");
                    }
                }
            }
        }
        ImGui::End();
        EndTransparent(transparent_stats_);
    }

    void TrackerAdvancedPlugin::DrawNotesWindow(const PluginSnapshot& snapshot)
    {
        ImGui::SetNextWindowSize({390.f, 320.f}, ImGuiCond_FirstUseEver);
        BeginTransparent(transparent_notes_);
        if (ImGui::Begin("Tracker Notes", &show_notes_)) {
            if (
                !HasActiveTask(snapshot)) {
                ImGui::TextDisabled("No active Task.");
            }
            else {
                const auto& task =
                    *snapshot.profile->tasks[snapshot.runtime.active_task_index].task;
                ImGui::TextUnformatted(task.name.c_str());
                ImGui::Separator();
                if (task.notes.empty()) {
                    ImGui::TextDisabled("No notes for this Task.");
                }
                for (const auto& note : task.notes) {
                    ImGui::Bullet();
                    ImGui::SameLine();
                    ImGui::TextWrapped("%s", note.c_str());
                }
            }
        }
        ImGui::End();
        EndTransparent(transparent_notes_);
    }

    void TrackerAdvancedPlugin::DrawActionsWindow(const PluginSnapshot& snapshot)
    {
        ImGui::SetNextWindowSize({380.f, 300.f}, ImGuiCond_FirstUseEver);
        BeginTransparent(transparent_actions_);
        if (ImGui::Begin("Tracker Actions", &show_actions_)) {
            if (
                !HasActiveTask(snapshot)) {
                ImGui::TextDisabled("No active Task.");
            }
            else {
                const auto& task =
                    *snapshot.profile->tasks[snapshot.runtime.active_task_index].task;
                auto drew_travel = false;
                auto drew_build = false;
                for (const auto action_id : task.action_ids) {
                    const auto found = snapshot.profile->actions.find(action_id);
                    if (found == snapshot.profile->actions.end()) continue;
                    const auto action = found->second;
                    if (action->type == ActionType::travel && !drew_travel) {
                        ImGui::TextUnformatted("Travel");
                        drew_travel = true;
                    }
                    if (action->type != ActionType::travel && !drew_build) {
                        if (drew_travel) ImGui::Separator();
                        ImGui::TextUnformatted("Builds");
                        drew_build = true;
                    }
                    ImGui::PushID(static_cast<int>(action_id));
                    if (ImGui::Button(action->label.c_str(), {-1.f, 0.f})) {
                        QueueAction(action_id);
                    }
                    ImGui::PopID();
                }
                if (!drew_travel && !drew_build) {
                    ImGui::TextDisabled("No actions for this Task.");
                }
            }
        }
        ImGui::End();
        EndTransparent(transparent_actions_);
    }

    void TrackerAdvancedPlugin::DrawProfileEditorWindow(const PluginSnapshot& snapshot)
    {
        ImGui::SetNextWindowSize({900.f, 700.f}, ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Run Profile Editor", &show_profile_editor_)) {
            ImGui::End();
            return;
        }
        std::scoped_lock lock(editor_mutex_);
        const auto locked =
            snapshot.runtime.state == RunState::running
            || snapshot.runtime.state == RunState::paused
            || snapshot.profile_mutation_pending;
        if (locked) {
            ImGui::TextColored(
                {1.f, 0.7f, 0.2f, 1.f},
                snapshot.profile_mutation_pending
                    ? "Profile editing is disabled while the profile transaction finishes."
                    : "Profile editing is disabled during an active run.");
        }
        ImGui::BeginDisabled(locked);

        if (editor_dirty_) {
            ImGui::TextColored(
                {1.f, 0.8f, 0.2f, 1.f},
                "Save or discard this draft before opening another profile.");
            if (ImGui::Button("Discard Draft")) {
                if (snapshot.profile) {
                    editor_profile_ = snapshot.profile->definition;
                    editor_path_ = loaded_profile_path_;
                }
                else {
                    editor_profile_.reset();
                    editor_path_.clear();
                }
                editor_selected_objective_id_ = 0;
                editor_selected_task_id_ = 0;
                editor_selected_action_id_ = 0;
                editor_dirty_ = false;
                {
                    std::scoped_lock request_lock(ui_requests_mutex_);
                    ui_requests_.push_back({
                        .type = UiRequestType::clear_profile_diagnostics,
                    });
                }
            }
        }
        ImGui::BeginDisabled(editor_dirty_ || snapshot.persistence_pending);
        if (ImGui::BeginTable(
                "ProfileSelection",
                2,
                ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings)) {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 80.f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Profile");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1.f);
            if (ImGui::BeginCombo(
                    "##profile-selection",
                    editor_path_.empty()
                        ? "(none)"
                        : Utf8(editor_path_.filename().wstring()).c_str())) {
                for (const auto& path : snapshot.profile_files) {
                    if (ImGui::Selectable(Utf8(path.filename().wstring()).c_str())) {
                        QueueProfileLoad(path.filename());
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::EndTable();
        }
        ImGui::EndDisabled();
        if (ImGui::Button("New")) {
            ProfileDefinition fresh{
                .id = "new-profile",
                .name = "New Profile",
                .objectives = {{
                    .id = 1,
                    .name = "Objective 1",
                    .tasks = {{
                        .id = 1,
                        .name = "Task 1",
                        .end_criterion = {.type = CriterionType::manual},
                    }},
                }},
            };
            editor_profile_ = std::move(fresh);
            editor_path_ = ProfilesDirectory() / L"new-profile.json";
            editor_selected_objective_id_ = 1;
            editor_selected_task_id_ = 1;
            editor_dirty_ = true;
        }
        if (editor_profile_) {
            ImGui::SameLine();
            ImGui::BeginDisabled(snapshot.persistence_pending);
            if (ImGui::Button("Save As / Overwrite")) {
                if (editor_path_.extension() != L".json") {
                    editor_path_ += L".json";
                }
                QueueProfileSave(*editor_profile_, editor_path_, true);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(editor_dirty_ || snapshot.persistence_pending);
            if (ImGui::Button("Delete file")) {
                ImGui::OpenPopup("Delete Profile File?");
            }
            ImGui::EndDisabled();
            if (ImGui::BeginPopupModal(
                    "Delete Profile File?",
                    nullptr,
                    ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextWrapped(
                    "Delete %s? If it is loaded, the in-memory profile will also be closed.",
                    Utf8(editor_path_.filename().wstring()).c_str());
                ImGui::BeginDisabled(snapshot.persistence_pending);
                if (ImGui::Button("Delete")) {
                    QueueProfileDelete(editor_path_);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            auto filename = Utf8(editor_path_.filename().wstring());
            auto& profile = *editor_profile_;
            if (!profile.description) profile.description.emplace();
            if (ImGui::BeginTable(
                    "ProfileIdentity",
                    2,
                    ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings)) {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 80.f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("File name");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-1.f);
                if (InputString("##profile-file-name", filename)) {
                    editor_path_ = ProfilesDirectory() / Wide(filename);
                    editor_dirty_ = true;
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Profile ID");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-1.f);
                editor_dirty_ |= InputString("##profile-id", profile.id);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Name");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-1.f);
                editor_dirty_ |= InputString("##profile-name", profile.name);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Description");
                ImGui::TableSetColumnIndex(1);
                editor_dirty_ |= InputStringMultiline(
                    "##profile-description",
                    *profile.description,
                    {ImGui::GetContentRegionAvail().x, 55.f});
                ImGui::EndTable();
            }
            if (ImGui::CollapsingHeader("Profile title trackers")) {
                editor_dirty_ |= EditTitleTrackers(profile.title_trackers, "profile-titles");
            }

            ImGui::Separator();
            ImGui::BeginChild("ProfileTree", {260.f, 0.f}, true);
            ImGui::TextUnformatted("Objectives and Tasks");
            for (auto& objective : profile.objectives) {
                ImGui::PushID(static_cast<int>(objective.id));
                const auto open = ImGui::TreeNodeEx(
                    objective.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
                if (ImGui::IsItemClicked()) {
                    editor_selected_objective_id_ = objective.id;
                    editor_selected_task_id_ = 0;
                }
                if (open) {
                    for (auto& task : objective.tasks) {
                        ImGui::PushID(static_cast<int>(task.id));
                        const auto selected = editor_selected_task_id_ == task.id;
                        if (ImGui::Selectable(
                                std::format("  {}", task.name).c_str(), selected)) {
                            editor_selected_objective_id_ = objective.id;
                            editor_selected_task_id_ = task.id;
                        }
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            if (ImGui::Button("Add objective", {-1.f, 0.f})) {
                const auto id = NextObjectiveId(profile);
                profile.objectives.push_back({
                    .id = id,
                    .name = std::format("Objective {}", id),
                });
                editor_selected_objective_id_ = id;
                editor_selected_task_id_ = 0;
                editor_dirty_ = true;
            }
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("ProfileDetails", {0.f, 0.f}, true);

            const auto objective_it = std::ranges::find(
                profile.objectives,
                editor_selected_objective_id_,
                &ObjectiveDefinition::id);
            if (objective_it == profile.objectives.end()) {
                ImGui::TextDisabled("Select an Objective or Task.");
            }
            else {
                auto& objective = *objective_it;
                if (!editor_selected_task_id_) {
                    ImGui::TextUnformatted("Objective");
                    editor_dirty_ |= InputString("Objective name", objective.name);
                    editor_dirty_ |= EditTitleTrackers(
                        objective.title_trackers, "objective-titles");
                    if (ImGui::Button("Add Task")) {
                        const auto id = NextTaskId(profile);
                        objective.tasks.push_back({
                            .id = id,
                            .name = std::format("Task {}", id),
                            .end_criterion = {.type = CriterionType::manual},
                        });
                        editor_selected_task_id_ = id;
                        editor_dirty_ = true;
                    }
                    ImGui::SameLine();
                    const auto objective_index = static_cast<size_t>(
                        std::distance(profile.objectives.begin(), objective_it));
                    ImGui::BeginDisabled(objective_index == 0);
                    if (ImGui::Button("Move Up")) {
                        std::swap(
                            profile.objectives[objective_index],
                            profile.objectives[objective_index - 1]);
                        editor_dirty_ = true;
                    }
                    ImGui::EndDisabled();
                    ImGui::SameLine();
                    ImGui::BeginDisabled(objective_index + 1 >= profile.objectives.size());
                    if (ImGui::Button("Move Down")) {
                        std::swap(
                            profile.objectives[objective_index],
                            profile.objectives[objective_index + 1]);
                        editor_dirty_ = true;
                    }
                    ImGui::EndDisabled();
                    ImGui::SameLine();
                    if (ImGui::Button("Delete Objective")) {
                        profile.objectives.erase(objective_it);
                        editor_selected_objective_id_ = 0;
                        editor_dirty_ = true;
                    }
                }
                else {
                    const auto task_it = std::ranges::find(
                        objective.tasks,
                        editor_selected_task_id_,
                        &TaskDefinition::id);
                    if (task_it != objective.tasks.end()) {
                        auto& task = *task_it;
                        ImGui::TextUnformatted("Task");
                        editor_dirty_ |= InputString("Task name", task.name);
                        auto seconds = static_cast<double>(task.expected_time_ms) / 1000.0;
                        if (ImGui::InputDouble(
                                "Expected segment (seconds)", &seconds, 1.0, 60.0, "%.1f")) {
                            task.expected_time_ms = static_cast<uint64_t>(
                                std::max(0.0, seconds) * 1000.0);
                            editor_dirty_ = true;
                        }
                        if (ImGui::CollapsingHeader(
                                "End criterion", ImGuiTreeNodeFlags_DefaultOpen)) {
                            editor_dirty_ |= EditCriterion(
                                task.end_criterion, snapshot.map_catalog);
                        }
                        if (ImGui::CollapsingHeader(
                                "Notes", ImGuiTreeNodeFlags_DefaultOpen)) {
                            for (size_t i = 0; i < task.notes.size();) {
                                ImGui::PushID(static_cast<int>(i));
                                editor_dirty_ |= InputString("##note", task.notes[i]);
                                ImGui::SameLine();
                                if (ImGui::SmallButton("Remove")) {
                                    task.notes.erase(
                                        task.notes.begin() + static_cast<ptrdiff_t>(i));
                                    editor_dirty_ = true;
                                    ImGui::PopID();
                                    continue;
                                }
                                ImGui::PopID();
                                ++i;
                            }
                            if (ImGui::SmallButton("Add note")) {
                                task.notes.emplace_back();
                                editor_dirty_ = true;
                            }
                        }
                        if (ImGui::CollapsingHeader("Actions")) {
                            for (const auto& action : profile.actions) {
                                auto attached =
                                    std::ranges::find(task.action_ids, action.id)
                                    != task.action_ids.end();
                                if (ImGui::Checkbox(
                                        std::format("{}##{}", action.label, action.id).c_str(),
                                        &attached)) {
                                    if (attached) {
                                        task.action_ids.push_back(action.id);
                                    }
                                    else {
                                        std::erase(task.action_ids, action.id);
                                    }
                                    editor_dirty_ = true;
                                }
                            }
                        }
                        if (ImGui::CollapsingHeader("Experience tracker")) {
                            auto enabled = task.experience_tracker.has_value();
                            if (ImGui::Checkbox("Enabled", &enabled)) {
                                if (enabled) {
                                    task.experience_tracker = ExperienceTrackerDefinition{
                                        .label = "Experience Tracker",
                                        .arm_criterion = {
                                            .type = CriterionType::map_loaded,
                                            .map = std::string{},
                                        },
                                        .increment_criterion = {
                                            .type = CriterionType::map_loaded,
                                            .map = std::string{},
                                        },
                                    };
                                }
                                else {
                                    task.experience_tracker.reset();
                                }
                                editor_dirty_ = true;
                            }
                            if (task.experience_tracker) {
                                auto& tracker = *task.experience_tracker;
                                editor_dirty_ |= InputString("Label", tracker.label);
                                editor_dirty_ |= EditMap(
                                    "Arm on map",
                                    tracker.arm_criterion.map,
                                    MapUsage::Any,
                                    snapshot.map_catalog);
                                editor_dirty_ |= EditMap(
                                    "Increment on map",
                                    tracker.increment_criterion.map,
                                    MapUsage::Any,
                                    snapshot.map_catalog);
                                auto goal = static_cast<int>(tracker.goal_experience);
                                if (ImGui::InputInt("Goal total experience", &goal)) {
                                    tracker.goal_experience =
                                        static_cast<uint32_t>(std::max(0, goal));
                                    editor_dirty_ = true;
                                }
                            }
                        }
                        if (ImGui::Button("Delete Task")) {
                            objective.tasks.erase(task_it);
                            editor_selected_task_id_ = 0;
                            editor_dirty_ = true;
                        }
                        else {
                            const auto task_index = static_cast<size_t>(
                                std::distance(objective.tasks.begin(), task_it));
                            ImGui::SameLine();
                            ImGui::BeginDisabled(task_index == 0);
                            if (ImGui::Button("Move Task Up")) {
                                std::swap(
                                    objective.tasks[task_index],
                                    objective.tasks[task_index - 1]);
                                editor_dirty_ = true;
                            }
                            ImGui::EndDisabled();
                            ImGui::SameLine();
                            ImGui::BeginDisabled(
                                task_index + 1 >= objective.tasks.size());
                            if (ImGui::Button("Move Task Down")) {
                                std::swap(
                                    objective.tasks[task_index],
                                    objective.tasks[task_index + 1]);
                                editor_dirty_ = true;
                            }
                            ImGui::EndDisabled();
                        }
                    }
                }
            }

            if (ImGui::CollapsingHeader("Action definitions")) {
                for (auto& action : profile.actions) {
                    if (ImGui::Selectable(
                            std::format("{}##action{}", action.label, action.id).c_str(),
                            editor_selected_action_id_ == action.id)) {
                        editor_selected_action_id_ = action.id;
                    }
                }
                if (ImGui::Button("Add action")) {
                    const auto id = NextActionId(profile);
                    profile.actions.push_back({
                        .id = id,
                        .type = ActionType::travel,
                        .label = "Travel",
                        .destination = std::string{},
                    });
                    editor_selected_action_id_ = id;
                    editor_dirty_ = true;
                }
                const auto action_it = std::ranges::find(
                    profile.actions,
                    editor_selected_action_id_,
                    &ActionDefinition::id);
                if (action_it != profile.actions.end()) {
                    auto& action = *action_it;
                    editor_dirty_ |= InputString("Action label", action.label);
                    auto type = static_cast<int>(action.type) - 1;
                    if (ImGui::Combo(
                            "Action type",
                            &type,
                            "Travel\0Player Build\0Hero Team Build\0")) {
                        const auto id = action.id;
                        const auto label = action.label;
                        action = {
                            .id = id,
                            .type = static_cast<ActionType>(type + 1),
                            .label = label,
                        };
                        if (action.type == ActionType::travel) action.destination.emplace();
                        if (action.type == ActionType::player_build) {
                            action.team_build.emplace();
                            action.build.emplace();
                        }
                        if (action.type == ActionType::hero_team_build) action.name.emplace();
                        editor_dirty_ = true;
                    }
                    if (action.type == ActionType::travel) {
                        if (!action.destination) action.destination.emplace();
                        editor_dirty_ |= InputString("Destination", *action.destination);
                    }
                    else if (action.type == ActionType::player_build) {
                        if (!action.team_build) action.team_build.emplace();
                        if (!action.build) action.build.emplace();
                        editor_dirty_ |= InputString("Team build", *action.team_build);
                        editor_dirty_ |= InputString("Build", *action.build);
                    }
                    else if (action.type == ActionType::hero_team_build) {
                        if (!action.name) action.name.emplace();
                        editor_dirty_ |= InputString("Hero team name", *action.name);
                    }
                    if (ImGui::Button("Delete action definition")) {
                        const auto id = action.id;
                        profile.actions.erase(action_it);
                        for (auto& item : profile.objectives) {
                            for (auto& task : item.tasks) {
                                std::erase(task.action_ids, id);
                            }
                        }
                        editor_selected_action_id_ = 0;
                        editor_dirty_ = true;
                    }
                }
            }
            ImGui::EndChild();
        }

        ImGui::EndDisabled();
        if (editor_dirty_) {
            ImGui::TextColored({1.f, 0.8f, 0.2f, 1.f}, "Unsaved profile changes");
        }
        for (const auto& error : snapshot.profile_errors) {
            ImGui::TextColored({1.f, 0.3f, 0.3f, 1.f}, "%s", error.c_str());
        }
        for (const auto& warning : snapshot.profile_warnings) {
            ImGui::TextColored({1.f, 0.75f, 0.25f, 1.f}, "%s", warning.c_str());
        }
        ImGui::End();
    }

    void TrackerAdvancedPlugin::DrawHistoryWindow(const PluginSnapshot& snapshot)
    {
        ImGui::SetNextWindowSize({760.f, 520.f}, ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Run History", &show_history_)) {
            ImGui::End();
            return;
        }
        if (!snapshot.profile || !snapshot.history || snapshot.history->attempts.empty()) {
            ImGui::TextDisabled("No saved attempts for this profile.");
            ImGui::End();
            return;
        }
        if (snapshot.history_dirty) {
            if (snapshot.history_write_pending) {
                ImGui::TextDisabled("Saving the latest history update...");
            }
            else {
                ImGui::TextColored(
                    {1.f, 0.4f, 0.3f, 1.f},
                    "The latest history update could not be written.");
            }
        }
        if (snapshot.persistence_retry_available) {
            if (ImGui::Button("Retry Persistence")) {
                std::scoped_lock request_lock(ui_requests_mutex_);
                ui_requests_.push_back({
                    .type = UiRequestType::retry_persistence,
                });
            }
        }
        const auto route_hash = ComputeRouteHash(snapshot.profile->definition);
        std::vector<const HistoryAttempt*> attempts;
        for (const auto& attempt : snapshot.history->attempts) {
            if (attempt.route_hash == route_hash) {
                attempts.push_back(&attempt);
            }
        }
        if (attempts.empty()) {
            ImGui::TextDisabled("Saved attempts use a different route version.");
            ImGui::End();
            return;
        }

        auto baseline = static_cast<int>(
            std::min(history_baseline_index_, attempts.size() - 1));
        if (ImGui::BeginCombo(
                "Baseline",
                std::format(
                    "{} ({})",
                    attempts[static_cast<size_t>(baseline)]->started_at,
                    attempts[static_cast<size_t>(baseline)]->result == AttemptResult::completed
                        ? "completed"
                        : "reset").c_str())) {
            for (size_t i = 0; i < attempts.size(); ++i) {
                if (ImGui::Selectable(
                        std::format(
                            "{} ({})",
                            attempts[i]->started_at,
                            attempts[i]->result == AttemptResult::completed
                                ? "completed"
                                : "reset").c_str(),
                        static_cast<int>(i) == baseline)) {
                    history_baseline_index_ = i;
                    baseline = static_cast<int>(i);
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(170.f);
        ImGui::Combo(
            "Graph",
            &history_graph_mode_,
            "Cumulative time\0Delta to baseline\0Segment time\0");

        for (size_t i = 0; i < attempts.size(); ++i) {
            auto [visibility, inserted] =
                history_visibility_.try_emplace(attempts[i]->id, i < 6);
            static_cast<void>(inserted);
            auto& visible = visibility->second;
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(RunColor(i)));
            ImGui::Checkbox(
                std::format(
                    "{}  {}  {}##run{}",
                    attempts[i]->started_at,
                    attempts[i]->result == AttemptResult::completed ? "Completed" : "Reset",
                    TrackerEngine::FormatDuration(attempts[i]->elapsed_ms),
                    i).c_str(),
                &visible);
            ImGui::PopStyleColor();
        }

        const auto graph_pos = ImGui::GetCursorScreenPos();
        const auto graph_size = ImVec2(
            ImGui::GetContentRegionAvail().x,
            std::max(180.f, ImGui::GetContentRegionAvail().y - 70.f));
        ImGui::InvisibleButton("HistoryGraph", graph_size);
        const auto graph_hovered = ImGui::IsItemHovered();
        const auto mouse = ImGui::GetIO().MousePos;
        const auto draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(
            graph_pos,
            {graph_pos.x + graph_size.x, graph_pos.y + graph_size.y},
            IM_COL32(15, 15, 20, 180));
        draw->AddRect(
            graph_pos,
            {graph_pos.x + graph_size.x, graph_pos.y + graph_size.y},
            IM_COL32(100, 100, 110, 255));

        const auto task_count = snapshot.profile->tasks.size();
        std::vector<std::vector<std::optional<double>>> series(attempts.size());
        auto min_value = std::numeric_limits<double>::infinity();
        auto max_value = -std::numeric_limits<double>::infinity();
        for (size_t run = 0; run < attempts.size(); ++run) {
            series[run].resize(task_count);
            for (size_t task = 0; task < task_count; ++task) {
                const auto record = FindRecord(
                    *attempts[run], snapshot.profile->tasks[task].task->id);
                if (!record || record->result == TaskResult::skipped) continue;
                std::optional<double> value;
                if (history_graph_mode_ == 0 && record->split_ms) {
                    value = static_cast<double>(*record->split_ms);
                }
                else if (history_graph_mode_ == 2 && record->segment_ms) {
                    value = static_cast<double>(*record->segment_ms);
                }
                else if (history_graph_mode_ == 1 && record->split_ms) {
                    const auto base_record = FindRecord(
                        *attempts[static_cast<size_t>(baseline)],
                        snapshot.profile->tasks[task].task->id);
                    if (base_record && base_record->split_ms) {
                        value = static_cast<double>(
                            static_cast<int64_t>(*record->split_ms)
                            - static_cast<int64_t>(*base_record->split_ms));
                    }
                }
                if (value) {
                    series[run][task] = value;
                    if (history_visibility_[attempts[run]->id]) {
                        min_value = std::min(min_value, *value);
                        max_value = std::max(max_value, *value);
                    }
                }
            }
        }
        if (!std::isfinite(min_value) || !std::isfinite(max_value)) {
            min_value = 0;
            max_value = 1;
        }
        if (min_value == max_value) max_value += 1;
        const auto point = [&](const size_t task, const double value) {
            const auto x = graph_pos.x
                + (task_count > 1
                    ? static_cast<float>(task) / static_cast<float>(task_count - 1)
                    : 0.f) * graph_size.x;
            const auto y = graph_pos.y + graph_size.y
                - static_cast<float>((value - min_value) / (max_value - min_value))
                    * graph_size.y;
            return ImVec2{x, y};
        };
        const auto signed_values = history_graph_mode_ == 1;
        const auto maximum_label = HistoryDuration(max_value, signed_values);
        const auto minimum_label = HistoryDuration(min_value, signed_values);
        draw->AddText(
            {graph_pos.x + 4.f, graph_pos.y + 3.f},
            IM_COL32(185, 185, 195, 220),
            maximum_label.c_str());
        draw->AddText(
            {graph_pos.x + 4.f, graph_pos.y + graph_size.y - ImGui::GetFontSize() - 3.f},
            IM_COL32(185, 185, 195, 220),
            minimum_label.c_str());

        for (size_t task = 0; task < task_count; ++task) {
            const auto objective_index = snapshot.profile->tasks[task].objective_index;
            if (
                task
                && objective_index
                    == snapshot.profile->tasks[task - 1].objective_index) {
                continue;
            }
            const auto boundary = point(task, min_value);
            if (task) {
                draw->AddLine(
                    {boundary.x, graph_pos.y},
                    {boundary.x, graph_pos.y + graph_size.y},
                    IM_COL32(105, 105, 120, 120));
            }
            const auto label = std::format("O{}", objective_index + 1);
            draw->AddText(
                {boundary.x + 3.f, graph_pos.y + ImGui::GetFontSize() + 5.f},
                IM_COL32(150, 150, 165, 180),
                label.c_str());
        }

        struct HoveredPoint {
            size_t run = 0;
            size_t task = 0;
            double value = 0.0;
            float distance_squared = std::numeric_limits<float>::infinity();
        };
        std::optional<HoveredPoint> hovered_point;
        for (size_t run = 0; run < attempts.size(); ++run) {
            if (!history_visibility_[attempts[run]->id]) continue;
            std::optional<ImVec2> previous;
            for (size_t task = 0; task < task_count; ++task) {
                if (!series[run][task]) {
                    previous.reset();
                    continue;
                }
                const auto current = point(task, *series[run][task]);
                if (previous) {
                    draw->AddLine(*previous, current, RunColor(run), 2.f);
                }
                draw->AddCircleFilled(current, 2.f, RunColor(run));
                if (graph_hovered) {
                    const auto dx = current.x - mouse.x;
                    const auto dy = current.y - mouse.y;
                    const auto distance_squared = dx * dx + dy * dy;
                    if (
                        distance_squared <= 64.f
                        && (!hovered_point
                            || distance_squared < hovered_point->distance_squared)) {
                        hovered_point = {
                            .run = run,
                            .task = task,
                            .value = *series[run][task],
                            .distance_squared = distance_squared,
                        };
                    }
                }
                previous = current;
            }
        }
        if (history_graph_mode_ == 1 && min_value < 0 && max_value > 0) {
            const auto zero_left = point(0, 0);
            draw->AddLine(
                {graph_pos.x, zero_left.y},
                {graph_pos.x + graph_size.x, zero_left.y},
                IM_COL32(160, 160, 160, 180));
        }
        if (hovered_point) {
            const auto& attempt = *attempts[hovered_point->run];
            const auto& task = snapshot.profile->tasks[hovered_point->task];
            const auto value = HistoryDuration(
                hovered_point->value, history_graph_mode_ == 1);
            static constexpr std::array ValueLabels{
                "Cumulative",
                "Delta",
                "Segment",
            };
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(attempt.started_at.c_str());
            ImGui::Text(
                "%s / %s",
                task.objective->name.c_str(),
                task.task->name.c_str());
            ImGui::Text(
                "%s: %s",
                ValueLabels[static_cast<size_t>(history_graph_mode_)],
                value.c_str());
            ImGui::EndTooltip();
        }

        if (const auto pb = PersonalBest(snapshot.history.get(), *snapshot.profile)) {
            ImGui::Text(
                "Full-run PB: %s (%s)",
                TrackerEngine::FormatDuration(pb->elapsed_ms).c_str(),
                pb->started_at.c_str());
        }
        else {
            ImGui::TextDisabled("No PB-eligible completed run yet.");
        }
        if (ImGui::CollapsingHeader("Best segments")) {
            if (ImGui::BeginTable(
                    "BestSegments",
                    2,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV
                        | ImGuiTableFlags_ScrollY,
                    {0.f, 150.f})) {
                ImGui::TableSetupColumn("Task", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Best", ImGuiTableColumnFlags_WidthFixed, 100.f);
                ImGui::TableHeadersRow();
                for (size_t task_index = 0; task_index < task_count; ++task_index) {
                    std::optional<uint64_t> best;
                    const auto task_id = snapshot.profile->tasks[task_index].task->id;
                    for (const auto attempt : attempts) {
                        if (
                            attempt->result == AttemptResult::reset
                            && !include_reset_segments_in_best_) {
                            continue;
                        }
                        const auto record = FindRecord(*attempt, task_id);
                        if (
                            !record
                            || record->result == TaskResult::skipped
                            || !record->segment_ms) {
                            continue;
                        }
                        if (task_index) {
                            const auto previous = FindRecord(
                                *attempt,
                                snapshot.profile->tasks[task_index - 1].task->id);
                            if (!previous || previous->result == TaskResult::skipped) {
                                continue;
                            }
                        }
                        if (!best || *record->segment_ms < *best) {
                            best = record->segment_ms;
                        }
                    }
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(
                        snapshot.profile->tasks[task_index].task->name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("%s", OptionalDuration(best).c_str());
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    void TrackerAdvancedPlugin::DrawResetModal(const PluginSnapshot& snapshot)
    {
        if (snapshot.reset.phase == ResetPromptView::Phase::none) {
            if (ImGui::BeginPopupModal(
                    "Reset active run?",
                    nullptr,
                    ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
            return;
        }
        ImGui::OpenPopup("Reset active run?");
        if (ImGui::BeginPopupModal(
                "Reset active run?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (snapshot.reset.phase == ResetPromptView::Phase::writing) {
                ImGui::TextUnformatted(
                    snapshot.reset.discard || snapshot.reset.history_committed
                        ? "Deleting active-run recovery data before reset..."
                        : "Saving the resolved Tasks before reset...");
            }
            else {
                ImGui::TextWrapped(
                    "The active Task and its partial segment will not be recorded.");
                if (!snapshot.reset.error.empty()) {
                    ImGui::TextColored(
                        {1.f, 0.3f, 0.3f, 1.f},
                        "%s",
                        snapshot.reset.error.c_str());
                }
                const auto retry_cleanup =
                    snapshot.reset.phase == ResetPromptView::Phase::failed
                    && (snapshot.reset.discard || snapshot.reset.history_committed);
                if (ImGui::Button(
                        retry_cleanup
                            ? "Retry Cleanup"
                            : snapshot.reset.phase == ResetPromptView::Phase::failed
                                ? "Retry Save"
                                : "Save and Reset")) {
                    QueueCommand(
                        snapshot.reset.phase == ResetPromptView::Phase::failed
                            ? UserCommand::reset_retry
                            : UserCommand::reset_save);
                }
                if (!snapshot.reset.history_committed) {
                    if (!snapshot.reset.discard) {
                        ImGui::SameLine();
                        if (ImGui::Button("Discard and Reset")) {
                            QueueCommand(UserCommand::reset_discard);
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel")) {
                        QueueCommand(UserCommand::reset_cancel);
                    }
                }
            }
            ImGui::EndPopup();
        }
    }

    void TrackerAdvancedPlugin::DrawDiagnostics(const PluginSnapshot& snapshot)
    {
        if (!ImGui::CollapsingHeader("Test Mode", ImGuiTreeNodeFlags_DefaultOpen)) {
            return;
        }
        if (!(snapshot.profile && !snapshot.profile->tasks.empty())) {
            ImGui::TextDisabled("Load a profile to select a Task test.");
            return;
        }
        test_task_index_ = std::min(
            test_task_index_,
            snapshot.profile->tasks.size() - 1);
        const auto& selected = snapshot.profile->tasks[test_task_index_];
        const auto preview = std::format(
            "{} / {}",
            selected.objective->name,
            selected.task->name);
        if (ImGui::BeginCombo("Task", preview.c_str())) {
            for (size_t index = 0; index < snapshot.profile->tasks.size(); ++index) {
                const auto& task = snapshot.profile->tasks[index];
                const auto label = std::format(
                    "{} / {}##test{}",
                    task.objective->name,
                    task.task->name,
                    index);
                if (ImGui::Selectable(label.c_str(), index == test_task_index_)) {
                    test_task_index_ = index;
                }
            }
            ImGui::EndCombo();
        }
        const auto active =
            snapshot.runtime.state == RunState::running
            || snapshot.runtime.state == RunState::paused
            || snapshot.persistence_pending;
        ImGui::BeginDisabled(active);
        if (ImGui::Button("Evaluate Selected State")) {
            QueueTaskTest(test_task_index_, true);
        }
        ImGui::SameLine();
        if (ImGui::Button("Arm Selected Task")) {
            QueueTaskTest(test_task_index_, false);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            std::scoped_lock request_lock(ui_requests_mutex_);
            ui_requests_.push_back({
                .type = UiRequestType::clear_diagnostics,
            });
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy")) {
            std::string text;
            for (const auto& line : snapshot.diagnostics) {
                text += line.text + '\n';
            }
            ImGui::SetClipboardText(text.c_str());
        }
        if (ImGui::BeginChild("Diagnostics", {0.f, 120.f}, true)) {
            for (const auto& line : snapshot.diagnostics) {
                ImGui::TextWrapped("%s", line.text.c_str());
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.f);
            }
        }
        ImGui::EndChild();
    }

    void TrackerAdvancedPlugin::DrawHotkeyEditor(
        const char* label, const size_t index)
    {
        ImGui::PushID(static_cast<int>(index));
        ImGui::TextUnformatted(label);
        ImGui::SameLine(150.f);
        const auto capturing = capture_hotkey_index_.load() == static_cast<int>(index);
        auto open_popup = false;
        if (ImGui::Button(
                capturing ? "Recording..." : HotkeyText(hotkeys_[index]).c_str(),
                {180.f, 0.f})) {
            if (capturing) {
                CancelHotkeyCapture();
            }
            else {
                BeginHotkeyCapture(index);
                open_popup = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear")) {
            hotkeys_[index] = {};
            if (capturing) {
                CancelHotkeyCapture();
            }
        }
        ImGui::PopID();
        if (open_popup) {
            ImGui::OpenPopup("Select Tracker Hotkey");
        }
    }

    void TrackerAdvancedPlugin::DrawHotkeyCapturePopup()
    {
        const auto capture_active = capture_hotkey_index_.load() >= 0;
        if (capture_active) {
            hotkey_capture_ui_tick_.store(GetTickCount64());
        }
        if (!ImGui::BeginPopupModal(
                "Select Tracker Hotkey",
                nullptr,
                ImGuiWindowFlags_AlwaysAutoResize)) {
            if (capture_active) {
                CancelHotkeyCapture();
            }
            return;
        }
        const auto capture = capture_hotkey_index_.load();
        if (
            cancel_hotkey_capture_.exchange(false)
            || capture < 0
            || static_cast<size_t>(capture) >= hotkeys_.size()) {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        std::bitset<256> selected;
        {
            std::scoped_lock lock(hotkey_capture_mutex_);
            selected = capture_keys_selected_;
        }
        const auto binding = BindingFromKeys(selected);
        ImGui::TextWrapped(
            "Press the desired key combination. Use Clear before replacing the existing primary key.");
        ImGui::Separator();
        if (binding) {
            ImGui::Text("Selected: %s", HotkeyText(*binding).c_str());
        }
        else {
            ImGui::TextColored(
                {1.f, 0.65f, 0.2f, 1.f},
                "Select one primary key with optional Ctrl, Alt, or Shift.");
        }

        if (ImGui::Button("Clear")) {
            std::scoped_lock lock(hotkey_capture_mutex_);
            capture_keys_selected_.reset();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            CancelHotkeyCapture();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!binding.has_value());
        if (ImGui::Button("Save")) {
            for (size_t i = 0; i < hotkeys_.size(); ++i) {
                if (
                    i != static_cast<size_t>(capture)
                    && binding->key
                    && hotkeys_[i].key == binding->key
                    && hotkeys_[i].control == binding->control
                    && hotkeys_[i].alt == binding->alt
                    && hotkeys_[i].shift == binding->shift) {
                    hotkeys_[i] = {};
                }
            }
            hotkeys_[static_cast<size_t>(capture)] = *binding;
            CancelHotkeyCapture();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }
}
