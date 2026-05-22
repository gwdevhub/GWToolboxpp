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

    void Initialize() override;
    void Terminate() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

    void DrawHelp() override;

    static void CreateAlias(const wchar_t* from, const wchar_t* to);

    // Update. Will always be called every frame.
    void Update(float delta) override;

    static void CHAT_CMD_FUNC(CmdReapplyTitle);
    static void CHAT_CMD_FUNC(CmdCustomMarker);

    // Add a /setting <setting_name> [1|0|toggle] command to easily change a static setting in memory. NB: static_setting_ptr MUST be a pointer to a STATIC location in memory!
    static void RegisterSettingChatCommand(const wchar_t* setting_name, const bool* static_setting_ptr, const wchar_t* description = nullptr);

    static void RemoveSettingChatCommand(const wchar_t* setting_name);

private:
    static bool ReadTemplateFile(const std::wstring& path, char* buff, size_t buffSize);

    static bool IsLuxon();

    static void CHAT_CMD_FUNC(CmdEnterMission);
    static void CHAT_CMD_FUNC(CmdAge2);
    static void CHAT_CMD_FUNC(CmdDialog);
    static void CHAT_CMD_FUNC(CmdTB);
    static void CHAT_CMD_FUNC(CmdObserverReset);
    static void CHAT_CMD_FUNC(CmdChest);
    static void CHAT_CMD_FUNC(CmdAfk);
    static void CHAT_CMD_FUNC(CmdShow);
    static void CHAT_CMD_FUNC(CmdHide);
    static void CHAT_CMD_FUNC(CmdToggle);
    static void CHAT_CMD_FUNC(CmdCamera);
    static void CHAT_CMD_FUNC(CmdSCWiki);
    static void CHAT_CMD_FUNC(CmdLoad);
    static void CHAT_CMD_FUNC(CmdPingBuild);
    static void CHAT_CMD_FUNC(CmdResize);
    static void CHAT_CMD_FUNC(CmdPingEquipment);
    static void CHAT_CMD_FUNC(CmdPingQuest);
    static void CHAT_CMD_FUNC(CmdMorale);
    static void CHAT_CMD_FUNC(CmdVolume);
    static void CHAT_CMD_FUNC(CmdSetHardMode);
    static void CHAT_CMD_FUNC(CmdSetNormalMode);
    static void CHAT_CMD_FUNC(CmdMute);
    // Trigger hall of monuments info for current target or given player name
    static void CHAT_CMD_FUNC(CmdHom);
    static void CHAT_CMD_FUNC(CmdWithdraw);
    static void CHAT_CMD_FUNC(CmdDeposit);

    static void TargetNearest(const wchar_t* model_id_or_name, GW::AgentTargetFlags type);

    static std::vector<ToolboxUIElement*> CHAT_CMD_FUNC(MatchingWindows);
    static GW::UI::WindowID CHAT_CMD_FUNC(MatchingGWWindow);

    uint32_t default_title_id = std::to_underlying(GW::Constants::TitleID::Lightbringer);





    struct QuestPing {
        GW::Constants::QuestID quest_id = static_cast<GW::Constants::QuestID>(0);
        GuiUtils::EncString name;
        GuiUtils::EncString description;
        GuiUtils::EncString objectives;
        void Update();
        void Init();
    } quest_ping;

protected:
    float SettingsWeighting() override { return 1.2f; };
};
