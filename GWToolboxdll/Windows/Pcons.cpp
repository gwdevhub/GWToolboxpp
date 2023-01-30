#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/CharContext.h>

#include <GWCA/GameContainers/Array.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/EffectMgr.h>

#include <Logger.h>
#include <Utils/GuiUtils.h>

#include <Modules/Resources.h>
#include <Widgets/AlcoholWidget.h>
#include <Windows/Pcons.h>
#include <Windows/PconsWindow.h>

float Pcon::size = 46.0f;
int Pcon::pcons_delay = 5000;
int Pcon::lunar_delay = 500;
bool Pcon::disable_when_not_found = true;
bool Pcon::refill_if_below_threshold = false;
DWORD Pcon::player_id = 0;
Color Pcon::enabled_bg_color = Colors::ARGB(102, 0, 255, 0);

DWORD Pcon::alcohol_level = 0;
bool Pcon::suppress_drunk_effect = false;
bool Pcon::suppress_drunk_text = false;
bool Pcon::suppress_drunk_emotes = false;
bool Pcon::suppress_lunar_skills = false;
bool Pcon::pcons_by_character = true;
bool Pcon::hide_city_pcons_in_explorable_areas = false;

// 22 is the highest bag index. 25 is the most slots in any single bag.
std::array<std::array<clock_t, 25>, 22> Pcon::reserved_bag_slots{};

// ================================================
Pcon::Pcon(const char* chatname,
    const char* abbrevname,
    const char* ininame,
    const wchar_t* filename_,
    ImVec2 uv0_, ImVec2 uv1_, int threshold_,
    const char* desc_)
    : threshold(threshold_)
    , filename(filename_)
    , timer(TIMER_INIT())
    , uv0(uv0_)
    , uv1(uv1_)
{
    enabled = settings_by_charname[L"default"] = new bool(false);
    if (desc_) desc = desc_;
    chat = chatname ? chatname : GuiUtils::WStringToString(filename);
    abbrev = abbrevname ? abbrevname : GuiUtils::RemovePunctuation(chat);
    ini = ininame ? ininame : GuiUtils::ToSlug(chat);
}
Pcon::~Pcon() {
    for (const auto& c : settings_by_charname) {
        delete c.second;
    }
    settings_by_charname.clear();
    Terminate();
}
// Resets pcon counters so it needs to recalc number and refill.
void Pcon::ResetCounts() {
    refill_attempted = false;
    pcon_quantity_checked = false;
    for (auto& i : reserved_bag_slots) {
        std::ranges::fill(i, 0);
    }
}
void Pcon::SetEnabled(bool b) {
    if (*enabled == b) return;
    *enabled = b;
    ResetCounts();
    Refill(refill_if_below_threshold && IsEnabled() && PconsWindow::Instance().GetEnabled());
}
bool Pcon::IsVisible() const {
    return visible;
}
wchar_t* Pcon::SetPlayerName() {
    const auto c = GW::GetCharContext();
    if (!(c && c->player_name))
        return nullptr;
    enabled = GetSettingsByName(pcons_by_character ? c->player_name : L"default");
    return c->player_name;
}
void Pcon::Draw(IDirect3DDevice9* device) {
    UNREFERENCED_PARAMETER(device);
    if (!texture) {
        texture = Resources::GetItemImage(filename);
    }
    if (*texture == nullptr) return;
    ImVec2 pos = ImGui::GetCursorPos();
    ImVec2 s(size, size);
    ImVec4 bg = IsEnabled() ? ImColor(enabled_bg_color).Value : ImVec4(0, 0, 0, 0);
    ImVec4 tint(1, 1, 1, 1);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    if (ImGui::ImageButton((ImTextureID)*texture, s, uv0, uv1, 0, bg, tint)) {
        OnButtonClick();
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        char out[128];
        snprintf(out, 128, "%s\n%s", chat.c_str(), desc.c_str());
        ImGui::SetTooltip(out);
    }
    if (maptype != GW::Constants::InstanceType::Loading) {
        ImFont* f = GuiUtils::GetFont(GuiUtils::FontSize::header1);
        ImVec2 nextPos = ImGui::GetCursorPos();
        ImGui::PushFont(f);
        ImVec4 color;
        if (quantity == 0) color = ImVec4(1, 0, 0, 1);
        else if (quantity < threshold) color = ImVec4(1, 1, 0, 1);
        else color = ImVec4(0, 1, 0, 1);

        ImGui::SetCursorPos(ImVec2(pos.x + 1, pos.y + 1));
        ImGui::TextColored(ImVec4(0, 0, 0, 1), "%d", quantity);
        ImGui::SetCursorPos(ImVec2(pos.x, pos.y));
        ImGui::TextColored(color, "%d", quantity);
        ImGui::PopFont();

        if (maptype == GW::Constants::InstanceType::Outpost && PconsWindow::Instance().show_storage_quantity) {
            ImGui::SetCursorPos(ImVec2(pos.x + 3, nextPos.y - ImGui::GetTextLineHeight()));
            ImGui::TextColored(ImVec4(0, 0, 0, 1), "%d", quantity_storage);
            ImGui::SetCursorPos(ImVec2(pos.x + 2, nextPos.y - ImGui::GetTextLineHeight() - 1));
            ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.75f, 1), "%d", quantity_storage);
        }
    }

    ImGui::SetCursorPos(pos);
    ImGui::Dummy(ImVec2(size, size));
}
void Pcon::Terminate() {
    texture = nullptr;
}

