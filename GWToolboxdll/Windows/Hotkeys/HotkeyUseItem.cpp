#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/Managers/ItemMgr.h>

#include <ImGuiAddons.h>
#include <Logger.h>

#include <Windows/Hotkeys/HotkeyUseItem.h>

HotkeyUseItem::HotkeyUseItem(const ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    if (!ini) {
        return;
    }
    item_id = static_cast<size_t>(ini->GetLongValue(section, "ItemID", 0));
    strcpy_s(name, ini->GetValue(section, "ItemName", ""));
    bag_idx = static_cast<size_t>(ini->GetLongValue(section, "Bag", bag_idx));
    slot_idx = static_cast<size_t>(ini->GetLongValue(section, "Slot", slot_idx));
    use_by = static_cast<UseBy>(ini->GetLongValue(section, "UseBy", use_by));
}

void HotkeyUseItem::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "UseBy", use_by);
    ini->SetLongValue(section, "ItemID", item_id);
    ini->SetValue(section, "ItemName", name);
    ini->SetLongValue(section, "Bag", bag_idx);
    ini->SetLongValue(section, "Slot", slot_idx);
}

int HotkeyUseItem::Description(char* buf, const size_t bufsz)
{
    if (use_by == SLOT) {
        return snprintf(buf, bufsz, "Use item in bag %d slot %d", bag_idx, slot_idx);
    }
    if (!name[0]) {
        return snprintf(buf, bufsz, "Use #%d", item_id);
    }
    return snprintf(buf, bufsz, "Use %s", name);
}

bool HotkeyUseItem::DrawSettings()
{
    bool hotkey_changed = false;

    constexpr const char* bags[6] = {"None", "Backpack", "Belt Pouch", "Bag 1", "Bag 2", "Equipment Pack"};
    ImGui::TextUnformatted("Use By: ");
    ImGui::SameLine();
    hotkey_changed |= ImGui::RadioButton("Item", (int*)&use_by, ITEM);
    ImGui::ShowHelp("Find and use an item by its model ID, regardless of location in inventory.");
    ImGui::SameLine();
    hotkey_changed |= ImGui::RadioButton("Slot", (int*)&use_by, SLOT);
    ImGui::ShowHelp("Use the item in a specific bag slot, regardless of what it is.\nUseful for quick equipment swapping or using consumables regardless of type.");
    if (use_by == SLOT) {
        hotkey_changed |= ImGui::Combo("Bag", (int*)&bag_idx, bags, _countof(bags));
        hotkey_changed |= ImGui::InputInt("Slot (1-25)", (int*)&slot_idx);
    }
    else {
        hotkey_changed |= ImGui::InputInt("Item Model ID", (int*)&item_id);
        hotkey_changed |= ImGui::InputText("Item Name", name, _countof(name));
    }
    hotkey_changed |= ImGui::Checkbox("Display error message on failure", &show_error_on_failure);
    hotkey_changed = hotkey_changed || TBHotkey::DrawSettings();
    return hotkey_changed;
}

void HotkeyUseItem::Execute()
{
    if (!CanUse()) {
        return;
    }

    if (use_by == SLOT) {
        const auto bag_id = static_cast<GW::Constants::Bag>(bag_idx);
        if (bag_id < GW::Constants::Bag::Backpack || bag_id > GW::Constants::Bag::Equipment_Pack || slot_idx < 1 || slot_idx > 25) {
            show_error_on_failure && (Log::Error("Invalid bag slot %d/%d!", bag_id, slot_idx), true);
            return;
        }
        const GW::Bag* bag = GW::Items::GetBag(bag_id);
        if (!bag) {
            show_error_on_failure && (Log::Error("Bag #%d not found!", bag_id), true);
            return;
        }
        const GW::ItemArray& items = bag->items;
        GW::Item* item = (items.valid() && slot_idx <= items.size()) ? items[slot_idx - 1] : nullptr;
        if (!(item && item->bag == bag)) {
            show_error_on_failure && (Log::Error("No item in bag %d slot %d!", bag_id, slot_idx), true);
            return;
        }
        if (!GW::Items::UseItem(item) && show_error_on_failure) {
            Log::Error("Failed to use item in bag %d slot %d!", bag_id, slot_idx);
        }
        return;
    }

    if (item_id == 0) {
        return;
    }

    bool used = GW::Items::UseItemByModelId(item_id, 1, 4);
    if (!used &&
        GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        used = GW::Items::UseItemByModelId(item_id, 8, 16);
    }

    if (!used && show_error_on_failure) {
        if (name[0] == '\0') {
            Log::Error("Item #%d not found!", item_id);
        }
        else {
            Log::Error("%s not found!", name);
        }
    }
}
