#include <Modules/ItemDescriptionHandler.h>

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

void __cdecl ItemDescriptionHandler::OnGetItemDescription_Wrapper(const uint32_t item_id, const uint32_t flags, const uint32_t quantity, const uint32_t unk, wchar_t** out_name, wchar_t** out_description)
{
    Instance().OnGetItemDescription(item_id, flags, quantity, unk, out_name, out_description);
}

void ItemDescriptionHandler::Initialize()
{
    GetItemDescription_Func = reinterpret_cast<GetItemDescription_pt>(GW::Scanner::Find("\x8b\xc3\x25\xfd\x00\x00\x00\x3c\xfd", "xxxxxxxxx", -0x5f));
    if (GetItemDescription_Func) {
        GW::HookBase::CreateHook(
            reinterpret_cast<void**>(&GetItemDescription_Func),
            reinterpret_cast<void*>(&OnGetItemDescription_Wrapper),
            reinterpret_cast<void**>(&GetItemDescription_Ret));
        GW::HookBase::EnableHooks(GetItemDescription_Func);
    }
}

void ItemDescriptionHandler::SignalTerminate()
{
    if (GetItemDescription_Func) {
        GW::HookBase::RemoveHook(GetItemDescription_Func);
        GetItemDescription_Func = nullptr;
    }

    ToolboxModule::SignalTerminate();
}

void ItemDescriptionHandler::RegisterDescriptionCallback(ItemDescriptionCallback callback, unsigned int altitude)
{
    auto it = this->callbacks.cbegin();
    while (it != this->callbacks.cend()) {
        if (it->altitude > altitude) {
            break;
        }

        it++;
    }

    this->callbacks.insert(it, CallbackEntry { altitude, callback });
}

void ItemDescriptionHandler::OnGetItemDescription(const uint32_t item_id, const uint32_t flags, const uint32_t quantity, const uint32_t unk, wchar_t** out_name, wchar_t** out_desc) {
    GW::Hook::EnterHook();

    {
        std::lock_guard guard(this->modified_strings_mutex);

        this->modified_name.clear();
        this->modified_description.clear();

        GetItemDescription_Ret(item_id, flags, quantity, unk, out_name, out_desc);

        if (out_name != nullptr && *out_name != nullptr) {
            this->modified_name = *out_name;
        }
        if (out_desc != nullptr && *out_desc != nullptr) {
            this->modified_description = *out_desc;
        }

        auto args = ItemDescriptionEventArgs {
            .item_id =  item_id,
            .flags = flags,
            .quantity = quantity,
            .unk = unk,
            .name = this->modified_name,
            .description = this->modified_description,
        };

        for (auto& cb : this->callbacks) {
            cb.callback(args);
        }

        if (out_name != nullptr) {
            if (modified_name.empty()) {
                *out_name = nullptr;
            }
            else {
                *out_name = modified_name.data();
            }
        }
        if (out_desc != nullptr) {
            if (modified_description.empty()) {
                *out_desc = nullptr;
            }
            else {
                *out_desc = modified_description.data();
            }
        }
    }

    GW::Hook::LeaveHook();
}

// Unregisters all instance of the provided callback
void ItemDescriptionHandler::UnregisterDescriptionCallback(ItemDescriptionCallback callback)
{
    std::erase_if(this->callbacks, [callback](const auto& cb) {
        return cb.callback == callback;
    });
}

// Unregisters all instances of the provided callback at the provided altitude.  Leaves all instances at other
// altitudes registered.
void ItemDescriptionHandler::UnregisterDescriptionCallback(ItemDescriptionCallback callback, unsigned int altitude)
{
    std::erase_if(this->callbacks, [callback, altitude](const auto& cb) {
        return cb.callback == callback && cb.altitude == altitude;
    });
}
