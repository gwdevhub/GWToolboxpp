#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Friendslist.h>
#include <GWCA/GameEntities/Hero.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Title.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/FriendListMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/QuestMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWToolbox.h>
#include <Logger.h>
#include <Utils/GuiUtils.h>

#include <Constants/EncStrings.h>
#include <Modules/ChatCommands.h>
#include <Modules/ChatSettings.h>
#include <Modules/DialogModule.h>
#include <Modules/GameSettings.h>
#include <Modules/HallOfMonumentsModule.h>
#include <Modules/InventoryManager.h>
#include <Modules/Resources.h>
#include <Utils/TextUtils.h>
#include <Widgets/PartyDamage.h>
#include <Widgets/TimerWidget.h>
#include <Windows/BuildsWindow.h>
#include <Windows/MainWindow.h>
#include <Windows/SettingsWindow.h>

#include <Utils/ToolboxUtils.h>
#include "CameraUnlockModule.h"
#include "ChatFilter.h"
#include "QuestModule.h"

constexpr auto CMDTITLE_KEEP_CURRENT = 0xfffe;
constexpr auto CMDTITLE_REMOVE_CURRENT = 0xffff;

namespace {

    struct SearchAgent {
        struct Query {
            std::wstring search;
            GW::AgentTargetFlags type;
        };
        clock_t started = 0;
        std::vector<std::pair<uint32_t, std::unique_ptr<GuiUtils::EncString>>> npc_names;
        std::vector<Query> queries;
        void Add(const wchar_t* _search, const GW::AgentTargetFlags type);
        void Update();
        void Terminate() { Reset(); }
        void Reset()
        {
            started = 0;
            queries.clear();
            npc_names.clear();
        }
    } npc_to_find;


    const wchar_t* next_word(const wchar_t* str)
    {
        while (*str && !isspace(*str)) {
            str++;
        }
        while (*str && isspace(*str)) {
            str++;
        }
        return *str ? str : nullptr;
    }

    const wchar_t* GetRemainingArgsWstr(const wchar_t* message, const int argc_start)
    {
        const wchar_t* out = message;
        for (auto i = 0; i < argc_start && out; i++) {
            out = next_word(out);
        }
        return out ? out : L"";
    };

    uint32_t GetAgentModelId(const GW::Agent* agent)
    {
        if (!agent) return 0;
        if (const auto ag = agent->GetAsAgentLiving()) {
            return ag->player_number;
        }
        if (const auto ag = agent->GetAsAgentItem()) {
            if (const auto item = GW::Items::GetItemById(ag->item_id)) return item->model_id;
            return 0;
        }
        if (const auto ag = agent->GetAsAgentGadget()) {
            return ag->gadget_id;
        }
        return 0;
    }

    bool IsMapReady()
    {
        return GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading && !GW::Map::GetIsObserving() && GW::MemoryMgr::GetGWWindowHandle() == GetActiveWindow();
    }

    float GetAngle(const GW::GamePos& pos)
    {
        constexpr float pi = DirectX::XM_PI;
        float tan_angle;
        if (pos.x == 0.0f) {
            if (pos.y >= 0.0f) {
                tan_angle = pi / 2;
            }
            else {
                tan_angle = pi / 2 * -1.0f;
            }
        }
        else if (pos.x < 0.0f) {
            if (pos.y >= 0.0f) {
                tan_angle = atan(pos.y / pos.x) + pi;
            }
            else {
                tan_angle = atan(pos.y / pos.x) - pi;
            }
        }
        else {
            tan_angle = atan(pos.y / pos.x);
        }
        tan_angle *= 180.0f / pi;
        return tan_angle;
    };

    void SafeChangeTarget(uint32_t agent_id)
    {
        GW::GameThread::Enqueue([agent_id] {
            GW::Agents::ChangeTarget(GW::Agents::GetAgentByID(agent_id));
        });
    };

    void TargetVipers()
    {
        GW::AgentArray* agents = GW::Agents::GetAgentArray();
        const GW::Agent* me = agents ? GW::Agents::GetControlledCharacter() : nullptr;
        if (me == nullptr) {
            return;
        }

        const auto wanted_angle = me->rotation_angle * 180.0f / DirectX::XM_PI;
        constexpr auto max_angle_diff = 22.5f; // Acceptable angle for vipers
        float max_distance = GW::Constants::SqrRange::Spellcast;

        GW::Agent* closest = nullptr;
        for (const auto agent : *agents) {
            if (agent == me || !GW::Agents::GetAgentMatchesFlags(agent, GW::TargetFilter::AnyLiving)) {
                continue;
            }
            const float this_distance = GetSquareDistance(me->pos, agent->pos);
            if (this_distance > max_distance) {
                continue;
            }
            const float agent_angle = GetAngle(me->pos - agent->pos);
            const float this_angle_diff = abs(wanted_angle - agent_angle);
            if (this_angle_diff > max_angle_diff) {
                continue;
            }
            closest = agent;
            max_distance = this_distance;
        }
        if (closest) {
            SafeChangeTarget(closest->agent_id);
        }
    }

    const auto AgentEETargetType = GW::TargetFilter::Allies | GW::AgentTargetFlags::Include_NPCMinipet | GW::AgentTargetFlags::Include_SpiritPet | GW::AgentTargetFlags::Include_Minion;

    void TargetEE()
    {
        // target best ebon escape target
        GW::AgentArray* agents = GW::Agents::GetAgentArray();
        const GW::Agent* me = agents ? GW::Agents::GetControlledCharacter() : nullptr;
        if (me == nullptr) {
            return;
        }

        const auto facing_angle = me->rotation_angle * 180.0f / DirectX::XM_PI;
        const auto wanted_angle = facing_angle > 0.0f ? facing_angle - 180.0f : facing_angle + 180.0f;
        constexpr auto max_angle_diff = 22.5f; // Acceptable angle for ebon escape
        constexpr auto max_distance = GW::Constants::SqrRange::Spellcast;
        float distance = 0.0f;

        const GW::Agent* closest = nullptr;
        for (const auto agent : *agents) {
            if (agent == me || !GW::Agents::GetAgentMatchesFlags(agent, AgentEETargetType)) {
                continue;
            }
            const float this_distance = GetSquareDistance(me->pos, agent->pos);
            if (this_distance > max_distance || distance > this_distance) {
                continue;
            }
            const auto agent_angle = GetAngle(me->pos - agent->pos);
            const auto this_angle_diff = abs(wanted_angle - agent_angle);
            if (this_angle_diff > max_angle_diff) {
                continue;
            }
            closest = agent;
            distance = this_distance;
        }
        if (closest) {
            SafeChangeTarget(closest->agent_id);
        }
    }

    bool IsNearestStr(const wchar_t* str)
    {
        return wcscmp(str, L"nearest") == 0 || wcscmp(str, L"closest") == 0;
    }


    typedef std::unordered_map<uint32_t, std::wstring> FlaggableHeroNames;
    void GetFlaggableHeroNames(std::function<void(FlaggableHeroNames*)> cb)
    {
        GW::WorldContext* w = GW::GetWorldContext();
        GW::HeroFlagArray* f = w ? &w->hero_flags : nullptr;
        if (!f) return cb(nullptr);
        auto names_out = new FlaggableHeroNames();
        struct DecodedParam {
            uint32_t agent_id = 0;
            FlaggableHeroNames* names_out;
            std::function<void(FlaggableHeroNames*)> cb;
            size_t hero_count;
        };

        auto decoded_cb = [](void* wparam, const wchar_t* decoded) {
            auto p = static_cast<DecodedParam*>(wparam);
            auto names_out = p->names_out;
            names_out->emplace(p->agent_id, TextUtils::ToLower(decoded));
            if (names_out->size() == p->hero_count) {
                GW::GameThread::Enqueue([names_out, cb = p->cb]() {
                    cb(names_out);
                    delete names_out;
                });
            }
            delete p;
        };
        for (const auto& flag : *f) {
            auto decoded_param = new DecodedParam{flag.agent_id, names_out, cb, f->size()};
            const auto name = GW::Agents::GetAgentEncName(flag.agent_id);
            GW::UI::AsyncDecodeStr(name, decoded_cb, decoded_param);
        }
    }

    struct DecodedTitleName {
        DecodedTitleName(const GW::Constants::TitleID in) : title(in)
        {
            const auto title_info = GW::PlayerMgr::GetTitleData(title);
            if (title_info) name.reset(title_info->name_id);
        };
        GW::Constants::TitleID title;
        GuiUtils::EncString name;
    };

    std::vector<DecodedTitleName*> title_names;
    bool title_names_sorted = false;

    GW::Array<GW::Title>* GetTitles()
    {
        const auto w = GW::GetWorldContext();
        return w ? &w->titles : nullptr;
    }

    GW::HookEntry createuicomponent_hook;

    GW::UI::UIInteractionCallback OnChatInteraction_Callback_Func = nullptr;
    GW::UI::UIInteractionCallback OnChatInteraction_Callback_Ret = nullptr;

    constexpr auto chat_tab_syntax = "'/chat [all|guild|team|trade|alliance|whisper]' 打开聊天频道。";
    constexpr auto dialog_syntax = "'/dialog [dialog_id]'（例如 '/dialog 0x184'）向当前对话的 NPC 发送对话 ID。\n"
                                   "'/dialog take' 自动从当前 NPC 接受第一个可用任务/奖励。";
    constexpr auto dropbuff_syntax = "'/dropbuff [skill_id]' 移除第一个维持技能/增益效果";
    constexpr auto dropitem_syntax = "'/dropitem <model_id> [quantity]' 从背包中丢弃匹配模型 ID 的物品。\n"
                                     "不指定数量则丢弃每个匹配的堆叠。";
    constexpr auto fps_syntax = "'/fps [limit (15-400)]' 设置 Guild Wars 的帧率限制。传入 '0' 移除限制。\n'/fps' 显示当前帧率限制";
    constexpr auto pref_syntax = "'/pref [preference] [number (0-4)]' 设置 Guild Wars 中的游戏内偏好设置。\n'/pref list' 列出可设置的偏好选项。";

    constexpr auto tb_syntax = "'/tb <名称>' 切换名为 <名称> 的窗口或小部件的显示状态。\n"
                               "'/tb save [配置]' 将当前工具箱设置保存到磁盘；如果指定 [配置]，则写入该配置，否则写入默认配置。\n"
                               "'/tb load [配置]' 从磁盘加载工具箱设置；如果指定 [配置]，则从该配置读取，否则从默认配置读取。\n"
                               "'/tb reset' 将工具箱和设置窗口移动到左上角。\n"
                               "'/tb quit' 或 '/tb exit' 完全关闭工具箱及其所有窗口。";

    constexpr auto withdraw_syntax = "'/withdraw [数量 (1-65535)] [model_id1 model_id2 ...]' 补货背包 "
                                     "至少 1 个或多个物品，通过 model_id 识别\n"
                                     "如果不传 model_ids，则从存储中提取 [数量][k] 金币\n"
                                     "如果数量为 'all' 且不传 model_ids，则提取所有可以持有或已拥有的金币。";
    constexpr auto deposit_syntax = "'/deposit [数量 (1-65535)] [model_id1 model_id2 ...]' 存入 [数量] 个物品，"
                                    "通过 model_id 识别，从背包存入存储。\n"
                                    "如果不传 model_ids，则从背包存入 [数量][k] 金币\n"
                                    "如果数量为 'all' 且不传 model_ids，则从背包中存入所有金币到存储。";

    constexpr auto CmdHeroBehaviour_syntax = "'/hero [avoid|guard|attack|target] [hero_index] [silent]' 在可探索区域中设置英雄行为或目标。\n"
                                             "如果不提供 hero_index，则调整所有英雄的行为。\n"
                                             "添加 'silent' 以抑制英雄的聊天消息。";

    constexpr auto disableheroskill_syntax = "'/disableheroskill <hero_index (1-7)> <slot (1-8)> [1|0]' 禁用、启用或切换英雄技能栏的技能槽。\n"
                                             "省略最后一个参数则切换当前状态。";

    constexpr auto target_syntax = "'/target closest' 定位离你最近的单位。\n"
                                   "'/target ee' 定位最佳的 \"黑檀逃脱\" 目标。\n"
                                   "'/target hos' 定位最佳的 \"毒蛇\" 目标。\n"
                                   "'/target [名称|model_id] [索引]' 按名称或模型 ID 定位最近的 NPC。\n   如果指定索引，则按 ID 定位第 index 个。\n"
                                   "'/target player [名称|player_number]' 按名称或玩家编号定位最近的玩家。\n"
                                   "'/target gadget [名称|gadget_id]' 按名称或 gadget_id 定位最近的交互对象。\n"
                                   "'/target priority [队伍成员]' 定位队伍成员的优先目标。";

    constexpr auto button_syntax = "'/button [button_label] [button_label...]' 例如 /button \"BtnBuy\" \"BtnAccept\" \"BtnOk\"\n"
                                   "如果您知道标签，可以通过此命令与屏幕上的 UI 按钮交互";

    constexpr auto useskill_syntax = "'/useskill [slot]' 开始按冷却使用技能。\n"
                                     "使用技能编号代替 [slot]（例如 '/useskill 5'）。\n"
                                     "使用 '/useskill [stop|off|slot|0]' 停止使用技能。";

    constexpr auto custommarker_syntax = "'/custommarker <x> <y>' 在世界地图坐标 (x, y) 放置自定义标记。\n"
                                         "'/custommarker clear' 移除自定义标记。";

    void CHAT_CMD_FUNC(CmdChatTab)
    {
        if (argc < 2) {
            return Log::Error(chat_tab_syntax);
        }
        uint32_t channel = 0xff;
        if (wcscmp(argv[1], L"all") == 0) {
            channel = 0;
        }
        else if (wcscmp(argv[1], L"guild") == 0) {
            channel = 2;
        }
        else if (wcscmp(argv[1], L"team") == 0) {
            channel = 3;
        }
        else if (wcscmp(argv[1], L"trade") == 0) {
            channel = 4;
        }
        else if (wcscmp(argv[1], L"alliance") == 0) {
            channel = 1;
        }
        else if (wcscmp(argv[1], L"whisper") == 0) {
            channel = 5;
        }
        else {
            return Log::Error(chat_tab_syntax);
        }
        channel |= 0x8000;
        GW::GameThread::Enqueue([channel] {
            // See OnChatUI_Callback for intercept
            SendUIMessage(GW::UI::UIMessage::kAppendMessageToChat, (void*)L"", (void*)channel);
        });
    }

