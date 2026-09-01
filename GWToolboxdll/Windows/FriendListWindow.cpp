#include "stdafx.h"

#include <GWCA/Packets/StoC.h>

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Friendslist.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Party.h>

#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/FriendListMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/TradeMgr.h>

#include <Logger.h>
#include <Timer.h>
#include <Color.h>

#include <Modules/Resources.h>
#include <Windows/FriendListWindow.h>

#include <Utils/ToolboxUtils.h>
#include <Windows/TravelWindow.h>


/* 超出范围的命名空间查找 */
using namespace ToolboxUtils;

namespace {
    std::thread settings_thread;

    GW::HookEntry ChatCmd_HookEntry;

    FriendListWindow& Instance()
    {
        return FriendListWindow::Instance();
    }

    const ImColor ProfColors[11] = {0xFFFFFFFF, 0xFFEEAA33, 0xFF55AA00,
                                    0xFF4444BB, 0xFF00AA55, 0xFF8800AA,
                                    0xFFBB3333, 0xFFAA0088, 0xFF00AAAA,
                                    0xFF996600, 0xFF7777CC};
    const ImColor StatusColors[5] = {
        IM_COL32(0x99, 0x99, 0x99, 255), // 离线
        IM_COL32(0x0, 0xc8, 0x0, 255),   // 在线
        IM_COL32(0xc8, 0x0, 0x0, 255),   // 忙碌
        IM_COL32(0xc8, 0xc8, 0x0, 255),  // 离开
        IM_COL32(0x99, 0x99, 0x99, 255)  // 离线
    };
    const char* statuses[] = {"离线", "在线", "忙碌", "离开", "已断开"};

    const char* GetStatusText(const GW::FriendStatus status)
    {
        switch (status) {
            case GW::FriendStatus::Offline:
                return "离线";
            case GW::FriendStatus::Online:
                return "在线";
            case GW::FriendStatus::DND:
                return "请勿打扰";
            case GW::FriendStatus::Away:
                return "离开";
        }
        return "未知";
    }

    ToolboxIni inifile{};
    const wchar_t* ini_filename = L"friends.ini"; // 旧版只读回退
    const wchar_t* json_filename = L"friends.json";
    bool loading = false;     // 从磁盘加载中？
    bool polling = false;     // 轮询进行中？
    bool poll_queued = false; // 用于避免线程队列过载
    bool friends_changed = false;
    bool friend_list_ready = false; // 当此值为 true 时允许处理
    bool need_to_reorder_friends = true;
    std::vector<FriendListWindow::Friend*> friends_online_sorted{};

    constexpr const char* alias_types[] = {
        "无",
        "追加",
        "替换"
    };

    FriendListWindow::Settings settings;

    clock_t friends_list_checked = 0;

    uint8_t poll_interval_seconds = 10;

    std::unordered_map<std::wstring, FriendListWindow::Friend*> uuid_by_name{};

    std::unordered_map<std::string, FriendListWindow::Friend*> friends{};

    bool show_location = true;

    GW::HookEntry FriendStatusUpdate_Entry;

    void LoadCharnames(const char* section, std::unordered_map<std::wstring, uint8_t>* out)
    {
        TNamesDepend values{};
        inifile.GetAllValues(section, "charname", values);
        for (auto i = values.cbegin(); i != values.cend(); ++i) {
            std::wstring char_wstr = TextUtils::StringToWString(i->pItem);
            std::wstring temp;
            std::vector<std::wstring> parts{};
            std::wstringstream wss(char_wstr);
            while (std::getline(wss, temp, L',')) {
                parts.push_back(temp);
            }
            std::wstring name = parts[0];
            uint8_t profession = 0;
            if (parts.size() > 1) {
                const auto p = _wtoi(&parts[1][0]);
                if (p > 0 && p < 11) {
                    profession = static_cast<uint8_t>(p);
                }
            }
            out->emplace(name, profession);
        }
    }

    using FriendRecords = std::map<std::string, FriendListWindow::FriendRecord>;

    // 读取磁盘上的好友记录；优先使用 friends.json，否则解析旧版 friends.ini。
    FriendRecords LoadRecords()
    {
        FriendRecords records;
        const auto json_path = Resources::GetSettingFile(json_filename);
        std::error_code ec;
        if (std::filesystem::exists(json_path, ec)) {
            std::ifstream file(json_path, std::ios::binary);
            const std::string buffer{std::istreambuf_iterator(file), {}};
            constexpr glz::opts lenient_opts{.error_on_unknown_keys = false};
            if (glz::read<lenient_opts>(records, buffer)) {
                records.clear();
            }
            return records;
        }
        inifile.Reset();
        inifile.SetMultiKey(true);
        inifile.LoadFile(Resources::GetLegacySettingFile(ini_filename).c_str());
        TNamesDepend entries;
        inifile.GetAllSections(entries);
        for (const auto& entry : entries) {
            auto& record = records[entry.pItem];
            record.alias = inifile.GetValue(entry.pItem, "alias", "");
            record.type = static_cast<int>(inifile.GetLongValue(entry.pItem, "type", record.type));
            std::unordered_map<std::wstring, uint8_t> charnames;
            LoadCharnames(entry.pItem, &charnames);
            for (const auto& [name, profession] : charnames) {
                record.charnames.emplace(TextUtils::WStringToString(name), profession);
            }
        }
        // 已到达旧版 .ini 回退；标记已更改，以便下次保存时迁移到 friends.json
        friends_changed = true;
        return records;
    }

    FriendListWindow::Friend* SetFriend(const uint8_t*, GW::FriendType, GW::FriendStatus, uint32_t, const wchar_t*, const wchar_t*);
    FriendListWindow::Friend* SetFriend(const GW::Friend*);

    std::wstring ParsePlayerName(const int argc, const LPWSTR* argv)
    {
        std::wstring player_name;
        for (auto i = 0; i < argc; i++) {
            std::wstring s(argv[i]);
            if (s.empty()) {
                continue;
            }
            if (!player_name.empty()) {
                player_name += L" ";
            }
            std::transform(s.begin() + 1, s.end(), s.begin() + 1, towlower);
            std::transform(s.begin(), s.begin() + 1, s.begin(), towupper);
            player_name += s;
        }
        return player_name;
    }

    // 通过编码聊天消息接收的编码消息类型
    enum class MessageType : wchar_t {
        CANNOT_ADD_YOURSELF_AS_A_FRIEND = 0x2f3,
        EXCEEDED_MAX_NUMBER_OF_FRIENDS,
        CHARACTER_NAME_X_DOES_NOT_EXIST,
        FRIEND_ALREADY_ADDED_AS_X,
        INCOMING_WHISPER = 0x76d,
        OUTGOING_WHISPER,
        PLAYER_NAME_IS_INVALID = 0x880,
        PLAYER_X_NOT_ONLINE
    };

