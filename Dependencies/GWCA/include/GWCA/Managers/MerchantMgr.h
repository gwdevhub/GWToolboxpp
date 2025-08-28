#pragma once

#include <GWCA/Utilities/Export.h>
#include <GWCA/GameContainers/Array.h>

namespace GW {

    struct Module;
    extern Module MerchantModule;

    typedef uint32_t ItemID;
    typedef Array<ItemID> MerchItemArray;

    struct Item;

    namespace Merchant {

        struct TransactionInfo {
            uint32_t  item_count = 0;
            uint32_t *item_ids = nullptr;
            uint32_t *item_quantities = nullptr;
        };

        struct QuoteInfo {
            uint32_t  unknown = 0;
            uint32_t  item_count = 0;
            uint32_t *item_ids = nullptr;
        };

        enum class TransactionType : uint32_t {
            AccountName,
            MerchantBuy,
            CollectorBuy,
            CrafterBuy,
            WeaponsmithCustomize,
            Services,
            DonateFaction,
            Unused,
            GuildRegistration,
            GuildCape,
            SkillTrainer,
            MerchantSell,
            TraderBuy,
            TraderSell,
            UnlockHero,
            UnlockItem,
            UnlockSkill
        };
        GWCA_API bool TransactItems();

        GWCA_API bool RequestQuote(TransactionType type, uint32_t item_id);

        GWCA_API size_t GetMerchantItems(TransactionType type, size_t buffer_len = 0, uint32_t* buffer = 0);
    };
}