    void CHAT_CMD_FUNC(CmdDuncan)
    {
        std::wstring out_message = std::format(L"{}\x2\x108\x107状态：\x1", GW::EncStrings::Quest::TheLastHeirophant);
        if (!GW::QuestMgr::GetQuest(GW::Constants::QuestID::The_Last_Hierophant)) {
            out_message += L"\x108\x107我没有这个任务！\x1";
        }
        else {
            const auto objectives = QuestModule::ParseQuestObjectives(GW::Constants::QuestID::The_Last_Hierophant);
            const wchar_t* objective_names[] = {L"Thommis", L"Rand", L"Selvetarm", L"Forgewight", L"Duncan"};
            for (size_t i = 0; i < _countof(objective_names); i++) {
                const wchar_t completed_mark = i < objectives.size() && objectives[i].is_completed ? L'\x2705' : ' ';
                const wchar_t* append_mark = i > 0 ? L", " : L"";
                out_message += std::format(L"\x2\x108\x107{}{} [{}]\x1", append_mark, objective_names[i], completed_mark);
            }
        }
        GW::UI::AsyncDecodeStr(
            out_message.c_str(),
            [](void*, const wchar_t* s) {
                GW::Chat::SendChat('#', s);
            },
            nullptr, GW::Constants::Language::English
        );
    }

    using FocusChatTab_pt = void(__fastcall*)(void* chat_frame, void* edx, uint32_t tab);
    FocusChatTab_pt FocusChatTab_Func = nullptr;

    void OnChatUI_Callback(GW::UI::InteractionMessage* message, void* wParam, void* lParam)
    {
        GW::Hook::EnterHook();
        // If a channel was given in the UI message, set it now.
        if (message->message_id == GW::UI::UIMessage::kAppendMessageToChat && lParam) {
            const auto frame = GW::UI::GetFrameById(message->frame_id);
            uint32_t control_action = 0xff;
            // Map tab number > key for the ui message
            switch ((uint32_t)lParam ^ 0x8000) {
                case 0:
                    control_action = 0x31;
                    break;
                case 1:
                    control_action = 0x35;
                    break;
                case 2:
                    control_action = 0x32;
                    break;
                case 3:
                    control_action = 0x33;
                    break;
                case 4:
                    control_action = 0x34;
                    break;
                case 5:
                    control_action = 3;
                    break;
            }
            if (frame && control_action != 0xff) {
                GW::UI::Keydown((GW::UI::ControlAction)control_action, frame);
                GW::Hook::LeaveHook();
                return;
            }
        }

        OnChatInteraction_Callback_Ret(message, wParam, lParam);
        GW::Hook::LeaveHook();
    }

    bool* is_muted = nullptr;

    void CHAT_CMD_FUNC(CmdDropBuff)
    {
        if (argc < 2) {
            Log::Warning(dropbuff_syntax);
            return;
        }
        uint32_t skill_id = 0;
        if (!TextUtils::ParseUInt(argv[1], &skill_id)) {
            Log::Warning(dropbuff_syntax);
            return;
        }
        const auto skill = GW::SkillbarMgr::GetSkillConstantData((GW::Constants::SkillID)skill_id);
        if (!skill) {
            Log::Warning(dropbuff_syntax);
            return;
        }
        const auto buff = GW::Effects::GetPlayerBuffBySkillId(skill->skill_id);
        if (!buff) return;
        if (!GW::Effects::DropBuff(buff->buff_id)) {
            Log::Warning("移除增益失败！");
            return;
        }
    }

