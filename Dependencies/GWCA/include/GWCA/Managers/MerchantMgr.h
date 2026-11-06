#pragma once
#include <GWCA/Runtime.h>

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
        bool TransactItems();

        bool RequestQuote(TransactionType type, uint32_t item_id);

        size_t GetMerchantItems(TransactionType type, size_t buffer_len = 0, uint32_t* buffer = 0);
    };
}

namespace GW::Merchant {
    extern "C" GWCA_API bool __cdecl GW_Merchant_TransactItems() noexcept;
    extern "C" GWCA_API bool __cdecl GW_Merchant_RequestQuote(TransactionType type, uint32_t item_id) noexcept;
    extern "C" GWCA_API size_t __cdecl GW_Merchant_GetMerchantItems(TransactionType type, size_t buffer_len, uint32_t* buffer) noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW::Merchant {
    inline bool TransactItems()
    {
        return GWCA_CALL_C(GW_Merchant_TransactItems);
    }

    inline bool RequestQuote(TransactionType type, uint32_t item_id)
    {
        return GWCA_CALL_C(GW_Merchant_RequestQuote, type, item_id);
    }

    inline size_t GetMerchantItems(TransactionType type, size_t buffer_len, uint32_t* buffer)
    {
        return GWCA_CALL_C(GW_Merchant_GetMerchantItems, type, buffer_len, buffer);
    }
}
#endif
