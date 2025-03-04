#pragma once

#include <GWCA/Utilities/Export.h>
#include <GWCA/GameContainers/Array.h>

namespace GW {

    struct Module;
    extern Module MerchantModule;

    typedef uint32_t ItemID;
    typedef Array<ItemID> MerchItemArray;

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
            MerchantBuy = 0x1,
            CollectorBuy,
            CrafterBuy,
            WeaponsmithCustomize,
            DonateFaction = 0x6,

            MerchantSell = 0xB,
            TraderBuy,
            TraderSell,

            UnlockRunePriestOfBalth = 0xF
        };
        GWCA_API bool TransactItems();

        GWCA_API bool RequestQuote(TransactionType type, uint32_t item_id);

        // note: can contain pointers to random items from your inventory
        GWCA_API MerchItemArray* GetMerchantItemsArray();
    };
}