    void CHAT_CMD_FUNC(CmdDropItem)
    {
        if (!IsMapReady()) {
            return;
        }
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
            Log::Warning("只能在可探索区域丢弃物品");
            return;
        }
        uint32_t model_id = 0;
        if (argc < 2 || !TextUtils::ParseUInt(argv[1], &model_id) || !model_id) {
            Log::Warning(dropitem_syntax);
            return;
        }
        uint32_t quantity = 0; // 0 == drop every matching stack
        if (argc >= 3 && (!TextUtils::ParseUInt(argv[2], &quantity) || quantity == 0 || quantity > 0xFFFF)) {
            Log::Warning(dropitem_syntax);
            return;
        }
        const auto is_droppable = [model_id](const InventoryManager::Item* item) {
            return item && item->model_id == model_id && !item->customized;
        };
        const auto items = InventoryManager::FindItemsBy(GW::Constants::Bag::Backpack, GW::Constants::Bag::Bag_2, is_droppable);
        uint16_t remaining = static_cast<uint16_t>(quantity);
        uint16_t dropped = 0;
        for (const auto item : items) {
            const uint16_t to_drop = quantity ? std::min<uint16_t>(item->quantity, remaining) : item->quantity;
            if (!GW::Items::DropItem(item, to_drop)) {
                continue;
            }
            dropped += to_drop;
            if (quantity) {
                remaining -= to_drop;
                if (remaining < 1) {
                    break;
                }
            }
        }
        if (!dropped) {
            Log::Warning("在背包中未找到模型 ID %u 的可丢弃物品", model_id);
            return;
        }
    }

    HallOfMonumentsAchievements hom_achievements;
    bool hom_loading = false;

    void OnAchievementsLoaded(HallOfMonumentsAchievements* result)
    {
        hom_loading = false;
        result->OpenInBrowser();
    }


    void CHAT_CMD_FUNC(CmdFps)
    {
        if (argc < 2) {
            const auto current_limit = GW::Render::GetFrameLimit();
            if (!current_limit) {
                Log::Flash("未设置帧率限制");
            }
            else {
                Log::Flash("帧率限制设置为 %d FPS", current_limit);
            }
        }
        uint32_t frame_limit = 0;
        if (!TextUtils::ParseUInt(argv[1], &frame_limit)) {
            return Log::Error(fps_syntax);
        }
        if (frame_limit && frame_limit < 15) {
            frame_limit = 15;
        }
        if (frame_limit && frame_limit > 400) {
            frame_limit = 400;
        }
        GW::Render::SetFrameLimit(frame_limit);
    }

    using CmdPrefCB = void(__cdecl*)(const wchar_t*, int argc, const LPWSTR* argv, uint32_t pref_id);

    // ReSharper disable once CppParameterMayBeConst
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void CmdValuePref(const wchar_t*, int argc, const LPWSTR* argv, uint32_t pref_id)
    {
        const auto pref = static_cast<GW::UI::NumberPreference>(pref_id);

        uint32_t value = 0xff;
        if (argc > 2 && TextUtils::ParseUInt(argv[2], &value)) {
            GW::GameThread::Enqueue([pref, value, pref_str = std::wstring(argv[1])] {
                if (!GW::UI::SetPreference(pref, value)) {
                    Log::ErrorW(L"设置偏好 %s 为 %d 失败", pref_str.c_str(), value);
                }
            });
            return;
        }

        if (argc < 3) {
            Log::InfoW(L"%s 的当前偏好值为 %d", argv[1], GetPreference(pref));
        }
        return Log::Error(pref_syntax);
    }

    // ReSharper disable once CppParameterMayBeConst
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void CmdEnumPref(const wchar_t*, int argc, const LPWSTR* argv, uint32_t pref_id)
    {
        const auto pref = static_cast<GW::UI::EnumPreference>(pref_id);

        uint32_t value = 0xff;
        if (argc > 2 && TextUtils::ParseUInt(argv[2], &value)) {
            GW::GameThread::Enqueue([pref, value, pref_str = std::wstring(argv[1])] {
                if (!GW::UI::SetPreference(pref, value)) {
                    Log::ErrorW(L"设置偏好 %s 为 %d 失败", pref_str.c_str(), value);
                }
            });
            return;
        }

        if (argc < 3) {
            Log::InfoW(L"%s 的当前偏好值为 %d", argv[1], GetPreference(pref));
        }

        uint32_t* values = nullptr;
        const auto available = GetPreferenceOptions(pref, &values);
        wchar_t available_vals_buffer[120];
        uint32_t offset = 0;
        offset += swprintf(&available_vals_buffer[offset], offset - _countof(available_vals_buffer), L"%s 的可用值：", argv[1]);
        for (size_t i = 0; i < available; i++) {
            offset += swprintf(&available_vals_buffer[offset], offset - _countof(available_vals_buffer), i > 0 ? L", %d" : L"%d", values[i]);
        }
        Log::InfoW(available_vals_buffer);
    }

    // ReSharper disable once CppParameterMayBeConst
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void CmdFlagPref(const wchar_t*, int argc, const LPWSTR* argv, uint32_t pref_id)
    {
        const auto pref = static_cast<GW::UI::FlagPreference>(pref_id);

        if (argc > 2) {
            if (wcscmp(argv[2], L"toggle") == 0) {
                SetPreference(pref, !GetPreference(pref));
                return;
            }
            uint32_t value = 0xff;
            if (TextUtils::ParseUInt(argv[2], &value)) {
                SetPreference(pref, value == 1 ? 1 : 0);
            }
            return;
        }
        Log::InfoW(L"Current preference value for %s is %d", argv[1], GetPreference(pref));
    }

    // Reduce a preference name to a comparable slug so user input matches the label regardless of case, spacing or punctuation.
    std::wstring SanitisePrefName(std::wstring s)
    {
        return TextUtils::RemovePunctuation(TextUtils::RemoveDiacritics(TextUtils::ToSlug(std::move(s))));
    }

    std::unique_ptr<GuiUtils::EncString> MakePrefLabel(uint32_t enc_string_id)
    {
        auto label = std::make_unique<GuiUtils::EncString>(enc_string_id, true);
        label->language(GW::Constants::Language::English);
        label->SetSanitiseCallback(SanitisePrefName);
        return label;
    }

    std::unique_ptr<GuiUtils::EncString> MakePrefLabel(const wchar_t* enc_string)
    {
        auto label = std::make_unique<GuiUtils::EncString>(enc_string, true);
        label->language(GW::Constants::Language::English);
        label->SetSanitiseCallback(SanitisePrefName);
        return label;
    }

    struct PrefMapCommand {
        PrefMapCommand(GW::UI::EnumPreference p, uint32_t enc_string_id) : preference_id(std::to_underlying(p)), preference_callback(CmdEnumPref), label(MakePrefLabel(enc_string_id)) {}

        PrefMapCommand(GW::UI::NumberPreference p, uint32_t enc_string_id) : preference_id(std::to_underlying(p)), preference_callback(CmdValuePref), label(MakePrefLabel(enc_string_id)) {}

        PrefMapCommand(GW::UI::FlagPreference p, uint32_t enc_string_id) : preference_id(std::to_underlying(p)), preference_callback(CmdFlagPref), label(MakePrefLabel(enc_string_id)) {}

        PrefMapCommand(GW::UI::FlagPreference p, const wchar_t* enc_string_id) : preference_id(std::to_underlying(p)), preference_callback(CmdFlagPref), label(MakePrefLabel(enc_string_id)) {}

        uint32_t preference_id;
        CmdPrefCB preference_callback;
        std::unique_ptr<GuiUtils::EncString> label;
    };

    using PrefMap = std::vector<PrefMapCommand>;
    PrefMap pref_map;

    const PrefMap& getPrefCommandOptions()
    {
        if (pref_map.empty()) {
            pref_map.emplace_back(GW::UI::FlagPreference::WaitForVSync, GW::EncStrings::VerticalSync);
            pref_map.emplace_back(GW::UI::NumberPreference::FullscreenGamma, GW::EncStrings::FullScreenGamma);
            pref_map.emplace_back(GW::UI::EnumPreference::AntiAliasing, GW::EncStrings::AntiAliasing);
            pref_map.emplace_back(GW::UI::EnumPreference::ShaderQuality, GW::EncStrings::ShaderQuality);
            pref_map.emplace_back(GW::UI::EnumPreference::TerrainQuality, GW::EncStrings::TerrainQuality);
            pref_map.emplace_back(GW::UI::EnumPreference::Reflections, GW::EncStrings::Reflections);
            pref_map.emplace_back(GW::UI::EnumPreference::ShadowQuality, GW::EncStrings::ShadowQuality);
            pref_map.emplace_back(GW::UI::EnumPreference::InterfaceSize, GW::EncStrings::InterfaceSize);
            pref_map.emplace_back(GW::UI::NumberPreference::TextureLod, GW::EncStrings::TextureQuality);
            pref_map.emplace_back(GW::UI::NumberPreference::Language, GW::EncStrings::TextLanguage);
            pref_map.emplace_back(GW::UI::NumberPreference::LanguageAudio, GW::EncStrings::AudioLanguage);
            pref_map.emplace_back(GW::UI::NumberPreference::ClockMode, GW::EncStrings::InGameClock);
            pref_map.emplace_back(GW::UI::FlagPreference::ChannelAlliance, GW::EncStrings::ChannelAlliance);
            pref_map.emplace_back(GW::UI::FlagPreference::ChannelGuild, GW::EncStrings::ChannelGuild);
            pref_map.emplace_back(GW::UI::FlagPreference::ChannelGroup, GW::EncStrings::ChannelTeam);
            pref_map.emplace_back(GW::UI::FlagPreference::ChannelEmotes, GW::EncStrings::ChannelEmotes);
            pref_map.emplace_back(GW::UI::FlagPreference::ChannelTrade, GW::EncStrings::ChannelTrade);
            pref_map.emplace_back(GW::UI::NumberPreference::VolMaster, GW::EncStrings::MasterVolume);
            pref_map.emplace_back(GW::UI::NumberPreference::VolMusic, GW::EncStrings::MusicVolume);
            pref_map.emplace_back(GW::UI::FlagPreference::DisableMouseWalking, GW::EncStrings::DisableMouseWalking);
            pref_map.emplace_back(GW::UI::FlagPreference::AlwaysShowFoeNames, L"\x108\x107显示敌人名称\x1");
            pref_map.emplace_back(GW::UI::FlagPreference::AlwaysShowAllyNames, L"\x108\x107显示友方名称\x1");
            pref_map.emplace_back(
                GW::UI::FlagPreference::EnableGamepad, L"\x108\x107"
                                                       "启用游戏手柄\x1"
            );
            pref_map.emplace_back(GW::UI::FlagPreference::LegacyStartMissionButton, GW::EncStrings::LegacyStartMissionButton);
            pref_map.emplace_back(GW::UI::FlagPreference::EnableMobileHUD, GW::EncStrings::EnableMobileHUD);
            pref_map.emplace_back(GW::UI::NumberPreference::ScreenBorderless, GW::EncStrings::Resolution);
            for (const auto& it : pref_map) {
                it.label->wstring();
            }
        }
        return pref_map;
    };


    void CHAT_CMD_FUNC(CmdPref)
    {
        const auto& options = getPrefCommandOptions();
        if (argc < 2) {
            return Log::Error(pref_syntax);
        }
        if (wcscmp(argv[1], L"list") == 0) {
            std::wstring buffer;

            for (auto& option : options) {
                if (!buffer.empty()) buffer += L", ";
                buffer += option.label->wstring();
            }
            return Log::InfoW(L"/pref 选项：\n%s", buffer.c_str());
        }

        // Match leniently: slug both sides so the input accepts any case/spacing/punctuation
        // and matches whether the label has resolved to its slug yet or still reads as its decoded text.
        const auto requested = SanitisePrefName(argv[1]);
        const auto found = std::ranges::find_if(options, [&requested](const PrefMapCommand& cmd) {
            return SanitisePrefName(cmd.label->wstring()) == requested;
        });
        if (found == options.end()) {
            return Log::ErrorW(L"未知偏好 \"%s\"。输入 '/pref list' 查看可设置的偏好选项。", argv[1]);
        }
        const PrefMapCommand* pref = &(*found);

        pref->preference_callback(message, argc, argv, pref->preference_id);
    }


    struct CmdAlias {
        char alias_cstr[256] = {};
        wchar_t alias_wstr[128] = {};
        char command_cstr[512] = {};
        wchar_t command_wstr[256] = {};
        bool processing = false;
    };

    std::vector<CmdAlias*> cmd_aliases;

    ChatCommands::Settings settings;

    void sort_cmd_aliases()
    {
        std::ranges::stable_sort(cmd_aliases, [](const auto* a, const auto* b) {
            if (a->alias_cstr[0] == '\0' && b->alias_cstr[0] != '\0') {
                return false;
            }
            if (b->alias_cstr[0] == '\0' && a->alias_cstr[0] != '\0') {
                return true;
            }
            return strcmp(a->alias_cstr, b->alias_cstr) < 0;
        });
    }

    GW::HookEntry OnSentChat_HookEntry;
    GW::HookEntry ChatCmd_HookEntry;

    void OnSendChat(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void*)
    {
        ASSERT(message_id == GW::UI::UIMessage::kSendChatMessage);
        const auto message = static_cast<GW::UI::UIPacket::kSendChatMessage*>(wparam)->message;
        if (!(message && *message)) return;
        const auto channel = GW::Chat::GetChannel(*message);
        if (channel != GW::Chat::CHANNEL_COMMAND || status->blocked) {
            return;
        }
        for (const auto alias : cmd_aliases) {
            const auto sent_alias = TextUtils::ToLower(&message[1]);
            if (wcscmp(alias->alias_wstr, sent_alias.c_str()) == 0 && !alias->processing && wcslen(alias->command_wstr) > 1) {
                status->blocked = true;
                alias->processing = true;
                std::wstring tmp;
                std::vector<std::wstring> parts;
                std::wstringstream wss(alias->command_wstr);
                while (std::getline(wss, tmp, L'\n')) {
                    if (tmp.length() < 2) continue;
                    GW::Chat::SendChat((char)tmp[0], &tmp[1]);
                }
                alias->processing = false;
            }
        }
    }


    void TargetNearest(const wchar_t* model_id_or_name, const GW::AgentTargetFlags type)
    {
        uint32_t model_id = 0;
        uint32_t index = 0; // 0=nearest. 1=first by id, 2=second by id, etc.

        // Searching by name; offload this to decode agent names first.
        if (TextUtils::ParseUInt(model_id_or_name, &model_id)) {
            if (const wchar_t* rest = GetRemainingArgsWstr(model_id_or_name, 1)) {
                TextUtils::ParseUInt(rest, &index);
            }
        }
        else {
            if (!IsNearestStr(model_id_or_name)) {
                npc_to_find.Add(model_id_or_name, type);
                return;
            }
        }

        const auto agents = GW::Agents::GetAgentArray();
        const auto me = agents ? GW::Agents::GetControlledCharacter() : nullptr;
        if (me == nullptr) {
            return;
        }

        float distance = GW::Constants::SqrRange::Compass;
        size_t closest = 0;
        size_t count = 0;

        for (const GW::Agent* agent : *agents) {
            if (agent == me || !GW::Agents::GetAgentMatchesFlags(agent, type)) continue;
            if (model_id && GetAgentModelId(agent) != model_id) continue;
            if (index == 0) {
                const float new_distance = GetSquareDistance(me->pos, agent->pos);
                if (new_distance < distance) {
                    closest = agent->agent_id;
                    distance = new_distance;
                }
            }
            else {
                ++count;
                if (count == index) {
                    closest = agent->agent_id;
                    break;
                }
            }
        }
        if (closest) {
            SafeChangeTarget(closest);
        }
    }


    void CHAT_CMD_FUNC(CmdTick)
    {
        GW::PartyMgr::Tick(!GW::PartyMgr::GetIsPlayerTicked());
    }

    void CHAT_CMD_FUNC(CmdCallTarget)
    {
        const auto* target = GW::Agents::GetTarget();
        if (!target) return;
        auto call_packet = GW::UI::UIPacket::kSendCallTarget{.call_type = GW::CallTargetType::AttackingOrTargetting, .agent_id = target->agent_id};
        GW::UI::SendUIMessage(GW::UI::UIMessage::kSendCallTarget, &call_packet);
    }

    void CHAT_CMD_FUNC(CmdHeroBehaviour)
    {
        GW::WorldContext* w = GW::GetWorldContext();
        GW::HeroFlagArray* flags = w ? &w->hero_flags : nullptr;
        if (!flags) return;
        if (argc < 2) {
            return Log::Warning(CmdHeroBehaviour_syntax);
        }

        // Check if last argument is "silent" - suppress hero behavior chat messages
        int effective_argc = argc;
        if (argc >= 2 && TextUtils::ToLower(argv[argc - 1]) == L"silent") {
            constexpr clock_t SUPPRESS_MS = 1000;
            ChatFilter::BlockMessageForMs(GW::EncStrings::HeroBehavior::Fight, SUPPRESS_MS);
            ChatFilter::BlockMessageForMs(GW::EncStrings::HeroBehavior::Guard, SUPPRESS_MS);
            ChatFilter::BlockMessageForMs(GW::EncStrings::HeroBehavior::Avoid, SUPPRESS_MS);
            effective_argc--;
        }

        auto behaviour = 0xff;
        const std::wstring arg1 = TextUtils::ToLower(argv[1]);
        if (arg1 == L"avoid") {
            behaviour = (uint32_t)GW::HeroBehavior::AvoidCombat; // avoid combat
        }
        else if (arg1 == L"guard") {
            behaviour = (uint32_t)GW::HeroBehavior::Guard; // guard
        }
        else if (arg1 == L"attack") {
            behaviour = (uint32_t)GW::HeroBehavior::Fight; // attack
        }
        else if (arg1 == L"target") {
            behaviour = 0xff; // target
        }
        else {
            return Log::Warning(CmdHeroBehaviour_syntax);
        }

        auto flag_hero = [behaviour](uint32_t agent_id) {
            if (behaviour == 0xff) {
                if (!GW::Agents::IsAgentCarryingBundle(agent_id)) GW::PartyMgr::SetHeroTarget(agent_id, GW::Agents::GetTargetId());
            }
            return GW::PartyMgr::SetHeroBehavior(agent_id, (GW::HeroBehavior)behaviour);
        };

        if (effective_argc < 3) {
            for (const auto& flag : *flags) {
                flag_hero(flag.agent_id);
            }
            return;
        }
        std::wstring hero_name = argv[2];
        size_t hero_index = 0; // This is 1 based!
        if (TextUtils::ParseUInt(hero_name.c_str(), &hero_index)) {
            if (hero_index < 1 || hero_index > flags->size()) {
                Log::LogW(L"未找到英雄 %d", hero_index);
                return;
            }
            size_t out_index = 0;
            for (const auto& flag : *flags) {
                const auto hero_id = static_cast<GW::Constants::HeroID>(flag.hero_id);
                HeroBuildsWindow::GetPartyHeroByID(hero_id, &out_index);
                if (out_index == hero_index) {
                    flag_hero(flag.agent_id);
                    return;
                }
            }
            return;
        }
        GetFlaggableHeroNames([hero_name, flag_hero](FlaggableHeroNames* hero_names) {
            bool flagged = false;
            if (hero_names) {
                for (const auto& [agent_id, name] : *hero_names) {
                    if (name.starts_with(hero_name)) {
                        flag_hero(agent_id);
                        flagged = true;
                    }
                }
            }
            if (!flagged) {
                Log::LogW(L"未找到英雄 %s", hero_name.c_str());
            }
        });
    }

    void CHAT_CMD_FUNC(CmdDisableHeroSkill)
    {
        uint32_t hero_index = 0, slot_index = 0;
        if (argc < 3 || !TextUtils::ParseUInt(argv[1], &hero_index) || hero_index < 1 || hero_index > 7 || !TextUtils::ParseUInt(argv[2], &slot_index) || slot_index < 1 || slot_index > 8) return Log::Warning(disableheroskill_syntax);
        const auto agent_id = GW::Agents::GetHeroAgentID(hero_index);
        if (!agent_id) return Log::Warning(disableheroskill_syntax);
        bool disabled;
        if (argc >= 4) {
            uint32_t flag = 0;
            if (!TextUtils::ParseUInt(argv[3], &flag) || flag > 1) return Log::Warning(disableheroskill_syntax);
            disabled = flag != 0;
        }
        else {
            const auto skillbar = GW::SkillbarMgr::GetSkillbar(agent_id);
            disabled = skillbar ? !((skillbar->disabled >> (slot_index - 1)) & 1) : true;
        }
        GW::PartyMgr::SetHeroSkillDisabled(agent_id, slot_index - 1, disabled);
    }

    const GW::AgentTargetFlags AnyLivingNpc = GW::TargetFilter::AnyLiving & ~GW::AgentTargetFlags::Accept_Player;

    static const std::unordered_map<std::wstring, GW::AgentTargetFlags> target_filters = {
        {L"item", GW::TargetFilter::Items}, {L"npc", AnyLivingNpc}, {L"gadget", GW::TargetFilter::Gadgets}, {L"player", GW::AgentTargetFlags::Accept_Player}, {L"ally", GW::TargetFilter::Allies}, {L"enemy", GW::TargetFilter::Enemies},
    };



    void CHAT_CMD_FUNC(CmdTarget)
    {
        if (argc < 2) {
            return Log::Warning(target_syntax);
        }
        const auto zero_w = L"0";
        const std::wstring arg1 = TextUtils::ToLower(argv[1]);
        if (arg1 == L"ee") return TargetEE();
        if (arg1 == L"vipers" || arg1 == L"hos") return TargetVipers();

        const bool is_nearest = IsNearestStr(arg1.c_str());
        const int name_arg = is_nearest ? 3 : 2;
        const auto name_w = [&] {
            return argc > name_arg ? GetRemainingArgsWstr(message, name_arg) : zero_w;
        };
        const std::wstring arg2 = is_nearest && argc > 2 ? TextUtils::ToLower(argv[2]) : L"npc";
        const std::wstring& cmd = is_nearest ? arg2 : arg1;

        const auto filter_it = target_filters.find(cmd);
        if (filter_it != target_filters.end()) return TargetNearest(name_w(), filter_it->second);

        if (arg1 == L"getid") {
            const auto target = GW::Agents::GetTargetAsAgentLiving();
            if (!target) return Log::Error("未选择目标！");
            return Log::Info("目标模型 ID（PlayerNumber）为 %d", target->player_number);
        }
        if (arg1 == L"getpos") {
            const auto target = GW::Agents::GetTargetAsAgentLiving();
            if (!target) return Log::Error("未选择目标！");
            return Log::Info("目标坐标为 (%.2f, %.2f)", target->pos.x, target->pos.y);
        }
        if (arg1 == L"priority") {
            const GW::PartyInfo* party = GW::PartyMgr::GetPartyInfo();
            if (!party || !party->players.valid()) return;
            uint32_t calledTargetId = 0;
            if (argc == 2) {
                const GW::AgentLiving* me = GW::Agents::GetControlledCharacter();
                if (!me) return;
                for (const auto& player : party->players) {
                    if (player.login_number == me->login_number) {
                        calledTargetId = player.calledTargetId;
                        break;
                    }
                }
            }
            else {
                uint32_t partyMemberNumber = 0;
                uint32_t partySize = party->players.size();
                if (party->heroes.valid()) partySize += party->heroes.size();
                if (!TextUtils::ParseUInt(argv[2], &partyMemberNumber) || partyMemberNumber == 0 || partyMemberNumber > partySize) {
                    return Log::Error("无效参数 '%ls'，请输入 1 到 %u 之间的整数", argv[2], partySize);
                }
                uint32_t count = 0;
                for (const GW::PlayerPartyMember& player : party->players) {
                    if (++count == partyMemberNumber) {
                        calledTargetId = player.calledTargetId;
                        break;
                    }
                    for (const GW::HeroPartyMember& hero : party->heroes) {
                        if (hero.owner_player_id == player.login_number && ++count >= partyMemberNumber) return;
                    }
                }
            }
            if (!calledTargetId) return;
            const GW::Agent* agent = GW::Agents::GetAgentByID(calledTargetId);
            if (!agent) return;
            return SafeChangeTarget(agent->agent_id);
        }

        if (is_nearest) return TargetNearest(arg2.c_str(), AnyLivingNpc);
        return TargetNearest(GetRemainingArgsWstr(message, 1), AnyLivingNpc);
    }



    void CHAT_CMD_FUNC(CmdButtonPress)
    {
        if (argc < 2) {
            Log::Warning(button_syntax);
            return;
        }
        for (int i = 1; i < argc; i++) {
            std::wstring label = argv[i];
            GW::GameThread::Enqueue([cpy = label]() {
                GW::UI::ButtonClick(GW::UI::GetFrameByLabel(cpy.c_str()));
            });
        }
    }

    std::vector<std::pair<const wchar_t*, GW::Chat::ChatCommandCallback>> chat_commands;

    const wchar_t* settings_via_chat_commands_cmd = L"tb_setting";

    std::string SlugifySection(const std::string_view section)
    {
        auto slug = TextUtils::ToLower(std::string(section));
        std::ranges::replace(slug, ' ', '_');
        return slug;
    }

    std::string SettingSlug(const SettingsRegistry::Entry& entry)
    {
        return SlugifySection(entry.section) + "." + TextUtils::ToLower(entry.key);
    }

    const char* SettingValueSyntax(const SettingsRegistry::Type type)
    {
        switch (type) {
            case SettingsRegistry::Type::Bool:
                return "[on|off|toggle]";
            case SettingsRegistry::Type::Int:
            case SettingsRegistry::Type::Uint:
            case SettingsRegistry::Type::Float:
                return "<数字>";
            case SettingsRegistry::Type::Color:
                return "<0xAARRGGBB>";
            case SettingsRegistry::Type::Float2:
                return "<x> <y>";
            default:
                return "<文本>";
        }
    }

    std::string SettingValueToString(const SettingsRegistry::Entry& entry)
    {
        switch (entry.type) {
            case SettingsRegistry::Type::Bool:
                return *static_cast<bool*>(entry.ptr) ? "on" : "off";
            case SettingsRegistry::Type::Int:
                return std::to_string(*static_cast<int*>(entry.ptr));
            case SettingsRegistry::Type::Uint:
                return std::to_string(*static_cast<unsigned int*>(entry.ptr));
            case SettingsRegistry::Type::Float:
                return std::format("{}", *static_cast<float*>(entry.ptr));
            case SettingsRegistry::Type::Color:
                return std::format("0x{:X}", *static_cast<Color*>(entry.ptr));
            case SettingsRegistry::Type::Float2: {
                const auto& arr = *static_cast<std::array<float, 2>*>(entry.ptr);
                return std::format("{} {}", arr[0], arr[1]);
            }
            case SettingsRegistry::Type::String:
                return *static_cast<std::string*>(entry.ptr);
        }
        return "";
    }

    const SettingsRegistry::Entry* ResolveSettingEntry(const std::string& arg_lower)
    {
        const auto& entries = SettingsRegistry::GetEntries();
        const SettingsRegistry::Entry* key_match = nullptr;
        size_t key_match_count = 0;
        std::vector<const SettingsRegistry::Entry*> partial_matches;
        for (const auto& entry : entries) {
            const auto slug = SettingSlug(entry);
            if (slug == arg_lower) {
                return &entry;
            }
            if (TextUtils::ToLower(entry.key) == arg_lower) {
                key_match = &entry;
                key_match_count++;
            }
            if (slug.find(arg_lower) != std::string::npos) {
                partial_matches.push_back(&entry);
            }
        }
        if (key_match_count == 1) {
            return key_match;
        }
        if (partial_matches.size() == 1) {
            return partial_matches.front();
        }
        if (partial_matches.empty()) {
            Log::Warning("未找到匹配 '%s' 的设置", arg_lower.c_str());
            return nullptr;
        }
        Log::Warning("'%s' 匹配了 %d 个设置：", arg_lower.c_str(), static_cast<int>(partial_matches.size()));
        for (size_t i = 0; i < partial_matches.size() && i < 10; i++) {
            Log::Warning("  %s", SettingSlug(*partial_matches[i]).c_str());
        }
        return nullptr;
    }

    // Parses a single chat-command token into the entry's live value.
    bool SettingValueFromString(const SettingsRegistry::Entry& entry, const std::wstring& value)
    {
        switch (entry.type) {
            case SettingsRegistry::Type::Bool: {
                auto& val = *static_cast<bool*>(entry.ptr);
                const auto lower = TextUtils::ToLower(value);
                if (lower == L"on" || lower == L"1" || lower == L"true") {
                    val = true;
                }
                else if (lower == L"off" || lower == L"0" || lower == L"false") {
                    val = false;
                }
                else if (lower == L"toggle") {
                    val = !val;
                }
                else {
                    return false;
                }
                return true;
            }
            case SettingsRegistry::Type::Int: {
                int parsed;
                if (!TextUtils::ParseInt(value.c_str(), &parsed)) {
                    return false;
                }
                *static_cast<int*>(entry.ptr) = parsed;
                return true;
            }
            case SettingsRegistry::Type::Uint: {
                unsigned int parsed;
                if (!TextUtils::ParseUInt(value.c_str(), &parsed)) {
                    return false;
                }
                *static_cast<unsigned int*>(entry.ptr) = parsed;
                return true;
            }
            case SettingsRegistry::Type::Float: {
                float parsed;
                if (!TextUtils::ParseFloat(value.c_str(), &parsed)) {
                    return false;
                }
                *static_cast<float*>(entry.ptr) = parsed;
                return true;
            }
            case SettingsRegistry::Type::Color: {
                unsigned int parsed;
                if (!TextUtils::ParseUInt(value.c_str(), &parsed, 16)) {
                    return false;
                }
                *static_cast<Color*>(entry.ptr) = parsed;
                return true;
            }
            case SettingsRegistry::Type::String:
                *static_cast<std::string*>(entry.ptr) = TextUtils::WStringToString(value);
                return true;
            default:
                return false; // Float2 isn't expressible as a single token
        }
    }

    void CHAT_CMD_FUNC(CmdSettingViaChatCommand)
    {
        if (argc < 2) {
            Log::Warning("语法：'/tb_setting <名称> [值]'");
            return;
        }
        const auto entry = ResolveSettingEntry(TextUtils::ToLower(TextUtils::WStringToString(argv[1])));
        if (!entry) {
            return;
        }
        const auto slug = SettingSlug(*entry);
        if (argc < 3 && entry->type == SettingsRegistry::Type::Bool) {
            auto& val = *static_cast<bool*>(entry->ptr);
            val = !val;
        }
        else if (argc > 2) {
            bool ok;
            if (entry->type == SettingsRegistry::Type::Float2) {
                float x = 0.f, y = 0.f;
                ok = argc > 3 && TextUtils::ParseFloat(argv[2], &x) && TextUtils::ParseFloat(argv[3], &y);
                if (ok) {
                    *static_cast<std::array<float, 2>*>(entry->ptr) = {x, y};
                }
            }
            else {
                // String values may span multiple args; every other type is a single token.
                ok = SettingValueFromString(*entry, entry->type == SettingsRegistry::Type::String ? GetRemainingArgsWstr(message, 2) : argv[2]);
            }
            if (!ok) {
                Log::Warning("语法：'/tb_setting %s %s'", slug.c_str(), SettingValueSyntax(entry->type));
                return;
            }
        }
        Log::Info("%s = %s", slug.c_str(), SettingValueToString(*entry).c_str());
    }

    const SettingsRegistry::Entry* FindSettingEntry(const std::string& section_slug, const std::string& key_lower)
    {
        for (const auto& entry : SettingsRegistry::GetEntries()) {
            if (TextUtils::ToLower(entry.key) == key_lower && SlugifySection(entry.section) == section_slug) {
                return &entry;
            }
        }
        return nullptr;
    }

    void CHAT_CMD_FUNC(CmdConfig)
    {
        const char* syntax = "/config set|get|toggle|load [section key [value]]...";
        if (argc < 4) {
            Log::Error(syntax);
            return;
        }
        enum ActionType : uint8_t { Set, Get, Toggle, Load } action = Set;

        const auto arg1 = TextUtils::ToLower(argv[1]);
        if (arg1 == L"set") {
            action = Set;
        }
        else if (arg1 == L"get") {
            action = Get;
        }
        else if (arg1 == L"toggle") {
            action = Toggle;
        }
        else if (arg1 == L"load") {
            action = Load;
        }
        else {
            Log::Error(syntax);
            return;
        }
        // make sure the loop will not run out of arguments mid tuple
        switch (action) {
            case Set:
            case Toggle:
                if (argc % 3 != 2) {
                    Log::Error(syntax);
                    return;
                }
                break;
            case Get:
            case Load:
                if (argc % 2 != 0) {
                    Log::Error(syntax);
                    return;
                }
                break;
        }

        const auto doc = GWToolbox::GetSettingsDoc();
        for (int i = 2; i < argc;) {
            const auto section = SlugifySection(TextUtils::WStringToString(argv[i]));
            i++;

            ASSERT(i < argc);
            const auto key = TextUtils::ToLower(TextUtils::WStringToString(argv[i]));
            i++;

            std::wstring value;
            if (action == Set || action == Toggle) {
                ASSERT(i < argc);
                value = argv[i];
                i++;
            }
            const auto entry = FindSettingEntry(section, key);
            if (!entry) {
                Log::Warning("忽略未知设置 '%s %s'", section.c_str(), key.c_str());
                continue;
            }
            switch (action) {
                case Set:
                    if (!SettingValueFromString(*entry, value)) {
                        Log::Warning("'%s' 的值无效，期望 %s", SettingSlug(*entry).c_str(), SettingValueSyntax(entry->type));
                        continue;
                    }
                    break;
                case Get:
                    break;
                case Toggle: {
                    // Toggle between the supplied value and the value last saved to disk
                    const auto before = SettingValueToString(*entry);
                    SettingsRegistry::LoadEntryFromDoc(*entry, *doc);
                    if (SettingValueToString(*entry) == before && !SettingValueFromString(*entry, value)) {
                        Log::Warning("'%s' 的值无效，期望 %s", SettingSlug(*entry).c_str(), SettingValueSyntax(entry->type));
                        continue;
                    }
                    break;
                }
                case Load:
                    SettingsRegistry::LoadEntryFromDoc(*entry, *doc);
                    break;
            }
            Log::Info("[%s] %s = %s", entry->section.c_str(), entry->key.c_str(), SettingValueToString(*entry).c_str());
        }
    }

    bool CanAddToParty()
    {
        return GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost && GW::PartyMgr::GetIsLeader() && GW::PartyMgr::GetPartySize() < GW::Map::GetMapInfo()->max_party_size;
    }

    using AddPartyMemberFn = std::function<void(uint32_t)>;

    void AddPartyMemberByName(const wchar_t* _search, std::map<uint32_t, std::wstring>* agent_names, AddPartyMemberFn add_fn)
    {
        if (!CanAddToParty()) return;

        auto search = new std::wstring(_search);
        Resources::EnqueueWorkerTask([agent_names, search, add_fn]() {
            bool success = false;
            for (clock_t i = 0; i < 1000; i += 20) {
                success = true;
                for (auto& it : *agent_names) {
                    if (it.second.empty()) {
                        success = false;
                        break;
                    }
                }
                if (success) break;
                Sleep(20);
            }
            if (success) {
                uint32_t best_id = 0;
                size_t best_pos = std::wstring::npos;

                for (auto& [id, name] : *agent_names) {
                    std::wstring name_lower = name;
                    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), towlower);

                    size_t pos = name_lower.find(*search);
                    if (pos != std::wstring::npos && (best_id == 0 || pos < best_pos || (pos == best_pos && name.length() < agent_names->at(best_id).length()))) {
                        best_id = id;
                        best_pos = pos;
                    }
                }

                if (best_id) {
                    GW::GameThread::Enqueue([add_fn, best_id]() {
                        add_fn(best_id);
                    });
                }
            }
            else {
                Log::Error("解析队伍成员名称超时");
            }

            delete agent_names;
            delete search;
        });
    }

    constexpr std::array profession_names = {L"", L"战士", L"游侠", L"僧侣", L"死灵法师", L"幻术师", L"元素使", L"刺客", L"祭祀", L"圣言者", L"神唤使"};

    // Returns matching profession index (1-10), or 0 if no match
    static GW::Constants::ProfessionByte FindProfessionMatch(const std::wstring& search)
    {
        for (size_t i = 1; i < profession_names.size(); i++) {
            if (wcsstr(profession_names.at(i), search.c_str())) {
                return (GW::Constants::ProfessionByte)i;
            }
        }
        return GW::Constants::ProfessionByte::None;
    }

    void CHAT_CMD_FUNC(CmdAddHenchman)
    {
        const auto w = GW::GetWorldContext();
        if (!w || argc < 2) return;
        const std::wstring search = TextUtils::ToLower(GetRemainingArgsWstr(message, 1));

        const auto profession = FindProfessionMatch(search);
        if (profession != GW::Constants::ProfessionByte::None) {
            for (auto& agent_id : w->henchmen_agent_ids) {
                if (GW::Agents::GetAgentPrimary(agent_id) == profession) {
                    GW::GameThread::Enqueue([agent_id]() {
                        GW::PartyMgr::AddHenchman(agent_id);
                    });
                }
            }
        }

        auto agent_names = new std::map<uint32_t, std::wstring>();
        for (auto& agent_id : w->henchmen_agent_ids) {
            (*agent_names)[agent_id] = L"";
            GW::Agents::AsyncGetAgentName(agent_id, (*agent_names)[agent_id]);
        }
        AddPartyMemberByName(search.c_str(), agent_names, [](uint32_t found) {
            GW::PartyMgr::AddHenchman(found);
        });
    }

    void CHAT_CMD_FUNC(CmdAddHero)
    {
        const auto w = GW::GetWorldContext();
        if (!w || argc < 2) return;
        const std::wstring search = TextUtils::ToLower(GetRemainingArgsWstr(message, 1));

        const auto profession = FindProfessionMatch(search);
        if (profession != GW::Constants::ProfessionByte::None) {
            for (auto& hero : w->hero_info) {
                if (hero.primary == (GW::Constants::Profession)profession) {
                    GW::GameThread::Enqueue([hero_id = hero.hero_id]() {
                        GW::PartyMgr::AddHero(hero_id);
                    });
                }
            }
        }

        auto agent_names = new std::map<uint32_t, std::wstring>();
        for (auto& hero : w->hero_info) {
            const auto hero_data = GW::PartyMgr::GetHeroConstData(hero.hero_id);
            if (hero_data && hero_data->name_id) {
                (*agent_names)[hero.hero_id] = L"";
                wchar_t enc_str[8];
                GW::UI::UInt32ToEncStr(hero_data->name_id, enc_str, _countof(enc_str));
                GW::UI::AsyncDecodeStr((const wchar_t*)enc_str, &(*agent_names)[hero.hero_id]);
            }
        }
        AddPartyMemberByName(search.c_str(), agent_names, [](uint32_t found) {
            GW::PartyMgr::AddHero((GW::Constants::HeroID)found);
        });
    }
    void CHAT_CMD_FUNC(CmdLeave)
    {
        if (GW::PartyMgr::GetPartySize() > 1) {
            GW::GameThread::Enqueue(GW::PartyMgr::LeaveParty);
        }
    }
    struct SkillToUse {
        uint32_t slot = 0; // 1-8 range
        float skill_usage_delay = 0.f;
        clock_t skill_timer = clock();
        void Update();
    } skill_to_use;

    void CHAT_CMD_FUNC(CmdUseSkill)
    {
        if (!IsMapReady()) {
            return;
        }
        if (argc < 2) {
            Log::Warning(useskill_syntax);
            return;
        }
        const std::wstring arg1 = TextUtils::ToLower(argv[1]);
        if (arg1 == L"stop" || arg1 == L"off") {
            skill_to_use.slot = 0;
            return;
        }
        uint32_t num = 0;
        if (!TextUtils::ParseUInt(argv[1], &num) || num > 8) {
            Log::Warning(useskill_syntax);
            return;
        }
        skill_to_use.slot = (skill_to_use.slot == num) ? 0 : num;
        skill_to_use.skill_usage_delay = .0f;
    }

    void HookOnChatInteraction()
    {
        if (OnChatInteraction_Callback_Func) return;
        const auto frame = GW::UI::GetFrameByLabel(L"Chat");
        if (!(frame && frame->frame_callbacks.size())) return;
        OnChatInteraction_Callback_Func = frame->frame_callbacks[0].callback;
        GW::Hook::CreateHook((void**)&OnChatInteraction_Callback_Func, OnChatUI_Callback, (void**)&OnChatInteraction_Callback_Ret);
        GW::Hook::EnableHooks(OnChatInteraction_Callback_Func);
    }

    void DrawChatCommandsHelp()
    {
        if (!ImGui::TreeNodeEx("聊天命令", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            return;
        }

        ImGui::Text("您可以创建\"发送聊天\"快捷键来执行任何命令。");
        ImGui::TextDisabled(
            "下方 <xyz> 表示参数，请使用适当的值（不含引号）。\n"
            "(a|b) 表示必选参数，此情况下为 'a' 或 'b'。\n"
            "[a|b] 表示可选参数，此情况下可为空、'a' 或 'b'。"
        );

        ImGui::Bullet();
        ImGui::Text("'/age2' 将当前地图时间输出到聊天。");
        ImGui::Bullet();
        ImGui::Text("'/armor' 是 '/pingitem armor' 的别名。");
        ImGui::Bullet();
        ImGui::Text("'/bonds [remove|add] [party_member_index|all] [all|skill_id]' 从单个队伍成员或所有队伍成员移除或添加增益。");
        ImGui::Bullet();
        ImGui::Text("'/borderless [on|off]' 切换、启用或禁用无边框窗口。");
        ImGui::Bullet();
        ImGui::Text(button_syntax);
        ImGui::Bullet();
        ImGui::Text("'/call' 标记当前目标。");
        ImGui::Bullet();
        ImGui::Text(CameraUnlockModule::camera_syntax);
        ImGui::Bullet();
        ImGui::Text(chat_tab_syntax);
        ImGui::Bullet();
        ImGui::Text("'/chest' 在前哨站打开迅雷仓库。");
        ImGui::Bullet();
        ImGui::Text(
            "'/climate' 显示自动天气状态和当前气候。\n"
            "'/climate [auto|off|<climate>]' 控制自动天气：'auto' 跟随地图，指定气候名称强制使用该气候，'off' 停止自动天气并清除当前天气。"
        );
        ImGui::Bullet();
        ImGui::Text(
            "'/config set|get|toggle|load [section key [value]]...' 编辑 GWToolbox.ini 中的配置值。\n"
            "\t'set' 将设置应用到运行中的配置。\n"
            "\t'get' 显示给定键的值。\n"
            "\t'toggle' 在给定值和磁盘配置之间切换。\n"
            "\t'load' 将键重置为磁盘配置。"
        );
        ImGui::Bullet();
        ImGui::Text(custommarker_syntax);
        ImGui::Bullet();
        ImGui::Text(
            "'/damage' 或 '/dmg' 将队伍伤害输出到聊天。\n"
            "'/damage me' 仅发送您自己的伤害。\n"
            "'/damage <number>' 发送队伍成员的伤害（例如 '/damage 3'）。\n"
            "'/damage reset' 重置队伍窗口中的伤害统计。"
        );
        ImGui::Bullet();
        ImGui::Text(deposit_syntax);
        ImGui::Bullet();
        ImGui::Text(dialog_syntax);
        ImGui::Bullet();
        ImGui::Text(dropbuff_syntax);
        ImGui::Bullet();
        ImGui::Text(dropitem_syntax);
        ImGui::Bullet();
        ImGui::Text(
            "'/enter [fow|uw]' 进入您所在前哨站的任务。\n"
            "如果在启程海滩、命运之流、乌尔戈茨深渊或深渊，将使用卷轴。\n"
            "如果在前哨站有可用任务，将开始任务倒计时。"
        );
        ImGui::Bullet();
        ImGui::Text("'/ff' 是 '/resign' 的别名");
        ImGui::Bullet();
        ImGui::Text("'/flag [all|clear|<number>]' 在小地图上标记英雄（与小地图旁的按钮功能相同）。");
        ImGui::Bullet();
        ImGui::Text("'/flag [all|<number>] [x] [y]' 将英雄标记到坐标 [x],[y]。");
        ImGui::Bullet();
        ImGui::Text("'/flag <number> clear' 清除英雄标记。");
        ImGui::Bullet();
        ImGui::Text(fps_syntax);
        if (GWToolbox::IsModuleEnabled("英雄装备")) {
            ImGui::Bullet();
            ImGui::Text("'/heroinventory [hero_index]' 切换英雄的独立物品窗口");
        }

        ImGui::Bullet();
        ImGui::Text(CmdHeroBehaviour_syntax);
        ImGui::Bullet();
        ImGui::Text(disableheroskill_syntax);
        const auto toggle_hint = "<name> 选项：盔甲、服装、服装头饰、披风、<窗口或小部件名称>";
        ImGui::Bullet();
        ImGui::Text("'/hide <名称>' 关闭名为 <名称> 的窗口、游戏内功能或小部件。");
        ImGui::ShowHelp(toggle_hint);
        ImGui::Bullet();
        ImGui::Text("'/hm' 或 '/hardmode' 在前哨站设置困难模式。");
        ImGui::Bullet();
        ImGui::Text("'/hom' 使用当前目标玩家的成就打开纪念堂计算器。");
        ImGui::Bullet();
        ImGui::Text("'/load [build template|build name] [Hero index]' 加载配装。配装名称如含空格需加引号。第一个英雄索引为 1，最后一个为 7。留空则用于玩家自己。");
        ImGui::Bullet();
        ImGui::TextUnformatted(
            "'/loadprefs' 从 '<GWToolbox Dir>/<当前 GW 账号邮箱>_GuildWarsSettings.ini' 加载 GW 设置\n"
            "'/loadprefs <文件名>' 从 '<GWToolbox Dir>/<文件名>.ini' 加载 GW 设置"
        );
        ImGui::Bullet();
        ImGui::TextUnformatted("'/nm' 或 '/normalmode' 在前哨站设置普通模式。");
        ImGui::Bullet();
        ImGui::TextUnformatted("'/morale' 将您当前的士气/死亡惩罚信息发送到队伍聊天。");
        ImGui::Bullet();
        ImGui::TextUnformatted(
            "'/marktarget' 在工具箱小地图上高亮当前目标。\n"
            "'/marktarget clear' 在工具箱小地图上取消高亮当前目标。\n"
            "'/marktarget clearall' 在工具箱小地图上清除所有高亮目标。"
        );
        ImGui::Bullet();
        ImGui::TextUnformatted("'/observer:reset' 重置观察者模式数据。");
        ImGui::Bullet();
        ImGui::TextUnformatted(
            "'/pingitem <装备部位>' 在聊天中标记您的装备。\n"
            "<装备部位> 选项：armor、head、chest、legs、boots、gloves、offhand、weapon、weapons、costume"
        );
        ImGui::Bullet();
        ImGui::TextUnformatted("'/pcons [on|off]' 切换、启用或禁用消耗品。");
        ImGui::Bullet();
        ImGui::TextUnformatted(pref_syntax);
        ImGui::Bullet();
        ImGui::TextUnformatted("'/resize <width> <height>' 调整 GW 窗口大小");
        ImGui::Bullet();
        ImGui::TextUnformatted(
            "'/saveprefs' 将 GW 设置保存到 '<GWToolbox Dir>/<当前 GW 账号邮箱>_GuildWarsSettings.ini'\n"
            "'/saveprefs <文件名>' 将 GW 设置保存到 '<GWToolbox Dir>/<文件名>.ini'"
        );
        ImGui::Bullet();
        ImGui::TextUnformatted("'/scwiki [<搜索词>]' 搜索 https://wiki.fbgmguild.com。");
        ImGui::Bullet();
        ImGui::TextUnformatted("'/show <名称>' 打开名为 <名称> 的窗口、游戏内功能或小部件。");
        ImGui::ShowHelp(toggle_hint);
        ImGui::Bullet();
        ImGui::Text("'/toggle <名称> [on|off|toggle]' 切换名为 <名称> 的窗口、游戏内功能或小部件。");
        ImGui::ShowHelp(toggle_hint);
        ImGui::Bullet();
        ImGui::Text(target_syntax);
        ImGui::Bullet();
        ImGui::Text(tb_syntax);
        ImGui::Bullet();
        ImGui::Text(
            "'/travel <城镇> [dis]'、'/tp <城镇> [dis]' 或 '/to <城镇> [dis]' 传送到与 <城镇> 名称最匹配的前哨站。\n"
            "[dis] 可以是：ae、ae1、ee、eg、int 等"
        );
        ImGui::Bullet();
        ImGui::Text("'/travel outpost' 传送到离您当前位置最近的已解锁前哨站。");
        ImGui::Bullet();
        ImGui::Text("'/travel [zv|zb|zm]' 传送到离每日任务最近的已解锁前哨站。");
        ImGui::Bullet();
        ImGui::Text(useskill_syntax);
        ImGui::Bullet();
        ImGui::Text("'/volume [master|music|background|effects|dialog|ui] <amount (0-100)>' 设置游戏内音量。");
        ImGui::Bullet();
        ImGui::Text(
            "'/weather' 列出天气状况及其是否开启。\n"
            "'/weather <condition> [on|off|toggle]' 按名称切换天气状况（开启一个会关闭其他）。\n"
            "'/weather auto' 开启自动天气（同 '/climate auto'）；'/weather off' 停止所有天气（同 '/climate off'）。"
        );
        ImGui::Bullet();
        ImGui::Text("'/wiki [quest|<搜索词>]' 搜索 GWW 当前任务或搜索词。默认搜索当前地图。");
        ImGui::Bullet();
        ImGui::Text(withdraw_syntax);



        ImGui::TreePop();
    }

    void DrawToolboxSettingChatCommandsHelp()
    {
        const auto& entries = SettingsRegistry::GetEntries();
        if (entries.empty() || !ImGui::TreeNodeEx("工具箱设置的聊天命令", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            return;
        }
        ImGui::TextUnformatted("这些命令允许您在游戏过程中直接切换或更改工具箱内的值。");
        static char filter_buf[128] = "";
        ImGui::InputTextWithHint("##tb_setting_filter", "过滤设置...", filter_buf, sizeof(filter_buf));
        const auto draw_entry_syntax = [](const SettingsRegistry::Entry& entry) {
            ImGui::Bullet();
            auto syntax = std::format("'/{} {} {}'", "tb_setting", SettingSlug(entry), SettingValueSyntax(entry.type));
            if (!entry.description.empty()) {
                syntax += " " + entry.description;
            }
            ImGui::TextUnformatted(syntax.c_str());
        };
        if (filter_buf[0]) {
            const auto filter_lower = TextUtils::ToLower(filter_buf);
            constexpr size_t max_shown = 100;
            size_t shown = 0;
            for (const auto& entry : entries) {
                if (SettingSlug(entry).find(filter_lower) == std::string::npos && TextUtils::ToLower(entry.label).find(filter_lower) == std::string::npos) {
                    continue;
                }
                draw_entry_syntax(entry);
                if (++shown >= max_shown) {
                    ImGui::TextDisabled("... 更多结果已隐藏，请细化过滤条件");
                    break;
                }
            }
            if (!shown) {
                ImGui::TextDisabled("没有匹配过滤条件的设置");
            }
        }
        else {
            std::map<std::string_view, std::vector<const SettingsRegistry::Entry*>> by_section;
            for (const auto& entry : entries) {
                by_section[entry.section].push_back(&entry);
            }
            for (const auto& [section, section_entries] : by_section) {
                if (!ImGui::TreeNodeEx(section.data(), ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
                    continue;
                }
                for (const auto* entry : section_entries) {
                    draw_entry_syntax(*entry);
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }

    void CmdGoldItemCommand(int argc, const LPWSTR* argv, const char* syntax, std::function<void(uint32_t)> gold_fn, std::function<void(uint16_t, std::vector<uint32_t>&)> item_fn)
    {
        if (argc < 2) return Log::Error("语法错误：\n%s", syntax);
        uint32_t wanted_quantity = 0;
        if (argc < 3) {
            std::wstring amount = argv[1];
            const bool platinum = amount.ends_with(L'k') || amount.ends_with(L'p');
            if (amount != L"max" && amount != L"all") {
                if (platinum) amount.pop_back();
                if (!(TextUtils::ParseUInt(amount.c_str(), &wanted_quantity) && wanted_quantity <= 0xFFFF)) return Log::Error("语法错误：\n%s", syntax);
                if (platinum) wanted_quantity *= 1000;
            }
            gold_fn(wanted_quantity);
            return;
        }
        if (!(TextUtils::ParseUInt(argv[1], &wanted_quantity) && wanted_quantity <= 0xFFFF)) return Log::Error("语法错误：\n%s", syntax);
        std::vector<uint32_t> model_ids;
        for (auto i = 2; i < argc; i++) {
            uint32_t model_id;
            if (!TextUtils::ParseUInt(argv[i], &model_id)) return Log::Error("语法错误：\n%s", syntax);
            model_ids.push_back(model_id);
        }
        item_fn(static_cast<uint16_t>(wanted_quantity), model_ids);
    }
} // namespace

void ChatCommands::CreateAlias(const wchar_t* alias, const wchar_t* message)
{
    if (alias && *alias == L'/') alias++;
    if (!(alias && *alias && message && *message)) return;
    const auto found = std::ranges::find_if(cmd_aliases, [alias, message](const CmdAlias* cmp) {
        return wcscmp(alias, cmp->alias_wstr) == 0 && wcscmp(message, cmp->command_wstr) == 0;
    });
    CmdAlias* alias_obj = nullptr;
    if (found != cmd_aliases.end()) {
        alias_obj = *found;
    }
    else {
        alias_obj = new CmdAlias();
        const auto alias_cstr = TextUtils::WStringToString(alias);
        strcpy(alias_obj->alias_cstr, alias_cstr.c_str());
        wcscpy(alias_obj->alias_wstr, alias);
        cmd_aliases.push_back(alias_obj);
    }

    const auto message_cstr = TextUtils::WStringToString(message);
    strcpy(alias_obj->command_cstr, message_cstr.c_str());
    wcscpy(alias_obj->command_wstr, message);
}

void ChatCommands::DrawHelp()
{
    DrawChatCommandsHelp();
    DrawToolboxSettingChatCommandsHelp();
}

void ChatCommands::DrawSettingsInternal()
{
    std::string preview = "选择...";
    switch (settings.default_title_id) {
        case CMDTITLE_KEEP_CURRENT:
            preview = "保持当前称号";
            break;
        case CMDTITLE_REMOVE_CURRENT:
            preview = "移除称号";
            break;
        default:
            const auto selected = std::ranges::find_if(title_names, [&](auto* it) {
                return std::to_underlying(it->title) == settings.default_title_id;
            });

            if (selected != title_names.end()) {
                preview = (*selected)->name.string();
            }
            break;
    }

    ImGui::Text("'/title' 命令的默认回退操作");
    ImGui::ShowHelp("如果您所在的区域没有合适的称号，工具箱将重新应用此称号。\n如果您当前角色没有选中的称号，则不会发生任何变化。");
    ImGui::Indent();
    if (ImGui::BeginCombo("###title_command_fallback", preview.c_str())) {
        if (ImGui::Selectable("保持当前称号", CMDTITLE_KEEP_CURRENT == settings.default_title_id)) {
            settings.default_title_id = CMDTITLE_KEEP_CURRENT;
        }
        if (ImGui::Selectable("移除称号", CMDTITLE_REMOVE_CURRENT == settings.default_title_id)) {
            settings.default_title_id = CMDTITLE_REMOVE_CURRENT;
        }
        for (auto* it : title_names) {
            if (ImGui::Selectable(it->name.string().c_str(), std::to_underlying(it->title) == settings.default_title_id)) {
                settings.default_title_id = std::to_underlying(it->title);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Unindent();

    ImGui::TextUnformatted("聊天命令别名");
    ImGui::TextDisabled("将触发找到的第一个匹配的命令别名");

    static auto OnConfirmDeleteAlias = [](bool result, void* wparam) {
        if (!result) return;
        auto alias = (CmdAlias*)wparam;
        const auto found = std::ranges::find(cmd_aliases, alias);
        if (found != cmd_aliases.end()) {
            cmd_aliases.erase(found);
            delete alias;
        }
    };

    const auto avail_w = ImGui::GetContentRegionAvail().x - 128.f;
    for (size_t i = 0, cnt = cmd_aliases.size(); i < cnt; i++) {
        const auto alias = cmd_aliases[i];
        ImGui::PushID(i);

        ImGui::PushItemWidth(avail_w * .3f);
        if (ImGui::InputText("###cmd_alias", alias->alias_cstr, _countof(CmdAlias::alias_cstr))) {
            swprintf(alias->alias_wstr, _countof(CmdAlias::alias_wstr), L"%S", alias->alias_cstr);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("此命令的别名");
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        const auto text_height = ImGui::GetTextLineHeightWithSpacing();
        const auto num_newlines = 1 + std::count(alias->command_cstr, alias->command_cstr + _countof(CmdAlias::command_cstr), '\n');
        if (ImGui::InputTextMultiline("##cmd_command", alias->command_cstr, _countof(CmdAlias::command_cstr), ImVec2(avail_w * .6f, text_height + num_newlines * ImGui::GetTextLineHeight()))) {
            swprintf(alias->command_wstr, _countof(CmdAlias::command_wstr), L"%S", alias->command_cstr);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("要触发的聊天命令");
        }
        ImGui::SameLine(avail_w);
        static bool confirm_delete = false;
        ImGui::SmallConfirmButton("删除", "确定要删除此条目吗？", OnConfirmDeleteAlias, alias);
        ImGui::PopID();
    }
    if (ImGui::Button("添加新别名")) {
        wchar_t tmp[32];
        swprintf(tmp, _countof(tmp), L"alias_%d", cmd_aliases.size());
        CreateAlias(tmp, L"#hello world");
    }
    ImGui::SameLine();
    if (ImGui::Button("排序")) {
        sort_cmd_aliases();
    }
}

void ChatCommands::LoadSettings(SettingsDoc& doc, ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(doc, ini);
    doc.GetStruct(Name(), settings);

    for (const auto* it : cmd_aliases) {
        delete it;
    }
    cmd_aliases.clear();

    std::vector<CmdAliasSetting> aliases;
    if (doc.Get(Name(), "cmd_aliases", aliases)) {
        for (const auto& it : aliases) {
            const auto alias_wstr = TextUtils::StringToWString(it.alias);
            const auto command_wstr = TextUtils::StringToWString(it.command);
            CreateAlias(alias_wstr.c_str(), command_wstr.c_str());
        }
    }
    else {
        const auto section_name = "Chat Command Aliases";

        TNamesDepend entries;
        ini->GetAllKeys(section_name, entries);
        for (const auto& entry : entries) {
            if (!entry.pItem[0]) {
                continue;
            }
            auto alias = std::string(entry.pItem);
            std::string cmd = ini->GetValue(section_name, entry.pItem, "");
            if (cmd.empty()) {
                continue;
            }
            std::ranges::replace(cmd, '\x2', '\n');
            static constexpr ctll::fixed_string index_regex = "(\\d+):(.+)";
            if (auto match = ctre::match<index_regex>(alias)) {
                alias = match.template get<2>().to_string();
            }
            const auto alias_wstr = TextUtils::StringToWString(alias);
            const auto command_wstr = TextUtils::StringToWString(cmd);
            CreateAlias(alias_wstr.c_str(), command_wstr.c_str());
        }
    }
    if (cmd_aliases.empty()) {
        CreateAlias(L"ff", L"/resign");
        CreateAlias(L"gh", L"/tp gh");
        CreateAlias(L"armor", L"/pingitem armor");
    }
    sort_cmd_aliases();
}

void ChatCommands::SaveSettings(SettingsDoc& doc)
{
    ToolboxModule::SaveSettings(doc);
    doc.SetStruct(Name(), settings);

    sort_cmd_aliases();

    std::vector<CmdAliasSetting> aliases;
    aliases.reserve(cmd_aliases.size());
    for (const auto alias : cmd_aliases) {
        aliases.emplace_back(alias->alias_cstr, alias->command_cstr);
    }
    doc.Set(Name(), "cmd_aliases", aliases);
}

void CHAT_CMD_FUNC(ChatCommands::CmdPingQuest)
{
    Instance().quest_ping.Init();
}

void CHAT_CMD_FUNC(ChatCommands::CmdCustomMarker)
{
    if (argc == 2 && wcscmp(argv[1], L"clear") == 0) {
        QuestModule::ClearCustomQuestMarker();
        return;
    }
    float x, y;
    if (argc < 3 || !TextUtils::ParseFloat(argv[1], &x) || !TextUtils::ParseFloat(argv[2], &y)) {
        return Log::Error(custommarker_syntax);
    }
    QuestModule::SetCustomQuestMarker({x, y});
}

void ChatCommands::Initialize()
{
    ToolboxModule::Initialize();
    SettingsRegistry::Register(this, settings);

    // TODO: Move all of these callbacks into pvt namespace
    chat_commands = {
        {L"addhenchman", CmdAddHenchman},
        {L"button", CmdButtonPress},
        {L"chat", CmdChatTab},
        {L"enter", CmdEnterMission},
        {L"age2", CmdAge2},
        {L"dialog", CmdDialog},
        {L"show", CmdShow},
        {L"hide", CmdHide},
        {L"toggle", CmdToggle},
        {L"tb", CmdTB},
        {L"chest", CmdChest},
        {L"duncan", CmdDuncan},
        {L"xunlai", CmdChest},
        {L"afk", CmdAfk},
        {L"target", CmdTarget},
        {L"tgt", CmdTarget},
        {L"xunlai", CmdChest},
        {L"useskill", CmdUseSkill},
        {L"scwiki", CmdSCWiki},
        {L"load", CmdLoad},
        {L"pingbuild", CmdPingBuild},
        {L"quest", CmdPingQuest},
        {L"resize", CmdResize},
        {L"settitle", CmdReapplyTitle},
        {L"title", CmdReapplyTitle},
        {L"withdraw", CmdWithdraw},
        {L"deposit", CmdDeposit},
        {L"pingitem", CmdPingEquipment},
        {L"tick", CmdTick},
        {L"hero", CmdHeroBehaviour},
        {L"disableheroskill", CmdDisableHeroSkill},
        {L"morale", CmdMorale},
        {L"volume", CmdVolume},
        {L"nm", CmdSetNormalMode},
        {L"normalmode", CmdSetNormalMode},
        {L"hm", CmdSetHardMode},
        {L"hardmode", CmdSetHardMode},
        {L"hom", CmdHom},
        {L"fps", CmdFps},
        {L"pref", CmdPref},
        {L"call", CmdCallTarget},
        {L"config", CmdConfig},
        {settings_via_chat_commands_cmd, CmdSettingViaChatCommand},
        {L"dropbuff", CmdDropBuff},
        {L"dropitem", CmdDropItem},
        {L"addhenchman", CmdAddHenchman},
        {L"addhero", CmdAddHero},
        {L"leave", CmdLeave},
        {L"custommarker", CmdCustomMarker},
    };


    RegisterUIMessageCallback(&OnSentChat_HookEntry, GW::UI::UIMessage::kSendChatMessage, OnSendChat);

    HookOnChatInteraction();

    for (auto& it : chat_commands) {
        GW::Chat::CreateCommand(&ChatCmd_HookEntry, it.first, it.second);
    }
    getPrefCommandOptions();
}

void ChatCommands::Terminate()
{
    ToolboxModule::Terminate();
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
    chat_commands.clear();
    if (FocusChatTab_Func) {
        GW::Hook::RemoveHook(FocusChatTab_Func);
    }
    if (OnChatInteraction_Callback_Func) {
        GW::Hook::RemoveHook(OnChatInteraction_Callback_Func);
    }

    GW::UI::RemoveUIMessageCallback(&OnSentChat_HookEntry);

    for (const auto it : title_names) {
        delete it;
    }
    title_names.clear();
    for (const auto it : cmd_aliases) {
        delete it;
    }
    cmd_aliases.clear();
}

void ChatCommands::Update(const float delta)
{
    HookOnChatInteraction();
    if (title_names.empty()) {
        const auto* titles = GetTitles();
        for (size_t i = 0; titles && i < titles->size(); i++) {
            if (GW::PlayerMgr::IsDeprecatedTitle(static_cast<GW::Constants::TitleID>(i))) {
                continue;
            }
            auto dtn = new DecodedTitleName(static_cast<GW::Constants::TitleID>(i));
            title_names.push_back(dtn);
            dtn->name.string(); // Trigger decode for sorting.
        }
    }
    else if (!title_names_sorted) {
        const auto can_sort = std::ranges::all_of(title_names, [](const auto& title_name) {
            return !title_name->name.IsDecoding();
        });
        if (can_sort) {
            std::ranges::sort(title_names, [](DecodedTitleName* first, DecodedTitleName* second) {
                return first->name.string() < second->name.string();
            });
            title_names_sorted = true;
        }
    }

    if (delta == 0.f) {
        return;
    }
    skill_to_use.Update();
    npc_to_find.Update();
    quest_ping.Update();
}

void ChatCommands::QuestPing::Init()
{
    const auto* quest = GW::QuestMgr::GetActiveQuest();
    if (quest) {
        quest_id = quest->quest_id;
        name.reset(quest->name);
        objectives.reset(quest->objectives);
    }
}

void ChatCommands::QuestPing::Update()
{
    if (!name.wstring().empty()) {
        wchar_t print_buf[128];
        swprintf(print_buf, _countof(print_buf), L"当前任务：%s", name.wstring().c_str());
        GW::Chat::SendChat('#', print_buf);
    }
    if (!objectives.wstring().empty()) {
        static constexpr ctll::fixed_string current_obj_pattern = LR"(\{s\}([^\{]+))";

        if (auto m = ctre::match<current_obj_pattern>(objectives.wstring())) {
            wchar_t print_buf[128];
            swprintf(print_buf, _countof(print_buf), L" - %s", m.get<1>().to_string().c_str());
            GW::Chat::SendChat('#', print_buf);
        }
        objectives.reset(nullptr);
    }
    if (!name.wstring().empty()) {
        wchar_t url_buf[64];
        swprintf(url_buf, _countof(url_buf), L"%SGame_link:Quest_%d", GuiUtils::WikiUrl(L"").c_str(), quest_id);
        GW::Chat::SendChat('#', url_buf);
        name.reset(nullptr);
    }
}

void SearchAgent::Add(const wchar_t* _search, const GW::AgentTargetFlags type)
{
    if (!_search || !_search[0]) return;

    // Each term (split on '|') is matched independently, giving OR semantics.
    const auto terms = TextUtils::Split(TextUtils::ToLower(_search), L"|");
    if (terms.empty()) return;
    for (const auto& term : terms) {
        queries.push_back({term, type});
    }

    // Anchor the timeout to the first pending query so a burst of hotkeys can't keep pushing it back.
    if (!started) {
        started = TIMER_INIT();
    }

    GW::AgentArray* agents = GW::Agents::GetAgentArray();
    if (!agents) return;

    for (const auto agent : *agents) {
        if (!GW::Agents::GetAgentMatchesFlags(agent, type)) continue;
        if (std::ranges::any_of(npc_names, [agent](const auto& n) { return n.first == agent->agent_id; })) {
            continue; // already queued for decoding by an earlier query
        }
        const wchar_t* enc_name = GW::Agents::GetAgentEncName(agent);
        if (enc_name && enc_name[0]) {
            npc_names.push_back({agent->agent_id, std::make_unique<GuiUtils::EncString>(enc_name)});
        }
    }
}

void SearchAgent::Update()
{
    if (!started) {
        return;
    }
    if (TIMER_DIFF(started) > 3000) {
        Log::Error("获取 NPC 名称超时");
        Reset();
        return;
    }
    for (const auto& str : npc_names | std::views::values) {
        if (str->wstring().empty()) {
            return; // Not all decoded yet
        }
    }
    float distance = GW::Constants::SqrRange::Compass;
    size_t closest = 0;
    const auto me = GW::Agents::GetControlledCharacter();
    if (!me) {
        return;
    }
    for (const auto& [agent_id, enc] : npc_names) {
        const auto agent = GW::Agents::GetAgentByID(agent_id);
        if (!agent) {
            continue;
        }
        const auto name = TextUtils::ToLower(enc->wstring());
        // Match a term only against agents of the type it was queued with, so each /target type stays scoped.
        const auto matches = std::ranges::any_of(queries, [&](const Query& q) {
            return name.find(q.search) != std::wstring::npos && GW::Agents::GetAgentMatchesFlags(agent, q.type);
        });
        if (!matches) {
            continue;
        }
        const auto dist = GW::GetSquareDistance(me->pos, agent->pos);
        if (dist < distance) {
            closest = agent_id;
            distance = dist;
        }
    }
    if (closest) {
        GW::Agents::ChangeTarget(closest);
    }
    Reset();
}

void SkillToUse::Update()
{
    if (!slot) {
        return;
    }
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable || GW::Map::GetIsObserving()) {
        slot = 0;
        return;
    }
    if ((clock() - skill_timer) / 1000.0f < skill_usage_delay) {
        return;
    }
    const auto skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
    if (!skillbar || !skillbar->IsValid()) {
        slot = 0;
        return;
    }
    const auto lslot = slot - 1;
    const GW::SkillbarSkill& skill = skillbar->skills[lslot];
    if (skill.skill_id == GW::Constants::SkillID::No_Skill || skill.skill_id == GW::Constants::SkillID::Mystic_Healing || skill.skill_id == GW::Constants::SkillID::Cautery_Signet) {
        slot = 0;
        return;
    }
    const auto skilldata_ptr = GW::SkillbarMgr::GetSkillConstantData(skill.skill_id);
    if (!skilldata_ptr) {
        slot = 0;
        return;
    }
    const GW::Skill& skilldata = *skilldata_ptr;
    if ((skilldata.adrenaline == 0 && skill.GetRecharge() == 0) || (skilldata.adrenaline > 0 && skill.adrenaline_a == skilldata.adrenaline)) {
        const auto wait_for_queue = !(skilldata.type == GW::Constants::SkillType::Shout || skilldata.type == GW::Constants::SkillType::Stance || skilldata.type == GW::Constants::SkillType::PetAttack);
        if (wait_for_queue && skillbar->cast_array.size()) return; // Don't use skill if we've got something queued
        GW::SkillbarMgr::UseSkill(lslot, GW::Agents::GetTargetId());
        skill_usage_delay = std::max(skilldata.activation + skilldata.aftercast, 0.25f); // a small flat delay of .3s for ping and to avoid spamming in case of bad target
        skill_timer = clock();
    }
}

bool ChatCommands::ReadTemplateFile(const std::wstring& path, char* buff, const size_t buffSize)
{
    const auto fileHandle = CreateFileW(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        // We don't print that error, because we can /load [template]
        // Log::Error("Failed openning file '%S'", path.c_str());
        return false;
    }

    const auto fileSize = GetFileSize(fileHandle, nullptr);
    if (fileSize >= buffSize) {
        Log::Error("缓冲区大小不足，文件大小为 %d", fileSize);
        CloseHandle(fileHandle);
        return false;
    }

    DWORD bytesReaded; // @Remark, necessary !!!!! failed on some Windows 7.
    if (ReadFile(fileHandle, buff, fileSize, &bytesReaded, nullptr) == FALSE) {
        Log::Error("ReadFile 失败！（%u）", GetLastError());
        CloseHandle(fileHandle);
        return false;
    }

    buff[fileSize] = 0;
    CloseHandle(fileHandle);
    return true;
}

void CHAT_CMD_FUNC(ChatCommands::CmdEnterMission)
{
    const auto error_use_from_outpost = "请在前哨站使用 '/enter' 开始任务或精英区域";
    const auto error_fow_uw_syntax = "使用 '/enter fow' 或 '/enter uw' 触发进入";
    const auto error_no_scrolls = "无法进入精英区域；未找到卷轴";
    const auto error_not_leading = "无法进入任务；您不是队伍领袖";

    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) {
        return Log::Error(error_use_from_outpost);
    }

    switch (GW::Map::GetMapID()) {
        case GW::Constants::MapID::Temple_of_the_Ages:
        case GW::Constants::MapID::Zin_Ku_Corridor_outpost:
        case GW::Constants::MapID::Chantry_of_Secrets_outpost:
        case GW::Constants::MapID::Embark_Beach: {
            if (argc < 2) {
                return Log::Error(error_fow_uw_syntax);
            }
            uint32_t item_id;
            const std::wstring arg1 = TextUtils::ToLower(argv[1]);
            if (arg1 == L"fow") {
                item_id = 22280;
            }
            else if (arg1 == L"uw") {
                item_id = 3746;
            }
            else {
                return Log::Error(error_fow_uw_syntax);
            }
            if (!GW::Items::UseItemByModelId(item_id, 1, 4) && !GW::Items::UseItemByModelId(item_id, 8, 16)) {
                return Log::Error(error_no_scrolls);
            }
        } break;
        default:
            const auto map_info = GW::Map::GetCurrentMapInfo();
            if (!map_info || !map_info->GetHasEnterButton()) {
                return Log::Error(error_use_from_outpost);
            }
            if (!GW::PartyMgr::GetIsLeader()) {
                return Log::Error(error_not_leading);
            }
            const auto p = GW::GetGameContext()->party;
            if (p && (p->flag & 0x8) != 0) {
                GW::Map::CancelEnterChallenge();
            }
            else {
                GW::Map::EnterChallenge();
            }
    }
}

void CHAT_CMD_FUNC(ChatCommands::CmdMorale)
{
    if (GW::GetGameContext()->world->morale == 100) {
        GW::Chat::SendChat('#', L"我没有士气加成或死亡惩罚！");
    }
    else {
        auto packet = GW::UI::UIPacket::kSendCallTarget{.call_type = GW::CallTargetType::Morale, .agent_id = GW::Agents::GetControlledCharacterId()};
        GW::UI::SendUIMessage(GW::UI::UIMessage::kSendCallTarget, &packet);
    }
}

void CHAT_CMD_FUNC(ChatCommands::CmdAge2)
{
    TimerWidget::Instance().PrintTimer();
}



void CHAT_CMD_FUNC(ChatCommands::CmdDialog)
{
    if (!IsMapReady()) {
        return;
    }
    if (argc <= 1) {
        Log::Warning(dialog_syntax);
        return;
    }
    uint32_t id = 0;
    auto dialog_str = std::wstring{argv[1]};
    int base = 10;
    if (dialog_str.starts_with(L"0x")) {
        base = 16;
        dialog_str = dialog_str.substr(2);
    }
    if (dialog_str == L"take" || dialog_str == L"0") {
        id = 0;
    }
    else if (!(TextUtils::ParseUInt(argv[1], &id, base) && id)) {
        Log::Warning(dialog_syntax);
        return;
    }
    if (!DialogModule::GetDialogAgent()) {
        const auto* target = GW::Agents::GetTargetAsAgentLiving();
        const auto* me = GW::Agents::GetControlledCharacter();
        if (target && me && target->allegiance == GW::Constants::Allegiance::Npc_Minipet && GetDistance(me->pos, target->pos) < GW::Constants::Range::Area) {
            GW::Agents::InteractAgent(target);
        }
    }
    DialogModule::SendDialog(id);
}

void CHAT_CMD_FUNC(ChatCommands::CmdChest)
{
    if (!IsMapReady()) {
        return;
    }
    GW::GameThread::Enqueue([]() {
        const auto frame = GW::UI::GetFrameByLabel(L"InvAccount");
        if (frame) {
            GW::UI::DestroyUIComponent(frame);
        }
        else {
            GW::Items::OpenXunlaiWindow();
        }
    });
}



void CHAT_CMD_FUNC(ChatCommands::CmdTB)
{
    if (!ImGui::GetCurrentContext()) {
        return; // Don't process window manips until ImGui is ready
    }
    if (argc < 2) {
        // e.g. /tbs
        MainWindow::Instance().visible ^= 1;
        return;
    }

    const std::wstring arg1 = TextUtils::ToLower(argv[1]);
    if (argc < 3) {
        if (arg1 == L"hide") {
            // e.g. /tb hide
            MainWindow::Instance().visible = false;
        }
        else if (arg1 == L"show") {
            // e.g. /tb show
            MainWindow::Instance().visible = true;
        }
        else if (arg1 == L"toggle") {
            // e.g. /tb toggle
            MainWindow::Instance().visible = !MainWindow::Instance().visible;
        }
        else if (arg1 == L"save") {
            // e.g. /tb save
            GWToolbox::SetSettingsFolder({});
            const auto file_location = GWToolbox::SaveSettings();
            const auto dir = file_location.parent_path();
            const auto dirstr = dir.wstring();
            const auto printable = TextUtils::str_replace_all(dirstr, LR"(\)", L"/");
            Log::InfoW(L"设置已保存到 [%s;file://%s]", printable.c_str(), printable.c_str());
        }
        else if (arg1 == L"load") {
            // e.g. /tb load
            GWToolbox::SetSettingsFolder({});
            const auto file_location = GWToolbox::LoadSettings();
            const auto dir = file_location.parent_path();
            const auto dirstr = dir.wstring();
            const auto printable = TextUtils::str_replace_all(dirstr, LR"(\)", L"/");
            Log::InfoW(L"设置已从 [%s;file://%s] 加载", printable.c_str(), printable.c_str());
        }
        else if (arg1 == L"reset") {
            // e.g. /tb reset
            ImGui::SetWindowPos(MainWindow::Instance().Name(), ImVec2(50.0f, 50.0f));
            ImGui::SetWindowPos(SettingsWindow::Instance().Name(), ImVec2(50.0f, 50.0f));
            MainWindow::Instance().visible = false;
            SettingsWindow::Instance().visible = true;
        }
        else if (arg1 == L"mini" || arg1 == L"minimize" || arg1 == L"collapse") {
            // e.g. /tb mini
            ImGui::SetWindowCollapsed(MainWindow::Instance().Name(), true);
        }
        else if (arg1 == L"maxi" || arg1 == L"maximize") {
            // e.g. /tb maxi
            ImGui::SetWindowCollapsed(MainWindow::Instance().Name(), false);
        }
        else if (arg1 == L"close" || arg1 == L"quit" || arg1 == L"exit") {
            // e.g. /tb close
            GWToolbox::SignalTerminate();
        }
        else {
            // e.g. /tb travel
            const std::vector<ToolboxUIElement*> windows = MatchingWindows(status, message, argc, argv);
            for (ToolboxUIElement* window : windows) {
                window->visible ^= 1;
            }
        }
        return;
    }
    const std::vector<ToolboxUIElement*> windows = MatchingWindows(status, message, argc, argv);
    const std::wstring arg2 = TextUtils::ToLower(argv[2]);
    if (arg2 == L"hide") {
        // e.g. /tb travel hide
        for (const auto& window : windows) {
            window->visible = false;
        }
    }
    else if (arg2 == L"show") {
        // e.g. /tb travel show
        for (const auto& window : windows) {
            window->visible = true;
        }
    }
    else if (arg2 == L"toggle") {
        // e.g. /tb travel toggle
        for (const auto& window : windows) {
            window->visible = !window->visible;
        }
    }
    else if (arg2 == L"mini" || arg2 == L"minimize" || arg2 == L"collapse") {
        // e.g. /tb travel mini
        for (const auto& window : windows) {
            ImGui::SetWindowCollapsed(window->Name(), true);
        }
    }
    else if (arg2 == L"maxi" || arg2 == L"maximize") {
        // e.g. /tb travel maxi
        for (const auto& window : windows) {
            ImGui::SetWindowCollapsed(window->Name(), false);
        }
    }
    else if (arg1 == L"save") {
        // e.g. /tb save pure
        const auto sanitised_foldername = TextUtils::SanitiseFilename(arg2);
        GWToolbox::SetSettingsFolder(sanitised_foldername);
        const auto file_location = GWToolbox::SaveSettings();
        const auto dir = file_location.parent_path();
        const auto dirstr = dir.wstring();
        const auto printable = TextUtils::str_replace_all(dirstr, LR"(\)", L"/");
        Log::InfoW(L"设置已保存到 %s", printable.c_str());
    }
    else if (arg1 == L"load") {
        // e.g. /tb load tas
        const auto sanitised_foldername = TextUtils::SanitiseFilename(arg2);
        const auto old_settings_folder = Resources::GetSettingsFolderName();
        GWToolbox::SetSettingsFolder(sanitised_foldername);
        // A config exists if it has split per-module files, a legacy single-doc json, or a legacy ini
        std::error_code ec;
        const auto modules_folder = Resources::GetSettingFile(GWTOOLBOX_MODULES_FOLDERNAME);
        const bool has_settings = (std::filesystem::exists(modules_folder, ec) && !std::filesystem::is_empty(modules_folder, ec)) || std::filesystem::exists(Resources::GetSettingFile(GWTOOLBOX_JSON_FILENAME), ec) ||
                                  std::filesystem::exists(Resources::GetLegacySettingFile(GWTOOLBOX_JSON_FILENAME), ec) || std::filesystem::exists(Resources::GetLegacySettingFile(GWTOOLBOX_INI_FILENAME), ec);
        if (!has_settings) {
            Log::ErrorW(L"设置文件夹 '%s' 不存在", arg2.c_str());
            GWToolbox::SetSettingsFolder(old_settings_folder);
            return;
        }
        const auto file_location = GWToolbox::LoadSettings();
        const auto dir = file_location.parent_path();
        const auto dirstr = dir.wstring();
        const auto printable = TextUtils::str_replace_all(dirstr, LR"(\)", L"/");
        Log::InfoW(L"设置已从 %s 加载", printable.c_str());
    }
    else {
        // Invalid argument
        Log::Error(tb_syntax);
    }
}

GW::UI::WindowID CHAT_CMD_FUNC(ChatCommands::MatchingGWWindow)
{
    const std::map<GW::UI::WindowID, const wchar_t*> gw_windows = {
        {GW::UI::WindowID_Compass, L"compass"}, {GW::UI::WindowID_HealthBar, L"healthbar"}, {GW::UI::WindowID_EnergyBar, L"energybar"}, {GW::UI::WindowID_ExperienceBar, L"experiencebar"}, {GW::UI::WindowID_Chat, L"chat"}
    };
    if (argc < 2) {
        return GW::UI::WindowID_Count;
    }
    const std::wstring arg = TextUtils::ToLower(argv[1]);
    if (!arg.empty() && arg != L"all") {
        for (const auto& [window_id, window_name] : gw_windows) {
            if (wcscmp(window_name, arg.c_str()) == 0) {
                return window_id;
            }
        }
    }
    return GW::UI::WindowID_Count;
}

std::vector<ToolboxUIElement*> CHAT_CMD_FUNC(ChatCommands::MatchingWindows)
{
    std::vector<ToolboxUIElement*> ret;
    if (argc <= 1) {
        ret.push_back(&MainWindow::Instance());
    }
    else {
        const std::wstring arg = TextUtils::ToLower(argv[1]);
        if (arg == L"all") {
            for (ToolboxUIElement* window : GWToolbox::GetUIElements()) {
                ret.push_back(window);
            }
        }
        else if (!arg.empty()) {
            const std::string name = TextUtils::WStringToString(arg);
            for (ToolboxUIElement* window : GWToolbox::GetUIElements()) {
                if (TextUtils::ToLower(window->Name()).find(name) == 0) {
                    ret.push_back(window);
                }
            }
        }
    }
    return ret;
}

void CHAT_CMD_FUNC(ChatCommands::CmdShow)
{
    std::wstring cmd = L"toggle ";
    cmd.append(GetRemainingArgsWstr(message, 1));
    cmd.append(L" on");
    GW::Chat::SendChat('/', cmd.c_str());
}

void CHAT_CMD_FUNC(ChatCommands::CmdHide)
{
    std::wstring cmd = L"toggle ";
    cmd.append(GetRemainingArgsWstr(message, 1));
    cmd.append(L" off");
    GW::Chat::SendChat('/', cmd.c_str());
}

void CHAT_CMD_FUNC(ChatCommands::CmdToggle)
{
    if (argc < 2) {
        Log::ErrorW(L"语法无效：%s", message);
        return;
    }
    const std::wstring last_arg = TextUtils::ToLower(argv[argc - 1]);
    bool ignore_last_arg = false;
    enum ActionType : uint8_t { Toggle, On, Off } action = Toggle;
    if (last_arg == L"on" || last_arg == L"1" || last_arg == L"show") {
        action = On;
        ignore_last_arg = true;
    }
    else if (last_arg == L"off" || last_arg == L"0" || last_arg == L"hide") {
        action = Off;
        ignore_last_arg = true;
    }
    const std::wstring second_arg = TextUtils::ToLower(argv[1]);

    auto equipment_slot = GW::EquipmentType::Unknown;

    if (second_arg == L"cape") {
        equipment_slot = GW::EquipmentType::Cape;
    }
    else if (second_arg == L"head" || second_arg == L"helm") {
        equipment_slot = GW::EquipmentType::Helm;
    }
    else if (second_arg == L"costume_head") {
        equipment_slot = GW::EquipmentType::CostumeHeadpiece;
    }
    else if (second_arg == L"costume") {
        equipment_slot = GW::EquipmentType::CostumeBody;
    }
    if (equipment_slot != GW::EquipmentType::Unknown) {
        GW::EquipmentStatus state = GW::Items::GetEquipmentVisibility(equipment_slot);
        switch (action) {
            case On:
                state = GW::EquipmentStatus::AlwaysShow;
                break;
            case Off:
                state = GW::EquipmentStatus::AlwaysHide;
                break;
            default:
                state = state == GW::EquipmentStatus::AlwaysShow ? GW::EquipmentStatus::AlwaysHide : GW::EquipmentStatus::AlwaysShow;
                break;
        }
        ASSERT(GW::Items::SetEquipmentVisibility(equipment_slot, state));
        return;
    }
    const std::vector<ToolboxUIElement*> windows = MatchingWindows(status, message, ignore_last_arg ? argc - 1 : argc, argv);
    for (ToolboxUIElement* window : windows) {
        switch (action) {
            case On:
                window->visible = true;
                break;
            case Off:
                window->visible = false;
                break;
            default:
                window->visible = !window->visible;
                break;
        }
    }
    const GW::UI::WindowID gw_window = MatchingGWWindow(status, message, ignore_last_arg ? argc - 1 : argc, argv);
    if (gw_window < GW::UI::WindowID_Count) {
        bool set = true;
        switch (action) {
            case Off:
                set = false;
                break;
            case Toggle:
                set = !GetWindowPosition(gw_window)->visible();
                break;
        }
        SetWindowVisible(gw_window, set);
    }
}

void CHAT_CMD_FUNC(ChatCommands::CmdAfk)
{
    GW::FriendListMgr::SetFriendListStatus(GW::FriendStatus::Away);
    if (argc > 1) {
        const wchar_t* afk_msg = next_word(message);
        ChatSettings::SetAfkMessage(afk_msg);
    }
    else {
        ChatSettings::SetAfkMessage(L"");
    }
}


void CHAT_CMD_FUNC(ChatCommands::CmdSCWiki)
{
    if (argc == 1) {
        ShellExecuteW(nullptr, L"open", L"https://wiki.fbgmguild.com/wiki/Main_Page", nullptr, nullptr, SW_SHOWNORMAL);
    }
    else {
        // the buffer is large enough, because you can type only 120 characters at once in the chat.
        wchar_t link[256] = L"https://wiki.fbgmguild.com/index.php?search=";
        int i;
        for (i = 1; i < argc - 1; i++) {
            wcscat_s(link, argv[i]);
            wcscat_s(link, L"+");
        }
        wcscat_s(link, argv[i]);
        ShellExecuteW(nullptr, L"open", link, nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void CHAT_CMD_FUNC(ChatCommands::CmdLoad)
{
    if (argc == 1) return;

    std::wstring dir;
    if (!GW::MemoryMgr::GetPersonalDir(dir)) return;

    const std::filesystem::path build_file = std::format(L"{}/GUILD WARS/Templates/Skills/{}.txt", dir, argv[1]);
    std::string content;
    if (!Resources::ReadFile(build_file, content)) content = TextUtils::WStringToString(argv[1]);
    if (argc == 2) {
        GW::SkillbarMgr::LoadSkillTemplate(GW::Agents::GetControlledCharacterId(), content.c_str());
    }
    else if (argc == 3) {
        uint32_t ihero_number;
        if (TextUtils::ParseUInt(argv[2], &ihero_number)) {
            // @Robustness:
            // Check that the number is actually valid or make sure LoadSkillTemplate is safe
            if (0 < ihero_number && ihero_number <= 8) {
                GW::SkillbarMgr::LoadSkillTemplate(GW::Agents::GetHeroAgentID(ihero_number), content.c_str());
            }
        }
    }
}

void CHAT_CMD_FUNC(ChatCommands::CmdPingBuild)
{
    if (argc < 2) {
        return;
    }

    std::wstring dir;
    if (!GW::MemoryMgr::GetPersonalDir(dir)) return;

    for (auto arg_idx = 1; arg_idx < argc; arg_idx++) {
        const LPWSTR arg = argv[arg_idx];

        const std::filesystem::path build_file = std::format(L"{}/GUILD WARS/Templates/Skills/{}.txt", dir, arg);
        std::string content;
        if (!Resources::ReadFile(build_file, content)) return;

        GW::SkillbarMgr::SkillTemplate skill_template{};
        if (!DecodeSkillTemplate(skill_template, content.c_str())) {
            continue;
        }

        GW::Chat::SendChat('#', std::format(L"[{};{}]", arg, TextUtils::StringToWString(content)).c_str());
    }
}

void CHAT_CMD_FUNC(ChatCommands::CmdPingEquipment)
{
    if (!IsMapReady()) {
        return;
    }
    if (argc < 2) {
        Log::Error("缺少 /pingitem 的参数");
        return;
    }
    const auto equipped_items_bag = GW::Items::GetBag(GW::Constants::Bag::Equipped_Items);
    if (!equipped_items_bag) return;
    const std::wstring arg1 = TextUtils::ToLower(argv[1]);
    if (arg1 == L"weapon") {
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 1), 3);
    }
    else if (arg1 == L"offhand" || arg1 == L"shield") {
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 2), 3);
    }
    else if (arg1 == L"chest") {
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 3), 2);
    }
    else if (arg1 == L"legs") {
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 4), 2);
    }
    else if (arg1 == L"head") {
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 5), 2);
    }
    else if (arg1 == L"boots" || arg1 == L"feet") {
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 6), 2);
    }
    else if (arg1 == L"gloves" || arg1 == L"hands") {
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 7), 2);
    }
    else if (arg1 == L"weapons") {
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 1), 3);
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 2), 3);
    }
    else if (arg1 == L"armor") {
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 5), 2);
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 3), 2);
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 7), 2);
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 4), 2);
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 6), 2);
    }
    else if (arg1 == L"costume") {
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 8), 1);
        GameSettings::PingItem(GW::Items::GetItemBySlot(equipped_items_bag, 9), 1);
    }
    else {
        Log::Error("无法识别的 /pingitem %ls", argv[1]);
    }
}

