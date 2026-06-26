#pragma once

#include <functional>
#include <windows.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Utilities/Hook.h>
#include <ToolboxUIPlugin.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class PlaymatePlugin : public ToolboxUIPlugin {
public:
    PlaymatePlugin();
    ~PlaymatePlugin() override;

    [[nodiscard]] const char* Name() const override { return "Playmate"; }
    [[nodiscard]] bool HasSettings() const override { return true; }

    void Initialize(ImGuiContext* ctx, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll) override;
    void SignalTerminate() override;
    void Terminate() override;
    bool CanTerminate() override;
    void Update(float delta_ms) override;
    void Draw(IDirect3DDevice9* device) override;
    void DrawSettings() override;
    void LoadSettings(const wchar_t* folder) override;
    void SaveSettings(const wchar_t* folder) override;

private:
    struct Snapshot {
        uint32_t map_id = 0;
        uint32_t instance_type = 0;
        uint32_t district = 0;
        uint32_t instance_time = 0;
        uint32_t active_quest_id = 0;
        uint32_t quest_count = 0;
        std::string active_quest_name;
        std::string active_quest_objectives;
    };

    struct TelemetryEvent {
        std::string source = "gwtoolboxpp-playmate";
        std::string persona = "Azele";
        std::string event_type;
        std::string sender;
        std::string channel;
        std::string message;
        uint32_t map_id = 0;
        uint32_t instance_type = 0;
        uint32_t district = 0;
        uint32_t instance_time = 0;
        uint32_t active_quest_id = 0;
        uint32_t quest_count = 0;
        std::string active_quest_name;
        std::string active_quest_objectives;
    };

    struct RepliesResponse {
        std::vector<std::string> replies;
    };

    void RegisterHooks();
    void RemoveHooks();
    void StartWorker();
    void StopWorker();
    void WorkerLoop();
    bool PostTelemetry(const TelemetryEvent& event);
    void PollReplies();
    void QueueTelemetry(std::string event_type, std::string sender, std::string channel, std::string message);
    void QueueSnapshotEvent(const char* event_type);
    void QueueReply(std::wstring reply);
    void FlushRepliesToChat();
    void ApplyConfig();
    void SetStatus(std::string status);

    [[nodiscard]] Snapshot BuildSnapshot() const;
    [[nodiscard]] std::pair<std::string, std::string> GetConfig() const;
    [[nodiscard]] std::string EventsUrl() const;
    [[nodiscard]] std::string RepliesUrl() const;

    static void OnSendChat(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void* lparam);
    static void OnWriteToChatLog(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void* lparam);
    static void OnMapOrQuestEvent(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void* lparam);

private:
    bool enabled_ = true;
    bool inject_replies_ = true;
    std::atomic_bool telemetry_enabled_ = true;
    std::atomic_bool reply_injection_enabled_ = true;
    std::atomic<int> poll_interval_ms_ = 1000;
    float poll_interval_sec_ = 1.0f;
    float snapshot_interval_sec_ = 8.0f;
    char backend_url_input_[256] = "http://127.0.0.1:8787";
    char api_token_input_[160] = "";

    mutable std::mutex config_mutex_;
    std::string backend_url_ = "http://127.0.0.1:8787";
    std::string api_token_;

    std::atomic_bool running_ = false;
    std::thread worker_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::deque<TelemetryEvent> outbound_;
    std::deque<std::wstring> inbound_replies_;

    mutable std::mutex status_mutex_;
    std::string status_ = "Idle";
    size_t sent_count_ = 0;
    size_t failed_count_ = 0;
    size_t received_count_ = 0;

    float snapshot_elapsed_ms_ = 0.0f;
    uint32_t last_map_id_ = 0;
    uint32_t last_active_quest_id_ = 0;

    GW::HookEntry send_chat_entry_;
    GW::HookEntry write_chat_entry_;
    GW::HookEntry world_event_entry_;
};
