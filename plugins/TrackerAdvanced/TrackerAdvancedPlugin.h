#pragma once

#include <ToolboxPlugin.h>

#include "TrackerEngine.h"
#include "TrackerPersistence.h"

#include <array>
#include <atomic>
#include <bitset>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace TrackerAdvanced {
    enum class UserCommand : uint8_t {
        start_resume,
        pause,
        split,
        undo,
        skip,
        reset,
        reset_save,
        reset_discard,
        reset_cancel,
        reset_retry,
        evaluate,
    };

    struct HotkeyBinding {
        uint32_t key = 0;
        bool control = false;
        bool alt = false;
        bool shift = false;
    };

    struct TitleProgressView {
        std::string token;
        std::string label;
        uint32_t current = 0;
        uint32_t maximum = 0;
        float fraction = 0.f;
        bool complete = false;
    };

    struct PublishedProfile {
        explicit PublishedProfile(ProfileDefinition value);

        ProfileDefinition definition;
        std::vector<FlatTaskView> tasks;
        std::unordered_map<uint32_t, const ActionDefinition*> actions;
    };

    struct RuntimeView {
        RunState state = RunState::ready;
        RunMode mode = RunMode::live;
        PauseReason pause_reason = PauseReason::invalid;
        uint64_t elapsed_ms = 0;
        uint64_t last_resolved_elapsed_ms = 0;
        size_t active_task_index = 0;
        std::vector<RuntimeTask> tasks;
        std::optional<ExperienceRuntime> experience;
        uint64_t experience_elapsed_ms = 0;
        std::vector<TitleProgressView> titles;
        bool safe_game_state = false;
        bool map_catalog_ready = false;
        std::string current_character;
        std::string status_message;
    };

    struct ResetPromptView {
        enum class Phase : uint8_t {
            none,
            choose,
            writing,
            failed,
        };

        Phase phase = Phase::none;
        bool discard = false;
        bool history_committed = false;
        std::string error;
    };

    struct DiagnosticLine {
        EngineNotification::Type type = EngineNotification::Type::info;
        std::string text;
    };

    struct PluginSnapshot {
        std::shared_ptr<const PublishedProfile> profile;
        std::shared_ptr<const HistoryFile> history;
        std::shared_ptr<const std::vector<MapCatalogEntry>> map_catalog;
        RuntimeView runtime;
        ResetPromptView reset;
        std::vector<std::filesystem::path> profile_files;
        std::vector<DiagnosticLine> diagnostics;
        std::vector<std::string> profile_errors;
        std::vector<std::string> profile_warnings;
        bool history_dirty = false;
        bool history_write_pending = false;
        bool recovery_pending = false;
        bool persistence_pending = false;
        bool persistence_retry_available = false;
        bool profile_mutation_pending = false;
    };

    class TrackerAdvancedPlugin final : public ToolboxPlugin {
    public:
        TrackerAdvancedPlugin() = default;
        ~TrackerAdvancedPlugin() override = default;

        [[nodiscard]] const char* Name() const override { return "Tracker Advanced"; }
        [[nodiscard]] bool* GetVisiblePtr() override { return &ui_enabled_; }
        [[nodiscard]] bool ShowInMainMenu() const override { return show_in_main_window_; }
        [[nodiscard]] bool ShowOnWorldMap() const override { return true; }
        [[nodiscard]] bool HasSettings() const override { return true; }

        void Initialize(ImGuiContext* context, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll) override;
        void SignalTerminate() override;
        [[nodiscard]] bool CanTerminate() override;
        void Terminate() override;
        void Update(float delta) override;
        void Draw(IDirect3DDevice9* device) override;
        bool WndProc(UINT message, WPARAM wparam, LPARAM lparam) override;
        void LoadSettings(const wchar_t* folder) override;
        void SaveSettings(const wchar_t* folder) override;
        void DrawSettings() override;
        bool DrawTabButton(bool show_icon, bool show_text, bool center_align_text) override;

        void QueueCommand(UserCommand command);
        void QueueAction(uint32_t action_id);
        void QueueProfileLoad(std::filesystem::path path);
        void QueueProfileRefresh();
        void QueueProfileSave(ProfileDefinition profile, std::filesystem::path path, bool load_after_save);
        void QueueProfileDelete(std::filesystem::path path);
        void QueueMode(RunMode mode);
        void QueueTaskTest(size_t task_index, bool evaluate_only);
        [[nodiscard]] std::shared_ptr<const PluginSnapshot> Snapshot() const;

    private:
        enum class UiRequestType : uint8_t {
            action,
            load_profile,
            refresh_profiles,
            save_profile,
            delete_profile,
            clear_profile_diagnostics,
            clear_diagnostics,
            retry_persistence,
            change_mode,
            test_task,
        };

        struct UiRequest {
            UiRequestType type = UiRequestType::action;
            uint32_t action_id = 0;
            std::filesystem::path path;
            std::optional<ProfileDefinition> profile;
            bool load_after_save = false;
            RunMode mode = RunMode::live;
            size_t task_index = 0;
            bool evaluate_only = false;
        };

        struct ResetTransaction {
            ResetPromptView::Phase phase = ResetPromptView::Phase::none;
            bool was_running = false;
            bool discard = false;
            bool history_committed = false;
            std::optional<HistoryFile> candidate_history;
            std::string attempt_id;
            std::string error;
        };

        struct PendingProfileSave {
            std::filesystem::path path;
            std::string json;
        };

        static constexpr size_t HotkeyCount = 6;

        void ProcessUserCommands();
        void ProcessUiRequests();
        void ProcessPersistenceResults();
        void ProcessEngineOutput();
        void ProcessPendingProfile();
        void ProcessHistoryReadResult(
            const TrackerPersistence::Result& result,
            bool explicit_backup);
        void ProcessActiveRunReadResult(
            const TrackerPersistence::Result& result,
            bool explicit_backup);
        void ApplyProfile(ProfileDefinition profile, const std::filesystem::path& path);
        void LoadProfile(const std::filesystem::path& path, bool bundled_fallback = false);
        [[nodiscard]] bool LoadProfileData(const TrackerPersistence::Result& result);
        void ValidateMapReferences(const ProfileDefinition& profile, ValidationResult& validation) const;
        void QueueHistoryRead(bool backup = false);
        void QueueActiveRunRead(bool backup = false);
        void QueueCheckpoint();
        void QueueHistoryWrite(std::string key);
        void QueueActiveRunDelete(std::string key);
        [[nodiscard]] bool HasPersistenceTransaction() const;
        void RetryPendingCleanup();
        void BeginReset();
        void BeginResetSave();
        void FinishResetDiscard();
        void CancelReset();
        void RetryResetSave();
        void ExecuteAction(uint32_t action_id);
        void PublishSnapshot();
        void AppendDiagnostics(std::vector<EngineNotification> notifications);
        void AddDiagnostic(EngineNotification::Type type, std::string text, bool force = false);
        [[nodiscard]] bool DiagnosticEnabled(EngineNotification::Type type) const;
        [[nodiscard]] AutomationSettings Automation() const;
        [[nodiscard]] std::filesystem::path ProfilesDirectory() const;
        [[nodiscard]] std::filesystem::path HistoryPath() const;
        [[nodiscard]] std::filesystem::path ActiveRunPath() const;
        [[nodiscard]] std::filesystem::path BundledProfilePath(const std::filesystem::path& path) const;
        [[nodiscard]] const ActionDefinition* ActiveAction(uint32_t action_id) const;
        [[nodiscard]] std::vector<TitleProgressView> BuildTitleProgress() const;
        [[nodiscard]] static uint32_t TitleMaximum(GW::Constants::TitleID title);
        [[nodiscard]] static std::string Utf8(const std::wstring& value);
        [[nodiscard]] static std::wstring Wide(const std::string& value);
        [[nodiscard]] static std::string HotkeyText(const HotkeyBinding& binding);
        [[nodiscard]] static size_t CommandIndex(UserCommand command);
        void BeginHotkeyCapture(size_t index);
        void CancelHotkeyCapture();

        void DrawObjectivesWindow(const PluginSnapshot& snapshot);
        void DrawStatsWindow(const PluginSnapshot& snapshot);
        void DrawNotesWindow(const PluginSnapshot& snapshot);
        void DrawActionsWindow(const PluginSnapshot& snapshot);
        void DrawProfileEditorWindow(const PluginSnapshot& snapshot);
        void DrawHistoryWindow(const PluginSnapshot& snapshot);
        void DrawResetModal(const PluginSnapshot& snapshot);
        void DrawDiagnostics(const PluginSnapshot& snapshot);
        void DrawHotkeyEditor(const char* label, size_t index);
        void DrawHotkeyCapturePopup();

        std::atomic_bool initialized_{false};
        std::atomic_bool game_initialized_{false};
        std::atomic_bool shutdown_requested_{false};
        std::atomic_bool shutdown_started_{false};
        std::atomic_bool persistence_shutdown_requested_{false};
        std::atomic_bool game_terminated_{false};
        bool ui_enabled_ = true;
        bool show_in_main_window_ = true;

        bool show_objectives_ = true;
        bool show_stats_ = true;
        bool show_notes_ = true;
        bool show_actions_ = true;
        bool show_profile_editor_ = false;
        bool show_history_ = false;
        bool transparent_objectives_ = false;
        bool transparent_stats_ = false;
        bool transparent_notes_ = false;
        bool transparent_actions_ = false;

        std::string selected_profile_file_ = "gwammsc.json";
        std::string profile_loader_selection_ = "gwammsc.json";
        std::string character_name_;
        bool auto_start_ = false;
        bool auto_resume_ = true;
        bool save_run_data_on_reset_ = false;
        bool prompt_before_discarding_reset_ = true;
        bool include_reset_segments_in_best_ = false;
        uint32_t checkpoint_seconds_ = 5;

        bool debug_lifecycle_ = true;
        bool debug_criteria_ = true;
        bool debug_components_ = true;

        std::array<HotkeyBinding, HotkeyCount> hotkeys_;
        std::atomic<int> capture_hotkey_index_{-1};
        std::atomic_bool cancel_hotkey_capture_{false};
        std::atomic<uint64_t> hotkey_capture_ui_tick_{0};
        std::mutex hotkey_capture_mutex_;
        std::bitset<256> capture_keys_held_;
        std::bitset<256> capture_keys_selected_;
        std::bitset<256> capture_keys_blocked_down_;
        std::bitset<256> capture_keys_suppressed_until_up_;
        std::mutex command_mutex_;
        uint32_t command_bits_ = 0;
        uint32_t queued_split_task_id_ = 0;
        uint32_t queued_skip_task_id_ = 0;

        mutable std::recursive_mutex settings_mutex_;
        mutable std::mutex ui_requests_mutex_;
        std::deque<UiRequest> ui_requests_;
        mutable std::mutex editor_mutex_;

        TrackerGame game_;
        TrackerEngine engine_;
        TrackerPersistence persistence_;
        std::filesystem::path data_root_;
        std::filesystem::path bundled_profiles_root_;
        std::filesystem::path loaded_profile_path_;
        std::optional<ProfileDefinition> pending_profile_;
        std::filesystem::path pending_profile_path_;
        bool pending_profile_from_bundle_ = false;
        bool pending_profile_preserve_backup_ = false;
        bool profile_recovery_pending_ = false;

        std::shared_ptr<const PublishedProfile> published_profile_;
        std::shared_ptr<const std::vector<MapCatalogEntry>> published_map_catalog_;
        HistoryFile history_;
        std::shared_ptr<const HistoryFile> published_history_;
        std::vector<std::filesystem::path> profile_files_;
        std::vector<std::string> profile_errors_;
        std::vector<std::string> profile_warnings_;
        std::deque<DiagnosticLine> diagnostics_;

        ResetTransaction reset_;
        bool history_dirty_ = false;
        bool history_write_removes_active_ = false;
        bool active_delete_required_ = false;
        bool preserve_history_backup_ = false;
        bool preserve_active_backup_ = false;
        std::optional<TrackerPersistence::JobId> history_write_job_;
        std::optional<TrackerPersistence::JobId> reset_history_job_;
        std::optional<TrackerPersistence::JobId> active_delete_job_;
        std::optional<TrackerPersistence::JobId> profile_load_job_;
        std::optional<TrackerPersistence::JobId> profile_delete_job_;
        std::optional<TrackerPersistence::JobId> history_read_job_;
        std::optional<TrackerPersistence::JobId> active_read_job_;
        std::filesystem::path profile_load_path_;
        std::filesystem::path pending_profile_delete_path_;
        std::unordered_map<TrackerPersistence::JobId, PendingProfileSave> pending_profile_saves_;
        uint8_t shutdown_history_attempts_ = 0;
        bool shutdown_history_abandoned_ = false;
        std::string last_status_;
        std::chrono::steady_clock::time_point next_checkpoint_{};

        std::atomic<std::shared_ptr<const PluginSnapshot>> snapshot_;

        std::optional<ProfileDefinition> editor_profile_;
        std::filesystem::path editor_path_;
        uint32_t editor_selected_objective_id_ = 0;
        uint32_t editor_selected_task_id_ = 0;
        uint32_t editor_selected_action_id_ = 0;
        bool editor_dirty_ = false;
        std::unordered_map<std::string, bool> history_visibility_;
        size_t history_baseline_index_ = 0;
        int history_graph_mode_ = 0;
        size_t test_task_index_ = 0;
    };
}
