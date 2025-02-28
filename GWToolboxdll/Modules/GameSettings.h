#pragma once

#include <GWCA/Packets/StoC.h>

#include <GWCA/Utilities/Hook.h>

#include <Modules/ItemDescriptionHandler.h>

#include <Color.h>
#include <ToolboxModule.h>

enum class DEFAULT_NAMETAG_COLOR : Color {
    NPC             = 0xFFA0FF00,
    PLAYER_SELF     = 0xFF40FF40,
    PLAYER_OTHER    = 0xFF9BBEFF,
    PLAYER_IN_PARTY = 0xFF6060FF,
    GADGET          = 0xFFFFFF00,
    ENEMY           = 0xFFFF0000,
    ITEM            = 0x0,
};

namespace GW {
    struct Item;
    struct Friend;
    enum class FriendStatus : uint32_t;

    namespace Constants {
        enum class SkillID : uint32_t;
    }

    namespace UI {
        enum class UIMessage : uint32_t;
    }
}

class GameSettings : public ToolboxModule {
    GameSettings() = default;
    ~GameSettings() override = default;

public:
    static GameSettings& Instance()
    {
        static GameSettings instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Game Settings"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_GAMEPAD; }
    static void PingItem(GW::Item* item, uint32_t parts = 3);
    static void PingItem(uint32_t item_id, uint32_t parts = 3);

    void Initialize() override;
    void Terminate() override;
    void LoadSettings(ToolboxIni* ini) override;
    void RegisterSettingsContent() override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;
    static void DrawInventorySettings();
    static void DrawPartySettings();

    static bool GetSettingBool(const char* setting);

    void Update(float delta) override;
    bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) override;

    // callback functions
    static void OnPingWeaponSet(GW::HookStatus*, GW::UI::UIMessage, void*, void*);
    static void OnAgentLoopingAnimation(GW::HookStatus*, const GW::Packet::StoC::GenericValue*);
    static void OnAgentMarker(GW::HookStatus* status, GW::Packet::StoC::GenericValue* pak);
    static void OnAgentEffect(GW::HookStatus*, const GW::Packet::StoC::GenericValue*);
    static void OnPartyDefeated(const GW::HookStatus*, GW::Packet::StoC::PartyDefeated*);
    void OnDungeonReward(GW::HookStatus*, GW::Packet::StoC::DungeonReward*) const;
    static void OnMapLoaded(GW::HookStatus*, GW::Packet::StoC::MapLoaded*);
    static void OnCinematic(const GW::HookStatus*, const GW::Packet::StoC::CinematicPlay*);
    static void OnMapTravel(const GW::HookStatus*, const GW::Packet::StoC::GameSrvTransfer*);
    static void OnPlayerLeaveInstance(GW::HookStatus*, const GW::Packet::StoC::PlayerLeaveInstance*);
    static void OnPlayerJoinInstance(GW::HookStatus*, GW::Packet::StoC::PlayerJoinInstance*);
    static void OnPartyInviteReceived(const GW::HookStatus*, const GW::Packet::StoC::PartyInviteReceived_Create*);
    void OnPartyPlayerJoined(const GW::HookStatus*, const GW::Packet::StoC::PartyPlayerAdd*);
    static void OnServerMessage(const GW::HookStatus*, GW::Packet::StoC::MessageServer*);
    void OnScreenShake(GW::HookStatus*, const void* packet) const;
    static void OnWriteChat(GW::HookStatus* status, GW::UI::UIMessage msgid, void* wParam, void*);
    static void OnAgentStartCast(GW::HookStatus* status, GW::UI::UIMessage, void*, void*);
    static void OnOpenWiki(GW::HookStatus*, GW::UI::UIMessage, void*, void*);
    static void OnAgentAdd(GW::HookStatus* status, const GW::Packet::StoC::AgentAdd* packet);
    static void OnUpdateAgentState(GW::HookStatus* status, GW::Packet::StoC::AgentState* packet);
    static void OnUpdateSkillCount(GW::HookStatus*, void* packet);
    static void OnAgentNameTag(GW::HookStatus* status, GW::UI::UIMessage msgid, void* wParam, void*);
private:
    void FactionEarnedCheckAndWarn();
    bool faction_checked = false;

    void MessageOnPartyChange();


    std::vector<std::wstring> previous_party_names{};

    std::vector<uint32_t> available_dialog_ids{};



    GW::HookEntry VanquishComplete_Entry;
    GW::HookEntry ItemClickCallback_Entry;
    GW::HookEntry FriendStatusCallback_Entry;
    GW::HookEntry PartyDefeated_Entry;
    GW::HookEntry OnAfterAgentAdd_Entry;
    GW::HookEntry AgentState_Entry;
    GW::HookEntry AgentRemove_Entry;
    GW::HookEntry AgentAdd_Entry;
    GW::HookEntry TradeStart_Entry;
    GW::HookEntry PartyPlayerAdd_Entry;
    GW::HookEntry PartyPlayerReady_Entry;
    GW::HookEntry PartyPlayerRemove_Entry;
    GW::HookEntry GameSrvTransfer_Entry;
    GW::HookEntry CinematicPlay_Entry;
    GW::HookEntry PlayerJoinInstance_Entry;
    GW::HookEntry PlayerLeaveInstance_Entry;
    GW::HookEntry OnDialog_Entry;
    GW::HookEntry OnChangeTarget_Entry;
    GW::HookEntry OnWriteChat_Entry;
    GW::HookEntry OnAgentStartCast_Entry;
    GW::HookEntry OnOpenWikiUrl_Entry;
    GW::HookEntry OnScreenShake_Entry;
    GW::HookEntry OnCast_Entry;
    GW::HookEntry OnPartyTargetChange_Entry;
    GW::HookEntry OnAgentNameTag_Entry;
    GW::HookEntry OnEnterMission_Entry;
    GW::HookEntry OnPreSendDialog_Entry;
    GW::HookEntry OnPostSendDialog_Entry;
};