void GetAchievements(const std::wstring& player_name)
{
    if (!(!player_name.empty() && player_name.size() < 20)) {
        return Log::Error("纪念堂命令的玩家名称无效");
    }
    hom_achievements = HallOfMonumentsAchievements{};
    HallOfMonumentsModule::AsyncGetAccountAchievements(player_name, &hom_achievements, OnAchievementsLoaded);
}

void CHAT_CMD_FUNC(ChatCommands::CmdHom)
{
    std::wstring player_name;
    if (argc > 1) {
        player_name = GetRemainingArgsWstr(message, 1);

        if (player_name == L"me") {
            player_name = GW::PlayerMgr::GetPlayerName(0);
            return GetAchievements(player_name);
        }
        if (player_name.find(L" ") != std::wstring::npos && player_name.size() < 20) {
            return GetAchievements(player_name);
        }
    }

    const auto target = GW::Agents::GetTargetAsAgentLiving();
    const auto player = target && target->IsPlayer() ? GW::PlayerMgr::GetPlayerByID(target->player_number) : nullptr;
    if (player) {
        player_name = player->name;
        return GetAchievements(player_name);
    }
}

void CHAT_CMD_FUNC(ChatCommands::CmdWithdraw)
{
    CmdGoldItemCommand(argc, argv, withdraw_syntax, GW::Items::WithdrawGold, InventoryManager::RefillUpToQuantity);
}

