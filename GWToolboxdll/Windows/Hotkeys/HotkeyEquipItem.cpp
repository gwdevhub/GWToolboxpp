#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <ImGuiAddons.h>
#include <Logger.h>

#include <Windows/Hotkeys/HotkeyEquipItem.h>
#include <Timer.h>

namespace {
    // @Cleanup: when toolbox closes, this array isn't freed properly
    std::map<GW::Constants::Bag, std::vector<HotkeyEquipItemAttributes*>> available_items;
}

HotkeyEquipItemAttributes::HotkeyEquipItemAttributes(const GW::Item* item)
{
    if (!item)
        set();
    else
        set(item->model_id, item->complete_name_enc ? item->complete_name_enc : item->name_enc, item->info_string, item->mod_struct, item->mod_struct_size);
}

HotkeyEquipItemAttributes::~HotkeyEquipItemAttributes()
{
    if (mod_struct) {
        delete mod_struct;
        mod_struct = nullptr;
        mod_struct_size = 0;
    }
}

HotkeyEquipItemAttributes::HotkeyEquipItemAttributes(const uint32_t _model_id, const wchar_t* _name_enc, const wchar_t* _info_string, const GW::ItemModifier* _mod_struct, const size_t _mod_struct_size)
{
    set(_model_id, _name_enc, _info_string, _mod_struct, _mod_struct_size);
}

HotkeyEquipItemAttributes* HotkeyEquipItemAttributes::set(const uint32_t _model_id, const wchar_t* _name_enc, const wchar_t* _info_string, const GW::ItemModifier* _mod_struct, const size_t _mod_struct_size)
{
    model_id = _model_id;
    enc_name.reset(_name_enc);
    enc_desc.reset(_info_string);
    if (mod_struct) {
        delete mod_struct;
        mod_struct = nullptr;
        mod_struct_size = 0;
    }
    mod_struct_size = _mod_struct_size;
    if (mod_struct_size) {
        ASSERT(_mod_struct);
        const size_t bytes = _mod_struct_size * sizeof(*_mod_struct);
        mod_struct = static_cast<GW::ItemModifier*>(malloc(bytes));
        memcpy(mod_struct, _mod_struct, bytes);
    }
    return this;
}

HotkeyEquipItemAttributes* HotkeyEquipItemAttributes::set(const HotkeyEquipItemAttributes& other)
{
    return set(other.model_id, other.enc_name.encoded().c_str(), other.enc_desc.encoded().c_str(), other.mod_struct, other.mod_struct_size);
}

bool HotkeyEquipItemAttributes::check(const GW::Item* item) const
{
    if (!item || item->model_id != model_id || item->mod_struct_size != mod_struct_size) {
        return false;
    }
    if (wcscmp(item->complete_name_enc ? item->complete_name_enc : item->name_enc, enc_name.encoded().c_str()) != 0) {
        return false;
    }
    if (mod_struct_size == item->mod_struct_size && memcmp(mod_struct, item->mod_struct, item->mod_struct_size * sizeof(*item->mod_struct)) != 0) {
        return false;
    }
    return true;
}

HotkeyEquipItem::HotkeyEquipItem(ToolboxIni* ini, const char* section)
    : TBHotkey(ini, section)
{
    if (!ini) return;
    // @Cleanup: Add error handling
    bag_idx = static_cast<size_t>(ini->GetLongValue(section, "Bag", bag_idx));
    slot_idx = static_cast<size_t>(ini->GetLongValue(section, "Slot", slot_idx));
    equip_by = static_cast<EquipBy>(ini->GetLongValue(section, "EquipBy", equip_by));
    if (equip_by == ITEM) {
        uint32_t model_id = ini->GetLongValue(section, "ModelId", 0);
        std::string in = ini->GetValue(section, "EncodedName", "");
        std::wstring enc_name;
        if (!in.empty()) {
            ASSERT(GuiUtils::IniToArray(in, enc_name));
        }
        in = ini->GetValue(section, "EncodedDesc", "");
        std::wstring enc_desc;
        if (!in.empty()) {
            ASSERT(GuiUtils::IniToArray(in, enc_desc));
        }
        in = ini->GetValue(section, "ModStruct", "");
        std::vector<uint32_t> mod_structs;
        if (!in.empty()) {
            ASSERT(GuiUtils::IniToArray(in, mod_structs));
        }
        item_attributes.set(model_id, enc_name.c_str(), enc_desc.c_str(), mod_structs.size() ? (GW::ItemModifier*)mod_structs.data() : nullptr, mod_structs.size());
    }
}

