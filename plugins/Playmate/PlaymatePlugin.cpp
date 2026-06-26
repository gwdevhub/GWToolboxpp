#include "PlaymatePlugin.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/QuestMgr.h>

#include <HttpClient.h>
#include <glaze/glaze.hpp>
#include <imgui.h>
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <format>
#include <utility>

namespace {
    PlaymatePlugin* active_plugin = nullptr;

    std::string WideToUtf8(const wchar_t* value)
    {
        if (!value || !*value) {
            return {};
        }
        const int len = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
        if (len <= 1) {
            return {};
        }
        std::string out;
        out.resize(static_cast<size_t>(len - 1));
        WideCharToMultiByte(CP_UTF8, 0, value, -1, out.data(), len, nullptr, nullptr);
        return out;
    }

    std::wstring Utf8ToWide(const std::string& value)
    {
        if (value.empty()) {
            return {};
        }
        const int len = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
        if (len <= 0) {
            return {};
        }
        std::wstring out;
        out.resize(static_cast<size_t>(len));
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), len);
        return out;
    }

    std::string ChannelName(const GW::Chat::Channel channel)
    {
        switch (channel) {
            case GW::Chat::CHANNEL_ALLIANCE: return "alliance";
            case GW::Chat::CHANNEL_ALLIES: return "allies";
            case GW::Chat::CHANNEL_ALL: return "local";
            case GW::Chat::CHANNEL_EMOTE: return "emote";
            case GW::Chat::CHANNEL_WARNING: return "warning";
            case GW::Chat::CHANNEL_GUILD: return "guild";
            case GW::Chat::CHANNEL_GLOBAL: return "global";
            case GW::Chat::CHANNEL_GROUP: return "party";
            case GW::Chat::CHANNEL_TRADE: return "trade";
            case GW::Chat::CHANNEL_ADVISORY: return "system";
            case GW::Chat::CHANNEL_WHISPER: return "whisper";
            case GW::Chat::CHANNEL_COMMAND: return "command";
            default: return "unknown";
        }
    }

    std::string TrimTrailingSlash(std::string value)
    {
        while (!value.empty() && value.back() == '/') {
            value.pop_back();
        }
        return value;
    }

    std::string PlayerMessageText(const wchar_t* raw)
    {
        if (!raw || !*raw) {
            return {};
        }
        const auto channel = GW::Chat::GetChannel(*raw);
        if (channel != GW::Chat::CHANNEL_UNKNOW) {
            return WideToUtf8(raw + 1);
        }
        return WideToUtf8(raw);
    }
}

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static PlaymatePlugin instance;
    return &instance;
}

PlaymatePlugin::PlaymatePlugin()
{
    ApplyConfig();
}

PlaymatePlugin::~PlaymatePlugin()
{
    StopWorker();
}

void PlaymatePlugin::Initialize(ImGuiContext* ctx, const ImGuiAllocFns allocator_fns, const HMODULE toolbox_dll)
{
    ToolboxUIPlugin::Initialize(ctx, allocator_fns, toolbox_dll);
    active_plugin = this;
    InitHttpClient();
    RegisterHooks();
    StartWorker();
    QueueSnapshotEvent("plugin_started");
}

void PlaymatePlugin::SignalTerminate()
{
    RemoveHooks();
    StopWorker();
    ToolboxUIPlugin::SignalTerminate();
}

void PlaymatePlugin::Terminate()
{
    RemoveHooks();
    StopWorker();
    ShutdownHttpClient();
    if (active_plugin == this) {
        active_plugin = nullptr;
    }
    ToolboxUIPlugin::Terminate();
}

bool PlaymatePlugin::CanTerminate()
{
    return !worker_.joinable();
}

void PlaymatePlugin::LoadSettings(const wchar_t* folder)
{
    ToolboxUIPlugin::LoadSettings(folder);
    std::string backend_url = backend_url_input_;
    std::string api_token = api_token_input_;
    LoadSetting("enabled", enabled_);
    LoadSetting("inject_replies", inject_replies_);
    LoadSetting("backend_url", backend_url);
    LoadSetting("api_token", api_token);
    LoadSetting("poll_interval_sec", poll_interval_sec_);
    LoadSetting("snapshot_interval_sec", snapshot_interval_sec_);

    strncpy_s(backend_url_input_, backend_url.c_str(), _TRUNCATE);
    strncpy_s(api_token_input_, api_token.c_str(), _TRUNCATE);
    poll_interval_sec_ = std::clamp(poll_interval_sec_, 0.25f, 30.0f);
    snapshot_interval_sec_ = std::clamp(snapshot_interval_sec_, 1.0f, 120.0f);
    ApplyConfig();
}

