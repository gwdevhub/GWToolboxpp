#pragma once

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/List.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Managers/UIMgr.h>

namespace GW {
    struct Module;
    extern Module FrameModule;

    struct ButtonFrame : UI::Frame {
		GWCA_API static ButtonFrame* Create(uint32_t parent_frame_id, uint32_t flags = 0x300, uint32_t child_offset_id = 0xff, const wchar_t* button_label = nullptr, const wchar_t* frame_label = nullptr);
        GWCA_API bool GetLabel(const wchar_t** enc_string);
        GWCA_API bool SetLabel(const wchar_t* enc_string);
        GWCA_API bool Click();
        GWCA_API bool MouseDown();
        GWCA_API bool MouseUp();
    };

    struct TabsFrame : UI::Frame {
        GWCA_API bool AddTab(const wchar_t* tab_name_enc, uint32_t flags, uint32_t tab_id, GW::UI::UIInteractionCallback* callback);
        GWCA_API bool DisableTab(uint32_t tab_id);
        GWCA_API bool EnableTab(uint32_t tab_id);
        GWCA_API bool GetCurrentTabIndex(uint32_t* tab_id);
        GWCA_API bool GetTabFrameId(uint32_t tab_id, uint32_t* frame_id);
        GWCA_API bool GetIsTabEnabled(uint32_t tab_id, uint32_t* is_enabled);
		GWCA_API GW::UI::Frame* GetTabByLabel(const wchar_t* label);
        GWCA_API GW::UI::Frame* GetCurrentTab();
		GWCA_API bool ChooseTab(GW::UI::Frame* tab_frame);
        GWCA_API GW::ButtonFrame* GetTabButton(GW::UI::Frame* tab_frame);

    };
    struct ScrollableFrame : UI::Frame {
        GWCA_API bool ClearItems();
        GWCA_API bool RemoveItem(uint32_t child_offset_id);
        GWCA_API bool AddItem(uint32_t flags, uint32_t child_offset_id, GW::UI::UIInteractionCallback* callback);
        GWCA_API bool GetItemFrameId(uint32_t child_offset_id, uint32_t* frame_id);
        // Returns false if the request failed, or nothing is selected
        GWCA_API bool GetSelectedValue(uint32_t* selected_value);
        GWCA_API bool GetItemRect(uint32_t child_offset_id, float rect[4]);
        GWCA_API bool GetCount(uint32_t* size);
    };
    struct DropdownFrame : UI::Frame {
        GWCA_API bool SelectOption(uint32_t option);
        GWCA_API bool GetCount(uint32_t* count);
        GWCA_API bool GetOptionValue(uint32_t index, uint32_t* value);
    };

}
