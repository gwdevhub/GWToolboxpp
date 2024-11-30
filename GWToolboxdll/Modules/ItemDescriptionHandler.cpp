#include <Modules/ItemDescriptionHandler.h>

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/GameEntities/Item.h>

#include <Modules/InventoryManager.h>

namespace {
    GetItemDescriptionCallback GetItemDescription_Func = nullptr, GetItemDescription_Ret = nullptr;

    std::vector < std::pair <GetItemDescriptionCallback, int>> callbacks;

    void __cdecl OnGetItemDescription(uint32_t item_id, uint32_t flags, uint32_t quantity, uint32_t unk, wchar_t** out_name, wchar_t** out_desc) {
        GW::Hook::EnterHook();
        GetItemDescription_Ret(item_id, flags, quantity, unk, out_name, out_desc);
        for (auto& cb : callbacks) {
            cb.first(item_id,flags,quantity,unk,out_name,out_desc);
        }
        GW::Hook::LeaveHook();
    }

    constexpr wchar_t SUFFIX_AND_PREFIX = 0xa31;
    constexpr wchar_t PREFIX_ONLY = 0xa30;
    constexpr wchar_t SUFFIX_ONLY = 0xa33;

    const wchar_t* GetSuffixModName(const GW::Item* item, size_t* out_len) {
        if (!item->complete_name_enc)
            return nullptr;
        auto found = wcschr(item->complete_name_enc, SUFFIX_AND_PREFIX);
        if (found) {
            found = wcschr(found, 0x10c) + 1;
            *out_len = wcschr(found, 0x1) - found;
            return found;
        }
        found = wcschr(item->complete_name_enc, SUFFIX_ONLY);
        if (found) {
            found = wcschr(found, 0x10b) + 1;
            *out_len = wcschr(found, 0x1) - found;
            return found;
        }
        return nullptr;
    }
    const wchar_t* GetPrefixModName(const GW::Item* item, size_t* out_len) {
        if (!item->complete_name_enc)
            return nullptr;
        auto found = wcschr(item->complete_name_enc, SUFFIX_AND_PREFIX);
        if (found) {
            found = wcschr(found, 0x10b) + 1;
            *out_len = wcschr(found, 0x1) - found;
            return found;
        }
        found = wcschr(item->complete_name_enc, PREFIX_ONLY);
        if (found) {
            found = wcschr(found, 0x10b) + 1;
            *out_len = wcschr(found, 0x1) - found;
            return found;
        }
        return nullptr;
    }
}
std::wstring ItemDescriptionHandler::GetItemDescription(const GW::Item* _item) {
    if (!_item) return L"";
    wchar_t* out = nullptr;
    OnGetItemDescription(_item->item_id, 0, 0, 0, nullptr, &out);
    return out ? out : L"";
}
std::wstring ItemDescriptionHandler::GetItemEncNameWithoutMods(const GW::Item* _item) {

    auto item = (InventoryManager::Item*)_item;

    size_t prefix_name_len = 0;
    const wchar_t* prefix_mod_name = nullptr;
    size_t suffix_name_len = 0;
    const wchar_t* suffix_mod_name = nullptr;

    if (!item->IsPrefixUpgradable()) {
        // Weapon prefix can't be modified; this is part of the item name
        prefix_mod_name = GetPrefixModName(item, &prefix_name_len);
    }
    if (!item->IsSuffixUpgradable()) {
        // Weapon suffix can't be modified; this is part of the item name
        suffix_mod_name = GetSuffixModName(item, &suffix_name_len);
    }

    std::wstring out;
    if (prefix_mod_name && suffix_mod_name) {
        out = std::format(L"{}\x10a{}\x1\x10b{}\x1\x10c{}\x1", SUFFIX_AND_PREFIX, item->name_enc, 
            std::wstring(prefix_mod_name, prefix_name_len), 
            std::wstring(suffix_mod_name, suffix_name_len));
    }
    else if (prefix_mod_name) {
        out = std::format(L"{}\x10a{}\x1\x10b{}\x1", PREFIX_ONLY, item->name_enc,
            std::wstring(prefix_mod_name, prefix_name_len));
    }
    else if (suffix_mod_name) {
        out = std::format(L"{}\x10a{}\x1\x10b{}\x1", SUFFIX_ONLY, item->name_enc,
            std::wstring(suffix_mod_name, suffix_name_len));
    }
    else {
        out = item->name_enc;
    }
    return out;
}
void ItemDescriptionHandler::Initialize()
{
    const auto address = GW::Scanner::Find("\x8b\xc3\x25\xfd\x00\x00\x00\x3c\xfd", "xxxxxxxxx", -0x5f);
    if (GW::Scanner::IsValidPtr(address, GW::ScannerSection::Section_TEXT)) {
        GetItemDescription_Func = (GetItemDescriptionCallback)address;
        GW::HookBase::CreateHook((void**) & GetItemDescription_Func, OnGetItemDescription, (void**)&GetItemDescription_Ret);
        GW::HookBase::EnableHooks(GetItemDescription_Func);
    }
#ifdef _DEBUG
    ASSERT(GetItemDescription_Func);
#endif
}

void ItemDescriptionHandler::SignalTerminate()
{
    ToolboxModule::SignalTerminate();
    if (GetItemDescription_Func) {
        GW::HookBase::RemoveHook(GetItemDescription_Func);
        GetItemDescription_Func = nullptr;
    }
}

void ItemDescriptionHandler::RegisterDescriptionCallback(GetItemDescriptionCallback callback,int altitude)
{
    UnregisterDescriptionCallback(callback);
    for (auto it = callbacks.cbegin(); it != callbacks.end(); it++) {
        if (it->second > altitude) {
            callbacks.insert(it, { callback,altitude });
            return;
        }
    }
    callbacks.push_back({ callback,altitude });
}

// Unregisters all instance of the provided callback
void ItemDescriptionHandler::UnregisterDescriptionCallback(GetItemDescriptionCallback callback)
{
    std::erase_if(callbacks, [callback](const auto& cb) {
        return cb.first == callback;
    });
}