void CHAT_CMD_FUNC(ChatCommands::CmdDeposit)
{
    CmdGoldItemCommand(argc, argv, deposit_syntax, GW::Items::DepositGold, InventoryManager::StoreItems);
}

void CHAT_CMD_FUNC(ChatCommands::CmdResize)
{
    if (argc != 3) {
        Log::Error("语法为 /resize 宽度 高度");
        return;
    }
    int width, height;
    if (!(TextUtils::ParseInt(argv[1], &width) && TextUtils::ParseInt(argv[2], &height))) {
        Log::Error("语法为 /resize 宽度 高度");
        return;
    }
    const HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
    RECT rect;
    GetWindowRect(hwnd, &rect);
    MoveWindow(hwnd, rect.left, rect.top, width, height, TRUE);
}

void CHAT_CMD_FUNC(ChatCommands::CmdReapplyTitle)
{
    auto title_id = settings.default_title_id;
    const auto title_for_map = std::to_underlying(GW::Map::GetTitleForMap(GW::Map::GetMapID()));
    if (argc > 1) {
        if (!TextUtils::ParseUInt(argv[1], &title_id)) {
            Log::Error("语法：/title [title_id]");
            return;
        }
        goto apply;
    }
    if (!IsMapReady()) {
        return;
    }

    if (title_for_map != std::to_underlying(GW::Constants::TitleID::None)) {
        title_id = title_for_map;
    }
apply:
    GW::Constants::TitleID current_title = GW::PlayerMgr::GetActiveTitleId();
    if (title_id == CMDTITLE_KEEP_CURRENT && current_title != GW::Constants::TitleID::None) {
        title_id = std::to_underlying(current_title);
    }
    if (current_title != GW::Constants::TitleID::None) {
        GW::PlayerMgr::RemoveActiveTitle();
    }
    switch (title_id) {
        case CMDTITLE_REMOVE_CURRENT:
            break;
        default:
            if (title_id > std::to_underlying(GW::Constants::TitleID::Codex)) {
                Log::Error("无效的 title_id %d", title_id);
                return;
            }
            GW::PlayerMgr::SetActiveTitle(static_cast<GW::Constants::TitleID>(title_id));
            break;
    }
}



