#pragma once
#include <GWCA/Runtime.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/List.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Managers/UIMgr.h>
#include <map>

namespace GW {
    struct Module;
    extern Module FrameModule;

    namespace UI {
        struct Frame;
        bool SetFrameMargins(GW::UI::Frame* frame, uint32_t flags, float size[4], float input_mask[4], uint32_t type);
    }

    struct ButtonFrame : UI::Frame {
		static ButtonFrame* Create(uint32_t parent_frame_id, uint32_t flags = 0x300, uint32_t child_offset_id = 0xff, const wchar_t* button_label = nullptr, const wchar_t* frame_label = nullptr);
        bool GetLabel(const wchar_t** enc_string);
        bool SetLabel(const wchar_t* enc_string);
        bool Click();
        bool MouseAction(UI::UIPacket::ActionState action);
        bool DoubleClick();
    };

    struct TabsFrame : UI::Frame {
        GW::UI::Frame* AddTab(const wchar_t* tab_name_enc, uint32_t flags, uint32_t tab_child_offset_id, GW::UI::UIInteractionCallback callback, void* wparam = 0);
        bool DisableTab(uint32_t tab_id);
        bool EnableTab(uint32_t tab_id);
        bool RemoveTab(uint32_t tab_id);
        bool GetCurrentTabIndex(uint32_t* tab_id);
        bool GetTabFrameId(uint32_t tab_id, uint32_t* out_frame_id);
        bool GetIsTabEnabled(uint32_t tab_id, uint32_t* is_enabled);
		GW::UI::Frame* GetTabByLabel(const wchar_t* label);
        GW::UI::Frame* GetCurrentTab();
        bool ChooseTab(uint32_t tab_index);
        bool ChooseTab(GW::UI::Frame* tab_frame) { return tab_frame && ChooseTab(tab_frame->child_offset_id); }
        GW::ButtonFrame* GetTabButton(GW::UI::Frame* tab_frame);

    };
    // Sorting handler is actually a boolean function - return "1" if frame_id_1 needs to be bubbled higher than frame_id_2
    typedef int(__cdecl* SortHandler_pt)(uint32_t frame_id_1, uint32_t frame_id_2);
    struct ItemListFrame : UI::Frame {
        // Returns false if the request failed, or nothing is selected
        bool GetSelectedValue(uint32_t* selected_value);
        bool SetSortHandler(SortHandler_pt sortHandler);
        SortHandler_pt GetSortHandler();
    };

    struct ScrollableFrame : UI::Frame {

        // Scrollable frame always has an "inner" page that handles the content. Only really need to mess with this if you're doing something odd.
        struct ScrollablePageContext {
            uint32_t flags;
            GW::UI::UIInteractionCallback page_callback;
            uint32_t wparam;
        };

        // Scrollable Frames always need a valid context, but pass nullptr to use the default one used for most of the scrollable frames in the game
        static ScrollableFrame* Create(uint32_t parent_frame_id, uint32_t flags = 0x20000, uint32_t child_offset_id = 0xff, ScrollableFrame::ScrollablePageContext* context = nullptr, const wchar_t* frame_label = nullptr);

        bool SetSortHandler(SortHandler_pt sortHandler);
        SortHandler_pt GetSortHandler();
        bool ClearItems();
        bool RemoveItem(uint32_t item_child_offset_id);
        // Returns the frame_id of the added item's frame, or 0 on failure.
        uint32_t AddItem(uint32_t flags, uint32_t item_child_offset_id, GW::UI::UIInteractionCallback callback);
        uint32_t GetItemFrameId(uint32_t item_child_offset_id);
        bool GetSelectedValue(uint32_t* selected_value);

        // This is actually the child_frame_id of the last child in the list - things that use sorting, or the child id to identify the frame, will not represent the size.
        bool GetCount(uint32_t* size);
        uint32_t GetItems(uint32_t* child_frame_id_buffer = nullptr, uint32_t buffer_len = 0);
        
        GW::ItemListFrame* GetPage();
        // Scrollable frame always has an "inner" page that handles the content. Only really need to mess with this if you're doing something odd.
        GW::ItemListFrame* SetPage(ScrollableFrame::ScrollablePageContext*);
    };




