#include "TrackerAdvancedPlugin.h"

#include <GWCA/GameEntities/Title.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/PlayerMgr.h>

#include <algorithm>
#include <bit>
#include <format>
#include <ranges>
#include <unordered_set>
#include <utility>

namespace {
    constexpr size_t MaxDiagnostics = 500;
    constexpr uint64_t HotkeyCaptureUiTimeoutMs = 1'000;

    std::string SafeFileStem(std::string value)
    {
        for (auto& character : value) {
            const auto valid =
                std::isalnum(static_cast<unsigned char>(character)) != 0
                || character == '-'
                || character == '_';
            if (!valid) {
                character = '_';
            }
        }
        return value.empty() ? "profile" : value;
    }

    const char* CriterionMapName(const TrackerAdvanced::CriterionDefinition& criterion)
    {
        using TrackerAdvanced::CriterionType;
        switch (criterion.type) {
            case CriterionType::map_loaded:
            case CriterionType::vanquish_complete:
                return criterion.map ? criterion.map->c_str() : nullptr;
            case CriterionType::mission_complete:
                return criterion.mission ? criterion.mission->c_str() : nullptr;
            case CriterionType::dungeon_complete:
                return criterion.dungeon ? criterion.dungeon->c_str() : nullptr;
            case CriterionType::player_level:
            case CriterionType::title_progress:
            case CriterionType::manual:
            case CriterionType::invalid:
                return nullptr;
        }
        return nullptr;
    }

    TrackerAdvanced::MapUsage CriterionMapUsage(
        const TrackerAdvanced::CriterionDefinition& criterion)
    {
        using TrackerAdvanced::CriterionType;
        using TrackerAdvanced::MapUsage;
        switch (criterion.type) {
            case CriterionType::mission_complete: return MapUsage::Mission;
            case CriterionType::dungeon_complete: return MapUsage::Dungeon;
            case CriterionType::vanquish_complete: return MapUsage::Vanquish;
            case CriterionType::map_loaded:
            case CriterionType::player_level:
            case CriterionType::title_progress:
            case CriterionType::manual:
            case CriterionType::invalid:
                return MapUsage::Any;
        }
        return MapUsage::Any;
    }

    uint32_t HotkeyKeyFromMessage(const UINT message, const WPARAM wparam)
    {
        switch (message) {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYUP:
                return static_cast<uint32_t>(wparam);
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
                return VK_MBUTTON;
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
                return GET_XBUTTON_WPARAM(wparam) == XBUTTON1
                    ? VK_XBUTTON1
                    : VK_XBUTTON2;
            default:
                return 0;
        }
    }

    bool IsHotkeyDownMessage(const UINT message)
    {
        return message == WM_KEYDOWN
            || message == WM_SYSKEYDOWN
            || message == WM_MBUTTONDOWN
            || message == WM_XBUTTONDOWN;
    }

    bool IsHotkeyUpMessage(const UINT message)
    {
        return message == WM_KEYUP
            || message == WM_SYSKEYUP
            || message == WM_MBUTTONUP
            || message == WM_XBUTTONUP;
    }
}

#ifndef DBBOX_BUILD
DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static TrackerAdvanced::TrackerAdvancedPlugin instance;
    return &instance;
}
#endif

namespace TrackerAdvanced {
    PublishedProfile::PublishedProfile(ProfileDefinition value)
        : definition(std::move(value))
        , tasks(FlattenTasks(definition))
    {
        for (const auto& action : definition.actions) {
            actions.emplace(action.id, &action);
        }
    }

    void TrackerAdvancedPlugin::Initialize(
        ImGuiContext* context,
        const ImGuiAllocFns allocator_fns,
        const HMODULE toolbox_dll)
    {
        ToolboxPlugin::Initialize(context, allocator_fns, toolbox_dll);
        wchar_t module_path[MAX_PATH]{};
        const auto length = GetModuleFileNameW(plugin_handle, module_path, _countof(module_path));
        if (length && length < _countof(module_path)) {
            bundled_profiles_root_ =
                std::filesystem::path(module_path).parent_path() / L"TrackerAdvancedProfiles";
        }
        next_checkpoint_ = std::chrono::steady_clock::now();
        initialized_.store(true);
    }

    void TrackerAdvancedPlugin::SignalTerminate()
    {
        shutdown_requested_.store(true);
        CancelHotkeyCapture();
        if (!initialized_.load()) {
            shutdown_started_.store(true);
            game_terminated_.store(true);
            if (!persistence_shutdown_requested_.exchange(true)) {
                persistence_.ShutdownAndWait();
            }
        }
    }

    bool TrackerAdvancedPlugin::CanTerminate()
    {
        return shutdown_started_.load()
            && game_terminated_.load()
            && persistence_shutdown_requested_.load()
            && persistence_.IsShutdownComplete();
    }

    void TrackerAdvancedPlugin::Terminate()
    {
        CancelHotkeyCapture();
        if (game_initialized_.load() && !game_terminated_.load()) {
            game_.Terminate();
            game_terminated_.store(true);
        }
        // Join before FreeLibrary so DLL teardown never waits on this worker under the loader lock.
        persistence_shutdown_requested_.store(true);
        persistence_.ShutdownAndWait();
        initialized_.store(false);
        ToolboxPlugin::Terminate();
    }

    void TrackerAdvancedPlugin::Update(float)
    {
        if (!initialized_.load()) {
            return;
        }
        if (
            capture_hotkey_index_.load() >= 0
            && GetTickCount64() - hotkey_capture_ui_tick_.load()
                > HotkeyCaptureUiTimeoutMs) {
            CancelHotkeyCapture();
        }

        if (!game_initialized_.load() && !shutdown_requested_.load()) {
            game_.Initialize();
            game_initialized_.store(true);
            PublishSnapshot();
        }

        ProcessPersistenceResults();
        if (!shutdown_requested_.load()) {
            ProcessPendingProfile();
            ProcessUiRequests();
            if (!game_initialized_.load()) {
                return;
            }
            if (!profile_recovery_pending_) {
                auto automation = Automation();
                if (HasPersistenceTransaction()) {
                    automation.auto_start = false;
                    automation.auto_resume = false;
                }
                engine_.Update(game_, automation);
                ProcessEngineOutput();
                ProcessUserCommands();
                engine_.RefreshHooks(game_, automation);
                ProcessEngineOutput();
            }
            const auto now = std::chrono::steady_clock::now();
            uint32_t checkpoint_seconds;
            {
                std::scoped_lock lock(settings_mutex_);
                checkpoint_seconds = checkpoint_seconds_;
            }
            if (
                !profile_recovery_pending_
                &&
                (engine_.State() == RunState::running || engine_.State() == RunState::paused)
                && now >= next_checkpoint_) {
                QueueCheckpoint();
                next_checkpoint_ = now + std::chrono::seconds(std::max(1u, checkpoint_seconds));
            }
        }
        else {
            if (!shutdown_started_.exchange(true)) {
                if (game_initialized_.load()) {
                    engine_.DisarmHooks(game_);
                    QueueCheckpoint();
                }
            }
            if (
                history_dirty_
                && !history_write_job_
                && shutdown_history_attempts_ < 3) {
                ++shutdown_history_attempts_;
                QueueHistoryWrite("history");
            }
            if (
                history_dirty_
                && !history_write_job_
                && shutdown_history_attempts_ >= 3
                && !shutdown_history_abandoned_) {
                shutdown_history_abandoned_ = true;
                AddDiagnostic(
                    EngineNotification::Type::error,
                    "History could not be saved during shutdown; the active-run checkpoint was preserved for recovery.",
                    true);
            }
            if (
                (!history_dirty_ || shutdown_history_abandoned_)
                &&
                !persistence_shutdown_requested_.load()
                && persistence_.RequestShutdownIfIdle()) {
                persistence_shutdown_requested_.store(true);
            }
        }

        if (
            shutdown_requested_.load()
            && game_initialized_.load()
            && !game_terminated_.load()
            && game_.IsMapCatalogReady()) {
            game_.Terminate();
            game_terminated_.store(true);
        }
        else if (shutdown_requested_.load() && !game_initialized_.load()) {
            game_terminated_.store(true);
        }
        if (game_initialized_.load() && !game_terminated_.load()) {
            PublishSnapshot();
        }
    }

