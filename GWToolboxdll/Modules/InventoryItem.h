#pragma once

#include <GWCA/GameEntities/Item.h>

namespace GW {
    namespace Constants {
        enum class Rarity : uint8_t;
    }
}

struct InventoryItem : GW::Item {
    GW::ItemModifier* GetModifier(uint32_t identifier) const;
    GW::Constants::Rarity GetRarity() const;
    uint32_t GetUses() const;
    bool IsIdentificationKit() const;
    bool IsSalvageKit() const;
    bool IsTome() const;
    bool IsLesserKit() const;
    bool IsExpertSalvageKit() const;
    bool IsPerfectSalvageKit() const;
    bool IsWeapon() const;
    bool IsArmor() const;
    bool IsSalvagable(bool check_bag = true, bool check_blocked_from_being_salvaged = true) const;
    bool IsHiddenFromMerchants() const;

    bool IsInventoryItem() const;
    bool IsStorageItem() const;

    bool IsRareMaterial() const;
    bool IsOfferedInTrade() const;
    bool CanOfferToTrade() const;

    [[nodiscard]] bool IsSparkly() const
    {
        return (interaction & 0x2000) == 0;
    }

    [[nodiscard]] bool GetIsIdentified() const
    {
        return (interaction & 1) != 0;
    }
    [[nodiscard]] bool CanBeIdentified() const;
    [[nodiscard]] bool IsPrefixUpgradable() const
    {
        return ((interaction >> 14) & 1) == 0;
    }

    [[nodiscard]] bool IsSuffixUpgradable() const
    {
        return ((interaction >> 15) & 1) == 0;
    }

    [[nodiscard]] bool IsUsable() const
    {
        return (interaction & 0x1000000) != 0;
    }

    [[nodiscard]] bool IsTradable() const
    {
        return (interaction & 0x100) == 0;
    }

    [[nodiscard]] bool IsInscription() const
    {
        return (interaction & 0x25000000) == 0x25000000;
    }

    [[nodiscard]] bool IsOldSchool() const;

    [[nodiscard]] bool IsUpgradable() const
    {
        return GetIsInscribable() || IsPrefixUpgradable() || IsSuffixUpgradable();
    }

    [[nodiscard]] bool IsBlue() const
    {
        return single_item_name && single_item_name[0] == 0xA3F;
    }

    [[nodiscard]] bool IsPurple() const
    {
        return (interaction & 0x400000) != 0;
    }

    [[nodiscard]] bool IsGreen() const
    {
        return (interaction & 0x10) != 0;
    }

    [[nodiscard]] bool IsGold() const
    {
        return (interaction & 0x20000) != 0;
    }
};