    bool WriteError(const MessageType message_type, const wchar_t* character_name)
    {
        wchar_t buffer[122];
        constexpr auto channel = GW::Chat::CHANNEL_GLOBAL;
        switch (message_type) {
            case MessageType::CHARACTER_NAME_X_DOES_NOT_EXIST:
            case MessageType::FRIEND_ALREADY_ADDED_AS_X:
                ASSERT(swprintf(buffer, _countof(buffer), L"%c\x107%s\x1", message_type, character_name) > 0);
                break;
            case MessageType::PLAYER_X_NOT_ONLINE:
                ASSERT(swprintf(buffer, _countof(buffer), L"%c\x101\x100\x107%s\x1\x108\x1", message_type, character_name) > 0);
                break;
            default:
                return false;
        }
        WriteChatEnc(channel, buffer);
        return true;
    }

    // 当私聊被此模块重定向时，设置此标志，防止无限重定向循环。
    bool is_redirecting_whisper = false;

    struct PendingWhisper {
        std::wstring charname;
        std::wstring message;
        clock_t pending_add = 0;

        void reset(const std::wstring& _charname = L"", const std::wstring& _message = L"")
        {
            charname = _charname;
            message = _message;
            pending_add = 0;
        }
    };

    PendingWhisper pending_whisper;

    void UpdatePendingWhisper()
    {
        if (!pending_whisper.pending_add) {
            return;
        }
        if (TIMER_DIFF(pending_whisper.pending_add) > 5000) {
            pending_whisper.reset(); // 添加好友超时
            return;
        }
        // 检查待添加的玩家是否已被添加。
        FriendListWindow::Poll();
        const auto lf = FriendListWindow::GetFriend(pending_whisper.charname.c_str());
        if (!(lf && lf->ValidUuid())) {
            return;
        }
        // 这是 TB 为找出玩家实际使用的角色而临时添加到好友列表的玩家。

        if (lf->IsOffline()) {
            ASSERT(WriteError(MessageType::PLAYER_X_NOT_ONLINE, pending_whisper.charname.c_str()));
        }
        else {
            ASSERT(lf->current_char);
            is_redirecting_whisper = true;
            GW::Chat::SendChat(lf->current_char->getNameW().c_str(), pending_whisper.message.c_str());
            is_redirecting_whisper = false;
        }
        pending_whisper.reset();

        ASSERT(lf->RemoveGWFriend());
        ASSERT(FriendListWindow::RemoveFriend(lf));
    }

    void OnAddFriendError(GW::HookStatus* status, wchar_t*)
    {
        if (!pending_whisper.charname.empty()) {
            ASSERT(WriteError(MessageType::PLAYER_X_NOT_ONLINE, pending_whisper.charname.c_str()));
            pending_whisper.reset();
            status->blocked = true;
        }
    }

    void OnOutgoingWhisperSuccess(GW::HookStatus*, wchar_t*)
    {
        pending_whisper.reset();
    }

    void OnPlayerNotOnline(GW::HookStatus* status, const wchar_t* message)
    {
        const auto player_name = TextUtils::GetPlayerNameFromEncodedString(message);
        if (const auto friend_ = FriendListWindow::GetFriend(player_name.c_str())) {
            if (!friend_->IsOffline() && friend_->current_char->getNameW() != player_name) {
                is_redirecting_whisper = true;
                GW::Chat::SendChat(friend_->current_char->getNameW().c_str(), pending_whisper.message.c_str());
                is_redirecting_whisper = false;
                pending_whisper.reset();
                status->blocked = true;
            }
            return;
        }
        if (!settings.add_offline_players_to_friends) {
            return;
        }
        if (pending_whisper.pending_add && player_name == pending_whisper.charname) {
            return; // 这是由工具箱生成的错误消息
        }
        // 否则，如果该玩家不在好友列表中，则临时添加他们。OnFriendCreated 将重新发送消息并移除好友。
        if (!pending_whisper.charname.empty()) {
            GW::FriendListMgr::AddFriend(pending_whisper.charname.c_str());
            pending_whisper.pending_add = TIMER_INIT();
            status->blocked = true;
        }
    }

    void OnFriendAlreadyAdded(GW::HookStatus* status, const wchar_t* message)
    {
        const auto player_name = TextUtils::GetPlayerNameFromEncodedString(message);
        if (const auto friend_ = FriendListWindow::GetFriend(player_name.c_str())) {
            friend_->SetCharacter(player_name.c_str());
        }
        if (!pending_whisper.charname.empty()) {
            ASSERT(WriteError(MessageType::PLAYER_X_NOT_ONLINE, pending_whisper.charname.c_str()));
            pending_whisper.reset();
            status->blocked = true;
        }
    }

    void OnFriendUpdated(GW::HookStatus*, const GW::Friend* old_state, const GW::Friend* new_state)
    {
        // 维护 charname 到 uuid 的映射日志，并保存到磁盘。
        if (!new_state) {
            // 好友从好友列表中移除。
            if (!old_state) {
                return; // 无旧状态或新状态；忽略此事件
            }
            FriendListWindow::RemoveFriend(FriendListWindow::GetFriend(old_state));
            return;
        }
        const auto lf = SetFriend(new_state);
        if (!lf) {
            return;
        }
        lf->last_update = clock();
    }

    bool GetIsMapReady()
    {
        if (!GW::Map::GetIsMapLoaded()) {
            return false;
        }
        const auto instance_type = GW::Map::GetInstanceType();
        return instance_type == GW::Constants::InstanceType::Explorable || instance_type == GW::Constants::InstanceType::Outpost;
    }

    bool cached_is_friend_list_ready = false;

    bool GetIsFriendListReady(const bool fresh = false)
    {
        if (fresh) {
            const auto fl = GW::FriendListMgr::GetFriendList();
            cached_is_friend_list_ready = fl && fl->friends.size() > 0;
        }
        return cached_is_friend_list_ready;
    }

    constexpr uint32_t OnStoCPacket_Headers[] = {
        GW::Packet::StoC::MessageLocal::STATIC_HEADER,
        GW::Packet::StoC::TradeStart::STATIC_HEADER,
        GW::Packet::StoC::MessageGlobal::STATIC_HEADER,
        GW::Packet::StoC::PartyInviteReceived_Create::STATIC_HEADER,
        GW::Packet::StoC::PlayerJoinInstance::STATIC_HEADER
    };