void Pcon::Update(int delay) {
    if (mapid != GW::Map::GetMapID() || maptype != GW::Map::GetInstanceType()) { // Map changed; reset vars
        mapid = GW::Map::GetMapID();
        maptype = GW::Map::GetInstanceType();
        SetPlayerName();
        ResetCounts();
        Refill(refill_if_below_threshold && IsEnabled() && PconsWindow::Instance().GetEnabled());
    }
    // Refill pcons if needed.
    UpdateRefill();
    if (maptype == GW::Constants::InstanceType::Loading || GW::Map::GetIsObserving())
        return;
    // Check pcon count in inventory
    if (!pcon_quantity_checked) {
        int qty = CheckInventory();
        if (qty < 0) return; // Inventory pointers not ready
        quantity = qty;
        if (maptype == GW::Constants::InstanceType::Outpost) {
            quantity_storage = CheckInventory(nullptr, nullptr, static_cast<int>(GW::Constants::Bag::Storage_1), static_cast<int>(GW::Constants::Bag::Storage_14));
            if (IsEnabled() && PconsWindow::Instance().GetEnabled() && !refilling) {
                // Only warn user of low pcon count if is enabled and we're in an outpost.
                if (quantity == 0) {
                    Log::Error("No more %s items found", chat.c_str());
                }
                else if (quantity < threshold) {
                    Log::Warning("Low on %s", chat.c_str());
                }
            }
        }
        pcon_quantity_checked = true;
    }
    // === Use item if possible ===
    if (IsEnabled() && PconsWindow::Instance().GetEnabled()) {
        if (delay < 0) delay = Pcon::pcons_delay;
        player = GW::Agents::GetPlayerAsAgentLiving();
        // NOTE: Only fails CanUseByEffect() if we've found an effects array for this map before.
        if (player != nullptr
            && !player->GetIsDead()
            && (player_id == 0 || player->agent_id == player_id)
            && TIMER_DIFF(timer) > delay
            && CanUseByInstanceType()
            && CanUseByEffect()) {

            bool used = false;
            int qty = CheckInventory(&used);
            AfterUsed(used, qty);
        }
    }

}
bool Pcon::ReserveSlotForMove(size_t bagIndex, size_t slot)
{
    if (IsSlotReservedForMove(bagIndex, slot))
        return false;
    reserved_bag_slots.at(bagIndex).at(slot) = TIMER_INIT();
    return true;
}
bool Pcon::UnreserveSlotForMove(size_t bagIndex, size_t slot)
{
    reserved_bag_slots.at(bagIndex).at(slot) = 0;
    return true;
}
bool Pcon::IsSlotReservedForMove(size_t bagIndex, size_t slot)
{
    const clock_t slot_reserved_at = reserved_bag_slots.at(bagIndex).at(slot);
    return slot_reserved_at && TIMER_DIFF(slot_reserved_at) < 3000; // 1000ms is reasonable for CtoS then StoC
}
void Pcon::AfterUsed(bool used, int qty) {
    if (qty >= 0) { // if not, bag was undefined and we just ignore everything
        quantity = qty;
        if (used) {
            timer = TIMER_INIT();
            if (quantity == 0) { // if we just used the last one
                mapid = GW::Map::GetMapID();
                maptype = GW::Map::GetInstanceType();
                Log::Warning("Just used the last %s", chat.c_str());
                if (disable_when_not_found) SetEnabled(false);
            }
        }
        else {
            // we should have used but didn't find the item
            if (disable_when_not_found) SetEnabled(false);
            if (mapid != GW::Map::GetMapID()
                || maptype != GW::Map::GetInstanceType()) { // only yell at the user once
                mapid = GW::Map::GetMapID();
                maptype = GW::Map::GetInstanceType();
                Log::Error("Cannot find %s", chat.c_str());
            }
        }
    }
}
GW::Item* Pcon::FindVacantStackOrSlotInInventory(GW::Item* likeItem) { // Scan bags, find an incomplete stack, or otherwise an empty slot.
    GW::Bag** bags = GW::Items::GetBagArray();
    if (bags == nullptr) return nullptr;
    size_t emptySlotIdx = (size_t)-1;
    GW::Bag* emptyBag = nullptr;

    for (size_t bagIndex = static_cast<size_t>(GW::Constants::Bag::Bag_2); bagIndex > 0; --bagIndex) { // Work from last bag to first; pcons at bottom of inventory
        GW::Bag* bag = bags[bagIndex];
        if (bag == nullptr) continue;   // No bag, skip
        GW::ItemArray& items = bag->items;
        if (!items.valid()) continue;   // No item array, skip
        for (size_t i = items.size(); i > 0; i--) { // Work from last slot to first; pcons at bottom of inventory
            size_t slotIndex = i - 1;
            GW::Item* item = items[slotIndex];
            if (!item || item == nullptr) {
                // Reserve this slot for later
                if (!emptyBag && ReserveSlotForMove(bag->index, slotIndex)) {
                    emptySlotIdx = slotIndex;
                    emptyBag = bag;
                }
                continue;
            }
            if (!likeItem)
                continue; // Only compare with existing items if we have something to compare to.
            if (likeItem->mod_struct_size != item->mod_struct_size || likeItem->model_id != item->model_id)
                continue; // This is not the same item - apples and pears.
            if (likeItem->item_id == item->item_id || item->quantity >= 250)
                continue; // Comparing against yourself, or this item is already a full stack.
            if (ReserveSlotForMove(bag->index, item->slot)) {
                if (emptySlotIdx != (size_t)-1) // Unlock the empty slot.
                    UnreserveSlotForMove(emptyBag->index, emptySlotIdx);
                return item;    // Found a stack with space.
            }
        }
    }
    if (!emptyBag) return nullptr;
    GW::Item* item = new GW::Item(); // Create a "fake" item...
    item->bag = emptyBag; // ...that belongs in the empty bag/slot we found...
    item->slot = static_cast<uint8_t>(emptySlotIdx);
    item->quantity = 0; // ...with 250 available slots.
    item->item_id = 0; // item_id to 0 for comparison
    return item;
}
uint32_t Pcon::MoveItem(GW::Item* item, GW::Bag* bag, size_t slot, size_t quantity) {
    if (!item || !bag) return 0;
    if (bag->items.size() < (unsigned)slot) return 0;
    if (quantity < 1) quantity = item->quantity;
    if (quantity > item->quantity) quantity = item->quantity;
    GW::Item* destItem = bag->items.valid() ? bag->items[slot] : nullptr;
    uint32_t originalQuantity = destItem ? destItem->quantity : 0u;
    uint32_t vacantQuantity = 250 - originalQuantity;
    if (quantity > 1 && vacantQuantity < quantity) quantity = vacantQuantity;
    GW::Items::MoveItem(item, bag, slot, quantity);
    return quantity;
}
void Pcon::Refill(bool do_refill) {
    if (refilling == do_refill)
        return;
    refilling = do_refill;
    if (!refilling) {
        if (pending_move_to_bag)
            UnreserveSlotForMove(pending_move_to_bag->index, pending_move_to_slot);
        pending_move_to_bag = 0;
        pending_move_to_slot = 0;
        pending_move_to_quantity = 0;
        return;
    }
    ResetCounts();
}
void Pcon::UpdateRefill() {
    if (!refilling)
        return;
    if (!IsVisible() || GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) {
        Refill(false);
        pcon_quantity_checked = false;
        return;
    }
    if (pending_move_to_quantity) {
        GW::Item* item = GW::Items::GetItemBySlot(pending_move_to_bag, pending_move_to_slot + 1);
        if (!item || !QuantityForEach(item) || item->quantity != pending_move_to_quantity)
            return; // Still waiting for move.
        UnreserveSlotForMove(item->bag->index, item->slot);
    }
    quantity = CheckInventory();
    if (quantity >= threshold) {
        Refill(false);
        pcon_quantity_checked = false;
        return;
    }
    quantity_storage = CheckInventory(nullptr, nullptr, static_cast<int>(GW::Constants::Bag::Storage_1), static_cast<int>(GW::Constants::Bag::Storage_14));
    int points_needed = threshold - quantity; // quantity is actually points e.g. 20 grog = 60 quantity
    size_t quantity_to_move = 0;
    GW::Bag** bags = GW::Items::GetBagArray();
    if (points_needed < 1 || bags == nullptr) {
        Refill(false);
        pcon_quantity_checked = false;
        return;
    }
    for (size_t bagIndex = static_cast<size_t>(GW::Constants::Bag::Storage_1); bagIndex <= static_cast<size_t>(GW::Constants::Bag::Storage_14); ++bagIndex) {
        GW::Bag* storageBag = bags[bagIndex];
        if (storageBag == nullptr) continue;    // No bag, skip
        GW::ItemArray& storageItems = storageBag->items;
        if (!storageItems.valid()) continue;    // No item array, skip
        for (size_t i = 0; i < storageItems.size() && storageItems.valid(); i++) {
            GW::Item* storageItem = storageItems[i];
            if (storageItem == nullptr) continue; // No item, skip
            size_t points_per_item = QuantityForEach(storageItem);
            if (points_per_item < 1) continue; // This is not the pcon you're looking for...
            GW::Item* inventoryItem = FindVacantStackOrSlotInInventory(storageItem); // Now find a slot in inventory to move them to.
            if (inventoryItem == nullptr) {
                printf("No more space for %s", chat.c_str());
                Refill(false);
                pcon_quantity_checked = false;
                return;
            }
            quantity_to_move = static_cast<size_t>(ceil((float)points_needed / (float)points_per_item));
            if (quantity_to_move > storageItem->quantity)       quantity_to_move = storageItem->quantity;
            size_t slot_to = inventoryItem->slot;
            GW::Bag* bag_to = inventoryItem->bag;
            pending_move_to_quantity = inventoryItem->quantity + MoveItem(storageItem, bag_to, slot_to, quantity_to_move);
            if (inventoryItem->quantity == 0)
                delete inventoryItem; // Empty slot was returned; free memory here.
            pending_move_to_bag = bag_to;
            pending_move_to_slot = slot_to;
            return;
        }
    }
}