void HotkeyEquipItem::Save(ToolboxIni* ini, const char* section) const
{
    TBHotkey::Save(ini, section);
    ini->SetLongValue(section, "EquipBy", equip_by);
    ini->SetLongValue(section, "Bag", bag_idx);
    ini->SetLongValue(section, "Slot", slot_idx);
    if (equip_by == ITEM) {
        ini->SetLongValue(section, "ModelId", item_attributes.model_id);
        std::string out;
        GuiUtils::ArrayToIni(item_attributes.enc_name.encoded().c_str(), &out);
        ini->SetValue(section, "EncodedName", out.c_str());
        ASSERT(GuiUtils::ArrayToIni(item_attributes.enc_desc.encoded().c_str(), &out));
        ini->SetValue(section, "EncodedDesc", out.c_str());
        ASSERT(GuiUtils::ArrayToIni((uint32_t*)item_attributes.mod_struct, item_attributes.mod_struct_size, &out));
        ini->SetLongValue(section, "ModStructSize", item_attributes.mod_struct_size);
        ini->SetValue(section, "ModStruct", out.c_str());
    }
}

int HotkeyEquipItem::Description(char* buf, const size_t bufsz)
{
    if (equip_by == SLOT) {
        return snprintf(buf, bufsz, "Equip Item in bag %d slot %d", bag_idx, slot_idx);
    }
    return snprintf(buf, bufsz, "Equip %s", item_attributes.name().c_str());
}