    GW::HookEntry OnPostStoCPacket_Entry;
    void OnPostStoCPacket(GW::HookStatus* status, GW::Packet::StoC::PacketBase* pak) {
        if (status->blocked)
            return;
        switch (pak->header) {
        case GW::Packet::StoC::PartyInviteReceived_Create::STATIC_HEADER: {
            if (FriendListWindow::GetIsPlayerIgnored(pak) && !GW::PartyMgr::RespondToPartyRequest(((uint32_t*)pak)[1], false))
                Log::Warning("拒绝来自被忽略玩家的邀请失败");
        } break;
        case GW::Packet::StoC::PlayerJoinInstance::STATIC_HEADER: {
            const auto p = (GW::Packet::StoC::PlayerJoinInstance*)pak;
            const auto player_name = TextUtils::SanitizePlayerName(p->player_name);
            const auto a = GW::PlayerMgr::GetPlayerByName(p->player_name);
            const auto f = a && a->primary ? Instance().GetFriend(player_name.data()) : nullptr;
            const auto fc = f ? f->GetCharacter(player_name.data()) : nullptr;
            if (fc) {
                ASSERT(a->primary > 0 && a->primary < 11);
                fc->profession = static_cast<uint8_t>(a->primary);
            }
        } break;
        }
    }

    std::vector<std::pair< const wchar_t*, GW::Chat::ChatCommandCallback>> chat_commands;

    clock_t offline_status_reminder_last_sent = 0;
    bool check_currently_offline_reminder = false;

    clock_t pending_cancel_trade = 0;