int Pcon::CheckInventory(bool *used, size_t *used_qty_ptr, size_t from_bag,
                            size_t to_bag) const
{
    size_t count = 0;
    size_t used_qty = 0;
    GW::Bag** bags = GW::Items::GetBagArray();
    if (bags == nullptr) return -1;
    for (size_t bagIndex = from_bag; bagIndex <= to_bag; ++bagIndex) {
        GW::Bag* bag = bags[bagIndex];
        if (bag == nullptr) continue;   // No bag, skip
        GW::ItemArray& items = bag->items;
        if (!items.valid()) continue;   // No item array, skip
        for (size_t i = 0; i < items.size(); i++) {
            GW::Item* item = items[i];
            if (item == nullptr) continue; // No item, skip
            size_t qtyea = QuantityForEach(item);
            if (qtyea < 1) continue; // This is not the pcon you're looking for...
            if (used != nullptr && !*used) {
                GW::Items::UseItem(item);
                *used = true;
                used_qty = qtyea;
            }
            count += qtyea * item->quantity;
        }
    }
    if (used_qty_ptr) *used_qty_ptr = used_qty;
    return static_cast<int>(count - used_qty);
}
bool Pcon::CanUseByInstanceType() const {
    return maptype == GW::Constants::InstanceType::Explorable;
}
bool* Pcon::GetSettingsByName(const wchar_t* name) {
    if (!settings_by_charname.contains(name)) {
        bool* def = settings_by_charname[L"default"];
        settings_by_charname[name] = new bool(*def);
    }
    return settings_by_charname[name];
}
void Pcon::LoadSettings(ToolboxIni* inifile, const char* section) {
    char buf_active[256];
    char buf_threshold[256];
    char buf_visible[256];
    snprintf(buf_active, 256, "%s_active", ini.c_str());
    snprintf(buf_threshold, 256, "%s_threshold", ini.c_str());
    snprintf(buf_visible, 256, "%s_visible", ini.c_str());
    threshold = inifile->GetLongValue(section, buf_threshold, threshold);

    bool* def = GetSettingsByName(L"default");
    *def = inifile->GetBoolValue(section, buf_active, *def);
    visible = inifile->GetBoolValue(section, buf_visible, visible);

    ToolboxIni::TNamesDepend entries;
    inifile->GetAllSections(entries);
    std::string sectionsub(section);
    sectionsub += ':';
    size_t section_len = sectionsub.size();
    for (ToolboxIni::Entry& entry : entries) {
        if (strncmp(entry.pItem, sectionsub.c_str(), section_len) != 0)
            continue;
        std::string str(entry.pItem);
        size_t charname_pos = section_len;
        std::wstring charname = GuiUtils::StringToWString(entry.pItem + charname_pos);
        bool* char_enabled = GetSettingsByName(charname.c_str());
        *char_enabled = inifile->GetBoolValue(entry.pItem, buf_active, *char_enabled);
    }
}
void Pcon::SaveSettings(ToolboxIni* inifile, const char* section) {
    char buf_active[256];
    char buf_threshold[256];
    char buf_visible[256];
    snprintf(buf_active, 256, "%s_active", ini.c_str());
    snprintf(buf_threshold, 256, "%s_threshold", ini.c_str());
    snprintf(buf_visible, 256, "%s_visible", ini.c_str());
    inifile->SetLongValue(section, buf_threshold, threshold);
    inifile->SetBoolValue(section, buf_visible, visible);

    for (const auto& charname_pcons : settings_by_charname) {
        bool _enabled = *charname_pcons.second;
        if (charname_pcons.first == L"default") {
            inifile->SetBoolValue(section, buf_active, _enabled);
            continue;
        }
        const auto& charname = charname_pcons.first;
        if (charname.empty())
            continue;
        std::string char_section(section);
        char_section.append(":");
        char_section.append(GuiUtils::WStringToString(charname).c_str());
        inifile->SetBoolValue(char_section.c_str(), buf_active, _enabled);
    }
}