void PlaymatePlugin::SaveSettings(const wchar_t* folder)
{
    SaveSetting("enabled", enabled_);
    SaveSetting("inject_replies", inject_replies_);
    SaveSetting("backend_url", std::string(backend_url_input_));
    SaveSetting("api_token", std::string(api_token_input_));
    SaveSetting("poll_interval_sec", poll_interval_sec_);
    SaveSetting("snapshot_interval_sec", snapshot_interval_sec_);
    ToolboxUIPlugin::SaveSettings(folder);
}

void PlaymatePlugin::DrawSettings()
{
    bool config_changed = false;
    config_changed |= ImGui::Checkbox("Enable telemetry", &enabled_);
    config_changed |= ImGui::Checkbox("Inject Azele replies into party chat", &inject_replies_);
    config_changed |= ImGui::InputText("Local backend URL", backend_url_input_, sizeof(backend_url_input_));
    config_changed |= ImGui::InputText("Local API token", api_token_input_, sizeof(api_token_input_), ImGuiInputTextFlags_Password);
    config_changed |= ImGui::SliderFloat("Reply poll interval", &poll_interval_sec_, 0.25f, 10.0f, "%.2fs");
    config_changed |= ImGui::SliderFloat("Snapshot interval", &snapshot_interval_sec_, 1.0f, 60.0f, "%.0fs");
    if (config_changed) {
        ApplyConfig();
    }

    std::lock_guard lock(status_mutex_);
    ImGui::Separator();
    ImGui::Text("Status: %s", status_.c_str());
    ImGui::Text("Sent: %zu", sent_count_);
    ImGui::SameLine();
    ImGui::Text("Failed: %zu", failed_count_);
    ImGui::SameLine();
    ImGui::Text("Replies: %zu", received_count_);
}

void PlaymatePlugin::Draw(IDirect3DDevice9*)
{
    if (!GetVisiblePtr() || !*GetVisiblePtr()) {
        return;
    }
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        DrawSettings();
    }
    ImGui::End();
}

void PlaymatePlugin::Update(const float delta_ms)
{
    FlushRepliesToChat();

    if (!enabled_) {
        return;
    }

    snapshot_elapsed_ms_ += delta_ms;
    const Snapshot snapshot = BuildSnapshot();
    const bool map_changed = snapshot.map_id != 0 && snapshot.map_id != last_map_id_;
    const bool quest_changed = snapshot.active_quest_id != last_active_quest_id_;
    const bool interval_elapsed = snapshot_elapsed_ms_ >= snapshot_interval_sec_ * 1000.0f;
    if (map_changed || quest_changed || interval_elapsed) {
        QueueSnapshotEvent(map_changed ? "map_changed" : quest_changed ? "active_quest_changed" : "snapshot");
        snapshot_elapsed_ms_ = 0.0f;
        last_map_id_ = snapshot.map_id;
        last_active_quest_id_ = snapshot.active_quest_id;
    }
}

void PlaymatePlugin::RegisterHooks()
{
    GW::UI::RegisterUIMessageCallback(&send_chat_entry_, GW::UI::UIMessage::kSendChatMessage, OnSendChat, 0x8000);
    GW::UI::RegisterUIMessageCallback(&write_chat_entry_, GW::UI::UIMessage::kWriteToChatLog, OnWriteToChatLog, 0x8000);
    GW::UI::RegisterUIMessageCallback(&world_event_entry_, GW::UI::UIMessage::kMapLoaded, OnMapOrQuestEvent, 0x8000);
    GW::UI::RegisterUIMessageCallback(&world_event_entry_, GW::UI::UIMessage::kMapChange, OnMapOrQuestEvent, 0x8000);
    GW::UI::RegisterUIMessageCallback(&world_event_entry_, GW::UI::UIMessage::kQuestAdded, OnMapOrQuestEvent, 0x8000);
    GW::UI::RegisterUIMessageCallback(&world_event_entry_, GW::UI::UIMessage::kQuestDetailsChanged, OnMapOrQuestEvent, 0x8000);
}

void PlaymatePlugin::RemoveHooks()
{
    GW::UI::RemoveUIMessageCallback(&send_chat_entry_);
    GW::UI::RemoveUIMessageCallback(&write_chat_entry_);
    GW::UI::RemoveUIMessageCallback(&world_event_entry_);
}

void PlaymatePlugin::StartWorker()
{
    if (running_.exchange(true)) {
        return;
    }
    worker_ = std::thread(&PlaymatePlugin::WorkerLoop, this);
}