    GW::HookEntry OnUIMessage_Entry;
    void OnUIMessage(GW::HookStatus* status, const GW::UI::UIMessage message_id, void* wparam, void*)
    {
        switch (message_id) {
        case GW::UI::UIMessage::kTradeSessionStart:
            // 注意：此时交易邀请窗口尚未在 UI 中绘制，因此在当前帧尝试取消会失败。
            if (wparam && FriendListWindow::GetIsPlayerIgnored(((uint32_t*)wparam)[1])) {
                pending_cancel_trade = TIMER_INIT();
                // 全局阻止，但为游戏控制帧触发 - 这仍会创建交易对话框，使 CancelTrade() 能够通过
                status->blocked = true;
                GW::UI::SendFrameUIMessage(GW::UI::GetChildFrame(GW::UI::GetFrameByLabel(L"Game"),6), message_id, wparam);
            }
            break;
        case GW::UI::UIMessage::kSetAgentNameTagAttribs:
        case GW::UI::UIMessage::kShowAgentNameTag: {
            if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost || !settings.friend_name_tag_enabled) {
                break;
            }
            const auto tag = static_cast<GW::UI::AgentNameTagInfo*>(wparam);
            const auto player_name = TextUtils::GetPlayerNameFromEncodedString(tag->name_enc);
            const auto friend_ = FriendListWindow::GetFriend(player_name.c_str());
            if (friend_ && friend_->type == GW::FriendType::Friend) {
                tag->text_color = settings.friend_name_tag_color;
            }
        }break;
        // 开始新私聊时，自动检查并重定向收件人
        case GW::UI::UIMessage::kStartWhisper: {
            const auto packet = (GW::UI::UIPacket::kStartWhisper*)wparam;
            if (const auto friend_ = FriendListWindow::GetFriend(packet->player_name)) {
                const auto& friendname = friend_->current_char->getNameW();
                if (!friend_->IsOffline() && friend_->current_char && friendname != packet->player_name) {
                    // TODO：这样做是否会导致之前的 wchar_t* 内存泄漏？
                    packet->player_name = const_cast<wchar_t*>(friendname.data());
                }
            }
        } break;
        case GW::UI::UIMessage::kWriteToChatLog: {
            const auto packet = (GW::UI::UIPacket::kWriteToChatLog*)wparam;
            wchar_t* message = packet->message;
            switch (static_cast<MessageType>(message[0])) {
            case MessageType::CANNOT_ADD_YOURSELF_AS_A_FRIEND: // 您不能将自己添加为好友。
            case MessageType::EXCEEDED_MAX_NUMBER_OF_FRIENDS:  // 您的好友列表已超出最大字符数限制。
            case MessageType::PLAYER_NAME_IS_INVALID:          // 玩家名称无效
            case MessageType::CHARACTER_NAME_X_DOES_NOT_EXIST: // 角色名称 "" 不存在
                OnAddFriendError(status, message);
                break;
            case MessageType::FRIEND_ALREADY_ADDED_AS_X: // 您尝试添加的角色已在您的好友列表中，名称为 ""。
                OnFriendAlreadyAdded(status, message);
                break;
            case MessageType::OUTGOING_WHISPER: // 服务器已成功发送您的私聊
                OnOutgoingWhisperSuccess(status, message);
                FriendListWindow::AddFriendAliasToMessage(&packet->message);
                check_currently_offline_reminder = true;
                break;
            case MessageType::INCOMING_WHISPER:
                FriendListWindow::AddFriendAliasToMessage(&packet->message);
                break;
            case MessageType::PLAYER_X_NOT_ONLINE: // 玩家 "" 不在线。如果可以找到正确的人则重定向！
                OnPlayerNotOnline(status, message);
                break;
            }
        }
                                               break;
        case GW::UI::UIMessage::kSendChatMessage: {
            const auto packet = (GW::UI::UIPacket::kSendChatMessage*)wparam;
            const auto message = packet->message;
            const auto channel = GW::Chat::GetChannel(*message);
            if (is_redirecting_whisper || channel != GW::Chat::CHANNEL_WHISPER) {
                return;
            }
            wchar_t* separator_pos = wcschr(message, ',');
            if (!separator_pos) {
                return;
            }
            // 如果收件人在好友列表中，但使用不同的角色名称，则立即重定向...
            const auto target = std::wstring(&message[1], separator_pos);
            const auto text = separator_pos + 1;
            if (const auto friend_ = FriendListWindow::GetFriend(target.c_str())) {
                const auto& friendname = friend_->current_char->getNameW();
                if (!friend_->IsOffline() && friend_->current_char && friendname != target) {
                    is_redirecting_whisper = true;
                    GW::Chat::SendChat(friendname.c_str(), text);
                    is_redirecting_whisper = false;
                    pending_whisper.reset();
                    status->blocked = true;
                    return;
                }
            }
            // ...否则继续发送
            pending_whisper.reset(target, text);
        } break;
        }
    }

    // 如果玩家刚发送了私聊，但状态设为离线，则在聊天中发送消息提醒。
    void UpdateOfflineReminder() {
        if (check_currently_offline_reminder) {
            check_currently_offline_reminder = false;
            if (TIMER_DIFF(offline_status_reminder_last_sent) > 10000 && GW::FriendListMgr::GetMyStatus() == GW::FriendStatus::Offline) {
                offline_status_reminder_last_sent = TIMER_INIT();
                Log::Flash("您当前处于离线状态，将无法收到私聊。\n请在聊天中输入 '/online' 将状态设为在线。");
            }
        }
    }

    /* 设置器 */
    // 从原始信息更新本地好友记录。
    FriendListWindow::Friend* SetFriend(const uint8_t* uuid, const GW::FriendType type, const GW::FriendStatus status, const uint32_t map_id, const wchar_t* charname, const wchar_t* alias)
    {
        if (type != GW::FriendType::Friend && type != GW::FriendType::Ignore) {
            return nullptr;
        }
        // 验证 UUID（当好友刚创建时，GW 不会立即拥有正确的 UUID）
        bool is_valid_uuid = false;
        for (size_t i = 0; !is_valid_uuid && i < sizeof(UUID); i++) {
            is_valid_uuid = uuid[i] != 0;
        }
        if (!is_valid_uuid) {
            return nullptr;
        }
        FriendListWindow::Friend* lf = FriendListWindow::GetFriend(uuid);
        if (!lf && charname) {
            lf = FriendListWindow::GetFriend(charname);
        }
        if (!lf && alias) {
            lf = FriendListWindow::GetFriend(alias);
        }

        if (!lf) {
            // 新好友（uuid_changed 将触发后续添加）
            lf = new FriendListWindow::Friend(&Instance());
        }
        const bool type_changed = lf->type != type;
        lf->type = type;
        const bool uuid_changed = memcmp(&lf->uuid_bytes, uuid, sizeof(UUID));
        const bool alias_changed = alias != lf->GetAliasW();
        if (uuid_changed) {
            // UUID 不同。这可能是因为 GW 已为该好友分配了 UUID。
            friends.erase(lf->uuid);
            lf->uuid_bytes = *(UUID*)uuid;
            lf->uuid = TextUtils::GuidToString(&lf->uuid_bytes);
            friends.emplace(lf->uuid, lf);
        }
        if (alias && alias_changed) {
            // 该 uuid 的好友别名已更改，或该别名的 uuid 已更改。
            uuid_by_name.erase(lf->GetAliasW());
            lf->setAlias(alias);
            uuid_by_name.emplace(lf->GetAliasW(), lf);
        }
        if (lf->current_map_id != map_id) {
            lf->current_map_id = map_id;
            lf->current_map_name = Resources::GetMapName(static_cast<GW::Constants::MapID>(map_id));
        }

        if (!charname || status == GW::FriendStatus::Offline) {
            lf->current_char = nullptr;
        }
        if (status != GW::FriendStatus::Offline && charname) {
            lf->current_char = lf->SetCharacter(charname);
            uuid_by_name.emplace(charname, lf);
        }
        const bool status_changed = lf->status != status;
        lf->status = status;

        if (status_changed || alias_changed || uuid_changed || type_changed) {
            need_to_reorder_friends = true;
        }

        friends_changed = true;
        return lf;
    }

    // 从现有好友更新本地好友记录。
    FriendListWindow::Friend* SetFriend(const GW::Friend* f)
    {
        return SetFriend(f->uuid, f->type, f->status, f->zone_id, &f->charname[0], &f->alias[0]);
    }

    void CHAT_CMD_FUNC(CmdInvite)
    {
        const auto player_name = ParsePlayerName(argc - 1, &argv[1]);
        const auto friend_ = player_name.empty() ? nullptr : FriendListWindow::GetFriend(player_name.c_str());
        if (friend_ && friend_->current_char && friend_->current_char->getNameW() != player_name) {
            GW::Chat::SendChat('/', std::format(L"invite {}", friend_->current_char->getNameW()).c_str());
            return;
        }
        status->blocked = false;
    }

    void CHAT_CMD_FUNC(CmdSetFriendListStatus)
    {
        std::wstring cmd = *argv;
        auto set = GW::FriendListMgr::GetMyStatus();
        const auto current = set;
        if (cmd == L"away") {
            set = GW::FriendStatus::Away;
        }
        else if (cmd == L"online") {
            set = GW::FriendStatus::Online;
        }
        else if (cmd == L"offline") {
            set = GW::FriendStatus::Offline;
        }
        else if (cmd == L"busy" || cmd == L"dnd") {
            set = GW::FriendStatus::DND;

        }
        if (current == set)
            return;
        if (GW::FriendListMgr::SetFriendListStatus(set)) {
            Log::Flash("您现在 %s", GetStatusText(set));
        }
        else {
            Log::ErrorW(L"设置好友列表状态失败");
        }
    }

    void CHAT_CMD_FUNC(CmdAddFriend)
    {
        if (argc < 2) {
            return Log::Error("缺少玩家名称");
        }
        const auto player_name = ParsePlayerName(argc - 1, &argv[1]);
        if (player_name.empty()) {
            return Log::Error("缺少玩家名称");
        }
        GW::FriendListMgr::AddFriend(player_name.c_str());
    }

    void CHAT_CMD_FUNC(CmdRemoveFriend)
    {
        if (argc < 2) {
            return Log::Error("缺少玩家名称");
        }
        const auto player_name = ParsePlayerName(argc - 1, &argv[1]);
        if (player_name.empty()) {
            return Log::Error("缺少玩家名称");
        }
        auto f = Instance().GetFriend(player_name.c_str());
        if (!f) {
            return Log::Error("未找到好友 '%ls'", player_name.c_str());
        }
        f->RemoveGWFriend();
    }

    // 将 /whisper player_name, message 重定向到 GW::SendChat
    void CHAT_CMD_FUNC(CmdWhisper)
    {
        const wchar_t* msg = wcschr(message, ' ');
        if (msg) {
            GW::Chat::SendChat('"', msg + 1);
        }
    }

}

// 判断与此数据包相关的玩家是否在当前玩家的忽略列表中。
bool FriendListWindow::GetIsPlayerIgnored(GW::Packet::StoC::PacketBase* pak)
{
    switch (pak->header) {
        case GAME_SMSG_CHAT_MESSAGE_LOCAL:
        case GAME_SMSG_TRADE_REQUEST:
            return GetIsPlayerIgnored(((uint32_t*)pak)[1]);
        case GAME_SMSG_CHAT_MESSAGE_GLOBAL: {
            const auto p = static_cast<GW::Packet::StoC::MessageGlobal*>(pak);
            return GetIsPlayerIgnored(std::wstring(p->sender_name));
        }
        case GAME_SMSG_PARTY_REQUEST_CANCEL:
        case GAME_SMSG_PARTY_REQUEST_RESPONSE:
        case GAME_SMSG_PARTY_JOIN_REQUEST: {
            const uint32_t party_id = ((uint32_t*)pak)[1];
            GW::PartyInfo* p = GW::PartyMgr::GetPartyInfo(party_id);
            if (p && p->players.size()) {
                return GetIsPlayerIgnored(p->players[0].login_number);
            }
        }
        break;
    }
    return false;
}

