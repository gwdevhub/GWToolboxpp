#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Party.h>

#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Modules/ResignLogModule.h>

#include <Utils/ToolboxUtils.h>
#include <Utils/GuiUtils.h>
#include <Defines.h>
#include <Timer.h>
#include <Utils/TextUtils.h>

namespace {

    enum class Status {
        Unknown,
        NotInParty,
        Disconnected,
        Connected,
        Resigned
    };
    struct PartyMemberStatus {
        Status status = Status::Unknown;
        uint32_t timestamp = 0;
    };

    std::unordered_map<uint32_t, PartyMemberStatus> party_member_statuses;
    std::queue<std::wstring> send_queue;
    clock_t last_sent = 0;

    GW::HookEntry ResignLog_HookEntry;
    GW::HookEntry ChatCmd_HookEntry;

    bool show_last_to_resign_message = false;

    bool IsMapReady() {
        return GW::UI::GetFrameByLabel(L"Log");
    }

    const char* GetStatusStr(const Status _status)
    {
        switch (_status) {
        case Status::Unknown:
            return "Unknown";
        case Status::Disconnected:
            return "Disconnected";
        case Status::NotInParty:
            return "Not in Party";
        case Status::Connected:
            return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable ? "Connected (not resigned)" : "Connected";
        case Status::Resigned:
            return "Resigned";
        default:
            return "";
        }
    }

    Status GetResignStatus(const uint32_t player_number)
    {
        return party_member_statuses.contains(player_number) ? party_member_statuses[player_number].status : Status::Unknown;
    }

    void UpdatePlayerStates() {
        const auto players = GW::PartyMgr::GetPartyPlayers();
        if (!players) return;
        for (auto& p : *players) {
            if (!party_member_statuses.contains(p.login_number)) {
                party_member_statuses[p.login_number] = {
                    Status::Unknown,0
                };
            }
            auto& st = party_member_statuses[p.login_number];
            if (!p.connected()) {
                if (st.status != Status::NotInParty) {
                    st = { Status::NotInParty, GW::Map::GetInstanceTime() };
                }
            }
            else {
                if (st.status == Status::NotInParty || st.status == Status::Unknown) {
                    st = { Status::Connected, GW::Map::GetInstanceTime() };
                }
            }
        }
    }

    void CheckAndWarnIfNotResigned()
    {
        if (!show_last_to_resign_message) {
            return;
        }
        const auto my_player_number = GW::PlayerMgr::GetPlayerNumber();
        if (GetResignStatus(my_player_number) == Status::Resigned) {
            return; // I've resigned
        }

        const auto players = GW::PartyMgr::GetPartyPlayers();
        if (!(players && players->size() > 1))
            return;

        uint32_t not_resigned = 0;
        for (const auto& player : *players) {
            if (!player.connected())
                continue;
            if (GetResignStatus(player.login_number) != Status::Resigned)
                not_resigned++;
        }
        if (not_resigned <= 1) { // one of the players who hasn't resigned is us
            Log::Warning("You're the only player left to resign. Type /resign in chat to resign.");
        }
    }

    void OnChatMessage(const wchar_t* message) {
        // 0x107 is the "start string" marker
        if (wmemcmp(message, L"\x7BFF\xC9C4\xAEAA\x1B9B\x107", 5) != 0)
            return;
        auto start = wcschr(message, 0x107);
        if (!start) return;
        start += 1;
        const auto end = wcschr(start, 0x1);
        if (!(end && start != end))
            return;

        std::wstring resigned_player_name(start, end - start);
        const auto players = GW::PartyMgr::GetPartyPlayers();
        if (!players) return;
        for (const auto& player : *players) {
            const auto player_name = TextUtils::SanitizePlayerName(GW::PlayerMgr::GetPlayerName(player.login_number));
            if (resigned_player_name != player_name)
                continue;
            party_member_statuses[player.login_number] = {
                Status::Resigned,
                GW::Map::GetInstanceTime()
            };
            CheckAndWarnIfNotResigned();
            return;
        }
    }

