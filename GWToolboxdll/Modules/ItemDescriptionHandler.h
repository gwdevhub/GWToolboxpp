#pragma once

#include <ToolboxModule.h>

struct ItemDescriptionEventArgs {
    const uint32_t item_id;
    const uint32_t flags;
    const uint32_t quantity;
    const uint32_t unk;

    std::wstring& name;
    std::wstring& description;
};

using ItemDescriptionCallback = void(*)(ItemDescriptionEventArgs& eventArgs);

class ItemDescriptionHandler final : public ToolboxModule {
    using GetItemDescription_pt = void(__cdecl*)(uint32_t item_id, uint32_t flags, uint32_t quantity, uint32_t unk, wchar_t** out_name, wchar_t** out_desc);
    GetItemDescription_pt GetItemDescription_Func = nullptr, GetItemDescription_Ret = nullptr;

    struct CallbackEntry {
        unsigned int altitude;
        ItemDescriptionCallback callback;
    };
    std::vector<CallbackEntry> callbacks;

    std::mutex modified_strings_mutex;
    std::wstring modified_name;
    std::wstring modified_description;

    static void __cdecl OnGetItemDescription_Wrapper(uint32_t item_id, uint32_t flags, uint32_t quantity, uint32_t unk, wchar_t** out_name, wchar_t** out_desc);
    void OnGetItemDescription(uint32_t item_id, uint32_t flags, uint32_t quantity, uint32_t unk, wchar_t** out_name, wchar_t** out_desc);
public:
    static ItemDescriptionHandler& Instance() {
        static ItemDescriptionHandler instance;

        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Item Description Handler"; }
    [[nodiscard]] const char* Description() const override { return "Allows dynamic modification of inventory item descriptions"; }

    void RegisterDescriptionCallback(ItemDescriptionCallback callback, unsigned int altitude = 0);
    void UnregisterDescriptionCallback(ItemDescriptionCallback callback);
    void UnregisterDescriptionCallback(ItemDescriptionCallback callback, unsigned int altitude);

    void Initialize() override;
    void SignalTerminate() override;
};