// 判断当前地图中的某玩家是否在当前玩家的忽略列表中。
bool FriendListWindow::GetIsPlayerIgnored(const uint32_t player_number)
{
    return GetIsPlayerIgnored(GetPlayerName(player_number));
}

// 判断此玩家名称是否在当前玩家的忽略列表中。
bool FriendListWindow::GetIsPlayerIgnored(const std::wstring& player_name)
{
    const auto* f = player_name.empty() ? nullptr : Instance().GetFriend(player_name.c_str());
    return f && f->type == GW::FriendType::Ignore;
}


/*  FriendListWindow::Friend    */
GW::Friend* FriendListWindow::Friend::GetFriend()
{
    return GW::FriendListMgr::GetFriend((uint8_t*)&uuid_bytes);
}

// 通过当前角色名称向此玩家发送私聊。
void FriendListWindow::Friend::StartWhisper() const
{
    if (!current_char || current_char->getNameW().empty()) {
        return Log::ErrorW(L"玩家 %s 未登录", alias.c_str());
    }
    GW::GameThread::Enqueue([charname = current_char->getNameW()] {
        SendUIMessage(GW::UI::UIMessage::kOpenWhisper, const_cast<wchar_t*>(charname.data()));
    });
}

// 获取属于此好友的角色（例如查找职业等）
FriendListWindow::Character* FriendListWindow::Friend::GetCharacter(const wchar_t* char_name)
{
    const auto it = characters.find(char_name);
    if (it == characters.end()) {
        return nullptr; // 未找到
    }
    return &it->second;
}

FriendListWindow::Character* FriendListWindow::Friend::SetCharacter(const wchar_t* char_name, const uint8_t profession)
{
    Character* existing = GetCharacter(char_name);
    if (!existing) {
        Character c;
        c.SetName(char_name);
        characters.emplace(c.getNameW(), c);
        existing = GetCharacter(c.getNameW().c_str());
        cached_charnames_hover = false;
    }
    if (profession && profession != existing->profession) {
        existing->profession = profession;
        cached_charnames_hover = false;
    }
    return existing;
}

// 从 GW 好友列表中移除此好友（例如，如果是工具箱好友，且仅为了获取信息而添加）。
bool FriendListWindow::Friend::RemoveGWFriend()
{
    GW::Friend* f = GetFriend();
    if (!f) {
        return false;
    }
    GW::FriendListMgr::RemoveFriend(f);
    last_update = clock();
    return true;
}

bool FriendListWindow::Friend::ValidUuid()
{
    const char* uuid_ptr = (char*)&uuid_bytes;
    for (size_t i = 0; i < sizeof(uuid_bytes); i++) {
        if (uuid_ptr[i] != 0) {
            return true;
        }
    }
    return false;
}

/* 获取器 */
std::string FriendListWindow::Friend::GetCharactersHover(const bool include_charname)
{
    if (!cached_charnames_hover) {
        std::wstring cached_charnames_hover_ws = L"角色列表：";
        cached_charnames_hover_ws += alias;
        cached_charnames_hover_ws += L"：";
        for (auto it2 =
                 characters.begin();
             it2 != characters.end(); ++it2) {
            cached_charnames_hover_ws += L"\n  ";
            cached_charnames_hover_ws += it2->first;
            if (it2->second.profession) {
                const auto prof_name = ToolboxUtils::GetProfessionName(static_cast<GW::Constants::Profession>(it2->second.profession));
                if (prof_name && !prof_name->wstring().empty()) {
                    cached_charnames_hover_ws += L"（";
                    cached_charnames_hover_ws += prof_name->wstring();
                    cached_charnames_hover_ws += L"）";
                }
            }
        }
        cached_charnames_hover_str =
            TextUtils::WStringToString(cached_charnames_hover_ws);
        cached_charnames_hover = true;
    }
    std::string str;
    if (include_charname && current_char) {
        str += current_char->GetNameA();
        str += "\n";
    }
    if (include_charname && current_map_name && !current_map_name->string().empty()) {
        str += current_map_name->string();
        str += "\n";
    }
    if (!str.empty()) {
        str += "\n";
    }
    str += cached_charnames_hover_str;
    return str;
}

// 通过角色名称查找现有好友记录。
FriendListWindow::Friend* FriendListWindow::GetFriend(const wchar_t* name)
{
    if (!(name && *name)) return nullptr;
    const auto it = uuid_by_name.find(name);
    return it == uuid_by_name.end() ? nullptr : it->second;
}

// 通过 GW 好友对象查找现有好友记录。
FriendListWindow::Friend* FriendListWindow::GetFriend(const GW::Friend* f)
{
    return f ? GetFriend(f->uuid) : nullptr;
}

FriendListWindow::Friend* FriendListWindow::GetFriend(const uint8_t* uuid)
{
    return GetFriendByUUID(TextUtils::GuidToString((GUID*)uuid));
}

// 通过 uuid 查找现有好友记录。
FriendListWindow::Friend* FriendListWindow::GetFriendByUUID(const std::string& uuid)
{
    const auto it = friends.find(uuid);
    return it == friends.end() ? nullptr : it->second;
}

bool FriendListWindow::RemoveFriend(const Friend* f)
{
    if (!f) {
        return false;
    }
    friends.erase(f->uuid);
    for (const auto& char_key : f->characters | std::views::keys) {
        uuid_by_name.erase(char_key);
    }
    uuid_by_name.erase(f->GetAliasW());
    // Cached view may hold the pointer we're about to free.
    friends_online_sorted.clear();
    need_to_reorder_friends = true;
    delete f;
    return true;
}

