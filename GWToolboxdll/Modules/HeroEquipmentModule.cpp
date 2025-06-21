#include "stdafx.h"

#include "Defines.h"
#include "HeroEquipmentModule.h"

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <ImGuiAddons.h>

#include <Modules/GwDatTextureModule.h>
#include <Utils/GuiUtils.h>

namespace {

    std::map<uint32_t, GW::UI::FramePosition> frame_layouts_by_child_id;

    IDirect3DTexture9** icon_texture = nullptr;
    // Precompute UV coordinates for each state
    ImVec2 uv_normal[2], uv_hover[2], uv_active[2];

    constexpr char hero_index_max = 7;


    struct UICallbackItem {
        GW::UI::UIInteractionCallback callback;
        void* wparam;
    };

    bool wcseq(const wchar_t* a, const wchar_t* b)
    {
        return a && b && wcscmp(a, b) == 0;
    }

    uint32_t GetMyHeroAgentId(uint32_t hero_index)
    {
        const auto player = GW::Agents::GetControlledCharacter();
        if (!player) return 0;
        if (hero_index == 0) return player->agent_id;
        const auto party = GW::PartyMgr::GetPartyInfo();
        if (!party) return 0;
        const auto& heroes = party->heroes;
        uint32_t hits = 0;
        for (const auto& hero : heroes) {
            if (hero.owner_player_id == player->player_number) hits++;
            if (hits == hero_index) return hero.agent_id;
        }
        return 0;
    }

    // Get the hero's agent_id that this inventory window is clamped to
    uint32_t GetAvatarListHeroAgentId(uint32_t frame_id)
    {
        uint32_t hero_index = GW::UI::GetParentFrame(GW::UI::GetFrameById(frame_id))->child_offset_id & 0xf;
        return GetMyHeroAgentId(hero_index);
    }

    bool block_inv_agent_changed_packet = false;

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

    GW::UI::Frame* GetHeroInventoryFrameBeingInteracted(uint32_t current_frame_id) {
        auto parent = GW::UI::GetFrameById(current_frame_id);
        while(true) {
            if (!parent) return nullptr;
            if (parent && (parent->child_offset_id & 0x0000fff0) == 0xfff0) {
                return parent;
            }
            parent = GW::UI::GetParentFrame(parent);
        }
        return nullptr;
    }

