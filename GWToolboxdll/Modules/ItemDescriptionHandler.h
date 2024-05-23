#pragma once

#include <ToolboxModule.h>

typedef void (__cdecl *GetItemDescriptionCallback)(uint32_t item_id, uint32_t flags, uint32_t quantity, uint32_t unk, wchar_t** out_name, wchar_t** out_desc);

namespace GW {
    struct Item;
}

class ItemDescriptionHandler final : public ToolboxModule {
public:
    static ItemDescriptionHandler& Instance() {
        static ItemDescriptionHandler instance;

        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Item Description Handler"; }
    [[nodiscard]] const char* Description() const override { return "Allows dynamic modification of inventory item descriptions"; }

    void Initialize() override;
    void SignalTerminate() override;

    static std::wstring GetItemEncNameWithoutMods(const GW::Item* item);

    static void RegisterDescriptionCallback(GetItemDescriptionCallback callback, int altitude = 0);
    static void UnregisterDescriptionCallback(GetItemDescriptionCallback callback);
};