/* FriendListWindow 基本功能等 */
void FriendListWindow::Initialize()
{
    ToolboxWindow::Initialize();
    SettingsRegistry::Register(this, settings);

    GW::FriendListMgr::RegisterFriendStatusCallback(&FriendStatusUpdate_Entry, OnFriendUpdated);


    constexpr GW::UI::UIMessage OnUIMessage_Headers[] = {
        GW::UI::UIMessage::kStartWhisper,
        GW::UI::UIMessage::kSetAgentNameTagAttribs,
        GW::UI::UIMessage::kShowAgentNameTag,
        GW::UI::UIMessage::kWriteToChatLog,
        GW::UI::UIMessage::kOpenWhisper,
        GW::UI::UIMessage::kSendChatMessage,
        GW::UI::UIMessage::kTradeSessionStart
    };

    for (const auto message_id : OnUIMessage_Headers) {
        RegisterUIMessageCallback(&OnUIMessage_Entry, message_id, OnUIMessage);
    }

    for (const auto header_id : OnStoCPacket_Headers) {
        GW::StoC::RegisterPacketCallback(&OnPostStoCPacket_Entry, header_id, OnPostStoCPacket, 0x8001);
    }

chat_commands = {
    {L"addfriend", CmdAddFriend},
    {L"removefriend", CmdRemoveFriend},
    {L"deletefriend", CmdRemoveFriend},
    {L"invite", CmdInvite},
    {L"t", CmdWhisper},
    {L"whisper", CmdWhisper},
    {L"tell", CmdWhisper},
    {L"w", CmdWhisper},
    {L"away", CmdSetFriendListStatus},
    {L"dnd", CmdSetFriendListStatus},
    {L"offline", CmdSetFriendListStatus},
    {L"online", CmdSetFriendListStatus},
    {L"busy", CmdSetFriendListStatus}
};
    for (auto& it : chat_commands) {
        GW::Chat::CreateCommand(&ChatCmd_HookEntry, it.first, it.second);
    }

}

void FriendListWindow::SignalTerminate()
{
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
    // 尝试在此处移除回调。
    GW::FriendListMgr::RemoveFriendStatusCallback(&FriendStatusUpdate_Entry);
    GW::StoC::RemoveCallbacks(&OnPostStoCPacket_Entry);
    GW::UI::RemoveUIMessageCallback(&OnUIMessage_Entry);
}

void FriendListWindow::Terminate()
{
    ToolboxWindow::Terminate();
    // 再次尝试移除回调。
    SignalTerminate();
    while (friends.begin() != friends.end()) {
        RemoveFriend(friends.begin()->second);
    }
    friends.clear();
    if (settings_thread.joinable()) {
        settings_thread.join();
    }
}

// 可选：在传入/传出的消息中添加好友别名
void FriendListWindow::AddFriendAliasToMessage(wchar_t** message_ptr)
{
    if (settings.show_alias_on_whisper == FriendAliasType::NONE) {
        return;
    }
    wchar_t* message = *message_ptr;
    const wchar_t* name_start = wcschr(message, 0x107);
    ASSERT(name_start != nullptr);
    const wchar_t* name_end = wcschr(name_start, 0x1);
    ASSERT(name_end != nullptr);
    const std::wstring player_name(name_start + 1, name_end);
    const auto friend_ = GetFriend(player_name.c_str());
    if (!friend_ || friend_->GetAliasW() == player_name) {
        return;
    }
    static std::wstring new_message;
    if (settings.show_alias_on_whisper == FriendAliasType::APPEND) {
        new_message = std::format(L"{}（{}）{}", std::wstring(message, name_end - message), friend_->GetAliasW(), name_end);
    }
    else if (settings.show_alias_on_whisper == FriendAliasType::REPLACE) {
        const auto player_name_pos = std::wstring(message).find(player_name);
        if (player_name_pos != std::wstring::npos) {
            new_message = std::wstring(message).replace(player_name_pos, player_name.length(), friend_->GetAliasW());
        }
    }
    // TODO：这样做是否会导致之前的 wchar_t* 内存泄漏？
    *message_ptr = const_cast<wchar_t*>(new_message.c_str());
}

void FriendListWindow::Update(const float)
{
    if (loading || !GW::Map::GetIsMapLoaded()) {
        return;
    }
    if (pending_cancel_trade) {
        if (GW::Trade::CancelTrade())
            pending_cancel_trade = 0;
        else if (TIMER_DIFF(pending_cancel_trade) > 1000) {
            pending_cancel_trade = 0;
            Log::Warning("拒绝来自被忽略玩家的交易失败");
        }
    }

    const GW::FriendList* fl = GW::FriendListMgr::GetFriendList();
    friend_list_ready = fl && fl->friends.valid();
    if (!friend_list_ready) {
        return;
    }
    if (!poll_queued) {
        const auto interval_check = poll_interval_seconds * CLOCKS_PER_SEC;
        if (!friends_list_checked || clock() - friends_list_checked > interval_check) {
            Poll();
        }
    }
    UpdatePendingWhisper();
    UpdateOfflineReminder();
}

void FriendListWindow::Poll()
{
    if (loading || polling) {
        return;
    }
    if (!GetIsMapReady()) {
        return;
    }
    if (!GetIsFriendListReady(true)) {
        return;
    }
    polling = true;
    const clock_t now = clock();

    // 1. 从工具箱列表中移除不再存在于 GW 列表中的好友
    auto it = friends.begin();
    while (it != friends.end()) {
        Friend* lf = it->second;
        if (lf->GetFriend()) {
            ++it;
            continue;
        }
        ASSERT(RemoveFriend(lf));
        it = friends.begin();
    }

    // 2. 从 GW 列表更新或添加好友到工具箱列表
    GW::FriendList* fl = GW::FriendListMgr::GetFriendList();
    ASSERT(fl);
    for (auto i = 0u; i < fl->friends.size(); i++) {
        const GW::Friend* f = fl->friends[i];
        if (!f) {
            continue;
        }
        Friend* lf = SetFriend(f->uuid, f->type, f->status, f->zone_id, f->charname, f->alias);
        if (!lf) {
            continue;
        }
        lf->last_update = now;
    }

    friends_list_checked = now;
    polling = false;
}

ImGuiWindowFlags FriendListWindow::GetWinFlags(ImGuiWindowFlags flags) const
{
    if (IsWidget()) {
        flags |= ImGuiWindowFlags_NoTitleBar;
        flags |= ImGuiWindowFlags_NoScrollbar;
        if (settings.lock_size_as_widget) {
            flags |= ImGuiWindowFlags_NoResize;
            flags |= ImGuiWindowFlags_AlwaysAutoResize;
        }
        if (settings.lock_move_as_widget) {
            flags |= ImGuiWindowFlags_NoMove;
        }
        return flags;
    }
    return ToolboxWindow::GetWinFlags(flags);
}

bool FriendListWindow::IsWidget() const
{
    return (settings.explorable_show_as == 1 && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable)
           || (settings.outpost_show_as == 1 && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)
           || (settings.loading_show_as == 1 && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading);
}

bool FriendListWindow::IsWindow() const
{
    return (settings.explorable_show_as == 0 && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable)
           || (settings.outpost_show_as == 0 && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)
           || (settings.loading_show_as == 1 && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading);
}

