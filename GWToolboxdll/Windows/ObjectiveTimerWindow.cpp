#include <Windows/ObjectiveTimerWindow.h>
#include <Modules/Resources.h>
#include <Modules/GameSettings.h>
#include <Widgets/TimerWidget.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/AgentIDs.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <GWToolbox.h>
#include <Utils/GuiUtils.h>
#include <Logger.h>
#include <GWCA/Context/CharContext.h>
#include <Modules/ChatCommands.h>

constexpr uint32_t TIME_UNKNOWN = std::numeric_limits<uint32_t>::max();
unsigned int ObjectiveTimerWindow::ObjectiveSet::cur_ui_id = 0;

namespace {
    ObjectiveTimerWindow::Settings settings;

    int n_columns = 4;

    bool loading = false;

    bool map_load_pending = false;
    GW::Packet::StoC::InstanceLoadInfo* InstanceLoadInfo = nullptr;
    GW::Packet::StoC::InstanceLoadFile* InstanceLoadFile = nullptr;
    GW::Packet::StoC::InstanceTimer* InstanceTimer = nullptr;

    // @清理：这些 ID 应该是 wchar_t[] 类型，例如 L"\x8101\x273F"，而 DoA 事件应该使用 wchar_t 比较，而不是自定义方式。
    enum DoA_ObjId : uint32_t {
        Foundry = 0x273F,
        Veil,
        Gloom,
        City
    };

    // 与 Kanaxai 每个房间对话框中第一个字符匹配的十六进制值。
    //const enum kanaxai_room_dialogs { Room5 = 0x5336, Room6, Room8, Room10, Room12, Room13, Room14, Room15 };


    // 房间 1-4 无对话框
    // 房间 5："Fear not the darkness. It is already within you."
    constexpr wchar_t kanaxai_dialog_r5[] = L"\x5336\xBEB8\x8555\x7267";
    // 房间 6 "Is it comforting to know the source of your fears? Or do you fear more now that you see them in front of you."
    constexpr wchar_t kanaxai_dialog_r6[] = L"\x5337\xAA3A\xE96F\x3E34";
    // 房间 7 无对话框
    // 房间 8 "Even if you banish me from your sight, I will remain in your mind."
    constexpr wchar_t kanaxai_dialog_r8[] = L"\x5338\xFD69\xA162\x3A04";
    // 房间 9 无对话框
    // 房间 10 "You mortals may be here to defeat me, but acknowledging my presence only makes the nightmare grow stronger."
    constexpr wchar_t kanaxai_dialog_r10[] = L"\x5339\xA7BA\xC67B\x5D81";
    // 房间 11 无对话框
    // 房间 12 "So, you have passed through the depths of the Jade Sea, and into the nightmare realm. It is too bad that I must send you back from whence you came."
    constexpr wchar_t kanaxai_dialog_r12[] = L"\x533A\xED06\x815D\x5FFB";
    // 房间 13 "I am Kanaxai, creator of nightmares. Let me make yours into reality."
    constexpr wchar_t kanaxai_dialog_r13[] = L"\x533B\xCAA6\xFDA9\x3277";
    // 房间 14 "I will fill your hearts with visions of horror and despair that will haunt you for all of your days."
    constexpr wchar_t kanaxai_dialog_r14[] = L"\x533C\xDD33\xA330\x4E27";
    // Kanaxai "What gives you the right to enter my lair? I shall kill you for your
    // audacity, after I destroy your mind with my horrifying visions, of course."
    constexpr wchar_t kanaxai_dialog_r15[] = L"\x533D\x9EB1\x8BEE\x2637";

    const enum DoorID : uint32_t {
        // 门开启的 object_id。
        Deep_room_1_first = 12669,
        // 房间 1 完成 = 房间 5 开启
        Deep_room_1_second = 11692,
        // 房间 1 完成 = 房间 5 开启
        Deep_room_2_first = 54552,
        // 房间 2 完成 = 房间 5 开启
        Deep_room_2_second = 1760,
        // 房间 2 完成 = 房间 5 开启
        Deep_room_3_first = 45425,
        // 房间 3 完成 = 房间 5 开启
        Deep_room_3_second = 48290,
        // 房间 3 完成 = 房间 5 开启
        Deep_room_4_first = 40330,
        // 房间 4 完成 = 房间 5 开启
        Deep_room_4_second = 60114,
        // 房间 4 完成 = 房间 5 开启
        Deep_room_5 = 29594,
        // 房间 5 完成 = 房间 1,2,3,4,6 开启
        Deep_room_6 = 49742,
        // 房间 6 完成 = 房间 7 开启
        Deep_room_7 = 55680,
        // 房间 7 完成 = 房间 8 开启
        // 注意：房间 8（失败）到房间 10（蝎子），无门。
        Deep_room_9 = 99887,
        // 利维坦触发？
        Deep_room_11 = 28961,
        // 房间 11 的门始终开启。用于在进入范围时开始房间 11。

        DoA_foundry_entrance_r1 = 39534,
        DoA_foundry_r1_r2       = 6356,
        DoA_foundry_r2_r3       = 45276,
        DoA_foundry_r3_r4       = 55421,
        DoA_foundry_r4_r5       = 49719,
        DoA_foundry_r5_bb       = 45667,
        DoA_foundry_behind_bb   = 1731,
        DoA_city_entrance       = 63939,
        DoA_city_wall           = 54727,
        DoA_city_jadoth         = 64556,
        DoA_veil_360_left       = 13005,
        DoA_veil_360_middle     = 11772,
        DoA_veil_360_right      = 28851,
        DoA_veil_derv           = 56510,
        DoA_veil_ranger         = 4753,
        DoA_veil_trench_necro   = 46650,
        DoA_veil_trench_mes     = 29594,
        DoA_veil_trench_ele     = 49742,
        DoA_veil_trench_monk    = 55680,
        DoA_veil_trench_gloom   = 28961,
        DoA_veil_to_gloom       = 3,
        DoA_gloom_to_foundry    = 17955,
        DoA_gloom_rift          = 47069,
        // 不完全是门，关闭时 animation_type=9
    };

    void PrintTime(char* buf, const size_t size, const DWORD time, const bool show_ms = true)
    {
        if (time == TIME_UNKNOWN) {
            std::snprintf(buf, size, "%s", "--:--");
        }
        else {
            const DWORD sec = time / 1000;
            if (show_ms && settings.show_decimal) {
                snprintf(buf, size, "%02lu:%02lu.%1lu", sec / 60, sec % 60, time / 100 % 10);
            }
            else {
                snprintf(buf, size, "%02lu:%02lu", sec / 60, sec % 60);
            }
        }
    }

    void ComputeNColumns()
    {
        n_columns = 0 + (settings.show_start_column ? 1 : 0) + (settings.show_end_column ? 1 : 0) + (settings.show_time_column ? 1 : 0);
    }

    float GetTimestampWidth() { return 65.0f * ImGui::FontScale(); }

    float GetLabelWidth()
    {
        return std::max(GetTimestampWidth(), ImGui::GetContentRegionAvail().x - GetTimestampWidth() * n_columns);
    }

    bool runs_dirty = false;

    // Today's date, refreshed once a second in Draw; every loaded run compares its start day against it.
    int today_yday = -1;
    int today_year = 0;

