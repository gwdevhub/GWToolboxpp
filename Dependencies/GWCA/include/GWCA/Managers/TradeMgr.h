#pragma once
#include <GWCA/Runtime.h>

#include <GWCA/Utilities/Export.h>

namespace GW {
    struct Module;
    extern Module TradeModule;
    struct TradeItem;

    namespace Trade {

        bool OpenTradeWindow(uint32_t agent_id);
        bool AcceptTrade();
        bool CancelTrade();
        bool ChangeOffer();
        bool SubmitOffer(uint32_t gold);
        bool RemoveItem(uint32_t slot);
        TradeItem* IsItemOffered(uint32_t item_id);

        // Passing quantity = 0 will prompt the player for the amount
        bool OfferItem(uint32_t item_id, uint32_t quantity = 0);
    };
}
// C Interop API

namespace GW::Trade {
    extern "C" GWCA_API bool __cdecl GW_Trade_OpenTradeWindow(uint32_t agent_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Trade_AcceptTrade() noexcept;
    extern "C" GWCA_API bool __cdecl GW_Trade_CancelTrade() noexcept;
    extern "C" GWCA_API bool __cdecl GW_Trade_ChangeOffer() noexcept;
    extern "C" GWCA_API bool __cdecl GW_Trade_SubmitOffer(uint32_t gold) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Trade_RemoveItem(uint32_t slot) noexcept;
    extern "C" GWCA_API TradeItem* __cdecl GW_Trade_IsItemOffered(uint32_t item_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_Trade_OfferItem(uint32_t item_id, uint32_t quantity) noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW::Trade {
    inline bool OpenTradeWindow(uint32_t agent_id)
    {
        return GWCA_CALL_C(GW_Trade_OpenTradeWindow, agent_id);
    }

    inline bool AcceptTrade()
    {
        return GWCA_CALL_C(GW_Trade_AcceptTrade);
    }

    inline bool CancelTrade()
    {
        return GWCA_CALL_C(GW_Trade_CancelTrade);
    }

    inline bool ChangeOffer()
    {
        return GWCA_CALL_C(GW_Trade_ChangeOffer);
    }

    inline bool SubmitOffer(uint32_t gold)
    {
        return GWCA_CALL_C(GW_Trade_SubmitOffer, gold);
    }

    inline bool RemoveItem(uint32_t slot)
    {
        return GWCA_CALL_C(GW_Trade_RemoveItem, slot);
    }

    inline TradeItem* IsItemOffered(uint32_t item_id)
    {
        return GWCA_CALL_C(GW_Trade_IsItemOffered, item_id);
    }

    inline bool OfferItem(uint32_t item_id, uint32_t quantity)
    {
        return GWCA_CALL_C(GW_Trade_OfferItem, item_id, quantity);
    }
}
#endif
