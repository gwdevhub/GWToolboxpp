#pragma once

#include <GWCA/Managers/UIMgr.h>

#include <ToolboxModule.h>

class GuildWarsUIModule : public ToolboxModule {
    GuildWarsUIModule() = default;
    ~GuildWarsUIModule() = default;

public:
    static GuildWarsUIModule& Instance() {
        static GuildWarsUIModule instance;
        return instance;
    }

    const char* Name() const override { return "Guild Wars UI"; }
    bool HasSettings() override { return false; }

    static uint32_t CreateButton(uint32_t parent_frame_id, const wchar_t* label, uint32_t flags = 0x20100, GW::UI::UIInteractionCallback event_callback = nullptr);
    static uint32_t CreateCheckbox(uint32_t parent_frame_id, const wchar_t* label, uint32_t flags = 0x8000, GW::UI::UIInteractionCallback event_callback = nullptr);

    static uint32_t CreateDropdown(uint32_t parent_frame_id, const wchar_t* label, uint32_t flags = 0x1301, GW::UI::UIInteractionCallback event_callback = nullptr);
    static void AddDropdownListOption(uint32_t frame_id, const wchar_t* label);
    static void SetDropdownListOption(uint32_t frame_id, uint32_t option_idx);
    static void SetCheckboxState(uint32_t frame_id, bool checked);

    static uint32_t CreateTextLabel(uint32_t parent_frame_id,const wchar_t* label, uint32_t flags = 0x28000, GW::UI::UIInteractionCallback event_callback = nullptr);

    void Initialize() override;
    void Terminate() override;
};
