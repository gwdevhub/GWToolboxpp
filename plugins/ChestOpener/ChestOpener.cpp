#include "ChestOpener.h"

#include <GWCA/GWCA.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Utilities/Scanner.h>

#include "PluginUtils.h"

namespace {
    GW::HookEntry OnSentChat_HookEntry;

    typedef void(__cdecl *SendPacket_pt)(uint32_t context, uint32_t size, void* packet);
    SendPacket_pt SendPacket_Func = nullptr;
    uintptr_t* game_srv_object_addr = nullptr;

    void OnSendChat(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void*)
    {
        if (message_id != GW::UI::UIMessage::kSendChatMessage) return;
        const auto wmessage = static_cast<GW::UI::UIPacket::kSendChatMessage*>(wparam)->message;
        if (!(wmessage && *wmessage)) return;
        const auto channel = GW::Chat::GetChannel(*wmessage);
        if (channel != GW::Chat::CHANNEL_COMMAND || status->blocked) return;

        const auto message = PluginUtils::WStringToString(wmessage);
        if (message.starts_with("/openchest")) {
            status->blocked = true;

            auto target = GW::Agents::GetTarget();
            auto player = GW::Agents::GetControlledCharacter();
            if (!target || !target->GetIsGadgetType()) return;
            if (!player || player->GetIsDead()) return;
            if (!GW::Items::GetItemByModelId(GW::Constants::ItemID::Lockpick)) return;
            if (!SendPacket_Func || !game_srv_object_addr || !*game_srv_object_addr) return;

            const auto id = target->agent_id;
            const auto send_packet = SendPacket_Func;
            const auto game_srv_object = *game_srv_object_addr;
            GW::GameThread::Enqueue([id, send_packet, game_srv_object]() {
                // GoSignpost
                uint32_t go_signpost_buf[3] = {0x51, id, 0};
                send_packet(game_srv_object, 0xC, go_signpost_buf);

                Sleep(250);

                //OpenLockedChest_Func(0x2);
                uint32_t open_chest_buf[2] = {0x53, 0x2};
                send_packet(game_srv_object, 0x8, open_chest_buf);
            });
        }
    }
}

#ifndef DBBOX_BUILD
DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static ChestOpener instance;
    return &instance;
}
#endif

void ChestOpener::Initialize(ImGuiContext* ctx, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, allocator_fns, toolbox_dll);

    GW::UI::RegisterUIMessageCallback(&OnSentChat_HookEntry, GW::UI::UIMessage::kSendChatMessage, OnSendChat);

    GW::Scanner::Initialize();

    SendPacket_Func = nullptr;
    game_srv_object_addr = nullptr;
    auto address = GW::Scanner::FindAssertion(R"(P:\Code\Net\Msg\MsgConn.cpp)", "bytes >= sizeof(dword)", 0, 0);
    if (GW::Scanner::IsValidPtr(address, GW::ScannerSection::Section_TEXT)) {
        const auto function = GW::Scanner::ToFunctionStart(address);
        if (GW::Scanner::IsValidPtr(function, GW::ScannerSection::Section_TEXT)) {
            SendPacket_Func = reinterpret_cast<SendPacket_pt>(function);
        }
    }

    address = GW::Scanner::FindAssertion(R"(P:\Code\Gw\Net\Cli\GcGameCmd.cpp)", "No valid case for switch variable 'code'", 0, -0x32);
    if (GW::Scanner::IsValidPtr(address, GW::ScannerSection::Section_TEXT)) {
        const auto candidate = *reinterpret_cast<uintptr_t**>(address);
        if (GW::Scanner::IsValidPtr(reinterpret_cast<uintptr_t>(candidate))) {
            game_srv_object_addr = candidate;
        }
    }
}

void ChestOpener::SignalTerminate()
{
    ToolboxPlugin::SignalTerminate();

    GW::UI::RemoveUIMessageCallback(&OnSentChat_HookEntry, GW::UI::UIMessage::kSendChatMessage);
}

void ChestOpener::DrawSettings()
{
    ToolboxPlugin::DrawSettings();

    ImGui::Text("Version 1.0.0");
}
