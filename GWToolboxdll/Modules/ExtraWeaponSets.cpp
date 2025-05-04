#include "stdafx.h"

#include "ExtraWeaponSets.h"
#include <GWCA/GameEntities/Item.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <Windows/Hotkeys.h>
#include <GWCA/Utilities/Hooker.h>

namespace {

    constexpr uint32_t extra_weapon_set_count = 4;

    GW::WeaponSet extra_weapon_sets[extra_weapon_set_count] = {0};

    HotkeyEquipItemAttributes weapon_set_item_attributes[_countof(GW::Inventory::weapon_sets) + _countof(extra_weapon_sets)][2];

    bool extra_weapon_sets_created = false;

    GW::UI::UIInteractionCallback OnWeaponBarUICallback_Func = 0, OnWeaponBarUICallback_Ret = 0;
    GW::UI::UIInteractionCallback OnWeaponSetUICallback_Func = 0, OnWeaponSetUICallback_Ret = 0;

    GW::Item* FindMatchingItem(HotkeyEquipItemAttributes& attributes)
    {
        const auto inv = GW::Items::GetInventory();
        if (!inv) return nullptr;
        const GW::Constants::Bag bag_ids_to_check[] = {GW::Constants::Bag::Equipped_Items, GW::Constants::Bag ::Equipment_Pack, GW::Constants::Bag::Bag_2, GW::Constants::Bag::Bag_1, GW::Constants::Bag::Belt_Pouch, GW::Constants::Bag::Backpack}; 
        for (const auto bag_id : bag_ids_to_check)
        {
            const auto bag = GW::Items::GetBag(bag_id);
            if (!bag) 
                continue;
            for (const auto item : bag->items) {
                if (attributes.check(item)) {
                    return item;
                }
            }
        }
        return nullptr;
    }

    void AssignExtraWeaponSetItems() {
        const auto found = GW::Items::GetItemByModelId(15971);
        weapon_set_item_attributes[0][0].set(found);

        for (size_t i = 0; i < _countof(weapon_set_item_attributes); i++) {
            const auto weapon_set = GetWeaponSet(i);
            weapon_set_item_attributes[i][0].set(weapon_set->weapon);
            weapon_set_item_attributes[i][1].set(weapon_set->offhand);
        }
    }

    GW::WeaponSet* GetWeaponSet(uint32_t weapon_set_id);

    bool GetWeaponSetItems(uint32_t weapon_set_id, GW::Item** item_id_1, GW::Item** item_id_2)
    {
        if (weapon_set_id < _countof(GW::Inventory::weapon_sets)) {
            const auto weapon_set = GetWeaponSet(weapon_set_id);
            *item_id_1 = weapon_set->weapon;
            *item_id_2 = weapon_set->offhand;
            return true;
        }
        weapon_set_id -= _countof(GW::Inventory::weapon_sets);
        if (weapon_set_id < _countof(weapon_set_item_attributes)) {
            *item_id_1 = FindMatchingItem(weapon_set_item_attributes[weapon_set_id][0]);
            *item_id_2 = FindMatchingItem(weapon_set_item_attributes[weapon_set_id][1]);
            return true;
        }
        return false;
    }

    void UpdateWeaponSetPositions(GW::UI::Frame* frame)
    {
        if (!frame) return;
        const auto first_weapon_set = GW::UI::GetChildFrame(frame, 0x10000000);
        const auto second_weapon_set = GW::UI::GetChildFrame(frame, 0x10000001);
        const auto third_weapon_set = GW::UI::GetChildFrame(frame, 0x10000002);
        const auto fourth_weapon_set = GW::UI::GetChildFrame(frame, 0x10000003);
        (first_weapon_set, second_weapon_set, third_weapon_set, fourth_weapon_set);
        for (size_t i = _countof(GW::Inventory::weapon_sets); i < _countof(GW::Inventory::weapon_sets) + _countof(extra_weapon_sets); ++i) {
            const auto weapon_set_frame = GW::UI::GetChildFrame(frame, 0x10000000 + i);
            const auto mirror_weapon_set_frame = weapon_set_frame ? GW::UI::GetChildFrame(frame, 0x10000000 + (i - 4)) : nullptr;
            if (!mirror_weapon_set_frame) continue;
            auto position_cpy = mirror_weapon_set_frame->position;
            // if (position_cpy.flags != 0x6) continue;
            const auto height = (position_cpy.bottom - position_cpy.top);
            if (!height) continue;
            position_cpy.top -= height;
            position_cpy.bottom -= height;
            GW::UI::SetFramePosition(weapon_set_frame, position_cpy);
            (weapon_set_frame);
        }
    }
    struct WeaponSetContext {
        float h0000;
        float h0004;
        uint32_t frame_id;
        uint32_t this_weapon_set_id;
        bool is_selected;
        float h0014;
        float scale; // .75 if not selected, 1.0 if selected
        void* h001c;
        uint32_t item_id1;
        uint32_t item_id2;
        uint32_t h0028;
    };
    static_assert(sizeof(WeaponSetContext) == 0x2c);

