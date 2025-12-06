#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Utils/GuiUtils.h>
#include <Defines.h>

#include <Widgets/LatencyWidget.h>

#include "Utils/FontLoader.h"
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Utilities/Hooker.h>

namespace {
    GW::HookEntry ChatCmd_HookEntry;
    constexpr size_t ping_history_len = 10; // GW checks last 10 pings for avg
    uint32_t ping_history[ping_history_len] = {0};
    size_t ping_index = 0;

    GW::HookEntry ContextCallback_Entry;
    GW::HookEntry Ping_Entry;
    int red_threshold = 250;
    bool show_avg_ping = false;
    int font_size = 0;
    float text_size = 40.f;

    GW::HookEntry ChatCommand_Hook;

    struct LatencyFrameContext {
        void* vtable;
        uint32_t frame_id;
        void* h0008;
        uint32_t frame_id_dupe;
        GW::Array<uint32_t> h0010;
        uint32_t average_ping;
        uint32_t h0024;
        uint32_t current_ping;
        uint32_t h002C;
        uint32_t state;
        uint32_t h0034;
    };
    static_assert(sizeof(LatencyFrameContext) == 0x38);

    LatencyFrameContext* cached_frame_context = nullptr;

    GW::UI::UIInteractionCallback OnLatencyFrame_UICallback_Func = 0, OnLatencyFrame_UICallback_Ret = 0;
    void OnLatencyFrame_UICallback(GW::UI::InteractionMessage* message, void* wparam, void* lparam)
    {
        GW::Hook::EnterHook();
        OnLatencyFrame_UICallback_Ret(message, wparam, lparam);
        switch (message->message_id) {
            case GW::UI::UIMessage::kDestroyFrame: {
                cached_frame_context = 0;
            } break;
            default: {
                if (!cached_frame_context && message->wParam)
                    cached_frame_context = *(LatencyFrameContext**)message->wParam;
            } break;
        }
        GW::Hook::LeaveHook();
    }

    LatencyFrameContext* GetLatencyFrameContext() {
        if (OnLatencyFrame_UICallback_Func) return cached_frame_context;
        const auto frame = GW::UI::GetFrameByLabel(L"DnStat");
        if (!(frame && frame->frame_callbacks.size())) return 0;
        OnLatencyFrame_UICallback_Func = frame->frame_callbacks[0].callback;

        GW::Hook::CreateHook((void**)&OnLatencyFrame_UICallback_Func, OnLatencyFrame_UICallback, (void**)&OnLatencyFrame_UICallback_Ret);
        GW::Hook::EnableHooks(OnLatencyFrame_UICallback_Func);
        cached_frame_context = (LatencyFrameContext*)GW::UI::GetFrameContext(frame);
        return cached_frame_context;
    }

    void CHAT_CMD_FUNC(CmdPing)
    {
        LatencyWidget::SendPing();
    }
}

void LatencyWidget::SendPing()
{
    char buffer[48];
    snprintf(buffer, sizeof(buffer), "Current Ping: %ums, Avg Ping: %ums", LatencyWidget::GetPing(), LatencyWidget::GetAveragePing());
    GW::Chat::SendChat('#', buffer);
}

uint32_t LatencyWidget::GetPing() { 
    const auto context = GetLatencyFrameContext();
    return context ? context->current_ping : 0;
}

uint32_t LatencyWidget::GetAveragePing()
{
    const auto context = GetLatencyFrameContext();
    return context ? context->average_ping : 0;
}

void LatencyWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(180.0f, 60.0f), ImGuiCond_FirstUseEver);
    const bool ctrl_pressed = ImGui::IsKeyDown(ImGuiMod_Ctrl);

    if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, !ctrl_pressed))) {
        const ImVec2 cur = ImGui::GetCursorPos();
        ImGui::PushFont(FontLoader::GetFontByPx(text_size), text_size);
        ImGui::SetCursorPos(cur);
        uint32_t ping = GetPing();
        ImGui::TextColored(GetColorForPing(ping), "%ums", ping);
        if (show_avg_ping) {
            ping = GetAveragePing();
            ImGui::TextColored(GetColorForPing(ping), "%ums", ping);
        }

        ImGui::PopFont();

        const ImVec2 size = ImGui::GetWindowSize();
        const ImVec2 min = ImGui::GetWindowPos();
        const ImVec2 max(min.x + size.x, min.y + size.y);
        if (ctrl_pressed && ImGui::IsMouseReleased(0) && ImGui::IsMouseHoveringRect(min, max)) {
            SendPing();
        }
    }

    ImGui::End();
    ImGui::PopStyleColor();
}

void LatencyWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);

    LOAD_UINT(red_threshold);
    LOAD_BOOL(show_avg_ping);
    LOAD_FLOAT(text_size);
}

void LatencyWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    SAVE_UINT(red_threshold);
    SAVE_BOOL(show_avg_ping);
    SAVE_FLOAT(text_size);
}

void LatencyWidget::DrawSettingsInternal()
{
    ImGui::SliderInt("Red ping threshold", &red_threshold, 0, 1000);
    ImGui::Checkbox("Show average ping", &show_avg_ping);
    ImGui::DragFloat("Text size", &text_size, 1.f, FontLoader::text_size_min, FontLoader::text_size_max, "%.f");
}

ImColor LatencyWidget::GetColorForPing(const uint32_t ping)
{
    const float x = ping / static_cast<float>(red_threshold);
    const auto myColor = ImColor(2.0f * x, 2.0f * (1 - x), 0.0f);
    return myColor;
}
