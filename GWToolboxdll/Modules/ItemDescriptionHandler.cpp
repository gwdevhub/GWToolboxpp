#include <Modules/ItemDescriptionHandler.h>

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

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
}

void ItemDescriptionHandler::Initialize()
{
    GetItemDescription_Func = reinterpret_cast<GetItemDescriptionCallback>(GW::Scanner::Find("\x8b\xc3\x25\xfd\x00\x00\x00\x3c\xfd", "xxxxxxxxx", -0x5f));
    if (GetItemDescription_Func) {
        GW::HookBase::CreateHook((void**) & GetItemDescription_Func, OnGetItemDescription, (void**)&GetItemDescription_Ret);
        GW::HookBase::EnableHooks(GetItemDescription_Func);
    }
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