    bool TrackerAdvancedPlugin::WndProc(
        const UINT message, const WPARAM wparam, const LPARAM lparam)
    {
        if (!initialized_.load() || shutdown_requested_.load()) {
            return false;
        }
        if (message == WM_ACTIVATE) {
            if (LOWORD(wparam) == WA_INACTIVE) {
                std::scoped_lock lock(hotkey_capture_mutex_);
                capture_keys_suppressed_until_up_ |=
                    capture_keys_held_ & capture_keys_blocked_down_;
                capture_keys_held_.reset();
                capture_keys_blocked_down_.reset();
            }
            return false;
        }
        const auto key = HotkeyKeyFromMessage(message, wparam);
        const auto input_available =
            GetForegroundWindow() == GW::MemoryMgr::GetGWWindowHandle()
            && !GW::Chat::GetIsTyping();
        auto capture_event = false;
        auto block_event = false;
        auto cancel_capture = false;
        {
            std::scoped_lock lock(hotkey_capture_mutex_);
            if (
                key
                && key < capture_keys_suppressed_until_up_.size()
                && capture_keys_suppressed_until_up_.test(key)) {
                if (IsHotkeyUpMessage(message)) {
                    capture_keys_suppressed_until_up_.reset(key);
                }
                return true;
            }
            const auto capture = capture_hotkey_index_.load();
            if (!input_available && capture >= 0) {
                if (
                    key
                    && IsHotkeyUpMessage(message)
                    && capture_keys_blocked_down_.test(key)) {
                    capture_keys_held_.reset(key);
                    capture_keys_blocked_down_.reset(key);
                    block_event = true;
                }
                capture_keys_suppressed_until_up_ |=
                    capture_keys_held_ & capture_keys_blocked_down_;
                capture_keys_held_.reset();
                capture_keys_blocked_down_.reset();
            }
            else if (
                input_available
                && capture >= 0
                && key
                && key < capture_keys_held_.size()) {
                capture_event = true;
                const auto key_down = IsHotkeyDownMessage(message);
                const auto was_held = capture_keys_held_.test(key);
                const auto was_down_before_capture =
                    !was_held
                    && (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
                    && (static_cast<uintptr_t>(lparam) & (1u << 30)) != 0;
                capture_keys_held_.set(key, key_down);
                if (key_down) {
                    capture_keys_selected_ |= capture_keys_held_;
                    if (!was_held && !was_down_before_capture) {
                        capture_keys_blocked_down_.set(key);
                    }
                    block_event = capture_keys_blocked_down_.test(key);
                    cancel_capture = key == VK_ESCAPE;
                }
                else {
                    block_event = capture_keys_blocked_down_.test(key);
                    capture_keys_blocked_down_.reset(key);
                }
            }
        }
        if (!input_available) {
            return block_event;
        }
        if (capture_event) {
            if (cancel_capture) {
                CancelHotkeyCapture();
            }
            return block_event;
        }
        if (
            !IsHotkeyDownMessage(message)
            || ((message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
                && (static_cast<uintptr_t>(lparam) & (1u << 30)) != 0)) {
            return false;
        }
        const auto key_down = key;
        const auto control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const auto alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        const auto shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (
            ImGui::GetIO().WantTextInput
            || ImGui::GetIO().WantCaptureKeyboard) {
            return false;
        }
        constexpr std::array Commands{
            UserCommand::start_resume,
            UserCommand::pause,
            UserCommand::split,
            UserCommand::undo,
            UserCommand::skip,
            UserCommand::reset,
        };
        std::scoped_lock lock(settings_mutex_);
        for (size_t i = 0; i < hotkeys_.size(); ++i) {
            const auto& binding = hotkeys_[i];
            if (
                binding.key == key_down
                && binding.control == control
                    && binding.alt == alt
                    && binding.shift == shift) {
                QueueCommand(Commands[i]);
                break;
            }
        }
        return false;
    }

    void TrackerAdvancedPlugin::LoadSettings(const wchar_t* folder)
    {
        CancelHotkeyCapture();
        std::scoped_lock lock(settings_mutex_);
        ToolboxPlugin::LoadSettings(folder);
        LoadSetting("ui_enabled", ui_enabled_);
        LoadSetting("show_in_main_window", show_in_main_window_);
        LoadSetting("show_objectives", show_objectives_);
        LoadSetting("show_stats", show_stats_);
        LoadSetting("show_notes", show_notes_);
        LoadSetting("show_actions", show_actions_);
        LoadSetting("show_profile_editor", show_profile_editor_);
        LoadSetting("show_history", show_history_);
        LoadSetting("transparent_objectives", transparent_objectives_);
        LoadSetting("transparent_stats", transparent_stats_);
        LoadSetting("transparent_notes", transparent_notes_);
        LoadSetting("transparent_actions", transparent_actions_);
        LoadSetting("selected_profile_file", selected_profile_file_);
        LoadSetting("character_name", character_name_);
        LoadSetting("auto_start", auto_start_);
        LoadSetting("auto_resume", auto_resume_);
        LoadSetting("save_run_data_on_reset", save_run_data_on_reset_);
        LoadSetting("prompt_before_discarding_reset", prompt_before_discarding_reset_);
        LoadSetting("include_reset_segments_in_best", include_reset_segments_in_best_);
        LoadSetting("checkpoint_seconds", checkpoint_seconds_);
        LoadSetting("debug_lifecycle", debug_lifecycle_);
        LoadSetting("debug_criteria", debug_criteria_);
        LoadSetting("debug_components", debug_components_);
        LoadSetting("hotkeys", hotkeys_);

        for (size_t current = 0; current < hotkeys_.size(); ++current) {
            if (!hotkeys_[current].key) {
                continue;
            }
            for (size_t previous = 0; previous < current; ++previous) {
                if (
                    hotkeys_[current].key == hotkeys_[previous].key
                    && hotkeys_[current].control == hotkeys_[previous].control
                    && hotkeys_[current].alt == hotkeys_[previous].alt
                    && hotkeys_[current].shift == hotkeys_[previous].shift) {
                    hotkeys_[current] = {};
                    break;
                }
            }
        }
        checkpoint_seconds_ = std::clamp(checkpoint_seconds_, 1u, 60u);
        data_root_ = std::filesystem::path(folder) / L"TrackerAdvanced";
        auto selected = std::filesystem::path(Wide(selected_profile_file_)).filename();
        if (selected.empty()) {
            selected = L"gwammsc.json";
        }
        selected_profile_file_ = Utf8(selected.wstring());
        profile_loader_selection_ = selected_profile_file_;
        static_cast<void>(persistence_.EnqueueListJsonFiles(
            ProfilesDirectory(), "profiles-list"));
        LoadProfile(ProfilesDirectory() / selected);
    }

    void TrackerAdvancedPlugin::SaveSettings(const wchar_t* folder)
    {
        std::scoped_lock lock(settings_mutex_);
        SaveSetting("ui_enabled", ui_enabled_);
        SaveSetting("show_in_main_window", show_in_main_window_);
        SaveSetting("show_objectives", show_objectives_);
        SaveSetting("show_stats", show_stats_);
        SaveSetting("show_notes", show_notes_);
        SaveSetting("show_actions", show_actions_);
        SaveSetting("show_profile_editor", show_profile_editor_);
        SaveSetting("show_history", show_history_);
        SaveSetting("transparent_objectives", transparent_objectives_);
        SaveSetting("transparent_stats", transparent_stats_);
        SaveSetting("transparent_notes", transparent_notes_);
        SaveSetting("transparent_actions", transparent_actions_);
        SaveSetting("selected_profile_file", selected_profile_file_);
        SaveSetting("character_name", character_name_);
        SaveSetting("auto_start", auto_start_);
        SaveSetting("auto_resume", auto_resume_);
        SaveSetting("save_run_data_on_reset", save_run_data_on_reset_);
        SaveSetting("prompt_before_discarding_reset", prompt_before_discarding_reset_);
        SaveSetting("include_reset_segments_in_best", include_reset_segments_in_best_);
        SaveSetting("checkpoint_seconds", checkpoint_seconds_);
        SaveSetting("debug_lifecycle", debug_lifecycle_);
        SaveSetting("debug_criteria", debug_criteria_);
        SaveSetting("debug_components", debug_components_);
        SaveSetting("hotkeys", hotkeys_);
        ToolboxPlugin::SaveSettings(folder);
    }

    void TrackerAdvancedPlugin::QueueCommand(const UserCommand command)
    {
        const auto snapshot = Snapshot();
        if (snapshot && snapshot->recovery_pending) {
            return;
        }
        std::scoped_lock lock(command_mutex_);
        if (
            (command == UserCommand::split || command == UserCommand::skip)
            && snapshot && snapshot->profile
            && snapshot->runtime.active_task_index < snapshot->profile->tasks.size()
            && snapshot->runtime.active_task_index < snapshot->runtime.tasks.size()
            && snapshot->runtime.tasks[snapshot->runtime.active_task_index].status
                == RuntimeTaskStatus::active) {
            const auto task_id =
                snapshot->profile->tasks[snapshot->runtime.active_task_index].task->id;
            if (command == UserCommand::split) {
                queued_split_task_id_ = task_id;
            }
            else {
                queued_skip_task_id_ = task_id;
            }
        }
        command_bits_ |= 1u << CommandIndex(command);
    }

    void TrackerAdvancedPlugin::QueueAction(const uint32_t action_id)
    {
        std::scoped_lock lock(ui_requests_mutex_);
        ui_requests_.push_back({
            .type = UiRequestType::action,
            .action_id = action_id,
        });
    }

    void TrackerAdvancedPlugin::QueueProfileLoad(std::filesystem::path path)
    {
        std::scoped_lock lock(ui_requests_mutex_);
        ui_requests_.push_back({
            .type = UiRequestType::load_profile,
            .path = std::move(path),
        });
    }

    void TrackerAdvancedPlugin::QueueProfileRefresh()
    {
        std::scoped_lock lock(ui_requests_mutex_);
        ui_requests_.push_back({
            .type = UiRequestType::refresh_profiles,
        });
    }

    void TrackerAdvancedPlugin::BeginHotkeyCapture(const size_t index)
    {
        if (index >= hotkeys_.size()) {
            return;
        }
        std::scoped_lock lock(hotkey_capture_mutex_);
        capture_keys_held_.reset();
        capture_keys_selected_.reset();
        capture_keys_blocked_down_.reset();
        capture_keys_held_.set(VK_CONTROL, (GetKeyState(VK_CONTROL) & 0x8000) != 0);
        capture_keys_held_.set(VK_MENU, (GetKeyState(VK_MENU) & 0x8000) != 0);
        capture_keys_held_.set(VK_SHIFT, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
        const auto& binding = hotkeys_[index];
        if (binding.key < capture_keys_selected_.size()) {
            capture_keys_selected_.set(binding.key, binding.key != 0);
        }
        capture_keys_selected_.set(VK_CONTROL, binding.control);
        capture_keys_selected_.set(VK_MENU, binding.alt);
        capture_keys_selected_.set(VK_SHIFT, binding.shift);
        capture_keys_selected_ |= capture_keys_held_;
        cancel_hotkey_capture_.store(false);
        hotkey_capture_ui_tick_.store(GetTickCount64());
        capture_hotkey_index_.store(static_cast<int>(index));
    }

    void TrackerAdvancedPlugin::CancelHotkeyCapture()
    {
        cancel_hotkey_capture_.store(true);
        hotkey_capture_ui_tick_.store(0);
        std::scoped_lock lock(hotkey_capture_mutex_);
        capture_hotkey_index_.store(-1);
        capture_keys_suppressed_until_up_ |=
            capture_keys_held_ & capture_keys_blocked_down_;
        capture_keys_held_.reset();
        capture_keys_selected_.reset();
        capture_keys_blocked_down_.reset();
    }

    void TrackerAdvancedPlugin::QueueProfileSave(
        ProfileDefinition profile,
        std::filesystem::path path,
        const bool load_after_save)
    {
        std::scoped_lock lock(ui_requests_mutex_);
        ui_requests_.push_back({
            .type = UiRequestType::save_profile,
            .path = std::move(path),
            .profile = std::move(profile),
            .load_after_save = load_after_save,
        });
    }

    void TrackerAdvancedPlugin::QueueProfileDelete(std::filesystem::path path)
    {
        std::scoped_lock lock(ui_requests_mutex_);
        ui_requests_.push_back({
            .type = UiRequestType::delete_profile,
            .path = std::move(path),
        });
    }

    void TrackerAdvancedPlugin::QueueMode(const RunMode mode)
    {
        std::scoped_lock lock(ui_requests_mutex_);
        ui_requests_.push_back({
            .type = UiRequestType::change_mode,
            .mode = mode,
        });
    }

    void TrackerAdvancedPlugin::QueueTaskTest(
        const size_t task_index,
        const bool evaluate_only)
    {
        std::scoped_lock lock(ui_requests_mutex_);
        ui_requests_.push_back({
            .type = UiRequestType::test_task,
            .task_index = task_index,
            .evaluate_only = evaluate_only,
        });
    }

    std::shared_ptr<const PluginSnapshot> TrackerAdvancedPlugin::Snapshot() const
    {
        return snapshot_.load();
    }

    void TrackerAdvancedPlugin::ProcessUserCommands()
    {
        uint32_t bits;
        uint32_t split_task_id;
        uint32_t skip_task_id;
        {
            std::scoped_lock lock(command_mutex_);
            bits = std::exchange(command_bits_, 0);
            split_task_id = std::exchange(queued_split_task_id_, 0);
            skip_task_id = std::exchange(queued_skip_task_id_, 0);
        }
        for (size_t i = 0; i <= CommandIndex(UserCommand::evaluate); ++i) {
            if ((bits & (1u << i)) == 0) {
                continue;
            }
            const auto command = static_cast<UserCommand>(i);
            ControlResult result{true, {}};
            const auto reset_transaction_command =
                command == UserCommand::reset_save
                || command == UserCommand::reset_discard
                || command == UserCommand::reset_cancel
                || command == UserCommand::reset_retry;
            if (HasPersistenceTransaction() && !reset_transaction_command) {
                if (
                    command == UserCommand::start_resume
                    || command == UserCommand::reset) {
                    RetryPendingCleanup();
                }
                last_status_ =
                    "Run controls are waiting for the current persistence transaction to finish.";
                continue;
            }
            switch (command) {
                case UserCommand::start_resume:
                    result = engine_.State() == RunState::paused
                        ? engine_.Resume(game_, Automation())
                        : engine_.Start(game_, Automation());
                    break;
                case UserCommand::pause:
                    result = engine_.Pause(game_);
                    break;
                case UserCommand::split: {
                    const auto active = engine_.ActiveTask();
                    if (!active || !split_task_id || active->task->id != split_task_id) {
                        result = {
                            false,
                            "The Task advanced before the queued Split; the command was ignored.",
                        };
                    }
                    else {
                        result = engine_.Split(game_);
                    }
                    break;
                }
                case UserCommand::undo:
                    result = engine_.Undo(game_);
                    break;
                case UserCommand::skip: {
                    const auto active = engine_.ActiveTask();
                    if (!active || !skip_task_id || active->task->id != skip_task_id) {
                        result = {
                            false,
                            "The Task advanced before the queued Skip; the command was ignored.",
                        };
                    }
                    else {
                        result = engine_.Skip(game_);
                    }
                    break;
                }
                case UserCommand::reset:
                    BeginReset();
                    break;
                case UserCommand::reset_save:
                    BeginResetSave();
                    break;
                case UserCommand::reset_discard:
                    FinishResetDiscard();
                    break;
                case UserCommand::reset_cancel:
                    CancelReset();
                    break;
                case UserCommand::reset_retry:
                    RetryResetSave();
                    break;
                case UserCommand::evaluate:
                    result = engine_.EvaluateCurrentTask(game_);
                    break;
            }
            ProcessEngineOutput();
            if (!result && !result.message.empty()) {
                last_status_ = result.message;
            }
        }
    }

    void TrackerAdvancedPlugin::ProcessUiRequests()
    {
        std::deque<UiRequest> requests;
        {
            std::scoped_lock lock(ui_requests_mutex_);
            requests.swap(ui_requests_);
        }
        const auto profile_change_blocked = [this] {
            return engine_.State() == RunState::running
                || engine_.State() == RunState::paused
                || HasPersistenceTransaction();
        };
        for (auto& request : requests) {
            switch (request.type) {
                case UiRequestType::action:
                    ExecuteAction(request.action_id);
                    break;
                case UiRequestType::load_profile:
                    if (profile_change_blocked()) {
                        last_status_ =
                            "Profile loading is unavailable while a run or persistence transaction is active.";
                    }
                    else {
                        LoadProfile(ProfilesDirectory() / request.path.filename());
                    }
                    break;
                case UiRequestType::refresh_profiles:
                    static_cast<void>(persistence_.EnqueueListJsonFiles(
                        ProfilesDirectory(), "profiles-list:refresh"));
                    last_status_ = "Refreshing run profiles...";
                    break;
                case UiRequestType::save_profile: {
                    if (!request.profile) {
                        break;
                    }
                    if (profile_change_blocked()) {
                        last_status_ =
                            "Profile saving is unavailable while a run or persistence transaction is active.";
                        break;
                    }
                    auto validation = ValidateProfile(*request.profile);
                    if (game_.IsMapCatalogReady()) {
                        ValidateMapReferences(*request.profile, validation);
                    }
                    profile_errors_ = validation.errors;
                    profile_warnings_ = validation.warnings;
                    if (!validation.Ok()) {
                        last_status_ = "Profile validation failed; no file was written.";
                        break;
                    }
                    const auto json = WriteProfile(*request.profile);
                    if (!json) {
                        last_status_ = json.error();
                        break;
                    }
                    const auto filename = request.path.filename();
                    const auto key = std::string("profile-save:")
                        + Utf8(filename.wstring())
                        + (request.load_after_save ? ":load" : "");
                    const auto path = ProfilesDirectory() / filename;
                    const auto job = persistence_.EnqueueWrite(path, *json, key);
                    pending_profile_saves_[job] = {
                        .path = path,
                        .json = *json,
                    };
                    break;
                }
                case UiRequestType::delete_profile:
                    if (profile_change_blocked()) {
                        last_status_ =
                            "Profile deletion is unavailable while a run or persistence transaction is active.";
                    }
                    else {
                        pending_profile_delete_path_ =
                            ProfilesDirectory() / request.path.filename();
                        profile_delete_job_ = persistence_.EnqueueDelete(
                            pending_profile_delete_path_, "profile-delete");
                    }
                    break;
                case UiRequestType::clear_profile_diagnostics:
                    profile_errors_.clear();
                    profile_warnings_.clear();
                    break;
                case UiRequestType::clear_diagnostics:
                    diagnostics_.clear();
                    break;
                case UiRequestType::retry_persistence:
                    RetryPendingCleanup();
                    break;
                case UiRequestType::change_mode:
                    if (HasPersistenceTransaction()) {
                        last_status_ =
                            "Mode changes are unavailable while a persistence transaction is active.";
                    }
                    else {
                        engine_.SetMode(request.mode, game_);
                        diagnostics_.clear();
                    }
                    break;
                case UiRequestType::test_task: {
                    if (HasPersistenceTransaction()) {
                        last_status_ =
                            "Task testing is unavailable while a persistence transaction is active.";
                        break;
                    }
                    const auto result = request.evaluate_only
                        ? engine_.EvaluateTaskForTest(request.task_index, game_)
                        : engine_.StartTaskTest(
                            request.task_index, game_, Automation());
                    if (!result && !result.message.empty()) {
                        last_status_ = result.message;
                    }
                    break;
                }
            }
        }
    }

    void TrackerAdvancedPlugin::ProcessPersistenceResults()
    {
        for (auto& result : persistence_.DrainResults()) {
            if (result.status == TrackerPersistence::ResultStatus::Superseded) {
                pending_profile_saves_.erase(result.id);
                continue;
            }
            if (result.key == "profiles-list" || result.key == "profiles-list:refresh") {
                if (result.Succeeded()) {
                    profile_files_ = std::move(result.files);
                    std::filesystem::path loaded_profile_path;
                    {
                        std::scoped_lock lock(editor_mutex_);
                        loaded_profile_path = loaded_profile_path_;
                    }
                    {
                        std::scoped_lock lock(settings_mutex_);
                        const auto selection_path =
                            std::filesystem::path(Wide(profile_loader_selection_)).filename();
                        const auto selection_exists = std::ranges::any_of(
                            profile_files_,
                            [&selection_path](const auto& path) {
                                return _wcsicmp(
                                    path.filename().c_str(),
                                    selection_path.c_str()) == 0;
                            });
                        if (!selection_exists) {
                            const auto loaded = loaded_profile_path.filename();
                            const auto loaded_exists = std::ranges::any_of(
                                profile_files_,
                                [&loaded](const auto& path) {
                                    return _wcsicmp(
                                        path.filename().c_str(),
                                        loaded.c_str()) == 0;
                                });
                            profile_loader_selection_ = loaded_exists
                                ? Utf8(loaded.wstring())
                                : profile_files_.empty()
                                    ? std::string{}
                                    : Utf8(profile_files_.front().filename().wstring());
                        }
                    }
                    if (result.key == "profiles-list:refresh") {
                        last_status_ = std::format(
                            "Found {} run profile{}.",
                            profile_files_.size(),
                            profile_files_.size() == 1 ? "" : "s");
                    }
                }
                else {
                    if (result.key == "profiles-list:refresh") {
                        last_status_ = "Could not refresh run profiles.";
                    }
                    AddDiagnostic(
                        EngineNotification::Type::error, result.message, true);
                }
                continue;
            }
            if (result.key.starts_with("profile:data:")) {
                if (!profile_load_job_ || result.id != *profile_load_job_) {
                    continue;
                }
                if (result.Succeeded()) {
                    if (LoadProfileData(result)) {
                        profile_load_job_.reset();
                    }
                    else if (!result.UsedBackup()) {
                        auto backup = result.path;
                        backup += L".bak";
                        const auto key = std::string("profile:backup:")
                            + Utf8(profile_load_path_.filename().wstring());
                        profile_load_job_ = persistence_.EnqueueRead(backup, key);
                        AddDiagnostic(
                            EngineNotification::Type::warning,
                            "The primary profile failed schema validation; trying its backup.",
                            true);
                    }
                    else {
                        profile_load_job_.reset();
                    }
                }
                else if (result.WasNotFound()) {
                    LoadProfile(result.path, true);
                }
                else {
                    profile_load_job_.reset();
                    profile_errors_ = {result.message};
                    last_status_ = "Could not read the selected profile.";
                    AddDiagnostic(EngineNotification::Type::error, result.message, true);
                }
                continue;
            }
            if (result.key.starts_with("profile:bundle:")) {
                if (!profile_load_job_ || result.id != *profile_load_job_) {
                    continue;
                }
                if (result.Succeeded()) {
                    static_cast<void>(LoadProfileData(result));
                }
                else {
                    profile_errors_ = {result.message};
                    last_status_ = "Could not load the selected profile.";
                }
                profile_load_job_.reset();
                continue;
            }
            if (result.key.starts_with("profile:backup:")) {
                if (!profile_load_job_ || result.id != *profile_load_job_) {
                    continue;
                }
                if (result.Succeeded()) {
                    static_cast<void>(LoadProfileData(result));
                }
                else {
                    profile_errors_.push_back(result.message);
                    last_status_ = "Neither the selected profile nor its backup is valid.";
                }
                profile_load_job_.reset();
                continue;
            }
            if (result.key.starts_with("profile-save:")) {
                const auto pending_save = pending_profile_saves_.find(result.id);
                if (!result.Succeeded()) {
                    last_status_ = result.message;
                    AddDiagnostic(EngineNotification::Type::error, result.message, true);
                }
                else {
                    static_cast<void>(persistence_.EnqueueListJsonFiles(
                        ProfilesDirectory(), "profiles-list"));
                    last_status_ = "Profile saved.";
                    if (result.HasWarning()) {
                        AddDiagnostic(
                            EngineNotification::Type::warning, result.message, true);
                    }
                    if (result.key.ends_with(":load")) {
                        LoadProfile(result.path);
                    }
                    if (pending_save != pending_profile_saves_.end()) {
                        std::scoped_lock lock(editor_mutex_);
                        if (
                            editor_profile_
                            && editor_path_.filename() == pending_save->second.path.filename()) {
                            const auto current = WriteProfile(*editor_profile_);
                            if (current && *current == pending_save->second.json) {
                                editor_dirty_ = false;
                            }
                        }
                    }
                }
                pending_profile_saves_.erase(result.id);
                continue;
            }
            if (result.key == "profile-install") {
                if (result.Succeeded()) {
                    static_cast<void>(persistence_.EnqueueListJsonFiles(
                        ProfilesDirectory(), "profiles-list"));
                    if (result.HasWarning()) {
                        AddDiagnostic(
                            EngineNotification::Type::warning, result.message, true);
                    }
                }
                else {
                    AddDiagnostic(EngineNotification::Type::warning, result.message, true);
                }
                continue;
            }
            if (result.key == "profile-delete") {
                if (!profile_delete_job_ || result.id != *profile_delete_job_) {
                    continue;
                }
                profile_delete_job_.reset();
                if (!result.Succeeded()) {
                    last_status_ = result.message;
                }
                else {
                    auto deleted_loaded_profile = false;
                    auto deleted_editor_profile = false;
                    {
                        std::scoped_lock lock(editor_mutex_);
                        deleted_loaded_profile =
                            !loaded_profile_path_.empty()
                            && loaded_profile_path_.filename()
                                == pending_profile_delete_path_.filename();
                        deleted_editor_profile =
                            !editor_path_.empty()
                            && editor_path_.filename()
                                == pending_profile_delete_path_.filename();
                        if (deleted_loaded_profile) {
                            loaded_profile_path_.clear();
                        }
                        if (deleted_loaded_profile) {
                            editor_profile_.reset();
                            editor_path_.clear();
                            editor_selected_objective_id_ = 0;
                            editor_selected_task_id_ = 0;
                            editor_selected_action_id_ = 0;
                            editor_dirty_ = false;
                        }
                        else if (deleted_editor_profile) {
                            if (published_profile_) {
                                editor_profile_ = published_profile_->definition;
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
                        }
                    }
                    if (deleted_loaded_profile) {
                        engine_.DisarmHooks(game_);
                        engine_.ClearProfile();
                        published_profile_.reset();
                        history_ = {};
                        published_history_ = std::make_shared<HistoryFile>(history_);
                        {
                            std::scoped_lock lock(settings_mutex_);
                            selected_profile_file_.clear();
                        }
                        profile_errors_.clear();
                        profile_warnings_.clear();
                        last_status_ = "Deleted the loaded profile; select or create another profile.";
                    }
                    else {
                        if (deleted_editor_profile) {
                            profile_errors_.clear();
                            profile_warnings_.clear();
                        }
                        last_status_ = "Profile deleted.";
                    }
                }
                pending_profile_delete_path_.clear();
                static_cast<void>(persistence_.EnqueueListJsonFiles(
                    ProfilesDirectory(), "profiles-list"));
                continue;
            }
            if (result.key == "history-read" || result.key == "history-read-backup") {
                if (!history_read_job_ || result.id != *history_read_job_) {
                    continue;
                }
                history_read_job_.reset();
                ProcessHistoryReadResult(
                    result, result.key == "history-read-backup");
                continue;
            }
            if (result.key == "active-read" || result.key == "active-read-backup") {
                if (!active_read_job_ || result.id != *active_read_job_) {
                    continue;
                }
                active_read_job_.reset();
                ProcessActiveRunReadResult(
                    result, result.key == "active-read-backup");
                continue;
            }
            if (result.key == "history-reset") {
                if (!reset_history_job_ || result.id != *reset_history_job_) {
                    continue;
                }
                reset_history_job_.reset();
                if (!result.Succeeded()) {
                    reset_.phase = ResetPromptView::Phase::failed;
                    reset_.error = result.message;
                    continue;
                }
                if (reset_.candidate_history) {
                    history_ = std::move(*reset_.candidate_history);
                    published_history_ = std::make_shared<HistoryFile>(history_);
                }
                history_dirty_ = false;
                preserve_history_backup_ = false;
                reset_.history_committed = true;
                QueueActiveRunDelete("active-delete-reset");
                if (result.HasWarning()) {
                    AddDiagnostic(
                        EngineNotification::Type::warning, result.message, true);
                }
                continue;
            }
            if (result.key == "history") {
                if (!history_write_job_ || result.id != *history_write_job_) {
                    continue;
                }
                history_write_job_.reset();
                if (!result.Succeeded()) {
                    history_dirty_ = true;
                    AddDiagnostic(EngineNotification::Type::error, result.message, true);
                }
                else {
                    history_dirty_ = false;
                    preserve_history_backup_ = false;
                    if (result.HasWarning()) {
                        AddDiagnostic(
                            EngineNotification::Type::warning, result.message, true);
                    }
                    if (history_write_removes_active_) {
                        QueueActiveRunDelete("active-delete");
                        history_write_removes_active_ = false;
                    }
                }
                continue;
            }
            if (
                result.key == "active-delete"
                || result.key == "active-delete-reset") {
                if (!active_delete_job_ || result.id != *active_delete_job_) {
                    continue;
                }
                active_delete_job_.reset();
                if (!result.Succeeded()) {
                    if (result.key == "active-delete-reset") {
                        reset_.phase = ResetPromptView::Phase::failed;
                        reset_.error = result.message;
                    }
                    else {
                        last_status_ =
                            "Active-run cleanup failed; retry it before starting another run.";
                    }
                    AddDiagnostic(EngineNotification::Type::error, result.message, true);
                }
                else {
                    active_delete_required_ = false;
                    if (result.HasWarning()) {
                        AddDiagnostic(
                            EngineNotification::Type::warning,
                            result.message,
                            true);
                    }
                    if (result.key == "active-delete-reset") {
                        const auto saved = reset_.history_committed;
                        engine_.ResetFresh(game_);
                        reset_ = {};
                        last_status_ = saved
                            ? "Saved the reset attempt and started a fresh run."
                            : "Discarded the active attempt and started a fresh run.";
                    }
                }
                continue;
            }
            if (result.key == "checkpoint") {
                if (!result.Succeeded()) {
                    AddDiagnostic(EngineNotification::Type::error, result.message, true);
                }
                else if (result.HasWarning()) {
                    preserve_active_backup_ = false;
                    AddDiagnostic(
                        EngineNotification::Type::warning, result.message, true);
                }
                else {
                    preserve_active_backup_ = false;
                }
                continue;
            }
            if (!result.Succeeded()) {
                AddDiagnostic(EngineNotification::Type::error, result.message, true);
            }
        }
    }

    void TrackerAdvancedPlugin::ProcessEngineOutput()
    {
        AppendDiagnostics(engine_.DrainNotifications());
        if (engine_.CheckpointRequested()) {
            QueueCheckpoint();
        }
        if (auto attempt = engine_.TakeFinishedAttempt()) {
            history_.profile_id = published_profile_
                ? published_profile_->definition.id
                : std::string{};
            history_.attempts.push_back(std::move(*attempt));
            published_history_ = std::make_shared<HistoryFile>(history_);
            history_dirty_ = true;
            history_write_removes_active_ = true;
            QueueHistoryWrite("history");
        }
        if (engine_.ActiveRunRemovalRequested() && !history_write_removes_active_) {
            QueueActiveRunDelete("active-delete");
        }
    }

    void TrackerAdvancedPlugin::ProcessPendingProfile()
    {
        if (
            !pending_profile_
            || !game_.IsMapCatalogReady()
            || profile_recovery_pending_) {
            return;
        }
        if (engine_.State() == RunState::running || engine_.State() == RunState::paused) {
            pending_profile_.reset();
            pending_profile_from_bundle_ = false;
            pending_profile_preserve_backup_ = false;
            last_status_ = "Profile loading was cancelled because a run became active.";
            return;
        }
        if (
            history_dirty_
            || history_write_job_
            || active_delete_required_
            || active_delete_job_
            || profile_delete_job_
            || reset_.phase != ResetPromptView::Phase::none) {
            return;
        }
        auto validation = ValidateProfile(*pending_profile_);
        ValidateMapReferences(*pending_profile_, validation);
        profile_errors_ = validation.errors;
        profile_warnings_ = validation.warnings;
        if (!validation.Ok()) {
            std::scoped_lock lock(editor_mutex_);
            if (!editor_dirty_) {
                editor_profile_ = *pending_profile_;
                editor_path_ = pending_profile_path_;
            }
            pending_profile_.reset();
            pending_profile_preserve_backup_ = false;
            last_status_ = "Profile validation failed. Open the Run Profile Editor for details.";
            return;
        }
        auto profile = std::move(*pending_profile_);
        const auto path = pending_profile_path_;
        const auto from_bundle = pending_profile_from_bundle_;
        const auto preserve_backup = pending_profile_preserve_backup_;
        pending_profile_.reset();
        pending_profile_from_bundle_ = false;
        pending_profile_preserve_backup_ = false;
        ApplyProfile(profile, path);
        if (from_bundle) {
            if (const auto json = WriteProfile(profile)) {
                static_cast<void>(persistence_.EnqueueWrite(
                    ProfilesDirectory() / path.filename(), *json, "profile-install"));
            }
        }
        else if (preserve_backup) {
            if (const auto json = WriteProfile(profile)) {
                static_cast<void>(persistence_.EnqueueWrite(
                    ProfilesDirectory() / path.filename(),
                    *json,
                    "profile-repair",
                    true));
            }
        }
    }

    void TrackerAdvancedPlugin::ApplyProfile(
        ProfileDefinition profile, const std::filesystem::path& path)
    {
        engine_.DisarmHooks(game_);
        published_profile_ = std::make_shared<PublishedProfile>(profile);
        engine_.SetProfile(std::move(profile));
        const auto loaded_path = ProfilesDirectory() / path.filename();
        {
            std::scoped_lock lock(settings_mutex_);
            selected_profile_file_ = Utf8(path.filename().wstring());
            profile_loader_selection_ = selected_profile_file_;
        }
        history_ = {
            .profile_id = published_profile_->definition.id,
        };
        published_history_ = std::make_shared<HistoryFile>(history_);
        {
            std::scoped_lock lock(editor_mutex_);
            loaded_profile_path_ = loaded_path;
            if (!editor_dirty_) {
                editor_profile_ = published_profile_->definition;
                editor_path_ = loaded_profile_path_;
                editor_selected_objective_id_ = 0;
                editor_selected_task_id_ = 0;
                editor_selected_action_id_ = 0;
            }
        }
        profile_recovery_pending_ = true;
        history_write_removes_active_ = false;
        preserve_history_backup_ = false;
        preserve_active_backup_ = false;
        QueueHistoryRead();
        uint32_t checkpoint_seconds;
        {
            std::scoped_lock lock(settings_mutex_);
            checkpoint_seconds = checkpoint_seconds_;
        }
        next_checkpoint_ = std::chrono::steady_clock::now()
            + std::chrono::seconds(std::max(1u, checkpoint_seconds));
        last_status_ = "Loaded profile '" + published_profile_->definition.name + "'.";
    }

    void TrackerAdvancedPlugin::LoadProfile(
        const std::filesystem::path& path, const bool bundled_fallback)
    {
        const auto filename = path.filename();
        const auto key = std::string(bundled_fallback ? "profile:bundle:" : "profile:data:")
            + Utf8(filename.wstring());
        const auto source = bundled_fallback ? BundledProfilePath(filename) : ProfilesDirectory() / filename;
        profile_load_path_ = ProfilesDirectory() / filename;
        profile_load_job_ = persistence_.EnqueueRead(source, key);
    }

    bool TrackerAdvancedPlugin::LoadProfileData(
        const TrackerPersistence::Result& result)
    {
        auto profile = ParseProfile(result.json);
        if (!profile) {
            profile_errors_ = {profile.error()};
            last_status_ = "The selected profile is invalid.";
            return false;
        }
        profile_errors_.clear();
        pending_profile_ = std::move(*profile);
        pending_profile_path_ = profile_load_path_;
        pending_profile_from_bundle_ = result.key.starts_with("profile:bundle:");
        pending_profile_preserve_backup_ =
            result.key.starts_with("profile:backup:");
        if (result.UsedBackup()) {
            AddDiagnostic(EngineNotification::Type::warning, result.message, true);
        }
        return true;
    }

    void TrackerAdvancedPlugin::ProcessHistoryReadResult(
        const TrackerPersistence::Result& result,
        const bool explicit_backup)
    {
        if (result.Succeeded()) {
            auto parsed = ParseHistory(result.json);
            const auto matches_profile =
                parsed
                && published_profile_
                && parsed->profile_id == published_profile_->definition.id;
            if (matches_profile) {
                history_ = std::move(*parsed);
                preserve_history_backup_ = explicit_backup;
                if (explicit_backup || result.UsedBackup()) {
                    AddDiagnostic(
                        EngineNotification::Type::warning,
                        explicit_backup
                            ? "Recovered run history from its schema-valid backup."
                            : result.message,
                        true);
                }
            }
            else {
                const auto error = parsed
                    ? std::string("History belongs to a different profile.")
                    : parsed.error();
                if (!explicit_backup && !result.UsedBackup()) {
                    AddDiagnostic(
                        EngineNotification::Type::warning,
                        "Primary history failed schema validation; trying its backup. "
                            + error,
                        true);
                    QueueHistoryRead(true);
                    return;
                }
                AddDiagnostic(EngineNotification::Type::error, error, true);
            }
        }
        else if (!result.WasNotFound()) {
            AddDiagnostic(EngineNotification::Type::error, result.message, true);
        }
        published_history_ = std::make_shared<HistoryFile>(history_);
        if (preserve_history_backup_) {
            QueueHistoryWrite("history");
        }
        QueueActiveRunRead();
    }

    void TrackerAdvancedPlugin::ProcessActiveRunReadResult(
        const TrackerPersistence::Result& result,
        const bool explicit_backup)
    {
        if (result.Succeeded()) {
            auto parsed = ParseActiveRun(result.json);
            if (!parsed) {
                if (!explicit_backup && !result.UsedBackup()) {
                    AddDiagnostic(
                        EngineNotification::Type::warning,
                        "Primary recovery state failed schema validation; trying its backup. "
                            + parsed.error(),
                        true);
                    QueueActiveRunRead(true);
                    return;
                }
                AddDiagnostic(EngineNotification::Type::error, parsed.error(), true);
            }
            else {
                const auto already_recorded = std::ranges::any_of(
                    history_.attempts,
                    [&parsed](const HistoryAttempt& attempt) {
                        return attempt.id == parsed->attempt_id;
                    });
                if (already_recorded) {
                    QueueActiveRunDelete("active-delete");
                    AddDiagnostic(
                        EngineNotification::Type::warning,
                        "Ignored stale recovery state for an attempt already saved in history.",
                        true);
                }
                else {
                    const auto restored = engine_.Restore(*parsed, game_);
                    if (!restored) {
                        if (!explicit_backup && !result.UsedBackup()) {
                            AddDiagnostic(
                                EngineNotification::Type::warning,
                                "Primary recovery state does not match this route; trying its backup. "
                                    + restored.message,
                                true);
                            QueueActiveRunRead(true);
                            return;
                        }
                        AddDiagnostic(
                            EngineNotification::Type::warning,
                            restored.message,
                            true);
                    }
                    else if (explicit_backup) {
                        preserve_active_backup_ = true;
                    }
                }
                if (explicit_backup || result.UsedBackup()) {
                    AddDiagnostic(
                        EngineNotification::Type::warning,
                        explicit_backup
                            ? "Recovered active-run state from its schema-valid backup."
                            : result.message,
                        true);
                }
            }
        }
        else if (!result.WasNotFound()) {
            AddDiagnostic(EngineNotification::Type::warning, result.message, true);
        }
        profile_recovery_pending_ = false;
    }

    void TrackerAdvancedPlugin::ValidateMapReferences(
        const ProfileDefinition& profile, ValidationResult& validation) const
    {
        const auto validate = [&](const CriterionDefinition& criterion, const std::string& path) {
            const auto name = CriterionMapName(criterion);
            if (name && !game_.ResolveMapId(name, CriterionMapUsage(criterion))) {
                validation.errors.push_back(
                    path + ": decoded English map name '" + name
                    + "' does not resolve to an appropriate Guild Wars map");
            }
        };
        for (size_t objective_index = 0; objective_index < profile.objectives.size(); ++objective_index) {
            const auto& objective = profile.objectives[objective_index];
            for (size_t task_index = 0; task_index < objective.tasks.size(); ++task_index) {
                const auto& task = objective.tasks[task_index];
                const auto path = std::format(
                    "objectives[{}].tasks[{}]", objective_index, task_index);
                validate(task.end_criterion, path + ".end_criterion");
                if (task.experience_tracker) {
                    validate(
                        task.experience_tracker->arm_criterion,
                        path + ".experience_tracker.arm_criterion");
                    validate(
                        task.experience_tracker->increment_criterion,
                        path + ".experience_tracker.increment_criterion");
                }
            }
        }
    }

    void TrackerAdvancedPlugin::QueueHistoryRead(const bool backup)
    {
        auto path = HistoryPath();
        if (backup) {
            path += L".bak";
        }
        history_read_job_ = persistence_.EnqueueRead(
            path, backup ? "history-read-backup" : "history-read");
    }

    void TrackerAdvancedPlugin::QueueActiveRunRead(const bool backup)
    {
        auto path = ActiveRunPath();
        if (backup) {
            path += L".bak";
        }
        active_read_job_ = persistence_.EnqueueRead(
            path, backup ? "active-read-backup" : "active-read");
    }

    void TrackerAdvancedPlugin::QueueCheckpoint()
    {
        if (reset_.phase == ResetPromptView::Phase::writing) {
            return;
        }
        const auto active = engine_.BuildActiveRunFile();
        if (!active) {
            return;
        }
        const auto json = WriteActiveRun(*active);
        if (!json) {
            AddDiagnostic(EngineNotification::Type::error, json.error(), true);
            return;
        }
        static_cast<void>(persistence_.EnqueueWrite(
            ActiveRunPath(),
            *json,
            "checkpoint",
            preserve_active_backup_));
    }

    void TrackerAdvancedPlugin::QueueHistoryWrite(std::string key)
    {
        const auto json = WriteHistory(history_);
        if (!json) {
            history_dirty_ = true;
            AddDiagnostic(EngineNotification::Type::error, json.error(), true);
            return;
        }
        history_write_job_ = persistence_.EnqueueWrite(
            HistoryPath(),
            *json,
            std::move(key),
            preserve_history_backup_);
    }

    void TrackerAdvancedPlugin::QueueActiveRunDelete(std::string key)
    {
        active_delete_required_ = true;
        if (active_delete_job_) {
            return;
        }
        active_delete_job_ =
            persistence_.EnqueueDelete(ActiveRunPath(), std::move(key));
    }

    bool TrackerAdvancedPlugin::HasPersistenceTransaction() const
    {
        return profile_recovery_pending_
            || profile_load_job_.has_value()
            || pending_profile_.has_value()
            || profile_delete_job_.has_value()
            || !pending_profile_saves_.empty()
            || history_read_job_.has_value()
            || active_read_job_.has_value()
            || history_dirty_
            || history_write_job_.has_value()
            || reset_history_job_.has_value()
            || history_write_removes_active_
            || active_delete_required_
            || active_delete_job_.has_value()
            || reset_.phase != ResetPromptView::Phase::none;
    }

    void TrackerAdvancedPlugin::RetryPendingCleanup()
    {
        if (history_dirty_ && !history_write_job_) {
            QueueHistoryWrite("history");
        }
        if (
            active_delete_required_
            && !active_delete_job_
            && reset_.phase == ResetPromptView::Phase::none) {
            QueueActiveRunDelete("active-delete");
        }
    }

    void TrackerAdvancedPlugin::BeginReset()
    {
        if (reset_.phase != ResetPromptView::Phase::none) {
            return;
        }
        bool save_on_reset;
        bool prompt_before_discard;
        {
            std::scoped_lock lock(settings_mutex_);
            save_on_reset = save_run_data_on_reset_;
            prompt_before_discard = prompt_before_discarding_reset_;
        }
        if (
            engine_.Mode() == RunMode::test
            || engine_.State() == RunState::ready
            || engine_.State() == RunState::completed) {
            engine_.ResetFresh(game_);
            return;
        }
        reset_.was_running = engine_.State() == RunState::running;
        if (reset_.was_running) {
            static_cast<void>(engine_.Pause(game_));
            if (engine_.CheckpointRequested()) {
                QueueCheckpoint();
            }
        }
        if (!engine_.HasUnsavedRunData()) {
            FinishResetDiscard();
        }
        else if (save_on_reset) {
            BeginResetSave();
        }
        else if (prompt_before_discard) {
            reset_.phase = ResetPromptView::Phase::choose;
        }
        else {
            FinishResetDiscard();
        }
    }

    void TrackerAdvancedPlugin::BeginResetSave()
    {
        if (reset_.phase == ResetPromptView::Phase::writing) {
            return;
        }
        reset_.discard = false;
        if (reset_.history_committed) {
            reset_.phase = ResetPromptView::Phase::writing;
            reset_.error.clear();
            QueueActiveRunDelete("active-delete-reset");
            return;
        }
        if (engine_.State() == RunState::running) {
            reset_.was_running = true;
            static_cast<void>(engine_.Pause(game_));
            if (engine_.CheckpointRequested()) {
                QueueCheckpoint();
            }
        }
        const auto attempt = engine_.BuildResetAttempt();
        if (!attempt) {
            FinishResetDiscard();
            return;
        }
        auto candidate = history_;
        candidate.profile_id = published_profile_
            ? published_profile_->definition.id
            : std::string{};
        candidate.attempts.push_back(*attempt);
        const auto json = WriteHistory(candidate);
        if (!json) {
            reset_.phase = ResetPromptView::Phase::failed;
            reset_.error = json.error();
            return;
        }
        reset_.candidate_history = std::move(candidate);
        reset_.attempt_id = attempt->id;
        reset_.phase = ResetPromptView::Phase::writing;
        reset_.error.clear();
        reset_history_job_ = persistence_.EnqueueWrite(
            HistoryPath(),
            *json,
            "history-reset",
            preserve_history_backup_);
    }

    void TrackerAdvancedPlugin::FinishResetDiscard()
    {
        if (reset_.phase == ResetPromptView::Phase::writing) {
            return;
        }
        if (engine_.State() == RunState::running) {
            reset_.was_running = true;
            static_cast<void>(engine_.Pause(game_));
            if (engine_.CheckpointRequested()) {
                QueueCheckpoint();
            }
        }
        if (reset_.history_committed) {
            reset_.discard = false;
        }
        else {
            reset_.discard = true;
        }
        reset_.phase = ResetPromptView::Phase::writing;
        reset_.error.clear();
        QueueActiveRunDelete("active-delete-reset");
        last_status_ = "Deleting active-run recovery data before reset...";
    }

    void TrackerAdvancedPlugin::CancelReset()
    {
        if (reset_.phase == ResetPromptView::Phase::writing) {
            return;
        }
        if (reset_.history_committed) {
            last_status_ =
                "The reset attempt is already in history; retry cleanup to finish resetting.";
            return;
        }
        const auto resume = reset_.was_running;
        reset_ = {};
        active_delete_required_ = false;
        if (resume) {
            const auto result = engine_.Resume(game_, Automation());
            if (!result) {
                last_status_ = result.message;
            }
        }
    }

    void TrackerAdvancedPlugin::RetryResetSave()
    {
        if (reset_.phase != ResetPromptView::Phase::failed) {
            return;
        }
        if (reset_.discard || reset_.history_committed) {
            reset_.phase = ResetPromptView::Phase::writing;
            reset_.error.clear();
            QueueActiveRunDelete("active-delete-reset");
        }
        else {
            BeginResetSave();
        }
    }

    void TrackerAdvancedPlugin::ExecuteAction(const uint32_t action_id)
    {
        const auto action = ActiveAction(action_id);
        if (!action) {
            last_status_ = "That action is not attached to the active Task.";
            return;
        }
        std::string command;
        switch (action->type) {
            case ActionType::travel:
                if (action->destination) {
                    command = "tp " + *action->destination;
                }
                break;
            case ActionType::player_build:
                if (action->team_build && action->build) {
                    command = std::format(
                        "loadbuild \"{}\" \"{}\"", *action->team_build, *action->build);
                }
                break;
            case ActionType::hero_team_build:
                if (action->name) {
                    command = "heroteam " + *action->name;
                }
                break;
            case ActionType::invalid:
                break;
        }
        if (command.empty()) {
            last_status_ = "The selected action is incomplete.";
            return;
        }
        GW::Chat::SendChat('/', command.c_str());
        last_status_ = "Executed /" + command;
    }

    void TrackerAdvancedPlugin::PublishSnapshot()
    {
        auto next = std::make_shared<PluginSnapshot>();
        if (!published_map_catalog_ && game_.IsMapCatalogReady()) {
            published_map_catalog_ =
                std::make_shared<std::vector<MapCatalogEntry>>(game_.MapCatalog());
        }
        next->profile = published_profile_;
        next->history = published_history_;
        next->map_catalog = published_map_catalog_;
        next->runtime.state = engine_.State();
        next->runtime.mode = engine_.Mode();
        next->runtime.pause_reason = engine_.CurrentPauseReason();
        next->runtime.elapsed_ms = engine_.ElapsedMs();
        next->runtime.last_resolved_elapsed_ms = engine_.LastResolvedElapsedMs();
        next->runtime.active_task_index = engine_.ActiveTaskIndex();
        next->runtime.tasks = engine_.TaskRuntime();
        if (const auto experience = engine_.ActiveExperience()) {
            next->runtime.experience = *experience;
            next->runtime.experience_elapsed_ms = engine_.ActiveExperienceElapsedMs();
        }
        next->runtime.titles = BuildTitleProgress();
        next->runtime.safe_game_state = game_.IsSafeGameState();
        next->runtime.map_catalog_ready = game_.IsMapCatalogReady();
        next->runtime.current_character = Utf8(game_.CurrentCharacterName());
        next->runtime.status_message = last_status_;
        next->reset = {
            .phase = reset_.phase,
            .discard = reset_.discard,
            .history_committed = reset_.history_committed,
            .error = reset_.error,
        };
        next->profile_files = profile_files_;
        next->diagnostics.assign(diagnostics_.begin(), diagnostics_.end());
        next->profile_errors = profile_errors_;
        next->profile_warnings = profile_warnings_;
        next->history_dirty = history_dirty_;
        next->history_write_pending = history_write_job_.has_value();
        next->recovery_pending = profile_recovery_pending_;
        next->profile_mutation_pending =
            profile_load_job_.has_value()
            || pending_profile_.has_value()
            || profile_recovery_pending_
            || profile_delete_job_.has_value()
            || !pending_profile_saves_.empty();
        next->persistence_pending = HasPersistenceTransaction();
        next->persistence_retry_available =
            (history_dirty_ && !history_write_job_)
            || (active_delete_required_
                && !active_delete_job_
                && reset_.phase == ResetPromptView::Phase::none);
        snapshot_.store(std::move(next));
    }

    void TrackerAdvancedPlugin::AppendDiagnostics(
        std::vector<EngineNotification> notifications)
    {
        for (auto& notification : notifications) {
            AddDiagnostic(notification.type, std::move(notification.text));
        }
    }

    void TrackerAdvancedPlugin::AddDiagnostic(
        const EngineNotification::Type type, std::string text, const bool force)
    {
        if (!force && !DiagnosticEnabled(type)) {
            return;
        }
        if (diagnostics_.size() == MaxDiagnostics) {
            diagnostics_.pop_front();
        }
        diagnostics_.push_back({type, std::move(text)});
    }

    bool TrackerAdvancedPlugin::DiagnosticEnabled(
        const EngineNotification::Type type) const
    {
        std::scoped_lock lock(settings_mutex_);
        if (engine_.Mode() != RunMode::test) {
            return type == EngineNotification::Type::error;
        }
        switch (type) {
            case EngineNotification::Type::lifecycle: return debug_lifecycle_;
            case EngineNotification::Type::criterion: return debug_criteria_;
            case EngineNotification::Type::component: return debug_components_;
            case EngineNotification::Type::warning:
            case EngineNotification::Type::error:
            case EngineNotification::Type::info:
                return true;
        }
        return true;
    }

    AutomationSettings TrackerAdvancedPlugin::Automation() const
    {
        std::scoped_lock lock(settings_mutex_);
        return {
            .character_name = Wide(character_name_),
            .auto_start = auto_start_,
            .auto_resume = auto_resume_,
        };
    }

    std::filesystem::path TrackerAdvancedPlugin::ProfilesDirectory() const
    {
        return data_root_ / L"profiles";
    }

    std::filesystem::path TrackerAdvancedPlugin::HistoryPath() const
    {
        const auto id = published_profile_
            ? published_profile_->definition.id
            : std::string("profile");
        return data_root_ / L"history" / Wide(SafeFileStem(id) + ".json");
    }

    std::filesystem::path TrackerAdvancedPlugin::ActiveRunPath() const
    {
        return data_root_ / L"active_run.json";
    }

    std::filesystem::path TrackerAdvancedPlugin::BundledProfilePath(
        const std::filesystem::path& path) const
    {
        return bundled_profiles_root_ / path.filename();
    }

    const ActionDefinition* TrackerAdvancedPlugin::ActiveAction(
        const uint32_t action_id) const
    {
        if (!published_profile_ || !engine_.ActiveTask()) {
            return nullptr;
        }
        const auto& task = *engine_.ActiveTask()->task;
        if (std::ranges::find(task.action_ids, action_id) == task.action_ids.end()) {
            return nullptr;
        }
        const auto found = published_profile_->actions.find(action_id);
        return found == published_profile_->actions.end() ? nullptr : found->second;
    }

    std::vector<TitleProgressView> TrackerAdvancedPlugin::BuildTitleProgress() const
    {
        std::vector<TitleProgressView> result;
        if (!published_profile_ || !game_.IsSafeGameState()) {
            return result;
        }
        std::vector<TitleTrackerDefinition> trackers = published_profile_->definition.title_trackers;
        std::optional<std::pair<std::string, uint32_t>> active_requirement;
        if (const auto active = engine_.ActiveTask()) {
            trackers.insert(
                trackers.end(),
                active->objective->title_trackers.begin(),
                active->objective->title_trackers.end());
            if (
                active->task->end_criterion.type == CriterionType::title_progress
                && active->task->end_criterion.title
                && active->task->end_criterion.required_progress) {
                active_requirement = std::pair{
                    *active->task->end_criterion.title,
                    *active->task->end_criterion.required_progress,
                };
                trackers.push_back({.title = *active->task->end_criterion.title});
            }
        }
        std::unordered_set<std::string> seen;
        for (const auto& tracker : trackers) {
            if (!seen.insert(tracker.title).second) {
                continue;
            }
            const auto supported = FindTitle(tracker.title);
            if (!supported) {
                continue;
            }
            const auto title = GW::PlayerMgr::GetTitleTrack(supported->id);
            const auto current = title ? title->current_points : 0;
            auto maximum = TitleMaximum(supported->id);
            if (!maximum && title && title->points_needed_next_rank != UINT32_MAX) {
                maximum = title->points_needed_next_rank;
            }
            const auto complete =
                active_requirement
                    && active_requirement->first == tracker.title
                ? current >= active_requirement->second
                : maximum != 0 && current >= maximum
                    || title && title->points_needed_next_rank == UINT32_MAX;
            if (tracker.hide_when_complete && complete) {
                continue;
            }
            result.push_back({
                .token = tracker.title,
                .label = std::string(supported->label),
                .current = current,
                .maximum = maximum,
                .fraction = maximum
                    ? std::clamp(
                        static_cast<float>(current) / static_cast<float>(maximum), 0.f, 1.f)
                    : 0.f,
                .complete = complete,
            });
        }
        return result;
    }

    uint32_t TrackerAdvancedPlugin::TitleMaximum(const GW::Constants::TitleID title)
    {
        using GW::Constants::TitleID;
        switch (title) {
            case TitleID::TyrianCarto:
            case TitleID::CanthanCarto:
            case TitleID::ElonianCarto:
            case TitleID::MasterOfTheNorth:
                return 1'000;
            case TitleID::ProtectorTyria:
            case TitleID::GuardianTyria:
                return 25;
            case TitleID::ProtectorCantha:
            case TitleID::GuardianCantha:
                return 13;
            case TitleID::ProtectorElona:
            case TitleID::GuardianElona:
            case TitleID::LDoA:
                return 20;
            case TitleID::LegendaryVanquisher:
            case TitleID::LegendaryCarto:
            case TitleID::LegendarySkillHunter:
                return 3;
            case TitleID::LegendaryGuardian:
                return 6;
            case TitleID::Drunkard:
            case TitleID::Party:
            case TitleID::Sweets:
                return 10'000;
            case TitleID::Survivor:
                return 1'337'500;
            case TitleID::KoaBD:
                return 30;
            case TitleID::Lightbringer:
            case TitleID::Sunspear:
                return 50'000;
            case TitleID::Asuran:
            case TitleID::Deldrimor:
            case TitleID::Vanguard:
            case TitleID::Norn:
                return 160'000;
            default:
                return 0;
        }
    }

    std::string TrackerAdvancedPlugin::Utf8(const std::wstring& value)
    {
        if (value.empty()) {
            return {};
        }
        const auto size = WideCharToMultiByte(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0,
            nullptr,
            nullptr);
        if (size <= 0) {
            return {};
        }
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            result.data(),
            size,
            nullptr,
            nullptr);
        return result;
    }

    std::wstring TrackerAdvancedPlugin::Wide(const std::string& value)
    {
        if (value.empty()) {
            return {};
        }
        const auto size = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0);
        if (size <= 0) {
            return {};
        }
        std::wstring result(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            result.data(),
            size);
        return result;
    }

    std::string TrackerAdvancedPlugin::HotkeyText(const HotkeyBinding& binding)
    {
        if (!binding.key) {
            return "Unbound";
        }
        std::string result;
        if (binding.control) result += "Ctrl+";
        if (binding.alt) result += "Alt+";
        if (binding.shift) result += "Shift+";
        switch (binding.key) {
            case VK_MBUTTON:
                return result + "Middle Mouse";
            case VK_XBUTTON1:
                return result + "Mouse 4";
            case VK_XBUTTON2:
                return result + "Mouse 5";
            default:
                break;
        }
        const auto scan = MapVirtualKeyW(binding.key, MAPVK_VK_TO_VSC) << 16;
        wchar_t name[64]{};
        if (GetKeyNameTextW(static_cast<LONG>(scan), name, _countof(name))) {
            result += Utf8(name);
        }
        else {
            result += std::format("VK {}", binding.key);
        }
        return result;
    }

    size_t TrackerAdvancedPlugin::CommandIndex(const UserCommand command)
    {
        return static_cast<size_t>(command);
    }
}