    void __fastcall OnItemSlotClicked(BagFrameContext* bag_context, void* edx, GW::UI::UIPacket::kMouseAction* wparam)
    {
        GW::Hook::EnterHook();
        auto parent = GetHeroInventoryFrameBeingInteracted(bag_context->frame_id);
        hero_inventory_item_slot_clicked = parent ? GetMyHeroAgentId(parent->child_offset_id & 0xf) : 0;
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

    void SaveFramePosition(uint32_t frame_id) {
        const auto frame = GetHeroInventoryFrameBeingInteracted(frame_id);
        if (frame) {
            frame_layouts_by_child_id[frame->child_offset_id] = frame->position;
        }
    }

    void ReselectHero(GW::UI::Frame* avatar_list_frame)
    {
        if (!avatar_list_frame) return;
        //if (avatar_list_frame->IsVisible())
        //    GW::UI::SetFrameVisible(avatar_list_frame, false);
        const auto hero_agent_id = GetAvatarListHeroAgentId(avatar_list_frame->frame_id);
        if (hero_agent_id) {
            uint32_t currently_selected_agent_id = 0;
            GW::UI::SendFrameUIMessage(avatar_list_frame, GW::UI::UIMessage::kFrameMessage_0x46, 0, (void*)&currently_selected_agent_id);
            if(currently_selected_agent_id == hero_agent_id)
                return;
            const auto actual_selected_agent_id = *current_inventory_agent_id;
            *current_inventory_agent_id = hero_agent_id;
            block_inv_agent_changed_packet = true;
            GW::UI::SendFrameUIMessage(avatar_list_frame, GW::UI::UIMessage::kFrameMessage_0x47, (void*)hero_agent_id);
            block_inv_agent_changed_packet = false;
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
        if (!GetHeroInventoryFrameBeingInteracted(message->frame_id)) {
            OnAvatarList_UICallback_Ret(message, wParam, lParam);
            GW::Hook::LeaveHook();
            return;
        }
        switch (message->message_id) {
            case GW::UI::UIMessage::kMouseClick2: {
                const auto packet = (GW::UI::UIPacket::kMouseAction*)wParam;
                if ((packet->child_offset_id > 0xffffff && packet->child_offset_id < 0x2000000)
                    && (packet->current_state == 8 && *(uint32_t*)packet->wparam == 0)) {
                    break; // Avatar image clicked; block the packet
                }
                OnAvatarList_UICallback_Ret(message, wParam, lParam);
            } break;
            default: {
                OnAvatarList_UICallback_Ret(message, wParam, lParam);
            } break;
        }

        switch (message->message_id) {
            case GW::UI::UIMessage::kFrameMessage_0x47: {
                ReselectHero(GW::UI::GetFrameById(message->frame_id));
            } break;
            case GW::UI::UIMessage::kInitFrame:
            case GW::UI::UIMessage::kMouseClick2:
            case GW::UI::UIMessage::kPartyRemoveHero:
            case GW::UI::UIMessage::kInventoryAgentChanged: {
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

    GW::UI::Frame* GetHeroInventoryWindow(char hero_index)
    {
        wchar_t inventory_window_label[] = L"HeroInventory_0";
        inventory_window_label[_countof(inventory_window_label) - 2] = L'0' + hero_index;
        return GW::UI::GetFrameByLabel(inventory_window_label);
    }

    GW::UI::Frame* CreateHeroInventoryWindow(char hero_index)
    {
        auto hero_frame = GetHeroInventoryWindow(hero_index);
        if (hero_frame) return hero_frame;
        if (!GetInventoryWindowUICallbacks()) return nullptr;

        const auto game_frame = GW::UI::GetChildFrame(GW::UI::GetFrameByLabel(L"Game"),6);
        if (!game_frame) return nullptr;

        if (!GetMyHeroAgentId(hero_index)) return nullptr;

        wchar_t inventory_window_label[] = L"HeroInventory_0";
        inventory_window_label[_countof(inventory_window_label) - 2] = L'0' + hero_index;
        const auto frame_id = GW::UI::CreateUIComponent(game_frame->frame_id, 0x20, 0xfff0 | hero_index, InventoryWindow_UICallbacks[0].callback, InventoryWindow_UICallbacks[0].wparam, inventory_window_label);
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

    void ToggleHeroInventoryWindow(char hero_index) {
        GW::GameThread::Enqueue([hero_index]() {
            auto hero_frame = GetHeroInventoryWindow(hero_index);
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
        if (argc > 1 && argv[1][1] == 0)
            ToggleHeroInventoryWindow((char)(argv[1][0] - L'0'));
    }

    void OnPreUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void*, void* lparam)
    {
        switch (message_id) {
            case GW::UI::UIMessage::kGetInventoryAgentId: {
                if (hero_inventory_item_slot_clicked) {
                    *(uint32_t*)lparam = hero_inventory_item_slot_clicked;
                }
                status->blocked = true;
            } break;
            case GW::UI::UIMessage::kInventoryAgentChanged: {
                if (block_inv_agent_changed_packet) {
                    status->blocked = true;
                    block_inv_agent_changed_packet = false;
                }
            } break;
        }
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
                SaveFramePosition(*(uint32_t*)wparam);
            } break;
        }
    }

} // namespace

void HeroEquipmentModule::Initialize()
{
    ToolboxModule::Initialize();

    auto address = GW::Scanner::Find("\x6a\x00\x6a\x00\x68\xb0\x01\x00\x10", "xxxxxxxxx", 10);
    if (address)
        current_inventory_agent_id = *(uint32_t**)address;

    HandleItemSlotClicked_Func = (HandleItemSlotClicked_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("InvBag.cpp","slot < m_slotItem.Count()",0,0));

    #ifdef _DEBUG
    ASSERT(current_inventory_agent_id && HandleItemSlotClicked_Func);
    #endif
    if (!(current_inventory_agent_id && HandleItemSlotClicked_Func)) {
        return;
    }
    if (HandleItemSlotClicked_Func) {
        GW::Hook::CreateHook((void**)&HandleItemSlotClicked_Func, OnItemSlotClicked, (void**)&HandleItemSlotClicked_Ret);
        GW::Hook::EnableHooks(HandleItemSlotClicked_Func);
    }
    GW::Chat::CreateCommand(&ChatCmdEntry, L"heroinventory", CmdHeroInventory);

    const GW::UI::UIMessage ui_messages[] = {
        GW::UI::UIMessage::kFloatingWindowMoved, 
        GW::UI::UIMessage::kGetInventoryAgentId, 
        GW::UI::UIMessage::kInventoryAgentChanged
    };

    for (const auto message_id : ui_messages) {
        GW::UI::RegisterUIMessageCallback(&ChatCmdEntry, message_id, OnPostUIMessage, 0x4000);
    }

    // Calculate icon uv coords
    icon_texture = GwDatTextureModule::LoadTextureFromFileId(0x231a5);
    constexpr float icon_uv_size = 23.5f / 256.f; // An icon is 23px of the 256px texture

    // Define the indices for button states
    constexpr int normal_index = 4; // Normal state (image 4)
    constexpr int hover_index = 5;  // Hovered state (image 5)
    constexpr int active_index = 6; // Clicked/Active state (image 6)

    // Helper function to calculate UV coordinates for an icon index
    auto calcUV = [icon_uv_size](int index, ImVec2* uv) {
        int row = index / 10;
        int col = index % 10;
        uv[0] = ImVec2(col * icon_uv_size, row * icon_uv_size);
        uv[1] = ImVec2(uv[0].x + icon_uv_size, uv[0].y + icon_uv_size);
    };

    calcUV(normal_index, uv_normal);
    calcUV(hover_index, uv_hover);
    calcUV(active_index, uv_active);

}

void HeroEquipmentModule::SignalTerminate()
{
    GW::GameThread::Enqueue(Cleanup);
    GW::UI::RemoveUIMessageCallback(&ChatCmdEntry);
    GW::Chat::DeleteCommand(&ChatCmdEntry);
    if (HandleItemSlotClicked_Func)
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
void HeroEquipmentModule::Draw(IDirect3DDevice9*)
{
    if (!(current_inventory_agent_id && HandleItemSlotClicked_Func)) return;
    wchar_t label[] = L"AgentCommander0";
    char window_label[] = "AgentCommanderInvBtn0";
    char btn_label[] = "##HeroEquipBtn0";
    wchar_t inventory_window_label[] = L"HeroInventory_0";

    // Load the texture once outside the loop

    if (!(icon_texture && *icon_texture)) return;

    const auto root = GW::UI::GetRootFrame();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);

    for (char i = 0; i < hero_index_max; i++) {
        label[_countof(label) - 2] = L'0' + i;
        window_label[_countof(window_label) - 2] = '0' + i;
        btn_label[_countof(btn_label) - 2] = '0' + i;
        inventory_window_label[_countof(inventory_window_label) - 2] = L'0' + i;

        const auto frame = GW::UI::GetFrameByLabel(label);
        if (!frame) continue;
        const auto top_left = frame->position.GetTopLeftOnScreen(root);
        const auto content_top_left = frame->position.GetContentTopLeft(root);
        const auto content_bottom_right = frame->position.GetContentBottomRight(root);
        const auto bottom_right = frame->position.GetBottomRightOnScreen(root);

        const float frame_height = content_top_left.y - top_left.y;
        const float btn_height = frame_height * .8f; // Make button 50% larger

        ImVec2 btn_size = ImVec2(btn_height, btn_height);

        // Adjust window position
        ImGui::SetNextWindowPos({content_bottom_right.x - btn_height * 2.f, top_left.y + frame_height * .1f});
        ImGui::SetNextWindowSize(btn_size);

        if (!ImGui::Begin(window_label, nullptr, GetWinFlags() | ImGuiWindowFlags_AlwaysAutoResize)) continue;
        ImVec2 btn_pos = ImGui::GetCursorScreenPos();

        // Create invisible button for interaction
        if (ImGui::InvisibleButton(btn_label, btn_size)) {
            ToggleHeroInventoryWindow(i + 1);
        }
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();

        // Determine which texture to use based on button state
        auto uv_min = &uv_normal[0];
        auto uv_max = &uv_normal[1];

        if (active) {
            uv_min = &uv_active[0];
            uv_max = &uv_active[1];
        }
        else if (hovered) {
            uv_min = &uv_hover[0];
            uv_max = &uv_hover[1];
        }

        // Draw the button texture
        ImGui::SetCursorScreenPos(btn_pos);
        ImGui::Image(*icon_texture, btn_size, *uv_min, *uv_max);

        ImGui::End();
    }

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();
}
void HeroEquipmentModule::Terminate()
{
    ToolboxModule::Terminate();
    Cleanup();
}
