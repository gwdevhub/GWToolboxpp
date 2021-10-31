#include "stdafx.h"


#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/AgentMgr.h>

#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Agent.h>

#include <ImGuiAddons.h>

#include <Modules/HintsModule.h>

namespace {
    struct HintUIMessage {
        uint32_t message_id = 0x10000000; // Used internally to avoid queueing more than 1 of the same hint
        wchar_t* message_encoded;
        uint32_t image_file_id = 0; // e.g. mouse imaage, light bulb, exclamation mark
        uint32_t message_timeout_ms = 15000;
        uint32_t style_bitmap = 0x12; // 0x18 = hint with left padding
        HintUIMessage(const wchar_t* message, uint32_t duration = 15000) {
            const size_t strlen = (wcslen(message) + 4) * sizeof(wchar_t);
            message_encoded = (wchar_t*)malloc(strlen);
            swprintf(message_encoded, strlen, L"\x108\x107%s\x1", message);
            message_id = (uint32_t)message;
            message_timeout_ms = duration;
        }
        ~HintUIMessage() {
            free(message_encoded);
        }
        void Show() {
            GW::UI::SendUIMessage(GW::UI::kShowHint, this);
        }
    };
}

//#define PRINT_CHAT_PACKETS
void HintsModule::Initialize() {
    GW::UI::RegisterUIMessageCallback(&hints_entry, OnUIMessage);
}

void HintsModule::OnUIMessage(GW::HookStatus* status, uint32_t message_id, void* wparam, void*) {
    switch (message_id) {
    case GW::UI::kShowHint: {
        HintUIMessage* msg = (HintUIMessage*)wparam;
        msg->message_timeout_ms = 120000;
        if (Instance().block_repeat_attack_hint && wcscmp(msg->message_encoded, L"\x9c3") == 0) {
            status->blocked = true;
        }
    } break;
    case GW::UI::kWriteToChatLog: {
        GW::UI::UIChatMessage* msg = (GW::UI::UIChatMessage*)wparam;
        if (msg->channel == GW::Chat::Channel::CHANNEL_GLOBAL && wcsncmp(msg->message, L"\x8101\x4793\xfda0\xe8e2\x6844", 5) == 0) {
            HintUIMessage(L"Heroes in your party gain experience from quests, so remember to add your low level heroes when accepting quest rewards.").Show();
        }
    } break;
    case GW::UI::kShowXunlaiChest: {
        GW::AgentLiving* chest = GW::Agents::GetTargetAsAgentLiving();
        if(chest && chest->player_number == 5001 && GW::GetDistance(GW::Agents::GetPlayer()->pos,chest->pos) < GW::Constants::Range::Nearby) {
            HintUIMessage(L"Type '/chest' into chat to open your Xunlai Chest from anywhere in an outpost, so you won't have to run to the chest every time.").Show();
        }
    } break;
    }
}
void HintsModule::DrawSettingInternal() {
    ImGui::Checkbox("Block \"ordering your character to attack repeatedly\" hint", &block_repeat_attack_hint);
    ImGui::ShowHelp("Guild Wars keeps showing a hint to tell you not to repeatedly attack a foe.\nTick to stop it from popping up.");
}