    void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* wParam, void*) {
        switch (message_id) {
        case GW::UI::UIMessage::kWriteToChatLog:
            OnChatMessage(((GW::UI::UIPacket::kWriteToChatLog*)wParam)->message);
            break;
        case GW::UI::UIMessage::kMapLoaded:
            for (auto& p : party_member_statuses) {
                if (p.second.status == Status::Resigned)
                    p.second.status = Status::Unknown;
            }
            while (!send_queue.empty()) send_queue.pop();
            UpdatePlayerStates();
            break;
        case GW::UI::UIMessage::kPartyAddPlayer:
        case GW::UI::UIMessage::kPartyRemovePlayer:
            UpdatePlayerStates();
            break;
        }
    }

    void CHAT_CMD_FUNC(CmdResignLog)
    {
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
            return;
        }
        const auto players = GW::PartyMgr::GetPartyPlayers();
        if (!players)
            return;
        std::wstring buffer;
        for (auto& p : *players) {
            if (ResignLogModule::PrintResignStatus(p.login_number, buffer))
                send_queue.push(buffer);
        }
        CheckAndWarnIfNotResigned();
    }

    void DrawGameSettings(const std::string&, const bool is_showing)
    {
        if (!is_showing) return;
        ImGui::Checkbox("Show message in chat when you're the last player to resign", &show_last_to_resign_message);
    }
}

void ResignLogModule::RegisterSettingsContent() {
    ToolboxModule::RegisterSettingsContent("Game Settings", ICON_FA_GAMEPAD, DrawGameSettings,0.9f);
}

bool ResignLogModule::PrintResignStatus(const uint32_t player_number, std::wstring& out, bool include_timestamp)
{
    const auto idx = GW::PartyMgr::GetPlayerPartyIndex(player_number);
    const auto player_name = idx ? GW::PlayerMgr::GetPlayerName(player_number) : nullptr;
    if (!player_name) {
        return false;
    }
    const auto status = party_member_statuses.contains(player_number) ? party_member_statuses[player_number] : PartyMemberStatus({
        Status::Unknown, 0
    });
    out = std::format(L"{}. {} - {}", idx, player_name, TextUtils::StringToWString(GetStatusStr(status.status)));
    if (include_timestamp && status.timestamp) {
        out += std::format(L" [{}:{:02}:{:02}:{:03}]", status.timestamp / (60 * 60 * 1000), status.timestamp / (60 * 1000) % 60, status.timestamp / 1000 % 60, status.timestamp % 1000);
    }

    return true;
}

void ResignLogModule::Initialize() {
    ToolboxModule::Initialize();

    GW::UI::UIMessage ui_messages[] = {
        GW::UI::UIMessage::kMapLoaded,
        GW::UI::UIMessage::kWriteToChatLog,
        GW::UI::UIMessage::kPartyAddPlayer,
        GW::UI::UIMessage::kPartyRemovePlayer
    };

    for (auto message_id : ui_messages) {
        GW::UI::RegisterUIMessageCallback(&ResignLog_HookEntry, message_id, OnUIMessage, 0x8000);
    }

    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"resignlog", CmdResignLog);

    UpdatePlayerStates();

}
void ResignLogModule::SignalTerminate() {
    GW::UI::RemoveUIMessageCallback(&ResignLog_HookEntry);
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
}

void ResignLogModule::Update(float) {
    if (!send_queue.empty() && TIMER_DIFF(last_sent) > 600) {
        last_sent = TIMER_INIT();
        if (IsMapReady()) {
            GW::Chat::SendChat('#', send_queue.front().c_str());
            send_queue.pop();
        }
    }
}

void ResignLogModule::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);
    LOAD_BOOL(show_last_to_resign_message);
}
void ResignLogModule::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    SAVE_BOOL(show_last_to_resign_message);
}
