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
        Unknown,        // 未知
        NotInParty,     // 不在队伍中
        Disconnected,   // 已断开
        Connected,      // 已连接
        Resigned        // 已退出
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

    ResignLogModule::Settings settings;

    bool IsMapReady() {
        return GW::UI::GetFrameByLabel(L"Log");
    }

    const char* GetStatusStr(const Status _status)
    {
        switch (_status) {
        case Status::Unknown:
            return "未知";
        case Status::Disconnected:
            return "已断开";
        case Status::NotInParty:
            return "不在队伍中";
        case Status::Connected:
            return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable ? "已连接（未退出）" : "已连接";
        case Status::Resigned:
            return "已退出";
        default:
            return "";
        }
    }

    Status GetResignStatus(const uint32_t player_number)
    {
        const auto found = party_member_statuses.find(player_number);
        return found != party_member_statuses.end() ? found->second.status : Status::Unknown;
    }

    void UpdatePlayerStates() {
        const auto players = GW::PartyMgr::GetPartyPlayers();
        if (!players) return;
        for (auto& p : *players) {
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
        if (!settings.show_last_to_resign_message) {
            return;
        }
        const auto my_player_number = GW::PlayerMgr::GetPlayerNumber();
        if (GetResignStatus(my_player_number) == Status::Resigned) {
            return; // 我已退出
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
        if (not_resigned <= 1) { // 未退出的玩家之一是我们
            Log::Warning("您是唯一尚未退出的人。请在聊天中输入 /resign 以退出。");
        }
    }

    void OnChatMessage(const wchar_t* message) {
        // 0x107 是“起始字符串”标记
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
        bool pending_only = false;
        if (argc > 1) {
            const std::wstring arg1 = TextUtils::ToLower(argv[1]);
            if (arg1 == L"pending" || arg1 == L"notresigned") {
                pending_only = true;
            }
        }
        std::wstring buffer;
        for (auto& p : *players) {
            if (pending_only && GetResignStatus(p.login_number) == Status::Resigned)
                continue;
            if (ResignLogModule::PrintResignStatus(p.login_number, buffer))
                send_queue.push(buffer);
        }
        CheckAndWarnIfNotResigned();
    }

    void DrawGameSettings(const std::string&, const bool is_showing)
    {
        if (!is_showing) return;
        ImGui::Checkbox("当您是最后一个退出者时在聊天中显示消息", &settings.show_last_to_resign_message);
    }
}

void ResignLogModule::RegisterSettingsContent() {
    ToolboxModule::RegisterSettingsContent("游戏设置", ICON_FA_GAMEPAD, DrawGameSettings,0.9f);
}

bool ResignLogModule::PrintResignStatus(const uint32_t player_number, std::wstring& out, bool include_timestamp)
{
    const auto idx = GW::PartyMgr::GetPlayerPartyIndex(player_number);
    const auto player_name = idx ? GW::PlayerMgr::GetPlayerName(player_number) : nullptr;
    if (!player_name) {
        return false;
    }
    const auto found = party_member_statuses.find(player_number);
    const auto status = found != party_member_statuses.end() ? found->second : PartyMemberStatus{};
    out = std::format(L"{}. {} - {}", idx, player_name, TextUtils::StringToWString(GetStatusStr(status.status)));
    if (include_timestamp && status.timestamp) {
        out += std::format(L" [{}:{:02}:{:02}:{:03}]", status.timestamp / (60 * 60 * 1000), status.timestamp / (60 * 1000) % 60, status.timestamp / 1000 % 60, status.timestamp % 1000);
    }

    return true;
}

void ResignLogModule::Initialize() {
    ToolboxModule::Initialize();
    SettingsRegistry::Register(this, settings);

    GW::UI::UIMessage ui_messages[] = {
        GW::UI::UIMessage::kMapLoaded,
        GW::UI::UIMessage::kWriteToChatLog,
        GW::UI::UIMessage::kPartyAddPlayer,
        GW::UI::UIMessage::kPartyRemovePlayer
    };

    for (auto message_id : ui_messages) {
        RegisterUIMessageCallback(&ResignLog_HookEntry, message_id, OnUIMessage, 0x8000);
    }

    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"resignlog", CmdResignLog);

    UpdatePlayerStates();

}

void ResignLogModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) {
    ToolboxModule::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
}

void ResignLogModule::SaveSettings(SettingsDoc& doc) {
    ToolboxModule::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
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