    struct EditableTextFrame : UI::Frame {
        const wchar_t* GetValue();
        bool SetValue(const wchar_t* value);
        bool SetMaxLength(uint32_t max_length);
        bool IsReadOnly();
        bool SetReadOnly(bool readonly);
    };

    struct ProgressBar final : ButtonFrame {
        uint32_t GetValue();
        bool SetValue(uint32_t value);
        bool SetMax(uint32_t value);
        bool SetColorId(uint32_t color_id);
        enum class ProgressBarStyle : uint32_t {
            kPeach,
            kPink,
            kGrey,
            kBlue,
            kGreen,
            kRed,
            kPurple,
            kOlive,
            kUnk
        };
        bool SetStyle(ProgressBar::ProgressBarStyle style);
    };

    struct CheckboxFrame final : ButtonFrame {
        static CheckboxFrame* Create(uint32_t parent_frame_id, uint32_t flags = 0x8000, uint32_t child_offset_id = 0xff, const wchar_t* text_label_enc_string = nullptr, const wchar_t* frame_label = nullptr);
        bool IsChecked();
        bool SetChecked(bool checked);

        uint32_t GetValue();
        bool SetValue(uint32_t value);
    };
    struct DropdownFrame final : UI::Frame {
        uint32_t GetOptions(uint32_t* buffer = nullptr, uint32_t capacity = 0);
        bool SelectOption(uint32_t value);
        bool SelectIndex(uint32_t index);
        bool AddOption(const wchar_t* label_enc_string, uint32_t value);
        bool GetCount(uint32_t* count);
        bool GetOptionValue(uint32_t index, uint32_t* value);
        bool GetOptionIndex(uint32_t value, uint32_t* index);
		bool GetSelectedIndex(uint32_t* index);
        // Some dropdowns are only used by reference to their index. Others actually have values assigned to their indeces.
		bool HasValueMapping();

        uint32_t GetValue();
        bool SetValue(uint32_t value);
    };
    struct SliderFrame final : UI::Frame {
        bool GetValue(uint32_t* selected_value);
        bool SetValue(uint32_t value);

        uint32_t GetValue() { uint32_t value = 0; return GetValue(&value) ? value : 0; }
    };

    struct TextLabelFrame : UI::Frame {
        static TextLabelFrame* Create(uint32_t parent_frame_id, uint32_t flags = 0x300, uint32_t child_offset_id = 0xff, const wchar_t* text_label_enc_string = nullptr, const wchar_t* frame_label = nullptr);
        const wchar_t* GetEncodedLabel();
        const wchar_t* GetDecodedLabel();
        bool SetLabel(const wchar_t* enc_string);
        bool SetFont(uint32_t);
    };
    struct MultiLineTextLabelFrame final : TextLabelFrame {
        const wchar_t* GetEncodedLabel();
        const wchar_t* GetDecodedLabel();
        bool SetLabel(const wchar_t* enc_string);
    };

    static_assert(sizeof(ProgressBar) == sizeof(UI::Frame));
    static_assert(sizeof(CheckboxFrame) == sizeof(UI::Frame));
    static_assert(sizeof(DropdownFrame) == sizeof(UI::Frame));
    static_assert(sizeof(SliderFrame) == sizeof(UI::Frame));

}