void FriendListWindow::Draw(IDirect3DDevice9*)
{
    if (!(visible && GetIsFriendListReady() && GetIsMapReady())) {
        return;
    }
    const bool is_widget = IsWidget();
    const bool is_window = IsWindow();
    if (!is_widget && !is_window) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2(0.0f, 72.0f), ImGuiCond_FirstUseEver);
    const auto window_size = ImVec2(540.0f * ImGui::FontScale(), 512.0f * ImGui::FontScale());
    const float cols[3] = {180.0f * ImGui::FontScale(), 360.0f * ImGui::FontScale(), 540.0f * ImGui::FontScale()};
    ImGui::SetNextWindowSize(window_size, ImGuiCond_FirstUseEver);
    if (is_widget) {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor(0).Value);
    }
    const bool ok = ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags());
    if (is_widget) {
        ImGui::PopStyleColor();
    }
    if (!ok) {
        return ImGui::End();
    }

    unsigned int colIdx = 0;
    const bool show_charname = ImGui::GetContentRegionAvail().x > cols[0];
    const bool _show_location = ImGui::GetContentRegionAvail().x > cols[1];
    if (!is_widget) {
        ImGui::Text("名称");
        if (show_charname) {
            ImGui::SameLine(cols[colIdx]);
            ImGui::Text("角色");
        }
        if (_show_location) {
            ImGui::SameLine(cols[++colIdx]);
            ImGui::Text("地图");
        }
        ImGui::Separator();
        ImGui::BeginChild("friend_list_scroll");
    }
    const float height = ImGui::GetTextLineHeightWithSpacing();
    if (settings.show_my_status) {
        auto status = std::to_underlying(GW::FriendListMgr::GetMyStatus());
        ImGui::Text("您的状态：");
        ImGui::SameLine();
        const ImVec2 pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(pos.x + 1, pos.y + 1));
        ImGui::TextColored(ImVec4(0, 0, 0, 1), statuses[status]);
        ImGui::SetCursorPos(ImVec2(pos.x, pos.y));
        ImGui::TextColored(StatusColors[status].Value, statuses[status]);
        if (ImGui::IsItemClicked()) {
            status++;
            if (status == 4) {
                status = 0;
            }
            GW::FriendListMgr::SetFriendListStatus(static_cast<GW::FriendStatus>(status));
        }
    }
    if (need_to_reorder_friends) {
        friends_online_sorted.clear();
        friends_online_sorted.reserve(friends.size());
        for (const auto& it : friends) {
            const auto lfp = it.second;
            if (lfp->type != GW::FriendType::Friend) {
                continue;
            }
            if (lfp->IsOffline()) {
                continue;
            }
            if (lfp->GetAliasW().empty()) {
                continue;
            }
            friends_online_sorted.push_back(lfp);
        }
        std::ranges::sort(friends_online_sorted, [](const Friend* lhs, const Friend* rhs) {
            return lhs->GetAliasW().compare(rhs->GetAliasW()) < 0;
        });
        need_to_reorder_friends = false;
    }
    char tmpbuf[32];
    for (const auto lfp : friends_online_sorted) {
        colIdx = 0;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, settings.hover_background_color.value);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, settings.hover_background_color.value);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
        ImGui::PushID(lfp->uuid.c_str());
        ImGui::Button("", ImVec2(ImGui::GetContentRegionAvail().x, height));
        const bool left_clicked = ImGui::IsItemClicked(0);
        const bool right_clicked = ImGui::IsItemClicked(1);
        if (right_clicked && lfp->current_map_id != 0) {
            ImGui::OpenPopup("##friend_ctx");
        }

        bool hovered = ImGui::IsItemHovered();
        ImGui::PopStyleVar(4);
        ImGui::SameLine(2.0f, 0);
        ImGui::PushStyleColor(ImGuiCol_Text, StatusColors[static_cast<size_t>(lfp->status)].Value);
        ImGui::Bullet();
        ImGui::PopStyleColor(4);
        if (ImGui::BeginPopup("##friend_ctx")) {
            if (lfp->current_map_id != 0) {
                const auto& map_name = Resources::GetMapName(static_cast<GW::Constants::MapID>(lfp->current_map_id))->string();
                const auto label = std::format("前往 {}", map_name);
                if (ImGui::MenuItem(label.c_str())) {
                    TravelWindow::Instance().TravelNearest(static_cast<GW::Constants::MapID>(lfp->current_map_id));
                }
            }
            ImGui::EndPopup();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(GetStatusText(lfp->status));
        }
        ImGui::SameLine(0);
        const auto& alias = lfp->GetAliasA();
        if (is_widget) {
            ImGui::TextShadowed(alias.c_str());
        }
        else {
            ImGui::Text(alias.c_str());
        }
        hovered = hovered || ImGui::IsItemHovered();
        if (!show_charname && hovered) {
            ImGui::SetTooltip(lfp->GetCharactersHover(true).c_str());
        }
        if (show_charname && lfp->current_char != nullptr) {
            ImGui::SameLine(cols[colIdx]);
            const auto& current_char_name_s = lfp->current_char->GetNameA();
            const uint8_t prof = lfp->current_char->profession;
            if (prof) {
                ImGui::PushStyleColor(ImGuiCol_Text, ProfColors[lfp->current_char->profession].Value);
            }
            if (is_widget) {
                ImGui::TextShadowed(current_char_name_s.c_str());
            }
            else {
                ImGui::Text(current_char_name_s.c_str());
            }
            if (prof) {
                ImGui::PopStyleColor();
            }
            if (lfp->characters.size() > 1) {
                ImGui::SameLine(0, 0);
                snprintf(tmpbuf, sizeof(tmpbuf), "（+%d）", lfp->characters.size() - 1);
                if (is_widget) {
                    ImGui::TextShadowed(tmpbuf);
                }
                else {
                    ImGui::Text(tmpbuf);
                }
                hovered |= ImGui::IsItemHovered();
                if (hovered) {
                    ImGui::SetTooltip(lfp->GetCharactersHover().c_str());
                }
            }
            if (show_location) {
                if (lfp->current_map_name) {
                    ImGui::SameLine(cols[++colIdx]);
                    if (is_widget) {
                        ImGui::TextShadowed(lfp->current_map_name->string().c_str());
                    }
                    else {
                        ImGui::Text(lfp->current_map_name->string().c_str());
                    }
                }
            }
        }
        ImGui::PopID();
        if (left_clicked && !lfp->IsOffline()) {
            lfp->StartWhisper();
        }
    }
    if (!is_widget) {
        ImGui::EndChild();
    }
    ImGui::End();
}

