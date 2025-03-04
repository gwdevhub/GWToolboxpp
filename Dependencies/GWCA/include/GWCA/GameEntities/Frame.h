#pragma once

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/List.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Managers/UIMgr.h>

namespace GW {
    struct TabsFrame : UI::Frame {
        GWCA_API bool AddTab(const wchar_t* tab_name_enc, uint32_t flags, uint32_t tab_id, GW::UI::UIInteractionCallback* callback);
        GWCA_API bool DisableTab(uint32_t tab_id);
        GWCA_API bool EnableTab(uint32_t tab_id);
        GWCA_API bool GetCurrentTabIndex(uint32_t* tab_id);
        GWCA_API bool GetTabFrameId(uint32_t tab_id, uint32_t* frame_id);
        GWCA_API bool GetIsTabEnabled(uint32_t tab_id, uint32_t* is_enabled);

    };
    struct ScrollableFrame : UI::Frame {
        GWCA_API bool ClearItems();
        GWCA_API bool RemoveItem(uint32_t child_offset_id);
        GWCA_API bool AddItem(uint32_t flags, uint32_t child_offset_id, GW::UI::UIInteractionCallback* callback);
        GWCA_API bool GetItemFrameId(uint32_t child_offset_id, uint32_t* frame_id);
        GWCA_API bool GetItemRect(uint32_t child_offset_id, float rect[4]);
        GWCA_API bool GetCount(uint32_t* size);
    };
    struct DropdownFrame : UI::Frame {
        GWCA_API bool SelectOption(uint32_t option);
        GWCA_API bool GetCount(uint32_t* count);
        GWCA_API bool GetOptionValue(uint32_t index, uint32_t* value);
    };
    struct ButtonFrame : UI::Frame {
        GWCA_API bool GetLabel(const wchar_t** enc_string);
        GWCA_API bool Click();
        GWCA_API bool MouseDown();
        GWCA_API bool MouseUp();
    };
}
