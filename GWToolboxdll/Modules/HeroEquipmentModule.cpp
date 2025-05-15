#include "stdafx.h"

#include "Defines.h"
#include "HeroEquipmentModule.h"
#include <GWCA/GameEntities/Item.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <Windows/Hotkeys.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

namespace {

    struct UICallbackItem {
        GW::UI::UIInteractionCallback callback;
        void* wparam;
    };

    bool wcseq(const wchar_t* a, const wchar_t* b)
    {
        return a && b && wcscmp(a, b) == 0;
    }

    std::vector<UICallbackItem> InventoryWindow_UICallbacks;

    GW::UI::UIInteractionCallback inventory_avatar_list_frame_callback = 0;

    uint32_t* current_inventory_agent_id = 0;

    struct InventoryWindow_UIContext {
        uint32_t h0000;
        uint32_t h0004;
    };

    std::vector<UICallbackItem>* GetInventoryWindowUICallbacks()
    {
        if (InventoryWindow_UICallbacks.size()) return &InventoryWindow_UICallbacks;
        auto inv_frame = GW::UI::GetFrameByLabel(L"Inventory");
        GW::UI::Frame* destroy_inv_frame = nullptr;
        if (!inv_frame) {
            GW::UI::Keypress(GW::UI::ControlAction::ControlAction_ToggleInventoryWindow);
            destroy_inv_frame = GW::UI::GetFrameByLabel(L"Inventory");
            if (!destroy_inv_frame) return nullptr;
            inv_frame = destroy_inv_frame;
        }
        if (inv_frame && inv_frame->frame_callbacks.size()) {
            for (const auto& cb : inv_frame->frame_callbacks) {
                InventoryWindow_UICallbacks.push_back({cb.callback, (void*)cb.h0008});
            }
        }
        GW::UI::DestroyUIComponent(destroy_inv_frame);
        return InventoryWindow_UICallbacks.size() ? &InventoryWindow_UICallbacks : nullptr;
    }

    void ReselectHero(GW::UI::Frame* avatar_list_frame)
    {
        if (!avatar_list_frame) return;
        uint32_t hero_index = GW::UI::GetParentFrame(avatar_list_frame)->child_offset_id & 0xf;
        const auto hero_agent_id = GW::PartyMgr::GetHeroAgentID(hero_index);
        if (hero_agent_id) {
            const uint32_t currently_selected_agent_id = 0;
            GW::UI::SendFrameUIMessage(avatar_list_frame, (GW::UI::UIMessage)0x46, 0, (void*)&currently_selected_agent_id);
            if (currently_selected_agent_id == hero_agent_id) return;
            const auto actual_selected_agent_id = *current_inventory_agent_id;
            *current_inventory_agent_id = hero_agent_id;
            GW::UI::SendFrameUIMessage(avatar_list_frame, (GW::UI::UIMessage)0x47, (void*)hero_agent_id);
            *current_inventory_agent_id = actual_selected_agent_id;
            return;
        }
        GW::UI::DestroyUIComponent(GW::UI::GetParentFrame(avatar_list_frame));
    }

    void OnHeroPickerFrame_UICallback(GW::UI::InteractionMessage* message, void* wParam, void* lParam) {
        GW::Hook::EnterHook();
        inventory_avatar_list_frame_callback(message, wParam, lParam);

        switch (message->message_id) {
            case GW::UI::UIMessage::kPartyRemoveHero:
            case GW::UI::UIMessage::kInventoryAgentChanged:
            case GW::UI::UIMessage::kResize: {
                ReselectHero(GW::UI::GetFrameById(message->frame_id));
            } break;
        }
        GW::Hook::LeaveHook();
    }

    void Cleanup() {
        for (size_t i = 0; i < 7; i++) {
            const auto frame_label = std::format(L"HeroInventory_{}", i);
            auto frame = GW::UI::GetFrameByLabel(frame_label.c_str());
            if (!frame) continue;
            frame->frame_callbacks.m_size = 0;// @Cleanup: possible memory leak on gw side
            GW::UI::DestroyUIComponent(frame);
        }
    }

