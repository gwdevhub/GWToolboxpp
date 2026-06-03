#include "ChestOpener.h"

#include <GWCA/GWCA.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Utilities/Scanner.h>

#include "PluginUtils.h"

namespace {
    GW::HookEntry OnSentChat_HookEntry;

    typedef void(__cdecl *SendPacket_pt)(uint32_t context, uint32_t size, void* packet);
    SendPacket_pt SendPacket_Func = nullptr;
    uintptr_t* game_srv_object_addr;

    typedef void(__cdecl* DoAction_pt)(uint32_t identifier);
    DoAction_pt OpenLockedChest_Func = 0;

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
            if (!OpenLockedChest_Func) return;

            // GoSignpost
            uint32_t go_signpost_buf[3] = {0x51, target->agent_id, 0};
            SendPacket_Func(*game_srv_object_addr, 0xC, go_signpost_buf);

            //OpenLockedChest_Func(0x2);
            uint32_t open_chest_buf[2] = {0x53, 0x2};
            SendPacket_Func(*game_srv_object_addr, 0x8, open_chest_buf);
        }
    }
}

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static ChestOpener instance;
    return &instance;
}

void ChestOpener::Initialize(ImGuiContext* ctx, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, allocator_fns, toolbox_dll);

    GW::UI::RegisterUIMessageCallback(&OnSentChat_HookEntry, GW::UI::UIMessage::kSendChatMessage, OnSendChat);

    auto address = GW::Scanner::Find("\x83\xc9\x01\x89\x4b\x24", "xxxxxx", 0x28);
    OpenLockedChest_Func = (DoAction_pt)GW::Scanner::FunctionFromNearCall(address);

    // SendPacket_Func = (SendPacket_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion(R"(p:\code\net\msg\msgconn.cpp)","bytes >= sizeof(dword)", 0, 0));
    // uintptr_t address = GW::Scanner::FindAssertion(R"(p:\code\gw\net\cli\gcgamecmd.cpp)","No valid case for switch variable 'code'", 0, -0x32);
    // game_srv_object_addr = *(uintptr_t **)address;
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