// ================================================
size_t PconGeneric::QuantityForEach(const GW::Item* item) const {
    if (item->model_id == (DWORD)itemID) return 1;
    return 0;
}

void PconGeneric::OnButtonClick() {
    using namespace GW::Constants;
    Pcon::OnButtonClick();

    if (PconsWindow::Instance().shift_click_toggles_category && ImGui::IsKeyDown(ImGuiKey_ModShift)) {
        namespace r = std::ranges;
        const std::vector<std::vector<DWORD>> categories{
            {ItemID::ConsEssence, ItemID::ConsGrail, ItemID::ConsArmor},
            {ItemID::GRC, ItemID::BRC, ItemID::RRC},
            {ItemID::Kabobs, ItemID::PahnaiSalad, ItemID::SkalefinSoup}
        };
        const auto found = r::find_if(categories, [this](const auto& category) {
            return r::find(category, itemID) != r::end(category);
        });
        if (found == r::end(categories)) return;
        const auto& category = *found;
        auto enable_if_same_category = [this, &category](Pcon* other) {
            auto* generic = dynamic_cast<PconGeneric*>(other);
            if (generic == nullptr) return;
            if (r::find(category, generic->itemID) != r::end(category)) {
                generic->SetEnabled(IsEnabled());
            }
        };
        r::for_each(PconsWindow::Instance().pcons, enable_if_same_category);
    }
}

