#pragma once

#include <GWCA/Packets/StoC.h>

#include <GWCA/Utilities/Hook.h>

#include <Color.h>
#include <ToolboxModule.h>

enum class DEFAULT_NAMETAG_COLOR : Color {
    NPC = 0xFFA0FF00,
    PLAYER_SELF = 0xFF40FF40,
    PLAYER_OTHER = 0xFF9BBEFF,
    PLAYER_IN_PARTY = 0xFF6060FF,
    GADGET = 0xFFFFFF00,
    ENEMY = 0xFFFF0000,
    ITEM = 0x0,
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
    static GameSettings& Instance() {
        static GameSettings instance;
        return instance;
    }
    const char* Name() const override { return "Game Settings"; }
    const char* Icon() const override { return ICON_FA_GAMEPAD; }
    static void PingItem(GW::Item* item, uint32_t parts = 3);
    static void PingItem(uint32_t item_id, uint32_t parts = 3);

    void Initialize() override;
    void Terminate() override;
    void LoadSettings(ToolboxIni* ini) override;
    void RegisterSettingsContent() override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingInternal() override;
    void DrawInventorySettings();
    void DrawPartySettings();

    static bool GetSettingBool(const char* setting);

    void Update(float delta) override;
    bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) override;

    // callback functions
    void OnPingWeaponSet(GW::HookStatus*, GW::UI::UIMessage, void*, void*) const;
    void OnAgentLoopingAnimation(GW::HookStatus*, GW::Packet::StoC::GenericValue*) const;
    void OnAgentMarker(GW::HookStatus* status, GW::Packet::StoC::GenericValue* pak) const;
    void OnAgentEffect(GW::HookStatus*, GW::Packet::StoC::GenericValue*) const;
    void OnFactionDonate(GW::HookStatus*, GW::UI::UIMessage, void*, void*) const;
    void OnPartyDefeated(GW::HookStatus*, GW::Packet::StoC::PartyDefeated*) const;
    void OnVanquishComplete(GW::HookStatus*, GW::Packet::StoC::VanquishComplete*) const;
    void OnDungeonReward(GW::HookStatus*, GW::Packet::StoC::DungeonReward*) const;
    void OnMapLoaded(GW::HookStatus*, GW::Packet::StoC::MapLoaded*) const;
    void OnCinematic(GW::HookStatus*, GW::Packet::StoC::CinematicPlay*) const;
    void OnMapTravel(GW::HookStatus*, GW::Packet::StoC::GameSrvTransfer*) const;
    void OnTradeStarted(GW::HookStatus*, GW::Packet::StoC::TradeStart*) const;
    void OnPlayerLeaveInstance(GW::HookStatus*, GW::Packet::StoC::PlayerLeaveInstance*) const;
    void OnPlayerJoinInstance(GW::HookStatus*, GW::Packet::StoC::PlayerJoinInstance*) const;
    void OnPartyInviteReceived(GW::HookStatus*, GW::Packet::StoC::PartyInviteReceived_Create*) const;
    void OnPartyPlayerJoined(GW::HookStatus*, GW::Packet::StoC::PartyPlayerAdd*);
    void OnLocalChatMessage(GW::HookStatus*, GW::Packet::StoC::MessageLocal*) const;
    void OnServerMessage(GW::HookStatus*, GW::Packet::StoC::MessageServer*) const;
    void OnScreenShake(GW::HookStatus*, void* packet) const;
    void OnWriteChat(GW::HookStatus* status, GW::UI::UIMessage msgid, void* wParam, void*) const;
    void OnAgentStartCast(GW::HookStatus* status, GW::UI::UIMessage, void*, void*) const;
    void OnOpenWiki(GW::HookStatus*, GW::UI::UIMessage, void*, void*) const;
    void OnCast(GW::HookStatus *, uint32_t agent_id, uint32_t slot, uint32_t target_id, uint32_t call_target) const;
    void OnAgentAdd(GW::HookStatus* status, GW::Packet::StoC::AgentAdd* packet) const;
    void OnUpdateAgentState(GW::HookStatus* status, GW::Packet::StoC::AgentState* packet) const;
    void OnUpdateSkillCount(GW::HookStatus*, void* packet);
    void OnAgentNameTag(GW::HookStatus* status, GW::UI::UIMessage msgid, void* wParam, void*) const;
    void OnDialogUIMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*) const;
    void CmdReinvite(const wchar_t* message, int argc, LPWSTR* argv) const;


private:
    void FactionEarnedCheckAndWarn();
    bool faction_checked = false;

    void MessageOnPartyChange();


    std::vector<std::wstring> previous_party_names;

    std::vector<uint32_t> available_dialog_ids;

    bool was_leading = true;
    bool hide_dungeon_chest_popup = false;
    bool skip_entering_name_for_faction_donate = false;
    bool stop_screen_shake = false;
    bool disable_camera_smoothing = false;

    bool improve_move_to_cast = false;
    bool check_message_on_party_change = true;

    bool is_prompting_hard_mode_mission = 0;

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