    struct WeaponBarContext {
        uint32_t frame_id;
        uint32_t current_weapon_set_id;
        uint32_t pending_weapon_set_id; // 0xffffffff when not waiting
    };
    static_assert(sizeof(WeaponBarContext) == 0xc);

    GW::WeaponSet* GetWeaponSet(uint32_t weapon_set_id)
    {
        if (weapon_set_id >= _countof(GW::Inventory::weapon_sets)) {
            weapon_set_id -= _countof(GW::Inventory::weapon_sets);
            
            ASSERT(weapon_set_id < _countof(extra_weapon_sets));

            auto& weapon_set = extra_weapon_sets[weapon_set_id];
            GetWeaponSetItems(weapon_set_id + _countof(GW::Inventory::weapon_sets), &weapon_set.weapon, &weapon_set.offhand);
            return &weapon_set;
        }
        const auto inv = GW::Items::GetInventory();
        if (!inv) return nullptr;
        return &inv->weapon_sets[weapon_set_id];
    }

    GW::WeaponSet* GetActiveWeaponSet(uint32_t* found_weapon_set_id)
    {
        const auto inv = GW::Items::GetInventory();
        if (!inv) return nullptr;
        *found_weapon_set_id = inv->active_weapon_set;
        return &inv->weapon_sets[inv->active_weapon_set];
    }

    GW::WeaponSet* GetInactiveWeaponSet(uint32_t* found_weapon_set_id)
    {
        const auto inv = GW::Items::GetInventory();
        if (!inv) return nullptr;
        for (size_t i = 0; i < _countof(GW::Inventory::weapon_sets); i++) {
            if (i == inv->active_weapon_set) continue;
            *found_weapon_set_id = i;
            return &inv->weapon_sets[i];
        }
        return nullptr;
    }