bool PconGeneric::CanUseByEffect() const {
    GW::Agent* _player = GW::Agents::GetPlayer();
    if (!_player)
        return false; // player doesn't exist?

    GW::EffectArray* effects = GW::Effects::GetPlayerEffects();
    if (!effects) return true;

    for (auto& effect : *effects) {
        if (effect.skill_id == effectID)
            return effect.GetTimeRemaining() < 1000;
    }
    return true;
}

// ================================================
bool PconCons::CanUseByEffect() const {
    if (!PconGeneric::CanUseByEffect()) return false;
    if (!GW::PartyMgr::GetIsPartyLoaded()) return false;

    GW::MapAgentArray* mapAgents = GW::Agents::GetMapAgentArray();
    if (!mapAgents) return false;

    size_t n_players = GW::Agents::GetAmountOfPlayersInInstance();
    for (size_t i = 1; i <= n_players; ++i) {
        DWORD currentPlayerAgID = GW::Agents::GetAgentIdByLoginNumber(i);
        if (currentPlayerAgID <= 0) return false;
        if (currentPlayerAgID >= mapAgents->size()) return false;
        if (mapAgents->at(currentPlayerAgID).GetIsDead()) return false;
    }

    return true;
}

void PconRefiller::Draw(IDirect3DDevice9* device) {
    if (maptype == GW::Constants::InstanceType::Explorable)
        return; // Don't draw in explorable areas - this is only for refilling in an outpost!
    Pcon::Draw(device);
}