void PlaymatePlugin::StopWorker()
{
    if (!running_.exchange(false)) {
        return;
    }
    queue_cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void PlaymatePlugin::WorkerLoop()
{
    auto next_poll = std::chrono::steady_clock::now();
    while (running_) {
        TelemetryEvent event;
        bool has_event = false;
        {
            std::unique_lock lock(queue_mutex_);
            queue_cv_.wait_until(lock, next_poll, [&] { return !running_ || !outbound_.empty(); });
            if (!running_) {
                break;
            }
            if (!outbound_.empty()) {
                event = std::move(outbound_.front());
                outbound_.pop_front();
                has_event = true;
            }
        }

        if (has_event) {
            if (PostTelemetry(event)) {
                std::lock_guard lock(status_mutex_);
                ++sent_count_;
                status_ = "Connected";
            }
            else {
                std::lock_guard lock(status_mutex_);
                ++failed_count_;
            }
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= next_poll) {
            PollReplies();
            next_poll = now + std::chrono::milliseconds(poll_interval_ms_.load());
        }
    }
}

bool PlaymatePlugin::PostTelemetry(const TelemetryEvent& event)
{
    const auto [backend_url, token] = GetConfig();
    if (backend_url.empty()) {
        SetStatus("Backend URL is empty");
        return false;
    }

    HttpRequest request;
    request.SetUrl(EventsUrl().c_str());
    request.SetMethod(HttpMethod::Post);
    request.SetUserAgent("GWToolbox++ Playmate");
    request.SetTimeoutMs(1500);
    request.SetConnectTimeoutMs(750);
    request.SetHeader("Content-Type", "application/json");
    if (!token.empty()) {
        request.SetHeader("Authorization", ("Bearer " + token).c_str());
    }

    const auto json = glz::write_json(event).value_or(std::string{});
    request.SetPostContent(json, ContentFlag::Copy);
    const bool ok = request.Perform() && request.GetStatusCode() >= 200 && request.GetStatusCode() < 300;
    if (!ok) {
        SetStatus(std::format("POST failed: {} HTTP {}", request.GetStatusStr(), request.GetStatusCode()));
    }
    return ok;
}

void PlaymatePlugin::PollReplies()
{
    if (!telemetry_enabled_.load() || !reply_injection_enabled_.load()) {
        return;
    }

    const auto [backend_url, token] = GetConfig();
    if (backend_url.empty()) {
        return;
    }

    HttpRequest request;
    request.SetUrl(RepliesUrl().c_str());
    request.SetMethod(HttpMethod::Get);
    request.SetUserAgent("GWToolbox++ Playmate");
    request.SetTimeoutMs(1200);
    request.SetConnectTimeoutMs(500);
    if (!token.empty()) {
        request.SetHeader("Authorization", ("Bearer " + token).c_str());
    }

    if (!(request.Perform() && request.GetStatusCode() >= 200 && request.GetStatusCode() < 300)) {
        return;
    }

    auto& content = request.GetContent();
    if (content.empty()) {
        return;
    }

    RepliesResponse parsed;
    if (auto ec = glz::read_json(parsed, content); !ec) {
        for (const auto& reply : parsed.replies) {
            if (!reply.empty()) {
                QueueReply(Utf8ToWide(reply));
            }
        }
        return;
    }

    QueueReply(Utf8ToWide(content));
}

void PlaymatePlugin::QueueTelemetry(std::string event_type, std::string sender, std::string channel, std::string message)
{
    if (!telemetry_enabled_.load() || message.empty()) {
        return;
    }
    if (channel == "trade") {
        return;
    }

    const Snapshot snapshot = BuildSnapshot();
    TelemetryEvent event;
    event.event_type = std::move(event_type);
    event.sender = std::move(sender);
    event.channel = std::move(channel);
    event.message = std::move(message);
    event.map_id = snapshot.map_id;
    event.instance_type = snapshot.instance_type;
    event.district = snapshot.district;
    event.instance_time = snapshot.instance_time;
    event.active_quest_id = snapshot.active_quest_id;
    event.quest_count = snapshot.quest_count;
    event.active_quest_name = snapshot.active_quest_name;
    event.active_quest_objectives = snapshot.active_quest_objectives;

    {
        std::lock_guard lock(queue_mutex_);
        if (outbound_.size() >= 256) {
            outbound_.pop_front();
        }
        outbound_.push_back(std::move(event));
    }
    queue_cv_.notify_one();
}

void PlaymatePlugin::QueueSnapshotEvent(const char* event_type)
{
    QueueTelemetry(event_type, "System", "system", event_type);
}

void PlaymatePlugin::QueueReply(std::wstring reply)
{
    if (reply.empty()) {
        return;
    }
    {
        std::lock_guard lock(queue_mutex_);
        inbound_replies_.push_back(std::move(reply));
    }
}

void PlaymatePlugin::FlushRepliesToChat()
{
    if (!reply_injection_enabled_.load()) {
        return;
    }

    std::deque<std::wstring> replies;
    {
        std::lock_guard lock(queue_mutex_);
        replies.swap(inbound_replies_);
    }
    for (const auto& reply : replies) {
        GW::Chat::WriteChat(GW::Chat::CHANNEL_GROUP, reply.c_str(), L"Azele", true);
        std::lock_guard lock(status_mutex_);
        ++received_count_;
    }
}

void PlaymatePlugin::ApplyConfig()
{
    poll_interval_sec_ = std::clamp(poll_interval_sec_, 0.25f, 30.0f);
    snapshot_interval_sec_ = std::clamp(snapshot_interval_sec_, 1.0f, 120.0f);
    telemetry_enabled_.store(enabled_);
    reply_injection_enabled_.store(inject_replies_);
    poll_interval_ms_.store(static_cast<int>(poll_interval_sec_ * 1000.0f));
    std::lock_guard lock(config_mutex_);
    backend_url_ = TrimTrailingSlash(backend_url_input_);
    api_token_ = api_token_input_;
}

void PlaymatePlugin::SetStatus(std::string status)
{
    std::lock_guard lock(status_mutex_);
    status_ = std::move(status);
}

PlaymatePlugin::Snapshot PlaymatePlugin::BuildSnapshot() const
{
    Snapshot snapshot;
    if (!GW::Map::GetIsMapLoaded()) {
        return snapshot;
    }

    snapshot.map_id = static_cast<uint32_t>(GW::Map::GetMapID());
    snapshot.instance_type = static_cast<uint32_t>(GW::Map::GetInstanceType());
    snapshot.district = static_cast<uint32_t>(std::max(0, GW::Map::GetDistrict()));
    snapshot.instance_time = GW::Map::GetInstanceTime();
    snapshot.active_quest_id = static_cast<uint32_t>(GW::QuestMgr::GetActiveQuestId());

    const GW::QuestLog* quest_log = GW::QuestMgr::GetQuestLog();
    if (quest_log && quest_log->valid()) {
        snapshot.quest_count = quest_log->size();
    }

    if (const GW::Quest* quest = GW::QuestMgr::GetActiveQuest()) {
        snapshot.active_quest_name = WideToUtf8(quest->name);
        snapshot.active_quest_objectives = WideToUtf8(quest->objectives);
    }
    return snapshot;
}

std::pair<std::string, std::string> PlaymatePlugin::GetConfig() const
{
    std::lock_guard lock(config_mutex_);
    return {backend_url_, api_token_};
}

std::string PlaymatePlugin::EventsUrl() const
{
    const auto [backend_url, _] = GetConfig();
    return backend_url + "/v1/playmate/events";
}

std::string PlaymatePlugin::RepliesUrl() const
{
    const auto [backend_url, _] = GetConfig();
    return backend_url + "/v1/playmate/replies";
}

void PlaymatePlugin::OnSendChat(GW::HookStatus*, const GW::UI::UIMessage message_id, void* wparam, void*)
{
    if (!active_plugin || message_id != GW::UI::UIMessage::kSendChatMessage || !wparam) {
        return;
    }
    const auto* packet = static_cast<GW::UI::UIPacket::kSendChatMessage*>(wparam);
    if (!packet->message || !*packet->message) {
        return;
    }

    const auto channel = GW::Chat::GetChannel(*packet->message);
    if (channel != GW::Chat::CHANNEL_GROUP) {
        return;
    }

    active_plugin->QueueTelemetry("player_chat", "Player", ChannelName(channel), PlayerMessageText(packet->message));
}

void PlaymatePlugin::OnWriteToChatLog(GW::HookStatus*, const GW::UI::UIMessage message_id, void* wparam, void*)
{
    if (!active_plugin || message_id != GW::UI::UIMessage::kWriteToChatLog || !wparam) {
        return;
    }

    const auto* packet = static_cast<GW::UI::UIPacket::kWriteToChatLog*>(wparam);
    if (!packet->message || !*packet->message) {
        return;
    }

    const auto channel = packet->channel;
    if (channel == GW::Chat::CHANNEL_TRADE || channel == GW::Chat::CHANNEL_GROUP) {
        return;
    }
    active_plugin->QueueTelemetry("chat_log", "Game", ChannelName(channel), WideToUtf8(packet->message));
}

void PlaymatePlugin::OnMapOrQuestEvent(GW::HookStatus*, const GW::UI::UIMessage message_id, void*, void*)
{
    if (!active_plugin) {
        return;
    }

    switch (message_id) {
        case GW::UI::UIMessage::kMapLoaded:
            active_plugin->QueueSnapshotEvent("map_loaded");
            break;
        case GW::UI::UIMessage::kMapChange:
            active_plugin->QueueSnapshotEvent("map_change");
            break;
        case GW::UI::UIMessage::kQuestAdded:
            active_plugin->QueueSnapshotEvent("quest_added");
            break;
        case GW::UI::UIMessage::kQuestDetailsChanged:
            active_plugin->QueueSnapshotEvent("quest_details_changed");
            break;
        default:
            break;
    }
}
