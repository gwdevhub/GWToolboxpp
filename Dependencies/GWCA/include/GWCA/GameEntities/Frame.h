#pragma once

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
        GWCA_API bool SetFrameMargins(GW::UI::Frame* frame, uint32_t flags, float size[4], float input_mask[4], uint32_t type);
    }

    struct ButtonFrame : UI::Frame {
		GWCA_API static ButtonFrame* Create(uint32_t parent_frame_id, uint32_t flags = 0x300, uint32_t child_offset_id = 0xff, const wchar_t* button_label = nullptr, const wchar_t* frame_label = nullptr);
        GWCA_API bool GetLabel(const wchar_t** enc_string);
        GWCA_API bool SetLabel(const wchar_t* enc_string);
        GWCA_API bool Click();
        GWCA_API bool MouseAction(UI::UIPacket::ActionState action);
        GWCA_API bool DoubleClick();
    };

    struct TabsFrame : UI::Frame {
        GWCA_API GW::UI::Frame* AddTab(const wchar_t* tab_name_enc, uint32_t flags, uint32_t child_offset_id, GW::UI::UIInteractionCallback callback, void* wparam = 0);
        GWCA_API bool DisableTab(uint32_t tab_id);
        GWCA_API bool EnableTab(uint32_t tab_id);
        GWCA_API bool RemoveTab(uint32_t tab_id);
        GWCA_API bool GetCurrentTabIndex(uint32_t* tab_id);
        GWCA_API bool GetTabFrameId(uint32_t tab_id, uint32_t* frame_id);
        GWCA_API bool GetIsTabEnabled(uint32_t tab_id, uint32_t* is_enabled);
		GWCA_API GW::UI::Frame* GetTabByLabel(const wchar_t* label);
        GWCA_API GW::UI::Frame* GetCurrentTab();
		GWCA_API bool ChooseTab(GW::UI::Frame* tab_frame);
        GWCA_API bool ChooseTab(uint32_t tab_index);
        GWCA_API GW::ButtonFrame* GetTabButton(GW::UI::Frame* tab_frame);

    };
    struct ScrollableFrame : UI::Frame {
        // Sorting handler is actually a boolean function - return "1" if frame_id_1 needs to be bubbled higher than frame_id_2
        typedef int(__cdecl* SortHandler_pt)(uint32_t frame_id_1, uint32_t frame_id_2);
        // Scrollable frame always has an "inner" page that handles the content. Only really need to mess with this if you're doing something odd.
        struct ScrollablePageContext {
            uint32_t flags;
            GW::UI::UIInteractionCallback page_callback;
            uint32_t wparam;
        };

        // Scrollable Frames always need a valid context, but pass nullptr to use the default one used for most of the scrollable frames in the game
        GWCA_API static ScrollableFrame* Create(uint32_t parent_frame_id, uint32_t flags = 0x20000, uint32_t child_offset_id = 0xff, ScrollablePageContext* context = nullptr, const wchar_t* frame_label = nullptr);

        GWCA_API bool SetSortHandler(SortHandler_pt sortHandler);
        GWCA_API SortHandler_pt GetSortHandler();
        GWCA_API bool ClearItems();
        GWCA_API bool RemoveItem(uint32_t child_offset_id);
        GWCA_API bool AddItem(uint32_t flags, uint32_t child_offset_id, GW::UI::UIInteractionCallback callback);
        GWCA_API uint32_t GetItemFrameId(uint32_t child_offset_id);
        // Returns false if the request failed, or nothing is selected
        GWCA_API bool GetSelectedValue(uint32_t* selected_value);
        GWCA_API uint32_t GetFirstChildFrameId(uint32_t* _offset_of_child_out = nullptr);
        GWCA_API uint32_t GetNextChildFrameId(uint32_t _frame_id, uint32_t* _offset_of_child_out = nullptr);
        GWCA_API uint32_t GetLastChildFrameId(uint32_t* _offset_of_child_out = nullptr);
        GWCA_API uint32_t GetPrevChildFrameId(uint32_t _frame_id, uint32_t* _offset_of_child_out = nullptr);
        GWCA_API bool GetItemRect(uint32_t child_offset_id, float rect[4]);
        // This is actually the child_frame_id of the last child in the list - things that use sorting, or the child id to identify the frame, will not represent the size.
        GWCA_API bool GetCount(uint32_t* size);
        GWCA_API uint32_t GetItems(uint32_t* child_frame_id_buffer = nullptr, uint32_t buffer_len = 0);
        
        GWCA_API GW::UI::Frame* GetPage();
        // Scrollable frame always has an "inner" page that handles the content. Only really need to mess with this if you're doing something odd.
        GWCA_API GW::UI::Frame* SetPage(ScrollablePageContext*);
    };


    struct FrameWithValue {
        FrameWithValue() = default;
        virtual ~FrameWithValue() = default;
        FrameWithValue(const FrameWithValue&) = default;
        FrameWithValue(FrameWithValue&&) = default;
        FrameWithValue& operator=(const FrameWithValue&) = default;
        FrameWithValue& operator=(FrameWithValue&&) = default;

        virtual uint32_t GetValue();
        virtual bool SetValue(uint32_t value);
    };

    struct EditableTextFrame : UI::Frame {
        GWCA_API const wchar_t* GetValue();
        GWCA_API bool SetValue(const wchar_t* value);
        GWCA_API bool SetMaxLength(uint32_t max_length);
        GWCA_API bool IsReadOnly();
        GWCA_API bool SetReadOnly(bool readonly);
    };

    struct ProgressBar final : ButtonFrame, FrameWithValue {
        GWCA_API uint32_t GetValue() override;
        GWCA_API bool SetValue(uint32_t value) override;
        GWCA_API bool SetMax(uint32_t value);
        GWCA_API bool SetColorId(uint32_t color_id);
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
        GWCA_API bool SetStyle(ProgressBarStyle style);
    };

    struct CheckboxFrame final : ButtonFrame, FrameWithValue {
        GWCA_API static CheckboxFrame* Create(uint32_t parent_frame_id, uint32_t flags = 0x8000, uint32_t child_offset_id = 0xff, const wchar_t* text_label_enc_string = nullptr, const wchar_t* frame_label = nullptr);
        GWCA_API bool IsChecked();
        GWCA_API bool SetChecked(bool checked);

        GWCA_API uint32_t GetValue() override;
        GWCA_API bool SetValue(uint32_t value) override;
    };
    struct DropdownFrame final : UI::Frame, FrameWithValue {
        GWCA_API  std::vector<uint32_t> GetOptions();
        GWCA_API bool SelectOption(uint32_t value);
        GWCA_API bool SelectIndex(uint32_t index);
        GWCA_API bool AddOption(const wchar_t* label_enc_string, uint32_t value);
        GWCA_API bool GetCount(uint32_t* count);
        GWCA_API bool GetOptionValue(uint32_t index, uint32_t* value);
        GWCA_API bool GetOptionIndex(uint32_t value, uint32_t* index);
		GWCA_API bool GetSelectedIndex(uint32_t* index);
        // Some dropdowns are only used by reference to their index. Others actually have values assigned to their indeces.
		GWCA_API bool HasValueMapping();

        GWCA_API uint32_t GetValue() override;
        GWCA_API bool SetValue(uint32_t value) override;
    };
    struct SliderFrame final : UI::Frame, FrameWithValue {
        GWCA_API bool GetValue(uint32_t* selected_value);
        GWCA_API bool SetValue(uint32_t value) override;

        GWCA_API uint32_t GetValue() override;
    };

    struct TextLabelFrame : UI::Frame {
        GWCA_API static TextLabelFrame* Create(uint32_t parent_frame_id, uint32_t flags = 0x300, uint32_t child_offset_id = 0xff, const wchar_t* text_label_enc_string = nullptr, const wchar_t* frame_label = nullptr);
        GWCA_API const wchar_t* GetEncodedLabel();
        GWCA_API const wchar_t* GetDecodedLabel();
        GWCA_API bool SetLabel(const wchar_t* enc_string);
    };
    struct MultiLineTextLabelFrame final : TextLabelFrame {
        GWCA_API const wchar_t* GetEncodedLabel();
        GWCA_API const wchar_t* GetDecodedLabel();
        GWCA_API bool SetLabel(const wchar_t* enc_string);
    };

}