// ================================================
bool PconCity::CanUseByInstanceType() const {
    return maptype == GW::Constants::InstanceType::Outpost;
}
bool PconCity::CanUseByEffect() const {
    using namespace GW::Constants;
    GW::Agent* _player = GW::Agents::GetPlayer();
    if (!_player)
        return false; // player doesn't exist?

    GW::EffectArray* effects = GW::Effects::GetPlayerEffects();
    if (!effects) return true;

    if (_player->move_x == 0.0f && _player->move_y == 0.0f)
        return false;

    for (DWORD i = 0; i < effects->size(); i++) {
        if (effects->at(i).GetTimeRemaining() < 1000) continue;
        if (effects->at(i).skill_id == SkillID::Sugar_Rush_short
            || effects->at(i).skill_id == SkillID::Sugar_Rush_medium
            || effects->at(i).skill_id == SkillID::Sugar_Rush_long
            || effects->at(i).skill_id == SkillID::Sugar_Jolt_short
            || effects->at(i).skill_id == SkillID::Sugar_Jolt_long) {
            return false; // already on
        }
    }
    return true;
}
bool PconCity::IsVisible() const {
    return visible && (!hide_city_pcons_in_explorable_areas || maptype == GW::Constants::InstanceType::Outpost);
}
size_t PconCity::QuantityForEach(const GW::Item* item) const {
    using namespace GW::Constants;
    switch (item->model_id) {
    case ItemID::CremeBrulee:
    case ItemID::ChocolateBunny:
    case ItemID::Fruitcake:
    case ItemID::SugaryBlueDrink:
    case ItemID::RedBeanCake:
    case ItemID::JarOfHoney:
    case ItemID::KrytalLokum:
    case ItemID::MandragorRootCake:
        return 1;
    default:
        return 0;
    }
}

