#include "stdafx.h"

#include "InventoryOverlayWidget.h"

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <Utils/GuiUtils.h>

#include "Color.h"
#include "Defines.h"

namespace GW {
    struct Item;
}

namespace {
    Color text_color = Colors::White();
    Color border_color = Colors::Black();

    struct InventorySlotInfo {
        GuiUtils::EncString name = nullptr;
    };
    std::unordered_map<uint32_t, InventorySlotInfo*> inventory_slot_frames;
    GW::UI::UIInteractionCallback inventory_slot_ui_callback = nullptr, inventory_slot_ui_callback_ret = nullptr;

    // Callback for generic inventory slots; called from loads of different parent frames, but behaves the same
    void OnInventorySlot_UICallback(GW::UI::InteractionMessage* message, void* wParam, void* lParam) {
        GW::Hook::EnterHook();
        inventory_slot_ui_callback_ret(message, wParam, lParam);
        switch (message->message_id) {
        case GW::UI::UIMessage::kDestroyFrame: {
            const auto found = inventory_slot_frames.find(message->frame_id);
            if (found != inventory_slot_frames.end()) {
                delete found->second;
                inventory_slot_frames.erase(found);
            }
        } break;
        case GW::UI::UIMessage::kInitFrame: {
            const auto found = inventory_slot_frames.find(message->frame_id);
            if (found == inventory_slot_frames.end()) {
                inventory_slot_frames[message->frame_id] = new InventorySlotInfo();
            }
        } break;

        }
        GW::Hook::LeaveHook();
    }

    // Grab item from inv slot frame via tooltip
    GW::Item* GetInventorySlotItem(const GW::UI::Frame* inv_slot_frame) {
        const auto tooltip = inv_slot_frame->tooltip_info;
        if (!tooltip) return nullptr;
        return GW::Items::GetItemById(*tooltip->payload);
    }
}


void InventoryOverlayWidget::Initialize()
{
    ToolboxWidget::Initialize();

    inventory_slot_ui_callback = (GW::UI::UIInteractionCallback)GW::Scanner::Find("\x3d\xef\x00\x00\x10\x0f\x87\x9b\x09\x00\x00", "xxxxxxxxxxx", -0x21);
    if (inventory_slot_ui_callback) {
        GW::Hook::CreateHook((void**)&inventory_slot_ui_callback, OnInventorySlot_UICallback, (void**)&inventory_slot_ui_callback_ret);
        GW::Hook::EnableHooks(inventory_slot_ui_callback);
    }
}

void InventoryOverlayWidget::Terminate()
{
    ToolboxWidget::Terminate();
    if (inventory_slot_ui_callback) {
        GW::Hook::DisableHooks(inventory_slot_ui_callback);
        inventory_slot_ui_callback = nullptr;
    }
    GW::Hook::RemoveHook(inventory_slot_ui_callback);
    for (const auto inventory_slot_info : inventory_slot_frames | std::views::values) {
        delete inventory_slot_info;
    }
    inventory_slot_frames.clear();
}

void InventoryOverlayWidget::Draw(IDirect3DDevice9*)
{
    if (!GW::Map::GetIsMapLoaded() || !visible) {
        return;
    }

    for (auto [frame_id, info] : inventory_slot_frames) {
        const auto frame = GW::UI::GetFrameById(frame_id);
        if (!(frame && frame->IsCreated() && frame->IsVisible()))
            continue;

        const ImVec2 top_left = {
            frame->position.GetRelativeTopLeft().x,
            frame->position.GetRelativeTopLeft().y,
        };
        const ImVec2 bottom_right = {
            frame->position.GetRelativeBottomRight().x,
            frame->position.GetRelativeBottomRight().y,
        };
        ImGui::GetBackgroundDrawList()->AddRect(top_left, bottom_right, border_color);
        if (const auto item = GetInventorySlotItem(frame)) {
            std::string item_id_str = std::to_string(item->item_id);
            const auto text_size = ImGui::CalcTextSize(item_id_str.c_str());
            const auto box_width = bottom_right.x - top_left.x;
            const auto x_offset = (box_width - text_size.x) / 2.f;
            const ImVec2 text_pos = { top_left.x + x_offset, top_left.y };
            ImGui::GetBackgroundDrawList()->AddText(text_pos, text_color, item_id_str.c_str());
            if (ImGui::IsMouseHoveringRect(top_left, bottom_right, false)) {
                info->name.reset(item->name_enc);
                ImGui::SetTooltip("%s", info->name.string().c_str());
            }
        }
    }
}

void InventoryOverlayWidget::DrawSettingsInternal()
{
    ToolboxWidget::DrawSettingsInternal();
    Colors::DrawSettingHueWheel("Text Color", &text_color);
    Colors::DrawSettingHueWheel("Text Color", &border_color);
}

void InventoryOverlayWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    LOAD_COLOR(text_color);
    LOAD_COLOR(border_color);
}

void InventoryOverlayWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    SAVE_COLOR(text_color);
    SAVE_COLOR(border_color);
}