void FriendListWindow::DrawSettingsInternal()
{
    ImGui::Checkbox("锁定小部件大小", &settings.lock_size_as_widget);
    ImGui::SameLine();
    ImGui::Checkbox("锁定小部件移动", &settings.lock_move_as_widget);
    const float dropdown_width = 160.0f * ImGui::FontScale();
    ImGui::Text("显示为");
    ImGui::SameLine();
    ImGui::PushItemWidth(dropdown_width);
    ImGui::Combo("###show_as_outpost", &settings.outpost_show_as, "窗口\0小部件\0隐藏");
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Text("在前哨站中");

    ImGui::Text("显示为");
    ImGui::SameLine();
    ImGui::PushItemWidth(dropdown_width);
    ImGui::Combo("###show_as_explorable", &settings.explorable_show_as, "窗口\0小部件\0隐藏");
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Text("在可探索区域中");

    ImGui::CheckboxWithHelp("临时将您私聊的离线玩家添加为好友", &settings.add_offline_players_to_friends, "当您私聊某人而他们离线时，工具箱将尝试将这些玩家添加到您的好友列表"
        "以确定他们是否在其他角色上在线。\n如果是，工具箱将把您的私聊重定向到该角色。\n"
        "之后，该玩家将从您的好友列表中移除。");

    Colors::DrawSettingHueWheel("小部件背景悬停颜色", &settings.hover_background_color.value);
    ImGui::CheckboxWithHelp("显示我的状态", &settings.show_my_status, "例如 '您的状态：在线'");

    ImGui::CheckboxWithHelp("为好友自定义名称标签颜色", &settings.friend_name_tag_enabled, "在前哨站中瞄准好友时");
    if (settings.friend_name_tag_enabled) {
        Colors::DrawSettingHueWheel("好友名称标签颜色", &settings.friend_name_tag_color.value);
    }
    DrawChatSettings();
}

void FriendListWindow::RegisterSettingsContent()
{
    ToolboxUIElement::RegisterSettingsContent();
    ToolboxModule::RegisterSettingsContent(
        "聊天设置", nullptr,
        [this](const std::string&, const bool is_showing) {
            if (!is_showing) {
                return;
            }
            DrawChatSettings();
        }, 0.91f);
}

void FriendListWindow::DrawChatSettings()
{
    ImGui::Text("发送/接收私聊时显示好友别名：");
    ImGui::ShowHelp("仅当好友别名与其角色名称不同时");
    ImGui::Combo("###show_alias_on_whisper", reinterpret_cast<int*>(&settings.show_alias_on_whisper), alias_types, _countof(alias_types));
}

void FriendListWindow::DrawHelp()
{
    if (!ImGui::TreeNodeEx("好友列表命令", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        return;
    }
    ImGui::Bullet();
    ImGui::Text("'/addfriend <角色名称>' 将角色添加到您的好友列表。");
    ImGui::Bullet();
    ImGui::Text("'/removefriend <角色名称|别名>' 从您的好友列表中移除角色。");
    ImGui::Bullet();
    ImGui::Text("'/away' 将您的好友列表状态设为“离开”。");
    ImGui::Bullet();
    ImGui::Text("'/online' 将您的好友列表状态设为“在线”。");
    ImGui::Bullet();
    ImGui::Text("'/offline' 将您的好友列表状态设为“离线”。");
    ImGui::Bullet();
    ImGui::Text("'/busy' 或 '/dnd' 将您的好友列表状态设为“请勿打扰”。");
    ImGui::TreePop();
}

void FriendListWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWindow::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
    LoadFromFile();
}

void FriendListWindow::SaveSettings(SettingsDoc& doc)
{
    ToolboxWindow::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
    SaveToFile();
}

void FriendListWindow::LoadFromFile()
{
    if (loading) {
        return;
    }
    loading = true;
    Log::Log("%s：从本地加载好友\n", Name());
    if (settings_thread.joinable()) {
        settings_thread.join();
    }
    settings_thread = std::thread([this] {
        uuid_by_name.clear();
        while (friends.begin() != friends.end()) {
            RemoveFriend(friends.begin()->second);
        }
        friends.clear();

        const auto records = LoadRecords();
        for (const auto& [uuid, record] : records) {
            auto lf = new Friend(this);
            lf->uuid = uuid;
            TextUtils::StringToGuid(lf->uuid, &lf->uuid_bytes);
            lf->setAlias(TextUtils::StringToWString(record.alias));
            lf->type = static_cast<GW::FriendType>(record.type);
            if (lf->uuid.empty() || lf->GetAliasW().empty()) {
                delete lf;
                continue; // 错误，别名或 uuid 为空。
            }

            for (const auto& [name, profession] : record.charnames) {
                lf->SetCharacter(TextUtils::StringToWString(name).c_str(), profession);
            }
            if (lf->characters.empty()) {
                delete lf;
                continue; // 错误，应至少有一个角色名...
            }
            friends.emplace(lf->uuid, lf);
            for (const auto& it : lf->characters) {
                uuid_by_name[it.first] = lf;
            }
            uuid_by_name[lf->GetAliasW()] = lf;
            need_to_reorder_friends = true;
        }
        Log::Log("%s：已从本地加载好友\n", Name());
        friends_list_checked = false;
        loading = false;
    });
}

void FriendListWindow::SaveToFile()
{
    if (!friends_changed) {
        return;
    }
    if (settings_thread.joinable()) {
        settings_thread.join();
    }
    settings_thread = std::thread([] {
        friends_changed = false;
        if (friends.empty()) {
            return; // 错误，应至少有一个好友
        }
        auto records = LoadRecords();
        for (auto it = friends.begin(); it != friends.end(); ++it) {
            Friend& lf = *it->second;
            auto& record = records[lf.uuid];
            record.type = static_cast<int>(lf.type);
            record.alias = lf.GetAliasA();
            // 追加到现有角色名，但不重复。这允许多个账户共同维护好友列表。
            for (const auto& char_it : lf.characters) {
                const auto charname = TextUtils::WStringToString(char_it.first);
                const auto found = record.charnames.find(charname);
                // 注意：不覆盖职业
                if (found == record.charnames.end() || char_it.second.profession != 0) {
                    record.charnames.emplace(charname, char_it.second.profession);
                }
            }
        }
        std::string buffer;
        if (glz::write<glz::opts{.prettify = true}>(records, buffer)) {
            return;
        }
        std::ofstream file(Resources::GetSettingFile(json_filename), std::ios::binary | std::ios::trunc);
        ASSERT(file && file.write(buffer.data(), static_cast<std::streamsize>(buffer.size())).good());
    });
}