namespace GW::UI {
    extern "C" GWCA_API bool __cdecl GW_UI_SetFrameMargins(GW::UI::Frame* frame, uint32_t flags, float size [4], float input_mask [4], uint32_t type) noexcept;
}
namespace GW {
    extern "C" GWCA_API ButtonFrame* __cdecl GW_ButtonFrame_Create(uint32_t parent_frame_id, uint32_t flags, uint32_t child_offset_id, const wchar_t* button_label, const wchar_t* frame_label) noexcept;
    extern "C" GWCA_API bool __cdecl GW_ButtonFrame_GetLabel(ButtonFrame* self, const wchar_t** enc_string) noexcept;
    extern "C" GWCA_API bool __cdecl GW_ButtonFrame_SetLabel(ButtonFrame* self, const wchar_t* enc_string) noexcept;
    extern "C" GWCA_API bool __cdecl GW_ButtonFrame_Click(ButtonFrame* self) noexcept;
    extern "C" GWCA_API bool __cdecl GW_ButtonFrame_MouseAction(ButtonFrame* self, UI::UIPacket::ActionState action) noexcept;
    extern "C" GWCA_API bool __cdecl GW_ButtonFrame_DoubleClick(ButtonFrame* self) noexcept;
    extern "C" GWCA_API GW::UI::Frame* __cdecl GW_TabsFrame_AddTab(TabsFrame* self, const wchar_t* tab_name_enc, uint32_t flags, uint32_t child_offset_id, GW::UI::UIInteractionCallback callback, void* wparam) noexcept;
    extern "C" GWCA_API bool __cdecl GW_TabsFrame_DisableTab(TabsFrame* self, uint32_t tab_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_TabsFrame_EnableTab(TabsFrame* self, uint32_t tab_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_TabsFrame_RemoveTab(TabsFrame* self, uint32_t tab_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_TabsFrame_GetCurrentTabIndex(TabsFrame* self, uint32_t* tab_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_TabsFrame_GetTabFrameId(TabsFrame* self, uint32_t tab_id, uint32_t* frame_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_TabsFrame_GetIsTabEnabled(TabsFrame* self, uint32_t tab_id, uint32_t* is_enabled) noexcept;
    extern "C" GWCA_API GW::UI::Frame* __cdecl GW_TabsFrame_GetTabByLabel(TabsFrame* self, const wchar_t* label) noexcept;
    extern "C" GWCA_API GW::UI::Frame* __cdecl GW_TabsFrame_GetCurrentTab(TabsFrame* self) noexcept;
    extern "C" GWCA_API bool __cdecl GW_TabsFrame_ChooseTab(TabsFrame* self, uint32_t tab_index) noexcept;
    extern "C" GWCA_API GW::ButtonFrame* __cdecl GW_TabsFrame_GetTabButton(TabsFrame* self, GW::UI::Frame* tab_frame) noexcept;
    extern "C" GWCA_API bool __cdecl GW_ItemListFrame_GetSelectedValue(ItemListFrame* self, uint32_t* selected_value) noexcept;
    extern "C" GWCA_API bool __cdecl GW_ItemListFrame_SetSortHandler(ItemListFrame* self, SortHandler_pt sortHandler) noexcept;
    extern "C" GWCA_API SortHandler_pt __cdecl GW_ItemListFrame_GetSortHandler(ItemListFrame* self) noexcept;
    extern "C" GWCA_API ScrollableFrame* __cdecl GW_ScrollableFrame_Create(uint32_t parent_frame_id, uint32_t flags, uint32_t child_offset_id, ScrollableFrame::ScrollablePageContext* context, const wchar_t* frame_label) noexcept;
    extern "C" GWCA_API bool __cdecl GW_ScrollableFrame_SetSortHandler(ScrollableFrame* self, SortHandler_pt sortHandler) noexcept;
    extern "C" GWCA_API SortHandler_pt __cdecl GW_ScrollableFrame_GetSortHandler(ScrollableFrame* self) noexcept;
    extern "C" GWCA_API bool __cdecl GW_ScrollableFrame_ClearItems(ScrollableFrame* self) noexcept;
    extern "C" GWCA_API bool __cdecl GW_ScrollableFrame_RemoveItem(ScrollableFrame* self, uint32_t child_offset_id) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_ScrollableFrame_AddItem(ScrollableFrame* self, uint32_t flags, uint32_t child_offset_id, GW::UI::UIInteractionCallback callback) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_ScrollableFrame_GetItemFrameId(ScrollableFrame* self, uint32_t child_offset_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_ScrollableFrame_GetSelectedValue(ScrollableFrame* self, uint32_t* selected_value) noexcept;
    extern "C" GWCA_API bool __cdecl GW_ScrollableFrame_GetCount(ScrollableFrame* self, uint32_t* size) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_ScrollableFrame_GetItems(ScrollableFrame* self, uint32_t* child_frame_id_buffer, uint32_t buffer_len) noexcept;
    extern "C" GWCA_API GW::ItemListFrame* __cdecl GW_ScrollableFrame_GetPage(ScrollableFrame* self) noexcept;
    extern "C" GWCA_API GW::ItemListFrame* __cdecl GW_ScrollableFrame_SetPage(ScrollableFrame* self, ScrollableFrame::ScrollablePageContext* gwca_arg0) noexcept;
    extern "C" GWCA_API const wchar_t* __cdecl GW_EditableTextFrame_GetValue(EditableTextFrame* self) noexcept;
    extern "C" GWCA_API bool __cdecl GW_EditableTextFrame_SetValue(EditableTextFrame* self, const wchar_t* value) noexcept;
    extern "C" GWCA_API bool __cdecl GW_EditableTextFrame_SetMaxLength(EditableTextFrame* self, uint32_t max_length) noexcept;
    extern "C" GWCA_API bool __cdecl GW_EditableTextFrame_IsReadOnly(EditableTextFrame* self) noexcept;
    extern "C" GWCA_API bool __cdecl GW_EditableTextFrame_SetReadOnly(EditableTextFrame* self, bool readonly) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_ProgressBar_GetValue(ProgressBar* self) noexcept;
    extern "C" GWCA_API bool __cdecl GW_ProgressBar_SetValue(ProgressBar* self, uint32_t value) noexcept;
    extern "C" GWCA_API bool __cdecl GW_ProgressBar_SetMax(ProgressBar* self, uint32_t value) noexcept;
    extern "C" GWCA_API bool __cdecl GW_ProgressBar_SetColorId(ProgressBar* self, uint32_t color_id) noexcept;
    extern "C" GWCA_API bool __cdecl GW_ProgressBar_SetStyle(ProgressBar* self, ProgressBar::ProgressBarStyle style) noexcept;
    extern "C" GWCA_API CheckboxFrame* __cdecl GW_CheckboxFrame_Create(uint32_t parent_frame_id, uint32_t flags, uint32_t child_offset_id, const wchar_t* text_label_enc_string, const wchar_t* frame_label) noexcept;
    extern "C" GWCA_API bool __cdecl GW_CheckboxFrame_IsChecked(CheckboxFrame* self) noexcept;
    extern "C" GWCA_API bool __cdecl GW_CheckboxFrame_SetChecked(CheckboxFrame* self, bool checked) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_CheckboxFrame_GetValue(CheckboxFrame* self) noexcept;
    extern "C" GWCA_API bool __cdecl GW_CheckboxFrame_SetValue(CheckboxFrame* self, uint32_t value) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_DropdownFrame_GetOptions(DropdownFrame* self, uint32_t* buffer, uint32_t capacity) noexcept;
    extern "C" GWCA_API bool __cdecl GW_DropdownFrame_SelectOption(DropdownFrame* self, uint32_t value) noexcept;
    extern "C" GWCA_API bool __cdecl GW_DropdownFrame_SelectIndex(DropdownFrame* self, uint32_t index) noexcept;
    extern "C" GWCA_API bool __cdecl GW_DropdownFrame_AddOption(DropdownFrame* self, const wchar_t* label_enc_string, uint32_t value) noexcept;
    extern "C" GWCA_API bool __cdecl GW_DropdownFrame_GetCount(DropdownFrame* self, uint32_t* count) noexcept;
    extern "C" GWCA_API bool __cdecl GW_DropdownFrame_GetOptionValue(DropdownFrame* self, uint32_t index, uint32_t* value) noexcept;
    extern "C" GWCA_API bool __cdecl GW_DropdownFrame_GetOptionIndex(DropdownFrame* self, uint32_t value, uint32_t* index) noexcept;
    extern "C" GWCA_API bool __cdecl GW_DropdownFrame_GetSelectedIndex(DropdownFrame* self, uint32_t* index) noexcept;
    extern "C" GWCA_API bool __cdecl GW_DropdownFrame_HasValueMapping(DropdownFrame* self) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_DropdownFrame_GetValue(DropdownFrame* self) noexcept;
    extern "C" GWCA_API bool __cdecl GW_DropdownFrame_SetValue(DropdownFrame* self, uint32_t value) noexcept;
    extern "C" GWCA_API bool __cdecl GW_SliderFrame_GetValue(SliderFrame* self, uint32_t* selected_value) noexcept;
    extern "C" GWCA_API bool __cdecl GW_SliderFrame_SetValue(SliderFrame* self, uint32_t value) noexcept;
    extern "C" GWCA_API TextLabelFrame* __cdecl GW_TextLabelFrame_Create(uint32_t parent_frame_id, uint32_t flags, uint32_t child_offset_id, const wchar_t* text_label_enc_string, const wchar_t* frame_label) noexcept;
    extern "C" GWCA_API const wchar_t* __cdecl GW_TextLabelFrame_GetEncodedLabel(TextLabelFrame* self) noexcept;
    extern "C" GWCA_API const wchar_t* __cdecl GW_TextLabelFrame_GetDecodedLabel(TextLabelFrame* self) noexcept;
    extern "C" GWCA_API bool __cdecl GW_TextLabelFrame_SetLabel(TextLabelFrame* self, const wchar_t* enc_string) noexcept;
    extern "C" GWCA_API bool __cdecl GW_TextLabelFrame_SetFont(TextLabelFrame* self, uint32_t gwca_arg0) noexcept;
    extern "C" GWCA_API const wchar_t* __cdecl GW_MultiLineTextLabelFrame_GetEncodedLabel(MultiLineTextLabelFrame* self) noexcept;
    extern "C" GWCA_API const wchar_t* __cdecl GW_MultiLineTextLabelFrame_GetDecodedLabel(MultiLineTextLabelFrame* self) noexcept;
    extern "C" GWCA_API bool __cdecl GW_MultiLineTextLabelFrame_SetLabel(MultiLineTextLabelFrame* self, const wchar_t* enc_string) noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW::UI {
    inline bool SetFrameMargins(GW::UI::Frame* frame, uint32_t flags, float size [4], float input_mask [4], uint32_t type)
    {
        return GWCA_CALL_C(GW_UI_SetFrameMargins, frame, flags, size, input_mask, type);
    }
}
namespace GW {
    inline ButtonFrame* ButtonFrame::Create(uint32_t parent_frame_id, uint32_t flags, uint32_t child_offset_id, const wchar_t* button_label, const wchar_t* frame_label)
    {
        return GWCA_CALL_C(GW_ButtonFrame_Create, parent_frame_id, flags, child_offset_id, button_label, frame_label);
    }

    inline bool ButtonFrame::GetLabel(const wchar_t** enc_string)
    {
        return GWCA_CALL_C(GW_ButtonFrame_GetLabel, this, enc_string);
    }

    inline bool ButtonFrame::SetLabel(const wchar_t* enc_string)
    {
        return GWCA_CALL_C(GW_ButtonFrame_SetLabel, this, enc_string);
    }

    inline bool ButtonFrame::Click()
    {
        return GWCA_CALL_C(GW_ButtonFrame_Click, this);
    }

    inline bool ButtonFrame::MouseAction(UI::UIPacket::ActionState action)
    {
        return GWCA_CALL_C(GW_ButtonFrame_MouseAction, this, action);
    }

    inline bool ButtonFrame::DoubleClick()
    {
        return GWCA_CALL_C(GW_ButtonFrame_DoubleClick, this);
    }

    inline GW::UI::Frame* TabsFrame::AddTab(const wchar_t* tab_name_enc, uint32_t flags, uint32_t tab_child_offset_id, GW::UI::UIInteractionCallback callback, void* wparam)
    {
        return GWCA_CALL_C(GW_TabsFrame_AddTab, this, tab_name_enc, flags, tab_child_offset_id, callback, wparam);
    }

    inline bool TabsFrame::DisableTab(uint32_t tab_id)
    {
        return GWCA_CALL_C(GW_TabsFrame_DisableTab, this, tab_id);
    }

    inline bool TabsFrame::EnableTab(uint32_t tab_id)
    {
        return GWCA_CALL_C(GW_TabsFrame_EnableTab, this, tab_id);
    }

    inline bool TabsFrame::RemoveTab(uint32_t tab_id)
    {
        return GWCA_CALL_C(GW_TabsFrame_RemoveTab, this, tab_id);
    }

    inline bool TabsFrame::GetCurrentTabIndex(uint32_t* tab_id)
    {
        return GWCA_CALL_C(GW_TabsFrame_GetCurrentTabIndex, this, tab_id);
    }

    inline bool TabsFrame::GetTabFrameId(uint32_t tab_id, uint32_t* out_frame_id)
    {
        return GWCA_CALL_C(GW_TabsFrame_GetTabFrameId, this, tab_id, out_frame_id);
    }

    inline bool TabsFrame::GetIsTabEnabled(uint32_t tab_id, uint32_t* is_enabled)
    {
        return GWCA_CALL_C(GW_TabsFrame_GetIsTabEnabled, this, tab_id, is_enabled);
    }

    inline GW::UI::Frame* TabsFrame::GetTabByLabel(const wchar_t* label)
    {
        return GWCA_CALL_C(GW_TabsFrame_GetTabByLabel, this, label);
    }

    inline GW::UI::Frame* TabsFrame::GetCurrentTab()
    {
        return GWCA_CALL_C(GW_TabsFrame_GetCurrentTab, this);
    }

    inline bool TabsFrame::ChooseTab(uint32_t tab_index)
    {
        return GWCA_CALL_C(GW_TabsFrame_ChooseTab, this, tab_index);
    }

    inline GW::ButtonFrame* TabsFrame::GetTabButton(GW::UI::Frame* tab_frame)
    {
        return GWCA_CALL_C(GW_TabsFrame_GetTabButton, this, tab_frame);
    }

    inline bool ItemListFrame::GetSelectedValue(uint32_t* selected_value)
    {
        return GWCA_CALL_C(GW_ItemListFrame_GetSelectedValue, this, selected_value);
    }

    inline bool ItemListFrame::SetSortHandler(SortHandler_pt sortHandler)
    {
        return GWCA_CALL_C(GW_ItemListFrame_SetSortHandler, this, sortHandler);
    }

    inline SortHandler_pt ItemListFrame::GetSortHandler()
    {
        return GWCA_CALL_C(GW_ItemListFrame_GetSortHandler, this);
    }

    inline ScrollableFrame* ScrollableFrame::Create(uint32_t parent_frame_id, uint32_t flags, uint32_t child_offset_id, ScrollableFrame::ScrollablePageContext* context, const wchar_t* frame_label)
    {
        return GWCA_CALL_C(GW_ScrollableFrame_Create, parent_frame_id, flags, child_offset_id, context, frame_label);
    }

    inline bool ScrollableFrame::SetSortHandler(SortHandler_pt sortHandler)
    {
        return GWCA_CALL_C(GW_ScrollableFrame_SetSortHandler, this, sortHandler);
    }

    inline SortHandler_pt ScrollableFrame::GetSortHandler()
    {
        return GWCA_CALL_C(GW_ScrollableFrame_GetSortHandler, this);
    }

    inline bool ScrollableFrame::ClearItems()
    {
        return GWCA_CALL_C(GW_ScrollableFrame_ClearItems, this);
    }

    inline bool ScrollableFrame::RemoveItem(uint32_t item_child_offset_id)
    {
        return GWCA_CALL_C(GW_ScrollableFrame_RemoveItem, this, item_child_offset_id);
    }

    inline uint32_t ScrollableFrame::AddItem(uint32_t flags, uint32_t item_child_offset_id, GW::UI::UIInteractionCallback callback)
    {
        return GWCA_CALL_C(GW_ScrollableFrame_AddItem, this, flags, item_child_offset_id, callback);
    }

    inline uint32_t ScrollableFrame::GetItemFrameId(uint32_t item_child_offset_id)
    {
        return GWCA_CALL_C(GW_ScrollableFrame_GetItemFrameId, this, item_child_offset_id);
    }

    inline bool ScrollableFrame::GetSelectedValue(uint32_t* selected_value)
    {
        return GWCA_CALL_C(GW_ScrollableFrame_GetSelectedValue, this, selected_value);
    }

    inline bool ScrollableFrame::GetCount(uint32_t* size)
    {
        return GWCA_CALL_C(GW_ScrollableFrame_GetCount, this, size);
    }

    inline uint32_t ScrollableFrame::GetItems(uint32_t* child_frame_id_buffer, uint32_t buffer_len)
    {
        return GWCA_CALL_C(GW_ScrollableFrame_GetItems, this, child_frame_id_buffer, buffer_len);
    }

    inline GW::ItemListFrame* ScrollableFrame::GetPage()
    {
        return GWCA_CALL_C(GW_ScrollableFrame_GetPage, this);
    }

    inline GW::ItemListFrame* ScrollableFrame::SetPage(ScrollableFrame::ScrollablePageContext* gwca_arg0)
    {
        return GWCA_CALL_C(GW_ScrollableFrame_SetPage, this, gwca_arg0);
    }

    inline const wchar_t* EditableTextFrame::GetValue()
    {
        return GWCA_CALL_C(GW_EditableTextFrame_GetValue, this);
    }

    inline bool EditableTextFrame::SetValue(const wchar_t* value)
    {
        return GWCA_CALL_C(GW_EditableTextFrame_SetValue, this, value);
    }

    inline bool EditableTextFrame::SetMaxLength(uint32_t max_length)
    {
        return GWCA_CALL_C(GW_EditableTextFrame_SetMaxLength, this, max_length);
    }

    inline bool EditableTextFrame::IsReadOnly()
    {
        return GWCA_CALL_C(GW_EditableTextFrame_IsReadOnly, this);
    }

    inline bool EditableTextFrame::SetReadOnly(bool readonly)
    {
        return GWCA_CALL_C(GW_EditableTextFrame_SetReadOnly, this, readonly);
    }

    inline uint32_t ProgressBar::GetValue()
    {
        return GWCA_CALL_C(GW_ProgressBar_GetValue, this);
    }

    inline bool ProgressBar::SetValue(uint32_t value)
    {
        return GWCA_CALL_C(GW_ProgressBar_SetValue, this, value);
    }

    inline bool ProgressBar::SetMax(uint32_t value)
    {
        return GWCA_CALL_C(GW_ProgressBar_SetMax, this, value);
    }

    inline bool ProgressBar::SetColorId(uint32_t color_id)
    {
        return GWCA_CALL_C(GW_ProgressBar_SetColorId, this, color_id);
    }

    inline bool ProgressBar::SetStyle(ProgressBar::ProgressBarStyle style)
    {
        return GWCA_CALL_C(GW_ProgressBar_SetStyle, this, style);
    }

    inline CheckboxFrame* CheckboxFrame::Create(uint32_t parent_frame_id, uint32_t flags, uint32_t child_offset_id, const wchar_t* text_label_enc_string, const wchar_t* frame_label)
    {
        return GWCA_CALL_C(GW_CheckboxFrame_Create, parent_frame_id, flags, child_offset_id, text_label_enc_string, frame_label);
    }

    inline bool CheckboxFrame::IsChecked()
    {
        return GWCA_CALL_C(GW_CheckboxFrame_IsChecked, this);
    }

    inline bool CheckboxFrame::SetChecked(bool checked)
    {
        return GWCA_CALL_C(GW_CheckboxFrame_SetChecked, this, checked);
    }

    inline uint32_t CheckboxFrame::GetValue()
    {
        return GWCA_CALL_C(GW_CheckboxFrame_GetValue, this);
    }

    inline bool CheckboxFrame::SetValue(uint32_t value)
    {
        return GWCA_CALL_C(GW_CheckboxFrame_SetValue, this, value);
    }

    inline uint32_t DropdownFrame::GetOptions(uint32_t* buffer, uint32_t capacity)
    {
        return GWCA_CALL_C(GW_DropdownFrame_GetOptions, this, buffer, capacity);
    }

    inline bool DropdownFrame::SelectOption(uint32_t value)
    {
        return GWCA_CALL_C(GW_DropdownFrame_SelectOption, this, value);
    }

    inline bool DropdownFrame::SelectIndex(uint32_t index)
    {
        return GWCA_CALL_C(GW_DropdownFrame_SelectIndex, this, index);
    }

    inline bool DropdownFrame::AddOption(const wchar_t* label_enc_string, uint32_t value)
    {
        return GWCA_CALL_C(GW_DropdownFrame_AddOption, this, label_enc_string, value);
    }

    inline bool DropdownFrame::GetCount(uint32_t* count)
    {
        return GWCA_CALL_C(GW_DropdownFrame_GetCount, this, count);
    }

    inline bool DropdownFrame::GetOptionValue(uint32_t index, uint32_t* value)
    {
        return GWCA_CALL_C(GW_DropdownFrame_GetOptionValue, this, index, value);
    }

    inline bool DropdownFrame::GetOptionIndex(uint32_t value, uint32_t* index)
    {
        return GWCA_CALL_C(GW_DropdownFrame_GetOptionIndex, this, value, index);
    }

    inline bool DropdownFrame::GetSelectedIndex(uint32_t* index)
    {
        return GWCA_CALL_C(GW_DropdownFrame_GetSelectedIndex, this, index);
    }

    inline bool DropdownFrame::HasValueMapping()
    {
        return GWCA_CALL_C(GW_DropdownFrame_HasValueMapping, this);
    }

    inline uint32_t DropdownFrame::GetValue()
    {
        return GWCA_CALL_C(GW_DropdownFrame_GetValue, this);
    }

    inline bool DropdownFrame::SetValue(uint32_t value)
    {
        return GWCA_CALL_C(GW_DropdownFrame_SetValue, this, value);
    }

    inline bool SliderFrame::GetValue(uint32_t* selected_value)
    {
        return GWCA_CALL_C(GW_SliderFrame_GetValue, this, selected_value);
    }

    inline bool SliderFrame::SetValue(uint32_t value)
    {
        return GWCA_CALL_C(GW_SliderFrame_SetValue, this, value);
    }

    inline TextLabelFrame* TextLabelFrame::Create(uint32_t parent_frame_id, uint32_t flags, uint32_t child_offset_id, const wchar_t* text_label_enc_string, const wchar_t* frame_label)
    {
        return GWCA_CALL_C(GW_TextLabelFrame_Create, parent_frame_id, flags, child_offset_id, text_label_enc_string, frame_label);
    }

    inline const wchar_t* TextLabelFrame::GetEncodedLabel()
    {
        return GWCA_CALL_C(GW_TextLabelFrame_GetEncodedLabel, this);
    }

    inline const wchar_t* TextLabelFrame::GetDecodedLabel()
    {
        return GWCA_CALL_C(GW_TextLabelFrame_GetDecodedLabel, this);
    }

    inline bool TextLabelFrame::SetLabel(const wchar_t* enc_string)
    {
        return GWCA_CALL_C(GW_TextLabelFrame_SetLabel, this, enc_string);
    }

    inline bool TextLabelFrame::SetFont(uint32_t gwca_arg0)
    {
        return GWCA_CALL_C(GW_TextLabelFrame_SetFont, this, gwca_arg0);
    }

    inline const wchar_t* MultiLineTextLabelFrame::GetEncodedLabel()
    {
        return GWCA_CALL_C(GW_MultiLineTextLabelFrame_GetEncodedLabel, this);
    }

    inline const wchar_t* MultiLineTextLabelFrame::GetDecodedLabel()
    {
        return GWCA_CALL_C(GW_MultiLineTextLabelFrame_GetDecodedLabel, this);
    }

    inline bool MultiLineTextLabelFrame::SetLabel(const wchar_t* enc_string)
    {
        return GWCA_CALL_C(GW_MultiLineTextLabelFrame_SetLabel, this, enc_string);
    }
}
#endif
