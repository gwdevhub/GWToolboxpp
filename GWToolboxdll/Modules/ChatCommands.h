#pragma once

#include <GWCA/Constants/Constants.h>

#include <GWCA/Managers/UIMgr.h>

#include <Utils/GuiUtils.h>

#include <ToolboxModule.h>
#include <ToolboxUIElement.h>
#include <Modules/PluginModule.h>

#define CHAT_CMD_FUNC(fn) fn([[maybe_unused]] const wchar_t* message,[[maybe_unused]] int argc,[[maybe_unused]] const LPWSTR* argv)

class ChatCommands : public ToolboxModule {
    const float DEFAULT_CAM_SPEED = 1000.f;            // 600 units per sec
    const float ROTATION_SPEED = DirectX::XM_PI / 3.f; // 6 seconds for full rotation
public:
    static ChatCommands& Instance()
    {
        static ChatCommands instance;
        return instance;
    }

    struct PendingTransmo {
        PendingTransmo(const DWORD _npcid = 0, const DWORD _scale = 0x64000000, const DWORD _npcmfid = 0, const DWORD _npcmfd = 0, const DWORD _flags = 0)
            : npc_id(_npcid)
            , scale(_scale)
            , npc_model_file_id(_npcmfid)
            , npc_model_file_data(_npcmfd)
            , flags(_flags) { };
        DWORD npc_id = 0;
        DWORD scale = 0x64000000;
        DWORD npc_model_file_id = 0;
        DWORD npc_model_file_data = 0;
        DWORD flags = 0;
    };

    enum TargetType : uint32_t {
        Gadget = 0,
        Player,
        Npc,
        Item,
        Living,
        Enemy,
        Ally
    };

    [[nodiscard]] const char* Name() const override { return "Chat Commands"; }
    [[nodiscard]] const char* SettingsName() const override { return "Chat Settings"; }

    void Initialize() override;
    void Terminate() override;
    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

    void DrawHelp() override;

    static void CreateAlias(const wchar_t* from, const wchar_t* to);

    bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) override;

    // Update. Will always be called every frame.
    void Update(float delta) override;

    static void CHAT_CMD_FUNC(CmdReapplyTitle);

private:
    static bool ReadTemplateFile(const std::wstring& path, char* buff, size_t buffSize);

    static bool IsLuxon();

    static void CHAT_CMD_FUNC(CmdEnterMission);
    static void CHAT_CMD_FUNC(CmdAge2);
    static void CHAT_CMD_FUNC(CmdDialog);
    static void CHAT_CMD_FUNC(CmdTB);
    static void CHAT_CMD_FUNC(CmdDamage);
    static void CHAT_CMD_FUNC(CmdObserverReset);
    static void CHAT_CMD_FUNC(CmdChest);
    static void CHAT_CMD_FUNC(CmdAfk);
    static void CHAT_CMD_FUNC(CmdTarget);
    static void CHAT_CMD_FUNC(CmdUseSkill);
    static void CHAT_CMD_FUNC(CmdShow);
    static void CHAT_CMD_FUNC(CmdHide);
    static void CHAT_CMD_FUNC(CmdToggle);
    static void CHAT_CMD_FUNC(CmdZoom);
    static void CHAT_CMD_FUNC(CmdCamera);
    static void CHAT_CMD_FUNC(CmdSCWiki);
    static void CHAT_CMD_FUNC(CmdLoad);
    static void CHAT_CMD_FUNC(CmdPing);
    static void CHAT_CMD_FUNC(CmdTransmo);
    static void CHAT_CMD_FUNC(CmdResize);
    static void CHAT_CMD_FUNC(CmdPingEquipment);
    static void CHAT_CMD_FUNC(CmdTransmoTarget);
    static void CHAT_CMD_FUNC(CmdTransmoParty);
    static void CHAT_CMD_FUNC(CmdTransmoAgent);
    static void CHAT_CMD_FUNC(CmdHeroBehaviour);
    static void CHAT_CMD_FUNC(CmdPingQuest);
    static void CHAT_CMD_FUNC(CmdMorale);
    static void CHAT_CMD_FUNC(CmdVolume);
    static void CHAT_CMD_FUNC(CmdSetHardMode);
    static void CHAT_CMD_FUNC(CmdSetNormalMode);
    static void CHAT_CMD_FUNC(CmdAnimation);
    static void CHAT_CMD_FUNC(CmdMute);
    // Trigger hall of monuments info for current target or given player name
    static void CHAT_CMD_FUNC(CmdHom);
    static void CHAT_CMD_FUNC(CmdWithdraw);
    static void CHAT_CMD_FUNC(CmdDeposit);

    static void TransmoAgent(DWORD agent_id, PendingTransmo& transmo);
    static bool GetNPCInfoByName(const std::string& name, PendingTransmo& transmo);
    static bool GetNPCInfoByName(const std::wstring& name, PendingTransmo& transmo);
    static bool ParseScale(int scale, PendingTransmo& transmo);
    static bool GetTargetTransmoInfo(PendingTransmo& transmo);
    static void TargetNearest(const wchar_t* model_id_or_name, TargetType type);
    static const wchar_t* GetRemainingArgsWstr(const wchar_t* message, int argc_start);

    static std::vector<ToolboxUIElement*> CHAT_CMD_FUNC(MatchingWindows);
    static GW::UI::WindowID CHAT_CMD_FUNC(MatchingGWWindow);

    float cam_speed = DEFAULT_CAM_SPEED;
    bool forward_fix_z = true;
    uint32_t default_title_id = static_cast<uint32_t>(GW::Constants::TitleID::Lightbringer);

    struct SearchAgent {
        clock_t started = 0;
        std::vector<std::pair<uint32_t, GuiUtils::EncString*>> npc_names;
        std::wstring search;
        void Init(const wchar_t* search, TargetType type = Npc);
        void Update();

        void Terminate()
        {
            for (const auto& name : npc_names | std::views::values) {
                delete name;
            }
            npc_names.clear();
        }
    } npc_to_find;

    struct SkillToUse {
        uint32_t slot = 0; // 1-8 range
        float skill_usage_delay = 0.f;
        clock_t skill_timer = clock();
        void Update();
    } skill_to_use;

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
