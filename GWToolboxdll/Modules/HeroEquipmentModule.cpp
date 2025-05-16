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

    std::map<uint32_t, GW::UI::FramePosition> frame_layouts_by_child_id;


    struct UICallbackItem {
        GW::UI::UIInteractionCallback callback;
        void* wparam;
    };

    bool wcseq(const wchar_t* a, const wchar_t* b)
    {
        return a && b && wcscmp(a, b) == 0;
    }

        // Get the hero's agent_id that this inventory window is clamped to
    uint32_t GetAvatarListHeroAgentId(uint32_t frame_id)
    {
        uint32_t hero_index = GW::UI::GetParentFrame(GW::UI::GetFrameById(frame_id))->child_offset_id & 0xf;
        return GW::PartyMgr::GetHeroAgentID(hero_index);
    }


    std::vector<UICallbackItem> InventoryWindow_UICallbacks;

    GW::UI::UIInteractionCallback OnAvatarList_UICallback_Ret = 0;
    GW::UI::UIInteractionCallback OnInventoryLoadout_UICallback_Ret = 0;

    std::queue<uint32_t> pending_destroy_frames;

    struct BagFrameContext {
        uint32_t h0000;
        uint32_t frame_id;
        uint32_t h0008;
        uint32_t h000c;
        uint32_t h0010;
        uint32_t h0014;
        uint32_t h0018;
    };
    static_assert(sizeof(BagFrameContext) == 0x1c);
    typedef void(__fastcall* HandleItemSlotClicked_pt)(BagFrameContext* bag_context, void*, GW::UI::UIPacket::kMouseAction* wparam);
    HandleItemSlotClicked_pt HandleItemSlotClicked_Func = 0, HandleItemSlotClicked_Ret = 0;

    uint32_t hero_inventory_item_slot_clicked = 0;

    void __fastcall OnItemSlotClicked(BagFrameContext* bag_context, void* edx, GW::UI::UIPacket::kMouseAction* wparam)
    {
        GW::Hook::EnterHook();
        hero_inventory_item_slot_clicked = 0;
        auto parent = GW::UI::GetFrameById(bag_context->frame_id);
        while (parent) {
            parent = GW::UI::GetParentFrame(parent);
            if (parent && (parent->child_offset_id & 0x0000fff0) == 0xfff0) {
                // This item belongs to one of the hero inventory windows.
                hero_inventory_item_slot_clicked = GW::PartyMgr::GetHeroAgentID(parent->child_offset_id & 0xf);
                break;
            }
        }
        HandleItemSlotClicked_Ret(bag_context, edx, wparam);
        hero_inventory_item_slot_clicked = 0;
        GW::Hook::LeaveHook();
    }

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
        //if (avatar_list_frame->IsVisible())
        //    GW::UI::SetFrameVisible(avatar_list_frame, false);
        const auto hero_agent_id = GetAvatarListHeroAgentId(avatar_list_frame->frame_id);
        if (hero_agent_id) {
            uint32_t currently_selected_agent_id = 0;
            GW::UI::SendFrameUIMessage(avatar_list_frame, (GW::UI::UIMessage)0x46, 0, (void*)&currently_selected_agent_id);
            if(currently_selected_agent_id == hero_agent_id)
                return;
            const auto actual_selected_agent_id = *current_inventory_agent_id;
            *current_inventory_agent_id = hero_agent_id;
            GW::UI::SendFrameUIMessage(avatar_list_frame, GW::UI::UIMessage::kFrameMessage_0x47, (void*)hero_agent_id);
            *current_inventory_agent_id = actual_selected_agent_id;
            return;
        }
        pending_destroy_frames.push(GW::UI::GetParentFrame(avatar_list_frame)->frame_id);
    }

    void OnInventoryLoadout_UICallback(GW::UI::InteractionMessage* message, void* wParam, void* lParam) {
        GW::Hook::EnterHook();
        switch (message->message_id) {
            case GW::UI::UIMessage::kGetInventoryAgentId: {
                // Don't respond to this packet; let the normal inventory window handle it.
                GW::Hook::LeaveHook();
                return;
            } break;
        }
        OnInventoryLoadout_UICallback_Ret(message, wParam, lParam);

        GW::Hook::LeaveHook();
    }

    void OnAvatarList_UICallback(GW::UI::InteractionMessage* message, void* wParam, void* lParam) {
        GW::Hook::EnterHook();
        OnAvatarList_UICallback_Ret(message, wParam, lParam);

        switch (message->message_id) {
            case GW::UI::UIMessage::kInitFrame:
            case GW::UI::UIMessage::kMouseClick2:
            case GW::UI::UIMessage::kPartyRemoveHero:
            case GW::UI::UIMessage::kInventoryAgentChanged:
            case GW::UI::UIMessage::kFrameMessage_0x47: {
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
        for (size_t i = 1; i < InventoryWindow_UICallbacks.size(); i++) {
            if (!GW::UI::AddFrameUIInteractionCallback(hero_frame, InventoryWindow_UICallbacks[i].callback, InventoryWindow_UICallbacks[i].wparam)) {
                return GW::UI::DestroyUIComponent(hero_frame), nullptr;
            }
        }
        const auto avatar_list_frame = GW::UI::GetChildFrame(hero_frame, 9);
        if (!(avatar_list_frame && avatar_list_frame->frame_callbacks.size())) {
            return GW::UI::DestroyUIComponent(hero_frame), nullptr;
        }
        OnAvatarList_UICallback_Ret = avatar_list_frame->frame_callbacks[0].callback;
        avatar_list_frame->frame_callbacks[0].callback = OnAvatarList_UICallback;

        const auto inventory_loadout_frame = GW::UI::GetChildFrame(hero_frame, 7);
        if (!(inventory_loadout_frame && inventory_loadout_frame->frame_callbacks.size())) {
            return GW::UI::DestroyUIComponent(hero_frame), nullptr;
        }
        OnInventoryLoadout_UICallback_Ret = inventory_loadout_frame->frame_callbacks[0].callback;
        inventory_loadout_frame->frame_callbacks[0].callback = OnInventoryLoadout_UICallback;
        ReselectHero(avatar_list_frame);
        if (frame_layouts_by_child_id.contains(hero_frame->child_offset_id)) {
            hero_frame->position = frame_layouts_by_child_id[hero_frame->child_offset_id];
            GW::UI::TriggerFrameRedraw(hero_frame);
        }
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

    void OnPostUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void* lparam) {
        switch (message_id) {
            case GW::UI::UIMessage::kGetInventoryAgentId: {
                if (hero_inventory_item_slot_clicked) {
                    *(uint32_t*)lparam = hero_inventory_item_slot_clicked;
                }
                status->blocked = true;
            } break;
            case GW::UI::UIMessage::kFloatingWindowMoved: {
                const auto frame = GW::UI::GetFrameById(*(uint32_t*)wparam);
                if (frame && (frame->child_offset_id & 0x0000fff0) == 0xfff0) {
                    frame_layouts_by_child_id[frame->child_offset_id] = frame->position;
                }
            } break;
        }
    }

} // namespace

void HeroEquipmentModule::Initialize()
{
    ToolboxModule::Initialize();

    auto address = GW::Scanner::Find("\x6a\x00\x6a\x00\x68\xb0\x01\x00\x10", "xxxxxxxxx", 10);
    ASSERT(address);
    current_inventory_agent_id = *(uint32_t**)address;

    HandleItemSlotClicked_Func = (HandleItemSlotClicked_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("InvBag.cpp","slot < m_slotItem.Count()",0,0));
    ASSERT(HandleItemSlotClicked_Func);
    GW::Hook::CreateHook((void**)&HandleItemSlotClicked_Func, OnItemSlotClicked, (void**)&HandleItemSlotClicked_Ret);
    GW::Hook::EnableHooks(HandleItemSlotClicked_Func);

    GW::Chat::CreateCommand(&ChatCmdEntry, L"hero1", CmdHeroInventory);
    GW::Chat::CreateCommand(&ChatCmdEntry, L"hero2", CmdHeroInventory);
    GW::Chat::CreateCommand(&ChatCmdEntry, L"hero3", CmdHeroInventory);
    GW::Chat::CreateCommand(&ChatCmdEntry, L"hero4", CmdHeroInventory);
    GW::Chat::CreateCommand(&ChatCmdEntry, L"hero5", CmdHeroInventory);
    GW::Chat::CreateCommand(&ChatCmdEntry, L"hero6", CmdHeroInventory);
    GW::Chat::CreateCommand(&ChatCmdEntry, L"hero7", CmdHeroInventory);

    const GW::UI::UIMessage ui_messages[] = {GW::UI::UIMessage::kFloatingWindowMoved, GW::UI::UIMessage::kGetInventoryAgentId};

    for (const auto message_id : ui_messages) {
        GW::UI::RegisterUIMessageCallback(&ChatCmdEntry, message_id, OnPostUIMessage, 0x4000);
    }

}

void HeroEquipmentModule::SignalTerminate()
{
    GW::GameThread::Enqueue(Cleanup);
    GW::UI::RemoveUIMessageCallback(&ChatCmdEntry);
    GW::Chat::DeleteCommand(&ChatCmdEntry);
    GW::Hook::RemoveHook(HandleItemSlotClicked_Func);
}

bool HeroEquipmentModule::CanTerminate()
{
    for (size_t i = 0; i < 7; i++) {
        const auto frame_label = std::format(L"HeroInventory_{}", i);
        if (GW::UI::GetFrameByLabel(frame_label.c_str())) return false;
    }
    return true;
}

void HeroEquipmentModule::Update(float) {
    while (!pending_destroy_frames.empty()) {
        const auto frame_id = pending_destroy_frames.front();
        pending_destroy_frames.pop();
        GW::GameThread::Enqueue([frame_id]() {
            GW::UI::DestroyUIComponent(GW::UI::GetFrameById(frame_id));
        });
    }
}

static_assert((sizeof(GW::UI::FramePosition) % sizeof(uint32_t)) == 0);

void HeroEquipmentModule::SaveSettings(ToolboxIni* ini) {
    for (const auto& [child_id,position] : frame_layouts_by_child_id) {
        std::string out;
        GuiUtils::ArrayToIni((uint32_t*)&position, sizeof(position) / sizeof(uint32_t), &out);
        const auto label = std::format("WindowPos_{}", child_id);
        ini->SetValue(Name(), label.c_str(), out.c_str());
    }
}
void HeroEquipmentModule::LoadSettings(ToolboxIni* ini)
{
    std::list<CSimpleIniA::Entry> keys;
    ini->GetAllKeys(Name(), keys);
    const char* prefix = "WindowPos_";
    for (auto& key : keys) {
        if (strncmp(key.pItem, prefix, strlen(prefix)) != 0) continue;
        unsigned int child_id = std::stoi(key.pItem + strlen(prefix));
        std::string value = ini->GetValue(Name(), key.pItem, "");
        if (value.empty()) continue;
        GW::UI::FramePosition position;
        if (GuiUtils::IniToArray(value.c_str(), (uint32_t*)&position, sizeof(position) / sizeof(uint32_t))) {
            frame_layouts_by_child_id[child_id] = position;
        }
    }
    // Redraw?
}

void HeroEquipmentModule::Terminate()
{
    ToolboxModule::Terminate();
    Cleanup();
}