    DWORD time_point_ms()
    {
        return static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    std::thread* websocket_server = nullptr;
    uWS::App* websocket_app = nullptr;
    enum WebsocketMode {
        None,
        LiveSplitOneJSON,
        LiveSplitServerCommand,
        Count
    };
    WebsocketMode websocket_mode = None;
    void EnableWebsocketServer(bool enable) {
        settings.websocket_server_port = std::max(settings.websocket_server_port, 0);
        if (!enable) {
            if (websocket_app) {
                websocket_app->close();
                delete websocket_app;
                websocket_app = nullptr;
            }
            if (websocket_server) {
                ASSERT(websocket_server->joinable());
                websocket_server->join();
                delete websocket_server;
                websocket_server = nullptr;
            }

        }
        else {
            if (websocket_server) return;
            EnableWebsocketServer(false);
            websocket_server = new std::thread([]() {
                // 应用程序需要在处理 WebSocket 连接的线程中创建
                websocket_app = new uWS::App();
                websocket_app
                    ->ws<int>(
                        "/*",
                        {/* 设置 */
                         .compression = uWS::SHARED_COMPRESSOR,
                         .maxPayloadLength = 16 * 1024,
                         .idleTimeout = 10,
                         .maxBackpressure = 1 * 1024 * 1024,
                         .sendPingsAutomatically = true,
                         /* 处理器 */
                         .upgrade = nullptr,
                         .open =
                             [](auto ws) {
                                 ws->subscribe("objective_events");
                             }
                        }
                    )
                    .listen(
                        settings.websocket_server_port,
                        [](auto* listen_socket) {
                            if (listen_socket) {
                                Log::Log("EnableWebsocketServer 正在监听端口 %d", settings.websocket_server_port);
                            }
                        }
                    )
                    .run();
            });
        }

    }

    void WebsocketSendMessage(std::string_view message) {
        if (websocket_app) {
            // @清理：应该从不同的线程发送到 WebSocket 吗？似乎不太对...
            if(websocket_mode == LiveSplitOneJSON) {
                std::string command = "{\"command\": \"" + std::string(message) + "\"}";
                websocket_app->publish("objective_events", command, uWS::OpCode::TEXT);
            } else {
                websocket_app->publish("objective_events", message, uWS::OpCode::TEXT);
            }
        }
    }
} // namespace

void ObjectiveTimerWindow::CheckIsMapLoaded()
{
    if (!map_load_pending || !InstanceLoadInfo || !InstanceLoadFile || !InstanceTimer) {
        return;
    }
    map_load_pending = false;
    // 使用 TimerWidget 的起始点，默认第一帧为 0% 加载，以符合 GWSCR 计时
    if (TimerWidget::Instance().GetStartPoint() != TIME_UNKNOWN && InstanceLoadInfo && InstanceLoadInfo->is_explorable) {
        AddObjectiveSet(static_cast<GW::Constants::MapID>(InstanceLoadInfo->map_id));
        Event(EventType::InstanceLoadInfo, InstanceLoadInfo->map_id);
    }
    if (InstanceLoadFile && InstanceLoadFile->map_fileID == 219215) {
        AddDoAObjectiveSet(InstanceLoadFile->spawn_point);
    }
}

void ObjectiveTimerWindow::Terminate() {
    ToolboxWindow::Terminate();
    for (size_t i = 0; i < 5000 && loading; i += 10) {
        Sleep(10);
    }
    ClearObjectiveSets();
    EnableWebsocketServer(false);
}
void ObjectiveTimerWindow::Initialize()
{
    ToolboxWindow::Initialize();
    SettingsRegistry::Register(this, settings);

    static GW::HookEntry PartyDefeated_Entry;
    static GW::HookEntry GameSrvTransfer_Entry;
    static GW::HookEntry InstanceLoadFile_Entry;
    static GW::HookEntry ObjectiveAdd_Entry;
    static GW::HookEntry ObjectiveUpdateName_Entry;
    static GW::HookEntry ObjectiveDone_Entry;
    static GW::HookEntry AgentUpdateAllegiance_Entry;
    static GW::HookEntry DoACompleteZone_Entry;
    static GW::HookEntry DisplayDialogue_Entry;
    static GW::HookEntry MessageServer_Entry;
    static GW::HookEntry InstanceLoadInfo_Entry;
    static GW::HookEntry ManipulateMapObject_Entry;
    static GW::HookEntry DungeonReward_Entry;
    static GW::HookEntry CountdownStart_Enty;

    // 用于创建或操作目标集的包钩子：
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyDefeated>(
        &PartyDefeated_Entry, [this](GW::HookStatus*, GW::Packet::StoC::PartyDefeated*) { StopObjectives(); });

    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::InstanceLoadInfo>(
        &InstanceLoadInfo_Entry,
        [this](GW::HookStatus*, const GW::Packet::StoC::InstanceLoadInfo* packet) {
            InstanceLoadInfo = new GW::Packet::StoC::InstanceLoadInfo;
            memcpy(InstanceLoadInfo, packet, sizeof(GW::Packet::StoC::InstanceLoadInfo));
            CheckIsMapLoaded();
            if (!GW::GetCharContext() || current_objective_set && current_objective_set->character_name != GW::GetCharContext()->player_name)
                StopObjectives();
        });
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::InstanceLoadFile>(
        &InstanceLoadFile_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::InstanceLoadFile* packet) {
            InstanceLoadFile = new GW::Packet::StoC::InstanceLoadFile;
            memcpy(InstanceLoadFile, packet, sizeof(GW::Packet::StoC::InstanceLoadFile));
            CheckIsMapLoaded();
        });
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::InstanceTimer>(
        &InstanceLoadFile_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::InstanceTimer* packet) {
            InstanceTimer = new GW::Packet::StoC::InstanceTimer;
            memcpy(InstanceTimer, packet, sizeof(GW::Packet::StoC::InstanceTimer));
            CheckIsMapLoaded();
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(
        &GameSrvTransfer_Entry, [this](GW::HookStatus*, GW::Packet::StoC::GameSrvTransfer* packet) {
            // 离开地图
            const GW::AreaInfo* info = GW::Map::GetMapInfo(static_cast<GW::Constants::MapID>(packet->map_id));
            if (!info) {
                return; // 应该始终有
            }

            static bool in_dungeon = false;
            const bool new_in_dungeon = info->type == GW::RegionType::Dungeon;
            if (in_dungeon && !new_in_dungeon) {
                // 从地城移动到外部
                StopObjectives();
            }
            else if (!packet->is_explorable) {
                // 传送到前哨站
                StopObjectives();
            }
            in_dungeon = new_in_dungeon;

            static uint32_t map_id = 0;
            Event(EventType::InstanceEnd, map_id);
            map_id = packet->map_id;
            // 重置加载地图变量（参见 CheckIsMapLoaded）
            if (InstanceLoadFile) {
                delete InstanceLoadFile;
            }
            InstanceLoadFile = nullptr;
            if (InstanceLoadInfo) {
                delete InstanceLoadInfo;
            }
            InstanceLoadInfo = nullptr;
            if (InstanceTimer) {
                delete InstanceTimer;
            }
            InstanceTimer = nullptr;
            map_load_pending = true;
        }, -5);
    // 触发事件的包钩子：
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageServer>(
        &MessageServer_Entry,
        [this](GW::HookStatus*, GW::Packet::StoC::MessageServer*) {
            const GW::Array<wchar_t>* buff = &GW::GetGameContext()->world->message_buff;
            if (!buff || !buff->valid() || !buff->size()) {
                return; // 消息缓冲区为空！？
            }
            const wchar_t* msg = buff->begin();
            // 注意：buff->size() 包含空终止符。所有 GW 字符串都以空终止，使用 wcslen 代替
            Event(EventType::ServerMessage, wcslen(msg), msg);
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DisplayDialogue>(
        &DisplayDialogue_Entry,
        [this](GW::HookStatus*, const GW::Packet::StoC::DisplayDialogue* packet) {
            // 注意：所有 GW 字符串都以空终止，使用 wcslen 避免检查所有 122 个字符
            Event(EventType::DisplayDialogue, wcslen(packet->message), packet->message);
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ManipulateMapObject>(
        &ManipulateMapObject_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::ManipulateMapObject* packet) {
            if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
                if (packet->animation_type == 16 && packet->animation_stage == 2) {
                    Event(EventType::DoorOpen, packet->object_id);
                }
                else if (packet->animation_type == 3 && packet->animation_stage == 2) {
                    Event(EventType::DoorClose, packet->object_id);
                }
                // TODO: 也许添加更通用的 ManipulateMapObject 包？
            }
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveUpdateName>(
        &ObjectiveUpdateName_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::ObjectiveUpdateName* packet) {
            Event(EventType::ObjectiveStarted, packet->objective_id);
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveDone>(
        &ObjectiveDone_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::ObjectiveDone* packet) {
            Event(EventType::ObjectiveDone, packet->objective_id);
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentUpdateAllegiance>(
        &AgentUpdateAllegiance_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::AgentUpdateAllegiance* packet) {
            if (const GW::Agent* agent = GW::Agents::GetAgentByID(packet->agent_id)) {
                if (const GW::AgentLiving* agentliving = agent->GetAsAgentLiving()) {
                    Event(EventType::AgentUpdateAllegiance, agentliving->player_number, packet->allegiance_bits);
                }
            }
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DoACompleteZone>(
        &DoACompleteZone_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::DoACompleteZone* packet) {
            if (packet->message[0] == 0x8101) {
                Event(EventType::DoACompleteZone, packet->message[1]);
            }
        });
    GW::StoC::RegisterPacketCallback(
        &CountdownStart_Enty, GAME_SMSG_INSTANCE_COUNTDOWN,
        [this](GW::HookStatus*, GW::Packet::StoC::PacketBase*) {
            Event(EventType::CountdownStart, std::to_underlying(GW::Map::GetMapID()));
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DungeonReward>(
        &DungeonReward_Entry, [this](GW::HookStatus*, GW::Packet::StoC::DungeonReward*) {
            Event(EventType::DungeonReward);
            if (ObjectiveSet* os = GetCurrentObjectiveSet()) {
                os->objectives.back()->SetDone();
                os->CheckSetDone();
            }
        });
    EnableWebsocketServer(websocket_mode != WebsocketMode::None);

}

void ObjectiveTimerWindow::Event(const EventType type, const uint32_t count, const wchar_t* msg) const
{
    Event(type, count, (uint32_t)msg);
}

void ObjectiveTimerWindow::Event(const EventType type, const uint32_t id1, const uint32_t id2) const
{
    if (ObjectiveSet* os = GetCurrentObjectiveSet()) {
        os->Event(type, id1, id2);

        if (show_debug_events) {
            switch (type) {
                case EventType::ServerMessage:
                case EventType::DisplayDialogue: {
                    const wchar_t* msg = (wchar_t*)id2;
                    Log::Info("事件: %d, %d, %x, %x, %x, %x, %x, %x", type, id1,
                              msg[0], msg[1], msg[2], msg[3], msg[4], msg[5]);
                }
                break;
                default:
                    Log::Info("事件: %d, %d, %d", type, id1, id2);
            }
        }
    }
}

void ObjectiveTimerWindow::AddObjectiveSet(const GW::Constants::MapID map_id)
{
    // clang-format off
    using namespace GW::Constants;
    switch (map_id) {
        // 精英区域：
        case MapID::Urgozs_Warren:
            AddUrgozObjectiveSet();
            break;
        case MapID::The_Deep:
            AddDeepObjectiveSet();
            break;
        case MapID::The_Fissure_of_Woe:
            AddFoWObjectiveSet();
            break;
        case MapID::The_Underworld:
            AddUWObjectiveSet();
            break;

        // 地城 - 1 层：
        case MapID::Ooze_Pit:
            AddDungeonObjectiveSet({MapID::Ooze_Pit});
            break;
        case MapID::Fronis_Irontoes_Lair_mission:
            AddDungeonObjectiveSet({MapID::Fronis_Irontoes_Lair_mission});
            break;
        case MapID::Secret_Lair_of_the_Snowmen:
            AddDungeonObjectiveSet({MapID::Secret_Lair_of_the_Snowmen});
            break;

        // 地城 - 2 层：
        case MapID::Sepulchre_of_Dragrimmar_Level_1:
            AddDungeonObjectiveSet({MapID::Sepulchre_of_Dragrimmar_Level_1, MapID::Sepulchre_of_Dragrimmar_Level_2});
            break;
        case MapID::Bogroot_Growths_Level_1:
            AddDungeonObjectiveSet({MapID::Bogroot_Growths_Level_1, MapID::Bogroot_Growths_Level_2});
            break;
        case MapID::Arachnis_Haunt_Level_1:
            AddDungeonObjectiveSet({MapID::Arachnis_Haunt_Level_1, MapID::Arachnis_Haunt_Level_2});
            break;

        // 地城 - 3 层：
        case MapID::Catacombs_of_Kathandrax_Level_1:
            AddDungeonObjectiveSet({MapID::Catacombs_of_Kathandrax_Level_1,
                                    MapID::Catacombs_of_Kathandrax_Level_2,
                                    MapID::Catacombs_of_Kathandrax_Level_3});
            break;
        case MapID::Rragars_Menagerie_Level_1:
            AddDungeonObjectiveSet({MapID::Rragars_Menagerie_Level_1,
                                    MapID::Rragars_Menagerie_Level_2,
                                    MapID::Rragars_Menagerie_Level_3});
            break;
        case MapID::Cathedral_of_Flames_Level_1:
            AddDungeonObjectiveSet({MapID::Cathedral_of_Flames_Level_1,
                                    MapID::Cathedral_of_Flames_Level_2,
                                    MapID::Cathedral_of_Flames_Level_3});
            break;
        case MapID::Darkrime_Delves_Level_1:
            AddDungeonObjectiveSet({MapID::Darkrime_Delves_Level_1,
                                    MapID::Darkrime_Delves_Level_2,
                                    MapID::Darkrime_Delves_Level_3});
            break;
        case MapID::Ravens_Point_Level_1:
            AddDungeonObjectiveSet({MapID::Ravens_Point_Level_1,
                                    MapID::Ravens_Point_Level_2,
                                    MapID::Ravens_Point_Level_3});
            break;
        case MapID::Vloxen_Excavations_Level_1:
            AddDungeonObjectiveSet({MapID::Vloxen_Excavations_Level_1,
                                    MapID::Vloxen_Excavations_Level_2,
                                    MapID::Vloxen_Excavations_Level_3});
            break;
        case MapID::Bloodstone_Caves_Level_1:
            AddDungeonObjectiveSet({MapID::Bloodstone_Caves_Level_1,
                                    MapID::Bloodstone_Caves_Level_2,
                                    MapID::Bloodstone_Caves_Level_3});
            break;
        case MapID::Shards_of_Orr_Level_1:
            AddDungeonObjectiveSet({MapID::Shards_of_Orr_Level_1,
                                    MapID::Shards_of_Orr_Level_2,
                                    MapID::Shards_of_Orr_Level_3});
            break;
        case MapID::Oolas_Lab_Level_1:
            AddDungeonObjectiveSet({MapID::Oolas_Lab_Level_1, MapID::Oolas_Lab_Level_2, MapID::Oolas_Lab_Level_3});
            break;
        case MapID::Heart_of_the_Shiverpeaks_Level_1:
            AddDungeonObjectiveSet({MapID::Heart_of_the_Shiverpeaks_Level_1,
                                    MapID::Heart_of_the_Shiverpeaks_Level_2,
                                    MapID::Heart_of_the_Shiverpeaks_Level_3});
            break;
        case MapID::Forsaken_Tunnels_Level1:
            AddDungeonObjectiveSet({MapID::Forsaken_Tunnels_Level1,
                                    MapID::Forsaken_Tunnels_Level2,
                                    MapID::Forsaken_Tunnels_Level3});
            break;
        case MapID::Forsaken_Tunnels_Presearing_Level1:
            AddDungeonObjectiveSet({MapID::Forsaken_Tunnels_Presearing_Level1,
                                    MapID::Forsaken_Tunnels_Presearing_Level2,
                                    MapID::Forsaken_Tunnels_Presearing_Level3});
            break;

        // 地城 - 5 层：
        case MapID::Frostmaws_Burrows_Level_1:
            AddDungeonObjectiveSet({MapID::Frostmaws_Burrows_Level_1,
                                    MapID::Frostmaws_Burrows_Level_2,
                                    MapID::Frostmaws_Burrows_Level_3,
                                    MapID::Frostmaws_Burrows_Level_4,
                                    MapID::Frostmaws_Burrows_Level_5});
            break;

        // 地城 - 不规则：
        case MapID::Slavers_Exile_Level_5:
            AddDungeonObjectiveSet({MapID::Slavers_Exile_Level_5});
            break;

        // 其他：
        case MapID::The_Underworld_PvP:
            if (const GW::AreaInfo* info = GW::Map::GetCurrentMapInfo()) {
                if (info->type == GW::RegionType::ExplorableZone) {
                    AddToPKObjectiveSet();
                }
            }
            break;
        default:
            break;
    }
    // clang-format on
    WebsocketSendMessage("reset");
    WebsocketSendMessage("start");

}

void ObjectiveTimerWindow::ObjectiveSet::StopObjectives()
{
    duration = GetDuration();
    active = false;
    for (Objective* obj : objectives) {
        switch (obj->status) {
            case Objective::Status::Started:
            case Objective::Status::Failed:
                obj->status = Objective::Status::Failed;
                failed = true;
                break;
            default:
                break;
        }
    }
}

void ObjectiveTimerWindow::AddObjectiveSet(ObjectiveSet* os)
{
    for (const auto& cos : objective_sets) {
        cos.second->StopObjectives();
        cos.second->need_to_collapse = true;
    }
    objective_sets.emplace(os->system_time, os);
    display_order_dirty = true;
    if (os->active) {
        current_objective_set = os;
    }
    runs_dirty = true;
}

void ObjectiveTimerWindow::AddDungeonObjectiveSet(const std::vector<GW::Constants::MapID>& levels, const uint32_t boss_model_id)
{
    const auto os = new ObjectiveSet;
    ASSERT(!levels.empty());
    os->name = Resources::GetMapName(levels[0])->string();
    for (size_t i = 0; i < levels.size(); i++) {
        char name[256];
        snprintf(name, sizeof(name), "第 %zu 层", i + 1);
        os->AddObjectiveAfterAll(new Objective(name))->AddStartEvent(EventType::InstanceLoadInfo, static_cast<uint32_t>(levels[i]));
    }
    os->objectives.front()->SetStarted();                         // 开始第一层
    os->objectives.back()->AddEndEvent(EventType::DungeonReward); // 最后一层以地城奖励结束
    if (boss_model_id) {
        os->objectives.back()->AddEndEvent(EventType::AgentUpdateAllegiance, boss_model_id, 0x6E6F6E63);
    }
    AddObjectiveSet(os);
}

void ObjectiveTimerWindow::AddDoAObjectiveSet(const GW::Vec2f spawn)
{
    constexpr int n_areas = 4;

    const auto starting_area = [&] {
        constexpr GW::Vec2f mallyx_spawn(-3931, -6214);
        constexpr GW::Vec2f area_spawns[] = {
            {-10514, 15231}, // 铸造厂
            {-18575, -8833}, // 城市
            {364, -10445},   // 纱幕
            {16034, 1244},   // 幽暗
        };
        double best_dist = GetDistance(spawn, mallyx_spawn);
        int starting_area = -1;
        for (auto i = 0; i < n_areas; i++) {
            const float dist = GetDistance(spawn, area_spawns[i]);
            if (best_dist > dist) {
                best_dist = dist;
                starting_area = i;
            }
        }
        return starting_area;
    }();

    if (starting_area == -1) {
        return; // 我们在打玛里克斯，不是 DoA！
    }

    const auto os = new ObjectiveSet;

    os->name = Resources::GetMapName(GW::Constants::MapID::Domain_of_Anguish)->string();

    const std::vector<std::function<void()>> add_doa_obj = {
        [&] {
            Objective* parent = os->AddObjectiveAfterAll(new Objective("铸造厂"))
                                  ->AddStartEvent(EventType::DoACompleteZone, Gloom)
                                  ->AddStartEvent(EventType::DoorOpen, DoA_foundry_entrance_r1)
                                  ->AddEndEvent(EventType::DoACompleteZone, Foundry);
            if (settings.show_detailed_objectives) {
                parent->AddChild(os->AddObjective(new Objective("房间 1"), 0)
                                   ->AddStartEvent(EventType::DoorClose, DoA_foundry_entrance_r1)
                                   ->AddEndEvent(EventType::DoorOpen, DoA_foundry_r1_r2));
                parent->AddChild(os->AddObjective(new Objective("房间 2"), 1)
                                   ->AddStartEvent(EventType::DoorClose, DoA_foundry_r1_r2)
                                   ->AddEndEvent(EventType::DoorOpen, DoA_foundry_r2_r3));
                parent->AddChild(os->AddObjective(new Objective("房间 3"), 2)
                                   ->AddStartEvent(EventType::DoorClose, DoA_foundry_r2_r3)
                                   ->AddEndEvent(EventType::DoorOpen, DoA_foundry_r3_r4));
                parent->AddChild(os->AddObjective(new Objective("房间 4"), 3)
                                   ->AddStartEvent(EventType::DoorClose, DoA_foundry_r3_r4)
                                   ->AddEndEvent(EventType::DoorOpen, DoA_foundry_r4_r5));

                // 也许计时蛇怪？（检查它们加入队伍）

                // 也许将 BB 事件改为使用对话框？"None shall escape. Prepare to die."
                // 将 BB 改为在门开启时开始，在狂怒生成时结束？
                parent->AddChild(os->AddObjective(new Objective("黑兽"), 4)
                                   ->AddStartEvent(EventType::DoorOpen, DoA_foundry_r5_bb)
                                   ->AddEndEvent(EventType::AgentUpdateAllegiance, 5221, 0x6E6F6E63)); // 所有三个相同

                // 0x8101 0x273D 0x98D8 0xB91A 0x47B8 狂怒：啊，你终于来了。我黑暗的主人告诉我
                // 我可能会有访客....
                parent->AddChild(os->AddObjective(new Objective("狂怒"), 5)
                                   ->AddStartEvent(EventType::DisplayDialogue, 4, L"\x8101\x273D\x98DB\xB91A")
                                   ->AddEndEvent(EventType::DoACompleteZone, Foundry));
            }
        },
        [&] {
            Objective* parent = os->AddObjectiveAfterAll(new Objective("城市"))
                                  ->AddStartEvent(EventType::DoACompleteZone, Foundry)
                                  ->AddEndEvent(EventType::DoACompleteZone, City);
            if (settings.show_detailed_objectives) {
                parent->AddChild(os->AddObjective(new Objective("外部"), 0)
                                   ->AddStartEvent(EventType::DoorOpen, DoA_city_entrance)
                                   ->AddEndEvent(EventType::DoorOpen, DoA_city_wall));
                parent->AddChild(os->AddObjective(new Objective("内部"), 1)
                                   ->AddStartEvent(EventType::DoorOpen, DoA_city_wall)
                                   ->AddEndEvent(EventType::DoACompleteZone, City));
            }

            // TODO: jadoth（在城市结束时开始，在宝箱生成时结束）
        },
        [&] {
            Objective* parent = os->AddObjectiveAfterAll(new Objective("纱幕"))
                                  ->AddStartEvent(EventType::DoACompleteZone, City)
                                  ->AddEndEvent(EventType::DoACompleteZone, Veil);
            if (settings.show_detailed_objectives) {
                parent->AddChild(os->AddObjective(new Objective("360"), 0)
                                   ->AddStartEvent(EventType::DoorOpen, DoA_veil_360_left)
                                   ->AddStartEvent(EventType::DoorOpen, DoA_veil_360_middle)
                                   ->AddStartEvent(EventType::DoorOpen, DoA_veil_360_right));
                parent->AddChild(os->AddObjective(new Objective("领主之下"), 1)
                                   ->AddStartEvent(EventType::DoorOpen, DoA_veil_ranger)
                                   ->AddStartEvent(EventType::DoorOpen, DoA_veil_derv));
                parent->AddChild(os->AddObjective(new Objective("领主"), 2)
                                   ->AddStartEvent(EventType::DoorOpen, DoA_veil_trench_gloom)
                                   ->AddStartEvent(EventType::DoorOpen, DoA_veil_trench_monk)
                                   ->AddStartEvent(EventType::DoorOpen, DoA_veil_trench_ele)
                                   ->AddStartEvent(EventType::DoorOpen, DoA_veil_trench_mes)
                                   ->AddStartEvent(EventType::DoorOpen, DoA_veil_trench_necro));
                parent->AddChild(os->AddObjective(new Objective("触须"), 3)
                                   ->AddStartEvent(EventType::DisplayDialogue, 4, L"\x8101\x34C1\x9FA1\xED8F\x1BE4")
                                   ->AddEndEvent(EventType::DoACompleteZone, Veil));
            }
        },
        [&] {
            Objective* parent = os->AddObjectiveAfterAll(new Objective("幽暗"))
                                  ->AddStartEvent(EventType::DoACompleteZone, Veil)
                                  ->AddEndEvent(EventType::DoACompleteZone, Gloom);
            if (settings.show_detailed_objectives) {
                parent->AddChild(os->AddObjective(new Objective("洞穴"), 0)
                                   ->AddStartEvent(EventType::DisplayDialogue, 4, L"\x8101\x5765\x9846\xA72B")
                                   ->AddEndEvent(EventType::DisplayDialogue, 4, L"\x8101\x5767\xA547\xB2C2"));

                // TODO: 裂隙可能无法在范围外触发

                // TODO: 死亡使者 ?

                parent->AddChild(os->AddObjective(new Objective("黑暗"), 1)
                                   ->AddStartEvent(EventType::DisplayDialogue, 4, L"\x8101\x273B\xB5DB\x8B13")
                                   ->AddEndEvent(EventType::DoACompleteZone, Gloom));
            }
        }
    };

    for (auto i = 0; i < n_areas; i++) {
        const auto idx = (starting_area + i) % n_areas;
        add_doa_obj[idx]();
    }

    os->objectives.front()->SetStarted();
    AddObjectiveSet(os);
}

void ObjectiveTimerWindow::AddUrgozObjectiveSet()
{
    const auto os = new ObjectiveSet;
    os->name = Resources::GetMapName(GW::Constants::MapID::Urgozs_Warren)->string();
    os->AddObjective(new Objective("区域 1 | 虚弱"))->SetStarted();
    os->AddObjectiveAfterAll(new Objective("区域 2 | 生命吸取"))->AddStartEvent(EventType::DoorOpen, 45420);
    os->AddObjectiveAfterAll(new Objective("区域 3 | 杠杆"))->AddStartEvent(EventType::DoorOpen, 11692);
    os->AddObjectiveAfterAll(new Objective("区域 4 | 桥梁狼群"))->AddStartEvent(EventType::DoorOpen, 54552);
    os->AddObjectiveAfterAll(new Objective("区域 5 | 更多狼群"))->AddStartEvent(EventType::DoorOpen, 1760);
    os->AddObjectiveAfterAll(new Objective("区域 6 | 能量吸取"))->AddStartEvent(EventType::DoorOpen, 40330);
    os->AddObjectiveAfterAll(new Objective("区域 7 | 力竭"))->AddStartEvent(EventType::DoorOpen, 60114);
    os->AddObjectiveAfterAll(new Objective("区域 8 | 支柱"))->AddStartEvent(EventType::DoorOpen, 37191);
    os->AddObjectiveAfterAll(new Objective("区域 9 | 血饮者"))->AddStartEvent(EventType::DoorOpen, 35500);
    os->AddObjectiveAfterAll(new Objective("区域 10 | 桥梁"))->AddStartEvent(EventType::DoorOpen, 34278);
    os->AddObjectiveAfterAll(new Objective("区域 11 | 乌尔戈兹"))
      ->AddStartEvent(EventType::DoorOpen, 15529)
      ->AddStartEvent(EventType::DoorOpen, 45631)
      ->AddStartEvent(EventType::DoorOpen, 53071)
      ->AddEndEvent(EventType::ServerMessage, 6, L"\x6C9C\x0\x0\x0\x0\x2810")
      ->AddEndEvent(EventType::ServerMessage, 6, L"\x6C9C\x0\x0\x0\x0\x1488");

    AddObjectiveSet(os);
}

void ObjectiveTimerWindow::AddDeepObjectiveSet()
{
    const auto os = new ObjectiveSet;
    os->name = Resources::GetMapName(GW::Constants::MapID::The_Deep)->string();
    os->AddObjective(new Objective("房间 1 | 抚慰"))
      ->SetStarted()
      ->AddEndEvent(EventType::DoorOpen, Deep_room_1_first)
      ->AddEndEvent(EventType::DoorOpen, Deep_room_1_second);
    os->AddObjective(new Objective("房间 2 | 死亡"))
      ->SetStarted()
      ->AddEndEvent(EventType::DoorOpen, Deep_room_2_first)
      ->AddEndEvent(EventType::DoorOpen, Deep_room_2_second);
    os->AddObjective(new Objective("房间 3 | 投降"))
      ->SetStarted()
      ->AddEndEvent(EventType::DoorOpen, Deep_room_3_first)
      ->AddEndEvent(EventType::DoorOpen, Deep_room_3_second);
    os->AddObjective(new Objective("房间 4 | 暴露"))
      ->SetStarted()
      ->AddEndEvent(EventType::DoorOpen, Deep_room_4_first)
      ->AddEndEvent(EventType::DoorOpen, Deep_room_4_second);
    os->AddObjective(new Objective("房间 5 | 痛苦"))
      ->AddStartEvent(EventType::DoorOpen, Deep_room_1_first)
      ->AddStartEvent(EventType::DoorOpen, Deep_room_1_second)
      ->AddStartEvent(EventType::DoorOpen, Deep_room_2_first)
      ->AddStartEvent(EventType::DoorOpen, Deep_room_2_second)
      ->AddStartEvent(EventType::DoorOpen, Deep_room_3_first)
      ->AddStartEvent(EventType::DoorOpen, Deep_room_3_second)
      ->AddStartEvent(EventType::DoorOpen, Deep_room_4_first)
      ->AddStartEvent(EventType::DoorOpen, Deep_room_4_second);

    os->AddObjectiveAfterAll(new Objective("房间 6 | 倦怠"))->AddStartEvent(EventType::DoorOpen, Deep_room_5);
    os->AddObjectiveAfterAll(new Objective("房间 7 | 衰竭"))->AddStartEvent(EventType::DoorOpen, Deep_room_6);

    // 8 和 9 合并，因为它们之间没有边界
    os->AddObjectiveAfterAll(new Objective("房间 8-9 | 失败/暗影"))
      ->AddStartEvent(EventType::DoorOpen, Deep_room_7);

    os->AddObjectiveAfterAll(new Objective("房间 10 | 蝎子"))
      ->AddStartEvent(EventType::DisplayDialogue, 4, kanaxai_dialog_r10);
    os->AddObjectiveAfterAll(new Objective("房间 11 | 恐惧"))->AddStartEvent(EventType::DoorOpen, Deep_room_11);
    os->AddObjectiveAfterAll(new Objective("房间 12 | 衰竭"))
      ->AddStartEvent(EventType::DisplayDialogue, 4, kanaxai_dialog_r12);
    // 13 和 14 合并，因为它们之间没有边界
    os->AddObjectiveAfterAll(new Objective("房间 13-14 | 腐朽/折磨"))
      ->AddStartEvent(EventType::DisplayDialogue, 4, kanaxai_dialog_r13);
    os->AddObjectiveAfterAll(new Objective("房间 15 | 卡纳克赛"))
      ->AddStartEvent(EventType::DisplayDialogue, 4, kanaxai_dialog_r15)
      ->AddEndEvent(EventType::ServerMessage, 6, L"\x6D4D\x0\x0\x0\x0\x2810")
      ->AddEndEvent(EventType::ServerMessage, 6, L"\x6D4D\x0\x0\x0\x0\x1488");
    AddObjectiveSet(os);
}

void ObjectiveTimerWindow::AddFoWObjectiveSet()
{
    const auto os = new ObjectiveSet;
    os->name = Resources::GetMapName(GW::Constants::MapID::The_Fissure_of_Woe)->string();

    os->AddQuestObjective("ToC", 309);
    os->AddQuestObjective("哀嚎之主", 310);
    os->AddQuestObjective("狮鹫", 311);
    os->AddQuestObjective("防御", 312);
    os->AddQuestObjective("熔炉", 313);
    os->AddQuestObjective("曼泽斯", 314);
    os->AddQuestObjective("恢复", 315);
    os->AddQuestObjective("科拜", 316);
    os->AddQuestObjective("ToS", 317);
    os->AddQuestObjective("燃烧森林", 318);
    os->AddQuestObjective("狩猎", 319);
    AddObjectiveSet(os);
}

void ObjectiveTimerWindow::AddUWObjectiveSet()
{
    const auto os = new ObjectiveSet;
    os->name = Resources::GetMapName(GW::Constants::MapID::The_Underworld)->string();
    os->AddQuestObjective("密室", 146);
    os->AddQuestObjective("恢复", 147);
    os->AddQuestObjective("护送", 148);
    os->AddQuestObjective("UWG", 149);
    os->AddQuestObjective("山谷", 150);
    os->AddQuestObjective("荒原", 151);
    os->AddQuestObjective("深坑", 152);
    os->AddQuestObjective("平原", 153);
    os->AddQuestObjective("山峦", 154);
    os->AddQuestObjective("水池", 155);
    os->AddObjective(new Objective("杜姆"))
      ->AddStartEvent(EventType::AgentUpdateAllegiance, GW::Constants::ModelID::UW::Dhuum, 0x6D6F6E31)
      ->AddEndEvent(EventType::ObjectiveDone, 157);
    AddObjectiveSet(os);
}

void ObjectiveTimerWindow::AddToPKObjectiveSet()
{
    // 预取顶级地图名称
    Resources::GetMapName(GW::Constants::MapID::Scarred_Earth);
    Resources::GetMapName(GW::Constants::MapID::The_Underworld_PvP);
    Resources::GetMapName(GW::Constants::MapID::The_Courtyard);
    Resources::GetMapName(GW::Constants::MapID::Tomb_of_the_Primeval_Kings);
    Resources::GetMapName(GW::Constants::MapID::The_Hall_of_Heroes);

    // 排队到下一线程以允许地图名称加载
    GW::GameThread::Enqueue(
        []() {
            const auto os = new ObjectiveSet;
            os->name = Resources::GetMapName(GW::Constants::MapID::Tomb_of_the_Primeval_Kings)->string();
            os->AddObjective(new Objective(Resources::GetMapName(GW::Constants::MapID::The_Underworld_PvP)->string().c_str()))
                ->SetStarted()
                ->AddStartEvent(EventType::InstanceLoadInfo, std::to_underlying(GW::Constants::MapID::The_Underworld_PvP))
                ->AddEndEvent(EventType::CountdownStart, std::to_underlying(GW::Constants::MapID::The_Underworld_PvP));
            os->AddObjective(new Objective(Resources::GetMapName(GW::Constants::MapID::Scarred_Earth)->string().c_str()))
                ->AddStartEvent(EventType::InstanceLoadInfo, std::to_underlying(GW::Constants::MapID::Scarred_Earth))
                ->AddEndEvent(EventType::CountdownStart, std::to_underlying(GW::Constants::MapID::Scarred_Earth));
            os->AddObjective(new Objective(Resources::GetMapName(GW::Constants::MapID::The_Courtyard)->string().c_str()))
                ->AddStartEvent(EventType::InstanceLoadInfo, std::to_underlying(GW::Constants::MapID::The_Courtyard))
                ->AddEndEvent(EventType::CountdownStart, std::to_underlying(GW::Constants::MapID::The_Courtyard));
            os->AddObjective(new Objective(Resources::GetMapName(GW::Constants::MapID::The_Hall_of_Heroes)->string().c_str()))
                ->AddStartEvent(EventType::InstanceLoadInfo, std::to_underlying(GW::Constants::MapID::The_Hall_of_Heroes))
                ->AddEndEvent(EventType::CountdownStart, std::to_underlying(GW::Constants::MapID::The_Hall_of_Heroes));
            Instance().AddObjectiveSet(os);
        },
        true
    );



}

void ObjectiveTimerWindow::Update(float)
{
    if (current_objective_set && current_objective_set->active) {
        current_objective_set->Update();
    }
    if (runs_dirty && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
        SaveRuns(); // 在地图加载之间保存记录
    }
}

void ObjectiveTimerWindow::Draw(IDirect3DDevice9*)
{
    if (loading) {
        return;
    }
    static DWORD today_refreshed_at = 0;
    if (const DWORD tick = GetTickCount(); today_yday < 0 || tick - today_refreshed_at >= 1000) {
        today_refreshed_at = tick;
        const time_t now = time(nullptr);
        const tm* nowinfo = localtime(&now);
        today_yday = nowinfo->tm_yday;
        today_year = nowinfo->tm_year;
    }
    if (clear_cached_times) {
        for (const auto& [_, os] : objective_sets) {
            os->InvalidateCachedStrings();
        }
        clear_cached_times = false;
    }
    if (visible && !loading) {
        ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
            if (objective_sets.empty()) {
                ImGui::Text("进入痛苦领域、火岛、地下世界、深渊、乌尔戈兹或地城以开始计时");
            }
            else {
                if (display_order_dirty) {
                    display_order.assign(objective_sets.size(), nullptr);
                    size_t n = display_order.size();
                    for (const auto& [_, os] : objective_sets) {
                        display_order[--n] = os;
                    }
                    display_order_dirty = false;
                }

                const float row_height = ImGui::GetFrameHeight();
                const float spacing = ImGui::GetStyle().ItemSpacing.y;
                float skipped_height = 0.f;
                const auto flush_skipped = [&skipped_height] {
                    if (skipped_height > 0.f) {
                        ImGui::Dummy(ImVec2(1.f, skipped_height));
                        skipped_height = 0.f;
                    }
                };
                for (size_t i = 0; i < display_order.size(); i++) {
                    auto* os = display_order[i];
                    if (os->IsFilteredOut()) {
                        continue;
                    }
                    if (os->IsCollapsedRow()) {
                        const float y = ImGui::GetCursorScreenPos().y + (skipped_height > 0.f ? skipped_height + spacing : 0.f);
                        if (!ImGui::IsRectVisible({0.f, y}, {1.f, y + row_height})) {
                            skipped_height = skipped_height > 0.f ? skipped_height + spacing + row_height : row_height;
                            continue;
                        }
                    }
                    flush_skipped();
                    if (!os->Draw()) {
                        objective_sets.erase(os->system_time);
                        delete os;
                        display_order_dirty = true;
                        break; // we're skipping the rest of this frame; NBD
                    }
                }
                flush_skipped();
            }
        }
        ImGui::End();
    }

    if (settings.show_current_run_window && current_objective_set) {
        ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
        char buf[256];
        sprintf(buf, "%s - %s###ObjectiveTimerCurrentRun", current_objective_set->name.c_str(), current_objective_set->GetDurationStr());

        if (ImGui::Begin(buf, &settings.show_current_run_window, GetWinFlags())) {
            ImGui::PushID(static_cast<int>(current_objective_set->ui_id));
            for (Objective* objective : current_objective_set->objectives) {
                objective->Draw();
            }
            ImGui::PopID();
        }

        ImGui::End();
    }
}

ObjectiveTimerWindow::ObjectiveSet* ObjectiveTimerWindow::GetCurrentObjectiveSet() const
{
    if (objective_sets.empty()) {
        return nullptr;
    }
    if (!current_objective_set || !current_objective_set->active) {
        return nullptr;
    }
    return current_objective_set;
}

void ObjectiveTimerWindow::DrawSettingsInternal()
{
    ImGui::Separator();
    ImGui::StartSpacedElements(275.f);
    ImGui::NextSpacedElement();
    // Latched, not assigned: Draw consumes it, and it may run before this settings pass.
    clear_cached_times |= ImGui::Checkbox("Show second decimal", &settings.show_decimal);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("显示“开始”列", &settings.show_start_column);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("显示“结束”列", &settings.show_end_column);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("显示“用时”列", &settings.show_time_column);
    ImGui::NextSpacedElement();
    ImGui::CheckboxWithHelp("显示详细目标", &settings.show_detailed_objectives, "目前仅影响痛苦领域目标");
    ImGui::NextSpacedElement();
    ImGui::CheckboxWithHelp("调试：记录事件", &show_debug_events,
        "将在聊天中输出目标计时器使用的事件。\n用于调试和请求添加更多内容");
    ImGui::NextSpacedElement();
    clear_cached_times |= ImGui::Checkbox("Show run start date/time", &settings.show_start_date_time);
    ImGui::NextSpacedElement();
    ImGui::CheckboxWithHelp("在独立窗口中显示当前记录", &settings.show_current_run_window, "通过聊天切换：/tb_setting show_current_run_window");
    ImGui::NextSpacedElement();
    if (ImGui::Checkbox("保存/加载记录到磁盘", &settings.save_to_disk)) {
        SaveRuns();
    }
    ImGui::ShowHelp(
        "将记录以 JSON 格式保存到磁盘，并在启动 GWToolbox 时从磁盘加载过往记录。");
    ImGui::NextSpacedElement();
    ImGui::CheckboxWithHelp("显示过往记录", &settings.show_past_runs, "在目标计时器窗口中显示以前日期的记录。");
    ImGui::NextSpacedElement();
    ImGui::CheckboxWithHelp("完成时自动 /age", &settings.auto_send_age,
        "当最终目标完成时，立即向游戏服务器发送 /age 命令以获取服务器端完成时间。");
    ComputeNColumns();

    bool enable_websocket_server = websocket_mode != WebsocketMode::None;
    if (ImGui::Checkbox("启用 LiveSplit WebSocket 服务器", &enable_websocket_server)) {
        websocket_mode = enable_websocket_server ? WebsocketMode::LiveSplitOneJSON : WebsocketMode::None;
        EnableWebsocketServer(enable_websocket_server);
    }
    if (enable_websocket_server) {
        ImGui::Indent();
        if (ImGui::InputInt("LiveSplit WebSocket 服务器端口", &settings.websocket_server_port)) {
            EnableWebsocketServer(false);
            EnableWebsocketServer(enable_websocket_server);
        }
        ImGui::Text("LiveSplit Server status: %s", websocket_app && websocket_server ? "Running" : "Stopped");
        if (websocket_app && websocket_server) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "(端口 %d)", settings.websocket_server_port);
        }
        if (ImGui::SmallButton("重启")) {
            EnableWebsocketServer(false);
            EnableWebsocketServer(enable_websocket_server);
        }
        ImGui::RadioButton("LiveSplit One JSON 格式", (int*)&websocket_mode, static_cast<int>(WebsocketMode::LiveSplitOneJSON));
        ImGui::RadioButton("LiveSplit 服务器命令格式", (int*)&websocket_mode, static_cast<int>(WebsocketMode::LiveSplitServerCommand));
        ImGui::Unindent();
    }

}

void ObjectiveTimerWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWindow::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
    auto stored_websocket_mode = (uint32_t)websocket_mode;
    if (!doc.Get(Name(), VAR_NAME(websocket_mode), stored_websocket_mode)) {
        stored_websocket_mode = (uint32_t)legacy->GetLongValue(Name(), VAR_NAME(websocket_mode), (long)websocket_mode);
    }
    if (stored_websocket_mode >= (uint32_t)WebsocketMode::Count)
        stored_websocket_mode = (uint32_t)WebsocketMode::None;
    websocket_mode = (WebsocketMode)stored_websocket_mode;
    ComputeNColumns();
    LoadRuns();
}

void ObjectiveTimerWindow::SaveSettings(SettingsDoc& doc)
{
    ToolboxWindow::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
    doc.Set(Name(), VAR_NAME(websocket_mode), (uint32_t)websocket_mode);
    SaveRuns();
}

void ObjectiveTimerWindow::LoadRuns()
{
    if (!settings.save_to_disk) {
        return;
    }
    // 由于这会进行大量文件读取和 JSON 解码，放在单独的线程中；可能会延迟渲染数秒
    while (loading) {
        Sleep(10);
    }
    loading = true;
    Resources::EnqueueWorkerTask([] {
        ObjectiveTimerWindow& instance = Instance();
        // ClearObjectiveSets();
        Resources::EnsureFolderExists(Resources::GetPath(L"runs"));
        WIN32_FIND_DATAW FindFileData;
        size_t max_objectives_in_memory = 200;
        std::wstring file_match = Resources::GetPath(L"runs", L"ObjectiveTimerRuns_*.json");
        std::wstring filename;
        std::set<std::wstring> obj_timer_files;
        HANDLE hFind = FindFirstFileW(file_match.c_str(), &FindFileData);
        if (hFind != INVALID_HANDLE_VALUE) {
            obj_timer_files.insert(FindFileData.cFileName);
            while (FindNextFileW(hFind, &FindFileData) != 0) {
                obj_timer_files.insert(FindFileData.cFileName);
            }
        }
        FindClose(hFind);

        for (auto it = obj_timer_files.rbegin(); it != obj_timer_files.rend() && instance.objective_sets.size() < max_objectives_in_memory; ++it) {
            try {
                std::ifstream file;
                std::wstring fn = Resources::GetPath(L"runs", *it);
                file.open(fn);
                if (file.is_open()) {
                    std::stringstream ss;
                    ss << file.rdbuf();
                    std::vector<ObjectiveSet::Serialized> os_arr;
                    constexpr glz::opts opts{.error_on_unknown_keys = false};
                    if (auto ec = glz::read<opts>(os_arr, ss.str()); !ec) {
                        for (const auto& elem : os_arr) {
                            ObjectiveSet* os = ObjectiveSet::FromJson(elem);
                            if (instance.objective_sets.contains(os->system_time)) {
                                delete os;
                                continue; // 不加载已存在的记录
                            }
                            os->StopObjectives();
                            os->need_to_collapse = true;
                            os->from_disk = true;
                            instance.objective_sets.emplace(os->system_time, os);
                            instance.display_order_dirty = true;
                        }
                    }
                    file.close();
                }
            } catch (const std::exception&) {
                Log::Error("从 JSON 加载 ObjectiveSets 失败");
            }
        }
        loading = false;
    });
}

void ObjectiveTimerWindow::SaveRuns()
{
    if (!settings.save_to_disk || objective_sets.empty()) {
        return;
    }
    while (loading) {
        Sleep(10);
    }
    loading = true;
    Resources::EnqueueWorkerTask([] {
        ObjectiveTimerWindow& instance = Instance();
        Resources::EnsureFolderExists(Resources::GetPath(L"runs"));
        std::map<std::wstring, std::vector<ObjectiveSet*>> objective_sets_by_file;
        wchar_t filename[36];
        for (auto& os : instance.objective_sets) {
            if (os.second->from_disk) {
                continue; // 无需重新保存已有记录
            }
            time_t tt = os.second->system_time;
            const tm* structtime = gmtime(&tt);
            if (!structtime) {
                continue;
            }
            swprintf(filename, 36, L"ObjectiveTimerRuns_%02d-%02d-%02d.json", structtime->tm_year + 1900, structtime->tm_mon + 1, structtime->tm_mday);
            objective_sets_by_file[filename].push_back(os.second);
        }
        for (auto& it : objective_sets_by_file) {
            try {
                std::ofstream file;
                file.open(Resources::GetPath(L"runs", it.first));
                if (file.is_open()) {
                    std::vector<ObjectiveSet::Serialized> os_arr;
                    os_arr.reserve(it.second.size());
                    for (const auto os : it.second) {
                        os_arr.push_back(os->ToJson());
                    }
                    file << glz::write_json(os_arr).value_or(std::string{}) << std::endl;
                    file.close();
                }
            } catch (const std::exception&) {
                Log::Error("保存 ObjectiveSets 到 JSON 失败");
            }
        }
        runs_dirty = false;
        loading = false;
    });
}

void ObjectiveTimerWindow::ClearObjectiveSets()
{
    for (const auto& os : objective_sets) {
        delete os.second;
    }
    objective_sets.clear();
    display_order.clear();
    display_order_dirty = true;
}

void ObjectiveTimerWindow::StopObjectives()
{
    if (current_objective_set) {
        current_objective_set->StopObjectives();
        WebsocketSendMessage("reset");
    }
    current_objective_set = nullptr;
}


// =============================================================================

ObjectiveTimerWindow::Objective::Objective(const char* _name)
    : start(TIME_UNKNOWN),
      done(TIME_UNKNOWN),
      duration(TIME_UNKNOWN)
{
    std::snprintf(name, _countof(name), "%s", _name);
}

ObjectiveTimerWindow::Objective* ObjectiveTimerWindow::Objective::AddStartEvent(
    const EventType et, const uint32_t id1, const uint32_t id2)
{
    start_events.emplace_back<Event>({et, id1, id2});
    return this;
}

ObjectiveTimerWindow::Objective* ObjectiveTimerWindow::Objective::AddStartEvent(
    const EventType et, const uint32_t count, const wchar_t* msg)
{
    start_events.emplace_back<Event>({et, count, (uint32_t)msg});
    return this;
}

ObjectiveTimerWindow::Objective* ObjectiveTimerWindow::Objective::AddEndEvent(
    const EventType et, const uint32_t id1, const uint32_t id2)
{
    end_events.emplace_back<Event>({et, id1, id2});
    return this;
}

ObjectiveTimerWindow::Objective* ObjectiveTimerWindow::Objective::AddEndEvent(
    const EventType et, const uint32_t count, const wchar_t* msg)
{
    end_events.emplace_back<Event>({et, count, (uint32_t)msg});
    return this;
}

ObjectiveTimerWindow::Objective* ObjectiveTimerWindow::Objective::SetStarted()
{
    if (IsStarted()) {
        return this;
    }
    start_time_point = time_point_ms();                      // 运行开始时间点
    start = start_time_point - parent->run_start_time_point; // 从运行开始起的毫秒数
    PrintTime(cached_start, sizeof(cached_start), start);
    status = Status::Started;
    return this;
}

ObjectiveTimerWindow::Objective* ObjectiveTimerWindow::Objective::SetDone()
{
    if (status == Status::Completed) {
        return this;
    }
    if (done == TIME_UNKNOWN) {
        done_time_point = time_point_ms();
        // 注意：目标可能没有触发开始点。
        done = done_time_point - parent->run_start_time_point;
    }
    PrintTime(cached_done, sizeof(cached_done), done);

    // 可能在目标“开始”之前调用此方法。
    // 适用于没有持续时间的情况，我们保持 start == TIME_UNKNOWN。
    if (start != TIME_UNKNOWN) {
        duration = done - start;
        PrintTime(cached_duration, sizeof(cached_duration), duration);
    }

    status = Status::Completed;
    runs_dirty = true;
    for (auto* obj : children) {
        obj->SetDone();
    }
    return this;
}

ObjectiveTimerWindow::Objective* ObjectiveTimerWindow::Objective::AddChild(Objective* child)
{
    children.push_back(child);
    child->indent = indent + 1;
    return children.back();
}

bool ObjectiveTimerWindow::Objective::IsStarted() const { return IsDone() || start != TIME_UNKNOWN; }
bool ObjectiveTimerWindow::Objective::IsDone() const { return done != TIME_UNKNOWN; }

const char* ObjectiveTimerWindow::Objective::GetEndTimeStr()
{
    if (status < Status::Completed) {
        return "--:--";
    }
    if (!cached_done[0]) {
        PrintTime(cached_done, sizeof(cached_done), done, settings.show_decimal);
    }
    return cached_done;
}

const char* ObjectiveTimerWindow::Objective::GetStartTimeStr()
{
    if (status < Status::Started) {
        return "--:--";
    }
    if (!cached_start[0]) {
        PrintTime(cached_start, sizeof(cached_start), start, settings.show_decimal);
    }
    return cached_start;
}

const char* ObjectiveTimerWindow::Objective::GetDurationStr()
{
    if (status < Status::Started) {
        return "--:--";
    }
    if (!cached_duration[0] || status == Status::Started) {
        PrintTime(cached_duration, sizeof(cached_duration), GetDuration(), settings.show_decimal);
    }
    return cached_duration;
}

DWORD ObjectiveTimerWindow::Objective::GetDuration()
{
    switch (status) {
        case Status::Started:
            ASSERT(start != TIME_UNKNOWN);
            return duration = time_point_ms() - start_time_point;
        case Status::Completed:
            ASSERT(done != TIME_UNKNOWN);
        // 注意：如果后续目标已开始，目标可能被标记为完成而未开始。
            if (start != TIME_UNKNOWN) {
                return duration = done - start;
            }
    }
    return duration;
}

void ObjectiveTimerWindow::Objective::Update()
{
    // 缓存时间等移至 Draw 和 GetDuration 函数
}

void ObjectiveTimerWindow::Objective::InvalidateCachedStrings()
{
    cached_start[0] = cached_done[0] = cached_duration[0] = '\0';
    for (Objective* child : children) {
        child->InvalidateCachedStrings();
    }
}

void ObjectiveTimerWindow::Objective::Draw()
{
    switch (status) {
        case Status::NotStarted:
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            break;
        case Status::Started:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            break;
        case Status::Completed:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
            break;
        case Status::Failed:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
            break;
        default:
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            break;
    }
    auto& style = ImGui::GetStyle();
    style.ButtonTextAlign.x = 0.0f;
    const float label_width = GetLabelWidth();
    for (auto i = 0; i < indent; i++) {
        ImGui::Indent();
    }
    if (ImGui::Button(name, ImVec2(label_width - indent * style.IndentSpacing, 0))) {
        char buf[256];
        sprintf(buf, "[%s] ~ 开始: %s ~ 结束: %s ~ 用时: %s", name, GetStartTimeStr(), GetEndTimeStr(), GetDurationStr());
        GW::Chat::SendChat('#', buf);
    }
    style.ButtonTextAlign.x = 0.5f;
    ImGui::PopStyleColor();

    const float ts_width = GetTimestampWidth();
    float offset = style.ItemSpacing.x + label_width + style.ItemSpacing.x;

    ImGui::PushItemWidth(ts_width);
    if (settings.show_start_column) {
        ImGui::SameLine(offset);
        ImGui::TextUnformatted(GetStartTimeStr());
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("开始");
        }
        offset += ts_width;
    }
    if (settings.show_end_column) {
        ImGui::SameLine(offset);
        ImGui::TextUnformatted(GetEndTimeStr());
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("结束");
        }
        offset += ts_width + style.ItemSpacing.x;
    }
    if (settings.show_time_column) {
        ImGui::SameLine(offset);
        ImGui::TextUnformatted(GetDurationStr());
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("用时");
        }
    }
    for (auto i = 0; i < indent; i++) {
        ImGui::Unindent();
    }
}

void ObjectiveTimerWindow::ObjectiveSet::Update() const
{
    if (!active) {
        return;
    }

    for (const Objective* obj : objectives) {
        obj->Update();
    }
}

void ObjectiveTimerWindow::ObjectiveSet::Event(const EventType type, const uint32_t id1, const uint32_t id2)
{
    auto Match = [&](const Objective::Event& event) -> bool {
        if (type != event.type) {
            return false;
        }
        switch (type) {
            // 对于这些，使用 id2 作为 wchar_t*
            case EventType::ServerMessage:
            case EventType::DisplayDialogue: {
                const wchar_t* msg1 = (wchar_t*)id2;
                const wchar_t* msg2 = (wchar_t*)event.id2;
                if (msg1 == nullptr) {
                    return false;
                }
                if (msg2 == nullptr) {
                    return false;
                }
                for (auto i = 0u; i < id1 && i < event.id1; i++) {
                    if (msg2[i] != 0 && msg1[i] != msg2[i]) {
                        return false;
                    }
                }
                return true;
            }

            default:
                if (id1 != 0 && id1 != event.id1) {
                    return false;
                }
                if (id2 != 0 && id2 != event.id2) {
                    return false;
                }
                return true;
        }
    };

    bool just_set_something_done = false;

    for (size_t i = 0; i < objectives.size(); i++) {
        Objective& obj = *objectives[i];
        if (obj.IsDone()) {
            continue; // 无需检查
        }

        if (!obj.IsStarted()) {
            for (auto& event : obj.start_events) {
                if (Match(event)) {
                    obj.SetStarted();
                    size_t to_set_done_from = i - obj.starting_completes_n_previous_objectives;
                    if (obj.starting_completes_n_previous_objectives == -1) {
                        to_set_done_from = 0;
                    }
                    for (size_t j = to_set_done_from; j < i; ++j) {
                        Objective* other = objectives[j];
                        if (!other->IsDone()) {
                            other->SetDone();
                            WebsocketSendMessage("split");
                        }
                    }
                    break;
                }
            }
        }

        for (const Objective::Event& event : obj.end_events) {
            if (Match(event)) {
                obj.SetDone();
                size_t to_set_done_from = i - obj.starting_completes_n_previous_objectives;
                if (obj.starting_completes_n_previous_objectives == -1) {
                    to_set_done_from = 0;
                }
                for (size_t j = to_set_done_from; j < i; ++j) {
                    Objective& other = *objectives[j];
                    if (!other.IsDone()) {
                        other.SetDone();
                    }
                }
                just_set_something_done = true;
                break;
            }
        }
    }

    if (just_set_something_done) {
        WebsocketSendMessage("split");
        CheckSetDone();
    }
}

void ObjectiveTimerWindow::ObjectiveSet::CheckSetDone()
{
    if (!std::ranges::any_of(objectives, [](const Objective* obj) { return obj->done == TIME_UNKNOWN; })) {
        duration = GetDuration();
        // 确保没有更晚完成的目标
        const auto max = std::max_element(objectives.begin(), objectives.end(),
                                          [](const Objective* a, const Objective* b) { return a->done < b->done; });
        duration = std::max((*max)->done, duration);
        active = false;
        if (settings.auto_send_age) {
            GW::Chat::SendChat('/', "age");
        }
        TimerWidget::Instance().SetRunCompleted(GameSettings::GetSettingBool("auto_age2_on_age"));
    }
}

ObjectiveTimerWindow::ObjectiveSet::ObjectiveSet()
    : system_time(static_cast<DWORD>(time(nullptr))),
      duration(TIME_UNKNOWN),
      ui_id(cur_ui_id++)
{
    run_start_time_point = TimerWidget::Instance().GetStartPoint() != TIME_UNKNOWN ? TimerWidget::Instance().GetStartPoint() : time_point_ms();
    character_name = GW::GetCharContext() ? GW::GetCharContext()->player_name : L"";
}

ObjectiveTimerWindow::ObjectiveSet::~ObjectiveSet()
{
    for (const auto* obj : objectives) {
        if (obj) {
            delete obj;
        }
    }
    objectives.clear();
}

ObjectiveTimerWindow::ObjectiveSet* ObjectiveTimerWindow::ObjectiveSet::FromJson(const Serialized& json)
{
    const auto os = new ObjectiveSet;
    os->active = false;
    os->system_time = static_cast<DWORD>(json.utc_start);
    os->name = json.name;
    os->run_start_time_point = static_cast<DWORD>(json.instance_start);
    if (json.duration) os->duration = static_cast<DWORD>(*json.duration);
    for (const auto& o : json.objectives) {
        os->objectives.emplace_back(Objective::FromJson(o));
    }
    os->StopObjectives();
    return os;
}

ObjectiveTimerWindow::ObjectiveSet::Serialized ObjectiveTimerWindow::ObjectiveSet::ToJson()
{
    Serialized out{
        .name = name,
        .instance_start = run_start_time_point,
        .utc_start = system_time,
        .duration = GetDuration(),
    };
    out.objectives.reserve(objectives.size());
    for (auto* obj : objectives) {
        out.objectives.push_back(obj->ToJson());
    }
    return out;
}

ObjectiveTimerWindow::Objective::Serialized ObjectiveTimerWindow::Objective::ToJson()
{
    return {
        .name = name,
        .status = static_cast<uint32_t>(std::to_underlying(status)),
        .start = start,
        .done = done,
        .indent = static_cast<uint32_t>(indent),
        .duration = GetDuration(),
    };
}

ObjectiveTimerWindow::Objective* ObjectiveTimerWindow::Objective::FromJson(const Serialized& json)
{
    const auto obj = new Objective(json.name.c_str());
    obj->status = static_cast<Status>(static_cast<int>(json.status));
    obj->start = static_cast<DWORD>(json.start);
    obj->done = static_cast<DWORD>(json.done);
    if (json.indent) obj->indent = static_cast<DWORD>(*json.indent);
    if (json.duration) obj->duration = static_cast<DWORD>(*json.duration);
    return obj;
}

const char* ObjectiveTimerWindow::ObjectiveSet::GetStartTimeStr()
{
    if (!cached_start[0]) {
        tm timeinfo{};
        GetStartTime(&timeinfo);
        const time_t now = time(nullptr);
        const tm* nowinfo = localtime(&now);
        int cached_str_offset = 0;
        if (timeinfo.tm_yday != nowinfo->tm_yday || timeinfo.tm_year != nowinfo->tm_year) {
            const char* months[] = {"一月", "二月", "三月", "四月", "五月", "六月", "七月", "八月", "九月", "十月", "十一月", "十二月"};
            cached_str_offset += snprintf(&cached_start[cached_str_offset], sizeof(cached_start) - cached_str_offset,
                                          "%s %02d, ", months[timeinfo.tm_mon], timeinfo.tm_mday);
        }
        snprintf(&cached_start[cached_str_offset], sizeof(cached_start) - cached_str_offset, "%02d:%02d",
                 timeinfo.tm_hour, timeinfo.tm_min);
    }
    return cached_start;
}

DWORD ObjectiveTimerWindow::ObjectiveSet::GetDuration()
{
    if (active) {
        return duration = time_point_ms() - run_start_time_point;
    }
    if (duration != TIME_UNKNOWN) {
        return duration;
    }
    Objective* last_objective_done = nullptr;
    for (const auto objective : objectives) {
        if (!objective->IsDone()) return TIME_UNKNOWN;
        if (!last_objective_done || last_objective_done->done < objective->done)
            last_objective_done = objective;
    }
    // ... 但对于已完成的记录，我们可以从目标中计算出来。
    return last_objective_done ? last_objective_done->done : TIME_UNKNOWN;
}

const char* ObjectiveTimerWindow::ObjectiveSet::GetDurationStr()
{
    if (!cached_time[0] || active) {
        PrintTime(cached_time, sizeof(cached_time), GetDuration(), settings.show_decimal);
    }
    return cached_time;
}

bool ObjectiveTimerWindow::ObjectiveSet::IsFilteredOut()
{
    if (settings.show_past_runs || !from_disk) {
        return false;
    }
    if (start_yday < 0) {
        tm timeinfo{};
        GetStartTime(&timeinfo);
        start_yday = timeinfo.tm_yday;
        start_year = timeinfo.tm_year;
    }
    return start_yday != today_yday || start_year != today_year;
}

bool ObjectiveTimerWindow::ObjectiveSet::Draw()
{
    if (IsFilteredOut()) {
        return true;
    }

    if (!cached_header[0] || active) {
        if (settings.show_start_date_time) {
            snprintf(cached_header, sizeof(cached_header), "%s - %s - %s%s###header%u", GetStartTimeStr(), name.c_str(), GetDurationStr(), failed ? " [Failed]" : "", ui_id);
        }
        else {
            snprintf(cached_header, sizeof(cached_header), "%s - %s%s###header%u", name.c_str(), GetDurationStr(), failed ? " [Failed]" : "", ui_id);
        }
    }

    bool is_open = true;
    drawn_expanded = ImGui::CollapsingHeader(cached_header, &is_open, ImGuiTreeNodeFlags_DefaultOpen);
    if (!is_open) {
        return false;
    }
    if (drawn_expanded) {
        ImGui::PushID(static_cast<int>(ui_id));
        for (Objective* objective : objectives) {
            objective->Draw();
        }
        ImGui::PopID();
    }
    if (need_to_collapse) {
        ImGui::GetCurrentWindow()->DC.StateStorage->SetInt(ImGui::GetID(cached_header), 0);
        need_to_collapse = false;
        drawn_expanded = false;
    }
    return true;
}

void ObjectiveTimerWindow::ObjectiveSet::InvalidateCachedStrings()
{
    cached_start[0] = cached_time[0] = cached_header[0] = '\0';
    for (Objective* objective : objectives) {
        objective->InvalidateCachedStrings();
    }
}

void ObjectiveTimerWindow::ObjectiveSet::GetStartTime(tm* timeinfo) const
{
    const time_t ts = system_time;
    memcpy(timeinfo, localtime(&ts), sizeof(timeinfo[0]));
}
