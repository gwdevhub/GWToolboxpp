#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/TradeMgr.h>

#include <Utils/ToolboxUtils.h>

#include "Modules/InventoryItem.h"

GW::ItemModifier* InventoryItem::GetModifier(const uint32_t identifier) const
{
    for (size_t i = 0; i < mod_struct_size; i++) {
        GW::ItemModifier* mod = &mod_struct[i];
        if (mod->identifier() == identifier) {
            return mod;
        }
    }
    return nullptr;
}

uint32_t InventoryItem::GetUses() const
{
    const GW::ItemModifier* mod = GetModifier(0x2458);
    return (mod ? mod->arg2() : 1) * quantity;
}

bool InventoryItem::IsSalvageKit() const
{
    return IsLesserKit() || IsExpertSalvageKit();
}

bool InventoryItem::IsTome() const
{
    const GW::ItemModifier* mod = GetModifier(0x2788);
    const uint32_t use_id = mod ? mod->arg() : 0;
    return use_id > 15 && use_id < 36;
}

bool InventoryItem::IsIdentificationKit() const
{
    const GW::ItemModifier* mod = GetModifier(0x25E8);
    return mod && mod->arg1() == 1;
}

bool InventoryItem::IsLesserKit() const
{
    const GW::ItemModifier* mod = GetModifier(0x25E8);
    return mod && mod->arg1() == 3;
}

bool InventoryItem::IsExpertSalvageKit() const
{
    const GW::ItemModifier* mod = GetModifier(0x25E8);
    return mod && mod->arg1() == 2;
}

bool InventoryItem::IsPerfectSalvageKit() const
{
    const GW::ItemModifier* mod = GetModifier(0x25E8);
    return mod && mod->arg1() == 6;
}

bool InventoryItem::IsRareMaterial() const
{
    const GW::ItemModifier* mod = GetModifier(0x2508);
    return mod && mod->arg1() > 11;
}

bool InventoryItem::IsInventoryItem() const
{
    return bag && (bag->IsInventoryBag() || bag->bag_type == GW::Constants::BagType::Equipped);
}

bool InventoryItem::IsStorageItem() const
{
    return bag && (bag->IsStorageBag() || bag->IsMaterialStorage());
}

GW::Constants::Rarity InventoryItem::GetRarity() const
{
    return GW::Items::GetRarity(this);
}

bool InventoryItem::IsWeapon() const
{
    switch (static_cast<GW::Constants::ItemType>(type)) {
        case GW::Constants::ItemType::Axe:
        case GW::Constants::ItemType::Sword:
        case GW::Constants::ItemType::Shield:
        case GW::Constants::ItemType::Scythe:
        case GW::Constants::ItemType::Bow:
        case GW::Constants::ItemType::Wand:
        case GW::Constants::ItemType::Staff:
        case GW::Constants::ItemType::Offhand:
        case GW::Constants::ItemType::Daggers:
        case GW::Constants::ItemType::Hammer:
        case GW::Constants::ItemType::Spear:
            return true;
        default:
            return false;
    }
}

bool InventoryItem::IsArmor() const
{
    switch (static_cast<GW::Constants::ItemType>(type)) {
        case GW::Constants::ItemType::Headpiece:
        case GW::Constants::ItemType::Chestpiece:
        case GW::Constants::ItemType::Leggings:
        case GW::Constants::ItemType::Boots:
        case GW::Constants::ItemType::Gloves:
            return true;
        default:
            return false;
    }
}

bool InventoryItem::IsOfferedInTrade() const
{
    return GW::Trade::IsItemOffered(item_id) != nullptr;
}

bool InventoryItem::CanBeIdentified() const
{
    if (GetIsIdentified()) return false;
    if (IsSalvagable(false)) return true;
    switch (type) {
        case GW::Constants::ItemType::Bundle:
        case GW::Constants::ItemType::Usable:
        case GW::Constants::ItemType::Quest_Item:
        case GW::Constants::ItemType::Storybook:
            return false;
    }
    if (IsWeapon() || IsArmor()) return true;
    if (IsGreen()) return false;
    switch (model_file_id) {
        case 0x44CAC:
            return false;
    }
    return true;
}

bool InventoryItem::IsOldSchool() const
{
    // Not OS if inscribable (Nightfall/EotN) or not salvagable
    if (GetIsInscribable() || !IsSalvagable(false)) return false;

    // OS off-hands (wand/focus/shield) have 2 inherent mods, no upgrade slots
    switch (type) {
        case GW::Constants::ItemType::Wand:
        case GW::Constants::ItemType::Offhand:
            return !IsUpgradable();
    }

    // Other OS weapons have 1 inherent + 1 suffix slot (so they ARE upgradable)
    return IsWeapon() && !IsPrefixUpgradable();
}