void CHAT_CMD_FUNC(ChatCommands::CmdVolume)
{
    const auto syntax = "语法：'/volume [master|music|background|effects|dialog|ui] [数值 (0-100)]'";
    wchar_t* value;
    GW::UI::NumberPreference pref;
    switch (argc) {
        case 2:
            pref = GW::UI::NumberPreference::VolMaster;
            value = argv[1];
            break;
        case 3: {
            const wchar_t* pref_str = argv[1];
            if (wcscmp(pref_str, L"master") == 0) {
                pref = GW::UI::NumberPreference::VolMaster;
            }
            else if (wcscmp(pref_str, L"music") == 0) {
                pref = GW::UI::NumberPreference::VolMusic;
            }
            else if (wcscmp(pref_str, L"background") == 0) {
                pref = GW::UI::NumberPreference::VolBackground;
            }
            else if (wcscmp(pref_str, L"effects") == 0) {
                pref = GW::UI::NumberPreference::VolEffect;
            }
            else if (wcscmp(pref_str, L"dialog") == 0) {
                pref = GW::UI::NumberPreference::VolDialog;
            }
            else if (wcscmp(pref_str, L"ui") == 0) {
                pref = GW::UI::NumberPreference::VolUi;
            }
            else {
                return Log::Error(syntax);
            }
            value = argv[2];
            break;
        }
        default:
            return Log::Error(syntax);
    }
    uint32_t value_dec;
    if (!TextUtils::ParseUInt(value, &value_dec, 10)) {
        return Log::Error(syntax);
    }
    if (value_dec > 100) {
        return Log::Error(syntax);
    }
    SetPreference(pref, value_dec);
}

void CHAT_CMD_FUNC(ChatCommands::CmdSetHardMode)
{
    if (!GW::GetWorldContext()->is_hard_mode_unlocked) {
        return;
    }
    GW::PartyMgr::SetHardMode(true);
}

void CHAT_CMD_FUNC(ChatCommands::CmdSetNormalMode)
{
    GW::PartyMgr::SetHardMode(false);
}