    void OnWeaponBar_UICallback(GW::UI::InteractionMessage* message, void* wparam, void* lparam)
    {
        GW::Hook::EnterHook();
        const auto context = message->wParam ? *(WeaponBarContext**)message->wParam : nullptr;
        (context);
        switch (message->message_id) {
            case GW::UI::UIMessage::kMouseClick2: {
                uint32_t actual_active_weapon_set_id = 0;
                GetActiveWeaponSet(&actual_active_weapon_set_id);
                OnWeaponBarUICallback_Ret(message, wparam, lparam);
                const auto packet = (GW::UI::UIPacket::kMouseAction*)wparam;
                if (packet->child_offset_id < 0x10000000 || packet->current_state != 6) break;
                if (packet->child_offset_id < 0x10000000 + _countof(GW::Inventory::weapon_sets)) {
                    // Standard weapon set frame clicked
                    uint32_t this_weapon_set_id = packet->child_offset_id - 0x10000000;
                    if (actual_active_weapon_set_id == this_weapon_set_id && context->current_weapon_set_id != this_weapon_set_id) {
                        GW::UI::UIPacket::kWeaponSwap weapon_swap;
                        weapon_swap.weapon_bar_frame_id = message->frame_id;
                        weapon_swap.weapon_set_id = packet->child_offset_id - 0x10000000;
                        context->pending_weapon_set_id = this_weapon_set_id;
                        GW::UI::SendFrameUIMessage(GW::UI::GetFrameById(message->frame_id), GW::UI::UIMessage::kWeaponSetSwapComplete, &weapon_swap);
                        context->pending_weapon_set_id = 0xffffffff;
                    }
                    break;
                }
                if (packet->child_offset_id < 0x10000000 + _countof(GW::Inventory::weapon_sets) + _countof(extra_weapon_sets)) {
                    // Extra weapon set frame clicked
                    GW::UI::UIPacket::kWeaponSwap weapon_swap;
                    weapon_swap.weapon_bar_frame_id = message->frame_id;
                    weapon_swap.weapon_set_id = packet->child_offset_id - 0x10000000;
                    context->pending_weapon_set_id = weapon_swap.weapon_set_id;
                    GW::UI::SendFrameUIMessage(GW::UI::GetFrameById(message->frame_id), GW::UI::UIMessage::kWeaponSetSwapComplete, &weapon_swap);
                    context->pending_weapon_set_id = 0xffffffff;
                }
            } break;
            case GW::UI::UIMessage::kWeaponSetSwapComplete: {
                OnWeaponBarUICallback_Ret(message, wparam, lparam);
                const auto frame = GW::UI::GetFrameById(message->frame_id);
                const auto packet = (GW::UI::UIPacket::kWeaponSwap*)wparam;
                for (size_t i = 0; i < _countof(GW::Inventory::weapon_sets) + _countof(extra_weapon_sets); ++i) {
                    GW::UI::SendFrameUIMessage(GW::UI::GetChildFrame(frame, 0x10000000 + i), GW::UI::UIMessage::kWeaponSetSwapComplete, packet);
                }
            } break;
            case GW::UI::UIMessage::kSetLayout: {
                OnWeaponBarUICallback_Ret(message, wparam, lparam);
                UpdateWeaponSetPositions(GW::UI::GetFrameById(message->frame_id));
            } break;
            default:
                OnWeaponBarUICallback_Ret(message, wparam, lparam);
                break;
        }
        GW::Hook::LeaveHook();
    }


    void OnWeaponSet_UICallback(GW::UI::InteractionMessage* message, void* wparam, void* lparam)
    {
        GW::Hook::EnterHook();
        const auto context = message->wParam ? *(WeaponSetContext**)message->wParam : nullptr;
        switch (message->message_id) {
            case GW::UI::UIMessage::kResize: {
                if (!context || context->this_weapon_set_id < _countof(GW::Inventory::weapon_sets)) {
                    OnWeaponBarUICallback_Ret(message, wparam, lparam);
                    break;
                }
                // When the widget resizes, it tries to re-grab the inventory info - we need to swap it out here to avoid an assertion failure
                const auto original_weapon_set_id = context->this_weapon_set_id;
                const auto overwrite_weapon_set = GetInactiveWeaponSet(&context->this_weapon_set_id);
                ASSERT(overwrite_weapon_set);
                const auto original_overwrite_weapon_set = *overwrite_weapon_set;
                *overwrite_weapon_set = *GetWeaponSet(original_weapon_set_id);

                // TODO: Switch for whether this weapon set is supposed to be selected!
                OnWeaponSetUICallback_Ret(message, wparam, lparam);

                // Swap it back when done
                *overwrite_weapon_set = original_overwrite_weapon_set;
                context->this_weapon_set_id = original_weapon_set_id;
            } break;
            default:
                OnWeaponSetUICallback_Ret(message, wparam, lparam);
                break;
        }
        GW::Hook::LeaveHook();
    }

