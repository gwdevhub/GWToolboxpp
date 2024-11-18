#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Item.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Widgets/InventoryOverlayWidget.h>

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Managers/ItemMgr.h>
namespace {
    bool show_in_outpost = true;
    bool show_in_explorable = true;

    std::vector<GW::UI::Frame*> inventory_slot_frames;

    std::unordered_map<std::wstring, ImColor> item_highlight_colors_by_name = {
        { {0x8101, 0x222F, 0xE5B2, 0xB4FB, 0x1FED},IM_COL32_WHITE} // Creme Brulee in white
    };
    std::unordered_map<GW::Constants::ItemType, ImColor> item_highlight_colors_by_type = {
    { GW::Constants::ItemType::Usable,ImColor(255, 204, 86)} // Consumable items in gold
    };

    void HighlightFrame(const GW::UI::Frame* frame, const ImColor& color, const GW::UI::Frame* root = GW::UI::GetRootFrame(), ImDrawList* draw_list = ImGui::GetBackgroundDrawList()) {
        // @TODO: Check parent frame scrollable position and crop to fit.
        const auto top_left = frame->position.GetTopLeftOnScreen(root);
        const auto bottom_right = frame->position.GetBottomRightOnScreen(root);
        draw_list->AddRect({ top_left.x, top_left.y }, { bottom_right.x, bottom_right.y }, color);
    }

    GW::UI::UIInteractionCallback InventorySlot_UICallback_Func;
    GW::UI::UIInteractionCallback InventorySlot_UICallback_Ret;

    void OnInventorySlot_UICallback(GW::UI::InteractionMessage* message, void* wParam, void* lParam) {
        GW::Hook::EnterHook();
        GW::UI::Frame* frame = nullptr;
        switch (message->message_id) {
        case GW::UI::UIMessage::kInitFrame:
            InventorySlot_UICallback_Ret(message, wParam, lParam);
            frame = GW::UI::GetFrameById(message->frame_id);
            ASSERT(frame);
            inventory_slot_frames.push_back(frame);
            return GW::Hook::LeaveHook();
        case GW::UI::UIMessage::kDestroyFrame:
            frame = GW::UI::GetFrameById(message->frame_id);
            ASSERT(frame);
            inventory_slot_frames.erase(std::remove(inventory_slot_frames.begin(), inventory_slot_frames.end(), frame), inventory_slot_frames.end());
            return GW::Hook::LeaveHook();
        }
        InventorySlot_UICallback_Ret(message, wParam, lParam);
        return GW::Hook::LeaveHook();
    }

    GW::Item* GetInventorySlotItem(GW::UI::Frame* frame) {
        if (!(frame && frame->tooltip_info && frame->tooltip_info->payload_len))
            return nullptr;
        return GW::Items::GetItemById(*(uint32_t*)frame->tooltip_info->payload);
    }

}
void InventoryOverlayWidget::Initialize() {
    ToolboxWidget::Initialize();
    InventorySlot_UICallback_Func = (GW::UI::UIInteractionCallback)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("InvSlot.cpp","!m_dragOverlayTexture"),0xfff);
    if (InventorySlot_UICallback_Func) {
        GW::Hook::CreateHook((void**)&InventorySlot_UICallback_Func, OnInventorySlot_UICallback, (void**)&InventorySlot_UICallback_Ret);
        GW::Hook::EnableHooks(InventorySlot_UICallback_Func);
    }
#if _DEBUG
    ASSERT(InventorySlot_UICallback_Func);
#endif
}
void InventoryOverlayWidget::SignalTerminate() {
    ToolboxWidget::SignalTerminate();
    if (InventorySlot_UICallback_Func) {
        GW::Hook::RemoveHook(InventorySlot_UICallback_Func);
        InventorySlot_UICallback_Func = 0;
    }
}

void InventoryOverlayWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    // @TODO
}

void InventoryOverlayWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    // @TODO
}

void InventoryOverlayWidget::DrawSettingsInternal()
{
    ToolboxWidget::DrawSettingsInternal();
    ImGui::SameLine();
    ImGui::Checkbox("Show overlay in outpost", &show_in_outpost);
    ImGui::SameLine();
    ImGui::Checkbox("Show overlay explorable", &show_in_explorable);

    // @TODO: Adding item colours by name (usability would be to add this to a context menu to manually add)
    // @TODO: Adding item colours by type

}

void InventoryOverlayWidget::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    const auto instance_type = GW::Map::GetInstanceType();
    switch (instance_type) {
    case GW::Constants::InstanceType::Outpost:
        if (!show_in_outpost) return;
        break;
    case GW::Constants::InstanceType::Explorable:
        if (!show_in_explorable) return;
        break;
    default:
        return;
    }
    if (inventory_slot_frames.empty())
        return;

    const auto viewport = ImGui::GetMainViewport();
    //const auto& viewport_offset = viewport->Pos;
    const auto draw_list = ImGui::GetBackgroundDrawList(viewport);

    const auto root = GW::UI::GetRootFrame();

    for (const auto frame : inventory_slot_frames) {
        const ImColor col = IM_COL32_WHITE;
        const auto item = GetInventorySlotItem(frame);
        if (!(item && item->name_enc)) continue;
        auto found_by_name = item_highlight_colors_by_name.find(item->name_enc);
        if (found_by_name != item_highlight_colors_by_name.end()) {
            HighlightFrame(frame, found_by_name->second, root, draw_list);
            continue;
        }
        const auto found_by_type = item_highlight_colors_by_type.find(item->type);
        if (found_by_type != item_highlight_colors_by_type.end()) {
            HighlightFrame(frame, found_by_type->second, root, draw_list);
            continue;
        }

        // @Enhancement: Other ways of colouring by item detail
    }
}
