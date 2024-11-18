#include "stdafx.h"

#include <GWCA/Utilities/Debug.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MemoryMgr.h>

#include <Defines.h>
#include <ImGuiAddons.h>
#include "VendorFix.h"

#include <GWCA/Managers/UIMgr.h>

#include <GWCA/GameEntities/Item.h>

#include <GWCA/Managers/ItemMgr.h>
#include "Timer.h"

namespace {

    void RefreshVendorItems(GW::UI::Frame* frame) {
        if (!frame) return;
        /*
        If the vendor frame has just been created, look at our inventory.
        Any valid item ID after thr 56th item won't be found immediately by a collector.

        Fortunately, triggering ui message GW::UI::UIMessage::kInventorySlotUpdated on the slot seems to work.

        Identify the "problem" slots, and let the vendor know by spoofing the packet.
        */

        const auto inventory = GW::Items::GetInventory();
        ASSERT(inventory);

        size_t item_tally = 0;
        const GW::Bag* check_bags[] = { inventory->backpack,inventory->belt_pouch,inventory->bag1,inventory->bag2,inventory->equipped_items };

        GW::UI::UIPacket::kInventorySlotUpdated ui_message;
        memset(&ui_message, 0, sizeof(ui_message));

        for (const auto bag : check_bags) {
            if (!bag) continue;
            for (const auto item : bag->items) {
                ui_message.unk = item_tally++;
                if (!item) continue;
                ui_message.item_id = item->item_id;
                ui_message.slot_id = item->slot;
                ui_message.bag_index = bag->index;
                GW::UI::SendFrameUIMessage(frame, GW::UI::UIMessage::kInventorySlotCleared, &ui_message);
                GW::UI::SendFrameUIMessage(frame, GW::UI::UIMessage::kInventorySlotUpdated, &ui_message);
            }
        }
    }

    typedef uint32_t(__fastcall* UICtlCallback)(uint32_t* ecx, void* edx, void** wParam);

    UICtlCallback Collector_Create_UICtlCallback_Func = nullptr, Collector_Create_UICtlCallback_Ret = nullptr;
    UICtlCallback Crafter_Create_UICtlCallback_Func = nullptr, Crafter_Create_UICtlCallback_Ret = nullptr;

    uint32_t __fastcall OnCollectorCreateWindow(uint32_t* ecx, void* edx, void** wParam) {
        GW::Hook::EnterHook();
        uint32_t ret = Collector_Create_UICtlCallback_Ret(ecx, edx, wParam);
        RefreshVendorItems(GW::UI::GetFrameById(ecx[1]));
        GW::Hook::LeaveHook();
        return ret;
    }
    uint32_t __fastcall OnCrafterCreateWindow(uint32_t* ecx, void* edx, void** wParam) {
        GW::Hook::EnterHook();
        uint32_t ret = Crafter_Create_UICtlCallback_Ret(ecx, edx, wParam);
        RefreshVendorItems(GW::UI::GetFrameById(ecx[1]));
        GW::Hook::LeaveHook();
        return ret;
    }
}
void VendorFix::Initialize() {
    ToolboxModule::Initialize();

    Collector_Create_UICtlCallback_Func = (UICtlCallback)GW::Scanner::FindAssertion("\\Code\\Gw\\Ui\\Game\\vendor\\vncollect.cpp", "collect.service == CHAR_TRANSACTION_COLLECT", -0x47);
    if (Collector_Create_UICtlCallback_Func) {
        GW::Hook::CreateHook((void**)&Collector_Create_UICtlCallback_Func, OnCollectorCreateWindow, (void**)&Collector_Create_UICtlCallback_Ret);
        GW::Hook::EnableHooks(Collector_Create_UICtlCallback_Func);
    }
    Crafter_Create_UICtlCallback_Func = (UICtlCallback)GW::Scanner::FindAssertion("\\Code\\Gw\\Ui\\Game\\vendor\\vncraft.cpp", "service.service == CHAR_TRANSACTION_CRAFT", -0x37);
    if (Crafter_Create_UICtlCallback_Func) {
        GW::Hook::CreateHook((void**)&Crafter_Create_UICtlCallback_Func, OnCrafterCreateWindow, (void**)&Crafter_Create_UICtlCallback_Ret);
        GW::Hook::EnableHooks(Crafter_Create_UICtlCallback_Func);
    }
}
void VendorFix::Terminate() {
    ToolboxModule::Terminate();

    GW::Hook::RemoveHook(Collector_Create_UICtlCallback_Func);
    GW::Hook::RemoveHook(Crafter_Create_UICtlCallback_Func);
}