    bool HookUICallbacks() {
        if (!OnWeaponBarUICallback_Func) {
            const auto frame = GW::UI::GetFrameByLabel(L"WeaponBar");
            if (!(frame && frame->frame_callbacks.size())) return false;
            OnWeaponBarUICallback_Func = frame->frame_callbacks[0].callback;
            GW::Hook::CreateHook((void**)&OnWeaponBarUICallback_Func, OnWeaponBar_UICallback, (void**)&OnWeaponBarUICallback_Ret);
            GW::Hook::EnableHooks(OnWeaponBarUICallback_Func);
        }
        if (!OnWeaponSetUICallback_Func) {
            const auto frame = GW::UI::GetFrameByLabel(L"WeaponBar");
            if (!(frame && frame->frame_callbacks.size())) return false;
            for (size_t i = 0; i < _countof(GW::Inventory::weapon_sets) && !OnWeaponSetUICallback_Func; i++) {
                const auto weapon_set_frame = GW::UI::GetChildFrame(frame, 0x10000000 + i);
                if (!(weapon_set_frame && weapon_set_frame->frame_callbacks.size())) continue;
                OnWeaponSetUICallback_Func = weapon_set_frame->frame_callbacks[0].callback;
                GW::Hook::CreateHook((void**)&OnWeaponSetUICallback_Func, OnWeaponSet_UICallback, (void**)&OnWeaponSetUICallback_Ret);
                GW::Hook::EnableHooks(OnWeaponSetUICallback_Func);
            }
        }
        return OnWeaponBarUICallback_Func && OnWeaponSetUICallback_Func;
    }
    void UnhookUICallbacks() {
        if (OnWeaponBarUICallback_Func) {
            GW::Hook::RemoveHook(OnWeaponBarUICallback_Func);
            OnWeaponBarUICallback_Func = 0;
        }
        if (OnWeaponSetUICallback_Func) {
            GW::Hook::RemoveHook(OnWeaponSetUICallback_Func);
            OnWeaponSetUICallback_Func = 0;
        }
    }

    void AddExtraWeaponSets()
    {
        if (!HookUICallbacks()) return;
        const auto frame = GW::UI::GetFrameByLabel(L"WeaponBar");
        if (!(frame && frame->frame_callbacks.size())) return;

        bool added = false;
        for (size_t i = _countof(GW::Inventory::weapon_sets); i < _countof(GW::Inventory::weapon_sets) + _countof(extra_weapon_sets); ++i) {
            if (GW::UI::GetChildFrame(frame, 0x10000000 + i)) {
                continue;
            }
            const auto label = std::format(L"Slot{}", i + 1);
            GW::UI::CreateUIComponent(frame->frame_id, 0, 0x10000000 + i, OnWeaponSetUICallback_Func, (void*)i, label.c_str());
            added = true;
        }
        if (added) {
            UpdateWeaponSetPositions(frame);
        }
        AssignExtraWeaponSetItems();
        extra_weapon_sets_created = true;
    }
    void RemoveExtraWeaponSets()
    {
        if (!extra_weapon_sets_created) return;
        const auto frame = GW::UI::GetFrameByLabel(L"WeaponBar");
        if (!frame) return;
        for (size_t i = 0; i < _countof(extra_weapon_sets); ++i) {
            GW::UI::DestroyUIComponent(GW::UI::GetChildFrame(frame, 0x10000000 + _countof(GW::Inventory::weapon_sets)  + i));
        }
        GW::UI::TriggerFrameRedraw(frame);
        extra_weapon_sets_created = false;
    }

    GW::HookEntry OnPostUIMessage_Entry;
    void OnPostUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void*, void*)
    {
        if (status->blocked) return;
        switch (message_id) {
            case GW::UI::UIMessage::kMapLoaded: {
                AddExtraWeaponSets();
            } break;
        }
    }

} // namespace

void ExtraWeaponSets::Initialize() {
    ToolboxModule::Initialize();

    GW::GameThread::Enqueue(AddExtraWeaponSets);
    GW::UI::RegisterUIMessageCallback(&OnPostUIMessage_Entry, GW::UI::UIMessage::kMapLoaded, OnPostUIMessage,0x8000);
}

void ExtraWeaponSets::SignalTerminate()
{
    GW::GameThread::Enqueue(RemoveExtraWeaponSets);
}

bool ExtraWeaponSets::CanTerminate()
{
    return !extra_weapon_sets_created;
}

void ExtraWeaponSets::Terminate()
{
    ToolboxModule::Terminate();
    if (extra_weapon_sets_created) RemoveExtraWeaponSets();
    UnhookUICallbacks();
    GW::UI::RemoveUIMessageCallback(&OnPostUIMessage_Entry);
}