    GW::UI::Frame* CreateHeroInventoryWindow(uint32_t hero_index)
    {
        const auto frame_label = std::format(L"HeroInventory_{}", hero_index);
        auto hero_frame = GW::UI::GetFrameByLabel(frame_label.c_str());
        if (hero_frame) return hero_frame;
        if (!GetInventoryWindowUICallbacks()) return nullptr;
        const auto game_frame = GW::UI::GetChildFrame(GW::UI::GetFrameByLabel(L"Game"),6);
        if (!game_frame) return nullptr;
        const auto frame_id = GW::UI::CreateUIComponent(game_frame->frame_id, 0x20, 0xfff0 | hero_index, InventoryWindow_UICallbacks[0].callback, InventoryWindow_UICallbacks[0].wparam, frame_label.c_str());
        if (!frame_id) return nullptr;
        hero_frame = GW::UI::GetFrameById(frame_id);
        const auto title = std::format(L"\x107\x108Hero {} Inventory\x1", hero_index);
        if (!GW::UI::SetFrameTitle(hero_frame, title.c_str())) {
            return GW::UI::DestroyUIComponent(hero_frame), nullptr;
        }
        for (size_t i = 1; i < InventoryWindow_UICallbacks.size(); i++) {
            if (!GW::UI::AddFrameUIInteractionCallback(hero_frame, InventoryWindow_UICallbacks[i].callback, InventoryWindow_UICallbacks[i].wparam)) {
                return GW::UI::DestroyUIComponent(hero_frame), nullptr;
            }
        }
        const auto avatar_list_frame = GW::UI::GetChildFrame(hero_frame, 9);
        if (!(avatar_list_frame && avatar_list_frame->frame_callbacks.size())) {
            return GW::UI::DestroyUIComponent(hero_frame), nullptr;
        }
        inventory_avatar_list_frame_callback = avatar_list_frame->frame_callbacks[0].callback;
        avatar_list_frame->frame_callbacks[0].callback = OnHeroPickerFrame_UICallback;
        return hero_frame;
    }

    void ToggleHeroInventoryWindow(uint32_t hero_index) {
        GW::GameThread::Enqueue([hero_index]() {
            const auto frame_label = std::format(L"HeroInventory_{}", hero_index);
            auto hero_frame = GW::UI::GetFrameByLabel(frame_label.c_str());
            if (hero_frame) {
                GW::UI::DestroyUIComponent(hero_frame);
            }
            else {
                CreateHeroInventoryWindow(hero_index);
            }
        });

    }

    GW::HookEntry ChatCmdEntry;

    void CHAT_CMD_FUNC(CmdHeroInventory) {
        if (wcseq(*argv, L"hero1")) return ToggleHeroInventoryWindow(1);
        if (wcseq(*argv, L"hero2")) return ToggleHeroInventoryWindow(2);
        if (wcseq(*argv, L"hero3")) return ToggleHeroInventoryWindow(3);
        if (wcseq(*argv, L"hero4")) return ToggleHeroInventoryWindow(4);
        if (wcseq(*argv, L"hero5")) return ToggleHeroInventoryWindow(5);
        if (wcseq(*argv, L"hero6")) return ToggleHeroInventoryWindow(6);
        if (wcseq(*argv, L"hero7")) return ToggleHeroInventoryWindow(7);
    }

} // namespace

void HeroEquipmentModule::Initialize()
{
    ToolboxModule::Initialize();

    auto address = GW::Scanner::Find("\x6a\x00\x6a\x00\x68\xb0\x01\x00\x10", "xxxxxxxxx", 10);
    ASSERT(address);
    current_inventory_agent_id = *(uint32_t**)address;
    GW::Chat::CreateCommand(&ChatCmdEntry, L"hero1", CmdHeroInventory);
    GW::Chat::CreateCommand(&ChatCmdEntry, L"hero2", CmdHeroInventory);
    GW::Chat::CreateCommand(&ChatCmdEntry, L"hero3", CmdHeroInventory);
    GW::Chat::CreateCommand(&ChatCmdEntry, L"hero4", CmdHeroInventory);
    GW::Chat::CreateCommand(&ChatCmdEntry, L"hero5", CmdHeroInventory);
    GW::Chat::CreateCommand(&ChatCmdEntry, L"hero6", CmdHeroInventory);
    GW::Chat::CreateCommand(&ChatCmdEntry, L"hero7", CmdHeroInventory);
}

void HeroEquipmentModule::SignalTerminate()
{
    GW::GameThread::Enqueue(Cleanup);
}

bool HeroEquipmentModule::CanTerminate()
{
    for (size_t i = 0; i < 7; i++) {
        const auto frame_label = std::format(L"HeroInventory_{}", i);
        if (GW::UI::GetFrameByLabel(frame_label.c_str())) return false;
    }
    return true;
}

void HeroEquipmentModule::Terminate()
{
    ToolboxModule::Terminate();
    Cleanup();
}
