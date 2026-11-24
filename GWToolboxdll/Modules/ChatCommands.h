#pragma once

#include <GWCA/Constants/Constants.h>

#include <GWCA/Managers/UIMgr.h>

#include <Utils/GuiUtils.h>

#include <ToolboxModule.h>
#include <ToolboxUIElement.h>
#include <Modules/PluginModule.h>

namespace GW {
    enum AgentTargetFlags : uint32_t;
}

class ChatCommands : public ToolboxModule {

public:
    static ChatCommands& Instance()
    {
        static ChatCommands instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Chat Commands"; }
    [[nodiscard]] const char* SettingsName() const override { return "Chat Settings"; }

    // MSVC can't reflect member names of internal-linkage types, so settings structs are nested in the class
    struct Settings {
        uint32_t default_title_id = std::to_underlying(GW::Constants::TitleID::Lightbringer);
    };

    struct CmdAliasSetting {
        std::string alias;
        std::string command;
    };

    void Initialize() override;
    void Terminate() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* ini) override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;

    void DrawHelp() override;

    static void CreateAlias(const wchar_t* from, const wchar_t* to);

    // Update. Will always be called every frame.
    void Update(float delta) override;

    static void CHAT_CMD_FUNC(CmdReapplyTitle);
    static void CHAT_CMD_FUNC(CmdCustomMarker);

protected:
    float SettingsWeighting() override { return 1.2f; };
};
