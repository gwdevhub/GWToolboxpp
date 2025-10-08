#pragma once

#include <stdint.h>

#include <GWCA/GameContainers/Array.h>


namespace GW {

    struct TradeItem {
        uint32_t item_id;
        uint32_t quantity;
    };
    struct TradePlayer {
        uint32_t gold;
        Array<TradeItem> items;
    };

    struct TradeContext {
        enum TradeStatus {
            TRADE_CLOSED = 0,
            TRADE_INITIATED = 1,
            TRADE_OFFER_SEND = 2,
            TRADE_ACCEPTED = 4
        };

        /* +h0000 */ uint32_t flags; // this is actually a flags
        /* +h0004 */ uint32_t h0004[3]; // Seemingly 3 null dwords
        /* +h0010 */ TradePlayer player;
        /* +h0024 */ TradePlayer partner;

        // bool GetPartnerAccepted();
        // bool GetPartnerOfferSent();

        bool GetIsTradeOffered()   const { return (flags & TRADE_OFFER_SEND) != 0; }
        bool GetIsTradeInitiated() const { return (flags & TRADE_INITIATED)  != 0; }
        bool GetIsTradeAccepted()  const { return (flags & TRADE_ACCEPTED)   != 0; }
    };

    GWCA_API TradeContext* GetTradeContext();
}