// ================================================
bool PconAlcohol::CanUseByEffect() const {
    return AlcoholWidget::Instance().GetAlcoholLevel() <= 1;
}
size_t PconAlcohol::QuantityForEach(const GW::Item* item) const {
    using namespace GW::Constants;
    switch (item->model_id) {
    case ItemID::Eggnog:
    case ItemID::DwarvenAle:
    case ItemID::HuntersAle:
    case ItemID::Absinthe:
    case ItemID::WitchsBrew:
    case ItemID::Ricewine:
    case ItemID::ShamrockAle:
    case ItemID::Cider:
        return 1;
    case ItemID::Grog:
    case ItemID::SpikedEggnog:
    case ItemID::AgedDwarvenAle:
    case ItemID::AgedHuntersAle:
    case ItemID::FlaskOfFirewater:
    case ItemID::KrytanBrandy:
        return 5;
    case ItemID::Keg: {
        GW::ItemModifier *mod = item->mod_struct;
        if (mod == nullptr) return 5; // we don't think this will ever happen

        for (DWORD i = 0; i < item->mod_struct_size; i++) {
            if (mod->identifier() == 0x2458) {
                return mod->arg2() * 5;
            }
            mod++;
        }
        return 5; // this should never happen, but we keep it as a fallback
    }
    default:
        return 0;
    }
}
void PconAlcohol::ForceUse() {
    GW::AgentLiving *_player = GW::Agents::GetPlayerAsAgentLiving();
    if (_player != nullptr && !_player->GetIsDead() &&
        (player_id == 0 || _player->agent_id == player_id)) {
        bool used = false;
        size_t used_qty = 0;
        int qty = CheckInventory(&used, &used_qty);
        if (used_qty == 1) {
            bool used2 = false;
            qty = CheckInventory(&used2, &used_qty); // use another!
        }

        AfterUsed(used, qty);
    }
}

// ================================================
void PconLunar::Update(int delay) {
    UNREFERENCED_PARAMETER(delay);
    Pcon::Update(lunar_delay);
}
size_t PconLunar::QuantityForEach(const GW::Item* item) const {
    using namespace GW::Constants;
    switch (item->model_id) {
    case ItemID::LunarPig:
    case ItemID::LunarRat:
    case ItemID::LunarOx:
    case ItemID::LunarTiger:
    case ItemID::LunarDragon:
    case ItemID::LunarHorse:
    case ItemID::LunarRabbit:
    case ItemID::LunarSheep:
    case ItemID::LunarSnake:
    case ItemID::LunarMonkey:
    case ItemID::LunarRooster:
    case ItemID::LunarDog:
        return 1;
    default:
        return 0;
    }
}
bool PconLunar::CanUseByEffect() const {
    GW::Agent* _player = GW::Agents::GetPlayer();
    if (!_player)
        return false; // player doesn't exist?

    return GW::Effects::GetPlayerEffectBySkillId(GW::Constants::SkillID::Lunar_Blessing) == nullptr;
}