bool HotkeyEquipItem::Draw()
{
    bool hotkey_changed = false;
    constexpr const char* bags[6] = {"None", "Backpack", "Belt Pouch", "Bag 1", "Bag 2", "Equipment Pack"};
    ImGui::TextUnformatted("Equip By: ");
    ImGui::SameLine();
    hotkey_changed |= ImGui::RadioButton("Item", (int*)&equip_by, ITEM);
    ImGui::ShowHelp("Find and equip an item by its attributes, regardless of location in inventory.");
    ImGui::SameLine();
    hotkey_changed |= ImGui::RadioButton("Slot", (int*)&equip_by, SLOT);
    ImGui::ShowHelp("Find and equip an item in a specific slot, regarless of what it is.\nUseful for using the same hotkey across characters.");
    if (equip_by == SLOT) {
        hotkey_changed |= ImGui::Combo("Bag", (int*)&bag_idx, bags, _countof(bags));
        hotkey_changed |= ImGui::InputInt("Slot (1-25)", (int*)&slot_idx);
    }
    else {
        static bool need_to_fetch_bag_items = false;
        if (!item_attributes.model_id) {
            ImGui::TextUnformatted("No item chosen");
        }
        else {
            ImGui::TextUnformatted(item_attributes.name().c_str());
            if (!item_attributes.desc().empty()) {
                ImGui::TextUnformatted(item_attributes.desc().c_str());
            }
        }
        if (ImGui::Button("Edit")) {
            need_to_fetch_bag_items = true;
            ImGui::OpenPopup("Choose Item to Equip");
        }
        if (ImGui::BeginPopupModal("Choose Item to Equip", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (need_to_fetch_bag_items) {
                // Free available_items ptrs
                for (auto& it : available_items) {
                    for (auto atts : it.second) {
                        delete atts;
                    }
                    it.second.clear();
                }
                available_items.clear();
                for (auto i = GW::Constants::Bag::Backpack; i <= GW::Constants::Bag::Equipment_Pack; i = (GW::Constants::Bag)((size_t)i + 1)) {
                    GW::Bag* bag = GW::Items::GetBag(i);
                    if (!bag) {
                        continue;
                    }
                    GW::ItemArray& items = bag->items;
                    for (size_t slot = 0, size = items.size(); slot < size; slot++) {
                        const GW::Item* cur_item = items[slot];
                        if (!IsEquippable(cur_item)) {
                            continue;
                        }
                        available_items[i].push_back(new HotkeyEquipItemAttributes(cur_item));
                    }
                }
                need_to_fetch_bag_items = false;
            }
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
            for (auto& it : available_items) {
                auto& items = it.second;
                if (items.empty()) {
                    continue;
                }

                ImGui::TextUnformatted(bags[(uint32_t)it.first]);
                ImGui::Indent();
                for (auto& ai : it.second) {
                    ImGui::PushID(&ai);
                    if (ImGui::Button(ai->name().c_str())) {
                        item_attributes.set(*ai);
                        hotkey_changed = true;
                        ImGui::CloseCurrentPopup();
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(ai->desc().c_str());
                        ImGui::EndTooltip();
                    }
                    ImGui::PopID();
                }
                ImGui::Unindent();
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            if (ImGui::Button("Cancel###cancel_hotkey_equip_item")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
    hotkey_changed |= ImGui::Checkbox("Display error message on failure", &show_error_on_failure);
    return hotkey_changed;
}

bool HotkeyEquipItem::IsEquippable(const GW::Item* _item)
{
    if (!_item) {
        return false;
    }
    switch (static_cast<GW::Constants::ItemType>(_item->type)) {
        case GW::Constants::ItemType::Axe:
        case GW::Constants::ItemType::Boots:
        case GW::Constants::ItemType::Bow:
        case GW::Constants::ItemType::Chestpiece:
        case GW::Constants::ItemType::Offhand:
        case GW::Constants::ItemType::Gloves:
        case GW::Constants::ItemType::Hammer:
        case GW::Constants::ItemType::Headpiece:
        case GW::Constants::ItemType::Leggings:
        case GW::Constants::ItemType::Wand:
        case GW::Constants::ItemType::Shield:
        case GW::Constants::ItemType::Staff:
        case GW::Constants::ItemType::Sword:
        case GW::Constants::ItemType::Daggers:
        case GW::Constants::ItemType::Scythe:
        case GW::Constants::ItemType::Spear:
        case GW::Constants::ItemType::Costume_Headpiece:
        case GW::Constants::ItemType::Costume:
            break;
        default:
            return false;
    }
    return true;
    // 2021-05-02: Disabled customised check, conflicts with obfuscator module, not worth the hassle - the hotkey will fail with a message on timeout anyway - Jon
    /*
    if (!_item->customized)
        return true;
    GW::GameContext *g = GW::GetGameContext();
    GW::CharContext *c = g ? g->character : nullptr;
    return c && c->player_name &&
           wcscmp(c->player_name, _item->customized) == 0;*/
}

GW::Item* HotkeyEquipItem::FindMatchingItem(const GW::Constants::Bag _bag_idx, GW::Bag** bag) const
{
    *bag = GW::Items::GetBag(_bag_idx);
    if (!(*bag)) {
        return nullptr;
    }
    GW::ItemArray& items = (*bag)->items;
    for (const auto _item : items) {
        if (item_attributes.check(_item)) {
            return _item;
        }
    }
    return nullptr;
}

void HotkeyEquipItem::Execute()
{
    if (!CanUse()) {
        return;
    }
    GW::Bag* bag = nullptr;
    GW::Item* item = nullptr;
    if (!ongoing) {
        item_id = 0;
        if (equip_by == SLOT) {
            const auto bag_id = static_cast<GW::Constants::Bag>(bag_idx);
            if (bag_id < GW::Constants::Bag::Backpack || bag_id > GW::Constants::Bag::Equipment_Pack || slot_idx < 1 || slot_idx > 25) {
                show_error_on_failure && (Log::Error("Invalid bag slot %d/%d!", bag_id, slot_idx), true);
                return;
            }
            bag = GW::Items::GetBag(bag_id);
            if (!bag) {
                show_error_on_failure && (Log::Error("Bag #%d not found!", bag_id), true);
                return;
            }
            GW::ItemArray& items = bag->items;
            item = (items.valid() && slot_idx <= items.size()) ? items[slot_idx - 1] : nullptr;
            if (!(item && item->bag == bag)) {
                show_error_on_failure && (Log::Error("Invalid bag slot %d/%d!", bag_id, slot_idx), true);
                return;
            }
        }
        else {
            item = FindMatchingItem(GW::Constants::Bag::Equipped_Items, &bag);
            for (auto i = GW::Constants::Bag::Backpack; i <= GW::Constants::Bag::Equipment_Pack && !item; i = (GW::Constants::Bag)((size_t) i + 1)) {
                item = FindMatchingItem(i, &bag);
            }
            if (!(item && item->bag == bag)) {
                show_error_on_failure && (Log::Error("No equippable item matching your hotkey"), true);
                return;
            }
        }

        if (!(item && IsEquippable(item))) {
            show_error_on_failure && (Log::Error("No equippable item in bag %d slot %d", bag_idx, slot_idx), true);
            return;
        }
        item_id = item->item_id;
        ongoing = true;
        start_time = TIMER_INIT();
        last_try = 0;
    }
    ongoing = false;
    if (TIMER_DIFF(start_time) > 5000) {
        show_error_on_failure && (Log::Error("Failed to equip item"), true);
        return;
    }
    item = GW::Items::GetItemById(item_id);
    if (!(item && IsEquippable(item))) {
        show_error_on_failure && (Log::Error("Failed to equip item"), true);
        return;
    }
    if (item->bag && item->bag->bag_type == GW::Constants::BagType::Equipped) {
        return; // Success!
    }
    const GW::AgentLiving* p = GW::Agents::GetControlledCharacter();
    if (!(p && p->GetIsAlive())) {
        show_error_on_failure && (Log::Error("Failed to equip item in bag %d slot %d", bag_idx, slot_idx), true);
        return;
    }
    const GW::Skillbar* s = GW::SkillbarMgr::GetPlayerSkillbar();
    if (p->GetIsKnockedDown() || (s && s->cast_array.size())) {
        ongoing = true;
        // Player knocked down or casting; wait.
        return;
    }
    if (p->skill) {
        ongoing = true;
        // Casting atm
        return;
    }
    if (!p->model_state) {
        ongoing = true;
        return;
    }
    if (TIMER_DIFF(last_try) > 500) {
        last_try = TIMER_INIT();
        if (p->GetIsIdle() || p->GetIsMoving()) {
            GW::Items::EquipItem(item);
        }
        else {
            // Move to clear model state e.g. attacking, aftercast
            GW::Agents::Move(p->pos);
        }
    }
}
