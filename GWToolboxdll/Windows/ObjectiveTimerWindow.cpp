#include "stdafx.h"

#include <Windows/ObjectiveTimerWindow.h>
#include <Modules/Resources.h>
#include <Widgets/TimerWidget.h>

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWToolbox.h>
#include <GuiUtils.h>
#include <Logger.h>

#define countof(arr) (sizeof(arr) / sizeof(arr[0]))

constexpr uint32_t TIME_UNKNOWN((uint32_t)-1);
unsigned int ObjectiveTimerWindow::ObjectiveSet::cur_ui_id = 0;

namespace
{
    // settings in the unnamed namespace. This is ugly. DO NOT COPY.
    // just doing it because Objective needs them and I'm too lazy to pass them all the way there.
    int n_columns = 4;
    bool show_decimal = false;
    bool show_start_column = true;
    bool show_end_column = true;
    bool show_time_column = true;
    bool show_start_date_time = false;
    bool save_to_disk = true;
    bool show_past_runs = false;

    enum DoA_ObjId : uint32_t
    {
        Foundry = 0x273F,
        Veil,
        Gloom,
        City
    };

    // Hex values matching the first char of Kanaxai's dialogs in each room.
    const enum kanaxai_room_dialogs { Room5 = 0x5336, Room6, Room8, Room10, Room12, Room13, Room14, Room15 };
    const wchar_t* kanaxai_dialogs[] = {
        // Room 1-4 no dialog
        L"\x5336\xBEB8\x8555\x7267", // Room 5 "Fear not the darkness. It is already within you."
        L"\x5337\xAA3A\xE96F\x3E34", // Room 6 "Is it comforting to know the source of your fears? Or do you fear more
                                     // now that you see them in front of you."
        // Room 7 no dialog
        L"\x5338\xFD69\xA162\x3A04", // Room 8 "Even if you banish me from your sight, I will remain in your mind."
        // Room 9 no dialog
        L"\x5339\xA7BA\xC67B\x5D81", // Room 10 "You mortals may be here to defeat me, but acknowledging my presence
                                     // only makes the nightmare grow stronger."
        // Room 11 no dialog
        L"\x533A\xED06\x815D\x5FFB", // Room 12 "So, you have passed through the depths of the Jade Sea, and into the
                                     // nightmare realm. It is too bad that I must send you back from whence you came."
        L"\x533B\xCAA6\xFDA9\x3277", // Room 13 "I am Kanaxai, creator of nightmares. Let me make yours into reality."
        L"\x533C\xDD33\xA330\x4E27", // Room 14 "I will fill your hearts with visions of horror and despair that will
                                     // haunt you for all of your days."
        L"\x533D\x9EB1\x8BEE\x2637"  // Kanaxai "What gives you the right to enter my lair? I shall kill you for your
                                     // audacity, after I destroy your mind with my horrifying visions, of course."
    };
    const enum DoorID : uint32_t {
        // object_id's for doors opening.
        Deep_room_1_first = 12669,  // Room 1 Complete = Room 5 open
        Deep_room_1_second = 11692, // Room 1 Complete = Room 5 open
        Deep_room_2_first = 54552,  // Room 2 Complete = Room 5 open
        Deep_room_2_second = 1760,  // Room 2 Complete = Room 5 open
        Deep_room_3_first = 45425,  // Room 3 Complete = Room 5 open
        Deep_room_3_second = 48290, // Room 3 Complete = Room 5 open
        Deep_room_4_first = 40330,  // Room 4 Complete = Room 5 open
        Deep_room_4_second = 60114, // Room 4 Complete = Room 5 open
        Deep_room_5 = 29594,        // Room 5 Complete = Room 1,2,3,4,6 open
        Deep_room_6 = 49742,        // Room 6 Complete = Room 7 open
        Deep_room_7 = 55680,        // Room 7 Complete = Room 8 open
        // NOTE: Room 8 (failure) to room 10 (scorpion), no door.
        Deep_room_9 = 99887,  // Trigger on leviathan?
        Deep_room_11 = 29320, // Room 11 door is always open. Use to START room 11 when it comes into range.
    
        DoA_foundry_entrance_r1 = 39534,
        DoA_foundry_r1_r2 = 6356,
        DoA_foundry_r2_r3 = 45276,
        DoA_foundry_r3_r4 = 55421,
        DoA_foundry_r4_r5 = 49719,
        DoA_foundry_r5_bb = 45667,
        DoA_foundry_behind_bb = 1731,
        DoA_city_entrance = 63939,
        DoA_city_wall = 54727,
        DoA_city_jadoth = 64556,
        DoA_veil_360_left = 13005, 
        DoA_veil_360_middle = 11772,
        DoA_veil_360_right = 28851,
        DoA_veil_derv = 56510, 
        DoA_veil_ranger = 4753,
        DoA_veil_trench_necro = 46650, 
        DoA_veil_trench_mes = 29594,
        DoA_veil_trench_ele = 49742,
        DoA_veil_trench_monk = 55680,
        DoA_veil_trench_gloom = 28961,
        DoA_veil_to_gloom = 3,
        DoA_gloom_to_foundry = 17955,
        DoA_gloom_rift = 47069, // not really a door, has animation type=9 when closed
    };
    const std::map<GW::Constants::MapID, uint32_t> mapToDungeonLevel = {
        {GW::Constants::MapID::Catacombs_of_Kathandrax_Level_1, 1},
        {GW::Constants::MapID::Catacombs_of_Kathandrax_Level_2, 2},
        {GW::Constants::MapID::Catacombs_of_Kathandrax_Level_3, 3},
        {GW::Constants::MapID::Rragars_Menagerie_Level_1, 1},
        {GW::Constants::MapID::Rragars_Menagerie_Level_2, 2},
        {GW::Constants::MapID::Rragars_Menagerie_Level_3, 3},
        {GW::Constants::MapID::Cathedral_of_Flames_Level_1, 1},
        {GW::Constants::MapID::Cathedral_of_Flames_Level_2, 2},
        {GW::Constants::MapID::Cathedral_of_Flames_Level_3, 3},
        {GW::Constants::MapID::Ooze_Pit, 1},
        {GW::Constants::MapID::Darkrime_Delves_Level_1, 1},
        {GW::Constants::MapID::Darkrime_Delves_Level_2, 2},
        {GW::Constants::MapID::Darkrime_Delves_Level_3, 3},
        {GW::Constants::MapID::Frostmaws_Burrows_Level_1, 1},
        {GW::Constants::MapID::Frostmaws_Burrows_Level_2, 2},
        {GW::Constants::MapID::Frostmaws_Burrows_Level_3, 3},
        {GW::Constants::MapID::Frostmaws_Burrows_Level_4, 4},
        {GW::Constants::MapID::Frostmaws_Burrows_Level_5, 5},
        {GW::Constants::MapID::Sepulchre_of_Dragrimmar_Level_1, 1},
        {GW::Constants::MapID::Sepulchre_of_Dragrimmar_Level_2, 2},
        {GW::Constants::MapID::Ravens_Point_Level_1, 1},
        {GW::Constants::MapID::Ravens_Point_Level_2, 2},
        {GW::Constants::MapID::Ravens_Point_Level_3, 3},
        {GW::Constants::MapID::Vloxen_Excavations_Level_1, 1},
        {GW::Constants::MapID::Vloxen_Excavations_Level_2, 2},
        {GW::Constants::MapID::Vloxen_Excavations_Level_3, 3},
        {GW::Constants::MapID::Bogroot_Growths_Level_1, 1},
        {GW::Constants::MapID::Bogroot_Growths_Level_2, 2},
        {GW::Constants::MapID::Bloodstone_Caves_Level_1, 1},
        {GW::Constants::MapID::Bloodstone_Caves_Level_2, 2},
        {GW::Constants::MapID::Bloodstone_Caves_Level_3, 3},
        {GW::Constants::MapID::Shards_of_Orr_Level_1, 1},
        {GW::Constants::MapID::Shards_of_Orr_Level_2, 2},
        {GW::Constants::MapID::Shards_of_Orr_Level_3, 3},
        {GW::Constants::MapID::Oolas_Lab_Level_1, 1},
        {GW::Constants::MapID::Oolas_Lab_Level_2, 2},
        {GW::Constants::MapID::Oolas_Lab_Level_3, 3},
        {GW::Constants::MapID::Arachnis_Haunt_Level_1, 1},
        {GW::Constants::MapID::Arachnis_Haunt_Level_2, 2},
        {GW::Constants::MapID::Slavers_Exile_Level_1, 1},
        {GW::Constants::MapID::Slavers_Exile_Level_2, 2},
        {GW::Constants::MapID::Slavers_Exile_Level_3, 3},
        {GW::Constants::MapID::Slavers_Exile_Level_4, 4},
        {GW::Constants::MapID::Slavers_Exile_Level_5, 5},
        {GW::Constants::MapID::Fronis_Irontoes_Lair_mission, 1},
        {GW::Constants::MapID::Secret_Lair_of_the_Snowmen, 1},
        {GW::Constants::MapID::Heart_of_the_Shiverpeaks_Level_1, 1},
        {GW::Constants::MapID::Heart_of_the_Shiverpeaks_Level_2, 2},
        {GW::Constants::MapID::Heart_of_the_Shiverpeaks_Level_3, 3},
        {GW::Constants::MapID::The_Underworld_PvP, 1},
        {GW::Constants::MapID::Scarred_Earth, 2},
        {GW::Constants::MapID::The_Courtyard, 3},
        {GW::Constants::MapID::The_Hall_of_Heroes, 4},
    };

    void PrintTime(char* buf, size_t size, DWORD time, bool show_ms = true)
    {
        if (time == TIME_UNKNOWN) {
            GuiUtils::StrCopy(buf, "--:--", size);
        } else {
            DWORD sec = time / 1000;
            if (show_ms && show_decimal) {
                snprintf(buf, size, "%02lu:%02lu.%1lu", (sec / 60), sec % 60, (time / 100) % 10);
            } else {
                snprintf(buf, size, "%02lu:%02lu", (sec / 60), sec % 60);
            }
        }
    }

    void AsyncGetMapName(char* buffer, size_t n, GW::Constants::MapID mapID = GW::Map::GetMapID())
    {
        static wchar_t enc_str[16];
        GW::AreaInfo* info = GW::Map::GetMapInfo(mapID);
        if (!GW::UI::UInt32ToEncStr(info->name_id, enc_str, n)) {
            buffer[0] = 0;
            return;
        }
        GW::UI::AsyncDecodeStr(enc_str, buffer, n);
    }

    void ComputeNColumns()
    {
        n_columns = 0 + (show_start_column ? 1 : 0) + (show_end_column ? 1 : 0) + (show_time_column ? 1 : 0);
    }

    float GetTimestampWidth() { return (65.0f * ImGui::GetIO().FontGlobalScale); }
    float GetLabelWidth()
    {
        return std::max(GetTimestampWidth(), ImGui::GetWindowContentRegionWidth() - (GetTimestampWidth() * n_columns));
    }
    static bool runs_dirty = false;
} // namespace

void ObjectiveTimerWindow::Initialize()
{
    ToolboxWindow::Initialize();

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

    // packet hooks used to create or manipulate objective sets:
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyDefeated>(
        &PartyDefeated_Entry, [this](GW::HookStatus*, GW::Packet::StoC::PartyDefeated*) { StopObjectives(); });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadInfo>(&InstanceLoadInfo_Entry,
        [this](GW::HookStatus*, GW::Packet::StoC::InstanceLoadInfo* packet) { 
            if (!packet->is_explorable) return;
            AddObjectiveSet((GW::Constants::MapID)packet->map_id);
            Event(EventType::InstanceLoadInfo, packet->map_id);
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadFile>(
        &InstanceLoadFile_Entry, [this](GW::HookStatus*, GW::Packet::StoC::InstanceLoadFile* packet) {
            if (packet->map_fileID == 219215) {
                AddDoAObjectiveSet(packet->spawn_point);
            }
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(
        &GameSrvTransfer_Entry, [this](GW::HookStatus*, GW::Packet::StoC::GameSrvTransfer* packet) {
            // Exited map
            const GW::AreaInfo* info = GW::Map::GetMapInfo((GW::Constants::MapID)packet->map_id);
            if (!info) return; // we should always have this

            static bool in_dungeon = false;
            bool new_in_dungeon = (info->type == GW::RegionType_Dungeon);
            if (in_dungeon && !new_in_dungeon) { // moved from dungeon to outside
                StopObjectives();
            }
            in_dungeon = new_in_dungeon;

            static uint32_t map_id = 0;
            Event(EventType::InstanceEnd, map_id);
            map_id = packet->map_id;
        });

    // packet hooks that trigger events:
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageServer>(&MessageServer_Entry, 
        [this](GW::HookStatus*, GW::Packet::StoC::MessageServer*) {
            const GW::Array<wchar_t>* buff = &GW::GameContext::instance()->world->message_buff;
            if (!buff || !buff->valid() || !buff->size()) return; // Message buffer empty!?
            const wchar_t* msg = buff->begin();
            Event(EventType::ServerMessage, buff->size(), (uint32_t)msg);
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DisplayDialogue>(&DisplayDialogue_Entry, 
        [this](GW::HookStatus*, GW::Packet::StoC::DisplayDialogue* packet) {
            Event(EventType::DisplayDialogue, 122, (uint32_t)packet->message);
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ManipulateMapObject>(
        &ManipulateMapObject_Entry, [this](GW::HookStatus*, GW::Packet::StoC::ManipulateMapObject* packet) {
            if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
                if (packet->animation_type == 16 && packet->animation_stage == 2) {
                    Event(EventType::DoorOpen, packet->object_id);
                } else if (packet->animation_type == 3 && packet->animation_stage == 2) {
                    Event(EventType::DoorClose, packet->object_id);
                }
                // TODO: maybe add a more generic ManipulateMapObject packet?
            }
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveUpdateName>(
        &ObjectiveUpdateName_Entry, [this](GW::HookStatus*, GW::Packet::StoC::ObjectiveUpdateName* packet) {
            Event(EventType::ObjectiveStarted, packet->objective_id);
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveDone>(
        &ObjectiveDone_Entry, [this](GW::HookStatus*, GW::Packet::StoC::ObjectiveDone* packet) {
            Event(EventType::ObjectiveDone, packet->objective_id);
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentUpdateAllegiance>(
        &AgentUpdateAllegiance_Entry, [this](GW::HookStatus*, GW::Packet::StoC::AgentUpdateAllegiance* packet) {
            if (const GW::Agent* agent = GW::Agents::GetAgentByID(packet->agent_id)) {
                if (const GW::AgentLiving* agentliving = agent->GetAsAgentLiving()) {
                    Event(EventType::AgentUpdateAllegiance, agentliving->player_number, packet->allegiance_bits);
                }
            }
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DoACompleteZone>(
        &DoACompleteZone_Entry, [this](GW::HookStatus*, GW::Packet::StoC::DoACompleteZone* packet) {
            if (packet->message[0] == 0x8101) {
                Event(EventType::DoACompleteZone, packet->message[1]);
            }
        });
    GW::StoC::RegisterPacketCallback(&CountdownStart_Enty, GAME_SMSG_INSTANCE_COUNTDOWN, 
        [this](GW::HookStatus*, GW::Packet::StoC::PacketBase*) {
            if (const GW::AreaInfo* map = GW::Map::GetCurrentMapInfo()) {
                if (map->type != GW::RegionType::RegionType_ExplorableZone) {
                    Event(EventType::CountdownStart, (uint32_t)GW::Map::GetMapID());
                }
            }
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DungeonReward>(
        &DungeonReward_Entry, [this](GW::HookStatus*, GW::Packet::StoC::DungeonReward*) {
            Event(EventType::DungeonReward);
            if (ObjectiveTimerWindow::ObjectiveSet* os = GetCurrentObjectiveSet()) {
                os->objectives.back().SetDone();
                os->CheckSetDone();
            }
        });


    /*GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveAdd>(&ObjectiveAdd_Entry,
[this](GW::HookStatus* status, GW::Packet::StoC::ObjectiveAdd *packet) -> bool {
    UNREFERENCED_PARAMETER(status);
    UNREFERENCED_PARAMETER(packet);
    // type 12 is the "title" of the mission objective, should we ignore it or have a "title" objective ?
    /*
    Objective *obj = GetCurrentObjective(packet->objective_id);
    if (obj) return false;
    ObjectiveSet *os = objective_sets.back();
    os->objectives.emplace_back(packet->objective_id);
    obj = &os->objectives.back();
    GW::UI::AsyncDecodeStr(packet->name, obj->name, sizeof(obj->name));
    // If the name isn't "???" we consider that the objective started
    if (wcsncmp(packet->name, L"\x8102\x3236", 2))
        obj->SetStarted();

    return false;
});*/
}
ObjectiveTimerWindow::~ObjectiveTimerWindow()
{
    if (run_loader.joinable()) {
        run_loader.join();
    }
    ClearObjectiveSets();
}
void ObjectiveTimerWindow::Event(EventType type, uint32_t id1, uint32_t id2)
{
    if (ObjectiveSet* os = GetCurrentObjectiveSet()) {
        os->Event(type, id1, id2);
        
        // debug:
        //switch (type) {
        //    case EventType::ServerMessage:
        //    case EventType::DisplayDialogue: {
        //        const wchar_t* msg = (wchar_t*)id2;
        //        Log::Info("Event: %d, %d, %x, %x, %x, %x", type, id1, msg[0], msg[1], msg[2], msg[3]);
        //    }
        //        break;
        //    default: Log::Info("Event: %d, %d, %d", type, id1, id2);
        //}
    }
}
void ObjectiveTimerWindow::AddObjectiveSet(GW::Constants::MapID map_id)
{
    // clang-format off
    using namespace GW::Constants;
    switch (map_id) {
            // elite areas:
        case MapID::Urgozs_Warren: AddUrgozObjectiveSet(); break;
        case MapID::The_Deep: AddDeepObjectiveSet(); break;
        case MapID::The_Fissure_of_Woe: AddFoWObjectiveSet(); break;
        case MapID::The_Underworld: AddUWObjectiveSet(); break;

            // dungeons - 1 level:
        case MapID::Ooze_Pit: AddDungeonObjectiveSet({MapID::Ooze_Pit}); break;
        case MapID::Fronis_Irontoes_Lair_mission: AddDungeonObjectiveSet({MapID::Fronis_Irontoes_Lair_mission}); break;
        case MapID::Secret_Lair_of_the_Snowmen: AddDungeonObjectiveSet({MapID::Secret_Lair_of_the_Snowmen}); break;

            // dungeons - 2 levels:
        case MapID::Sepulchre_of_Dragrimmar_Level_1:
            AddDungeonObjectiveSet({MapID::Sepulchre_of_Dragrimmar_Level_1, MapID::Sepulchre_of_Dragrimmar_Level_2});
            break;
        case MapID::Bogroot_Growths_Level_1:
            AddDungeonObjectiveSet({MapID::Bogroot_Growths_Level_1, MapID::Bogroot_Growths_Level_2});
            break;
        case MapID::Arachnis_Haunt_Level_1: 
            AddDungeonObjectiveSet({MapID::Arachnis_Haunt_Level_1, MapID::Arachnis_Haunt_Level_2});
            break;

            // dungeons - 3 levels:
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
                MapID::Catacombs_of_Kathandrax_Level_3});
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
            
            // dungeons - 5 levels:
        case MapID::Frostmaws_Burrows_Level_1: 
            AddDungeonObjectiveSet({MapID::Frostmaws_Burrows_Level_1, 
                MapID::Frostmaws_Burrows_Level_2, 
                MapID::Frostmaws_Burrows_Level_3,
                MapID::Frostmaws_Burrows_Level_4, 
                MapID::Frostmaws_Burrows_Level_5});
            break;

            // dungeons - irregular:
        case MapID::Slavers_Exile_Level_5: 
            AddDungeonObjectiveSet({MapID::Slavers_Exile_Level_5}); 
            break;

            // Others:
        case MapID::The_Underworld_PvP:
            if (const GW::AreaInfo* info = GW::Map::GetCurrentMapInfo()) {
                if (info->type == GW::RegionType::RegionType_ExplorableZone) {
                    AddToPKObjectiveSet();
                }
            }
            break;
        default: break;
    }
    // clang-format on
}

void ObjectiveTimerWindow::ObjectiveSet::StopObjectives()
{
    active = false;
    for (Objective& obj : objectives) {
        switch (obj.status) {
            case Objective::Status::Started:
            case Objective::Status::Failed:
                obj.status = Objective::Status::Failed;
                failed = true;
                break;
            default: break;
        }
    }
}

void ObjectiveTimerWindow::AddObjectiveSet(ObjectiveSet* os)
{
    for (auto& cos : objective_sets) {
        cos.second->StopObjectives();
        cos.second->need_to_collapse = true;
    }
    objective_sets.emplace(os->system_time, os);
    if (os->active) current_objective_set = os;
    runs_dirty = true;
}

void ObjectiveTimerWindow::AddDungeonObjectiveSet(const std::vector<GW::Constants::MapID>& levels)
{
    ObjectiveSet* os = new ObjectiveSet;
    ::AsyncGetMapName(os->name, sizeof(os->name));
    for (size_t i = 0; i < levels.size(); ++i) {
        char name[256];
        snprintf(name, sizeof(name), "Level %d", i);
        os->AddObjectiveAfterAll(Objective(name)).AddStartEvent(EventType::InstanceLoadInfo, (uint32_t)levels[i]);
    }
    os->objectives.front().SetStarted(); // start first level
    os->objectives.back().AddEndEvent(EventType::DungeonReward); // last level finished with dungeon reward
    AddObjectiveSet(os);
}

void ObjectiveTimerWindow::AddDoAObjectiveSet(GW::Vec2f spawn)
{
    constexpr int n_areas = 4;

    const int starting_area = [&]() -> int {
        const GW::Vec2f mallyx_spawn(-3931, -6214);
        const GW::Vec2f area_spawns[] = {
            {-10514, 15231}, // foundry
            {-18575, -8833}, // city
            {364, -10445},   // veil
            {16034, 1244},   // gloom
        };
        double best_dist = GW::GetDistance(spawn, mallyx_spawn);
        int starting_area = -1;
        for (int i = 0; i < n_areas; ++i) {
            float dist = GW::GetDistance(spawn, area_spawns[i]);
            if (best_dist > dist) {
                best_dist = dist;
                starting_area = i;
            }
        }
        return starting_area;
    }();

    if (starting_area == -1) return; // we're doing mallyx, not doa!

    ObjectiveSet* os = new ObjectiveSet;
    ::AsyncGetMapName(os->name, sizeof(os->name));
    
    std::vector<std::function<void()>> add_doa_obj = {
        [&]() {
            os->AddObjective(Objective("Foundry"))
                .AddStartEvent(EventType::DoACompleteZone, Gloom)
                .AddStartEvent(EventType::DoorOpen, DoorID::DoA_foundry_entrance_r1)
                .AddEndEvent(EventType::DoACompleteZone, Foundry);
            os->AddObjective(Objective("Room 1", 1))
                .AddStartEvent(EventType::DoorClose, DoorID::DoA_foundry_entrance_r1)
                .AddEndEvent(EventType::DoorOpen, DoorID::DoA_foundry_r1_r2);
            os->AddObjective(Objective("Room 2", 1))
                .AddStartEvent(EventType::DoorClose, DoorID::DoA_foundry_r1_r2)
                .AddEndEvent(EventType::DoorOpen, DoorID::DoA_foundry_r2_r3);
            os->AddObjective(Objective("Room 3", 1))
                .AddStartEvent(EventType::DoorClose, DoorID::DoA_foundry_r2_r3)
                .AddEndEvent(EventType::DoorOpen, DoorID::DoA_foundry_r3_r4);
            os->AddObjective(Objective("Room 4", 1))
                .AddStartEvent(EventType::DoorClose, DoorID::DoA_foundry_r3_r4)
                .AddEndEvent(EventType::DoorOpen, DoorID::DoA_foundry_r4_r5);

            // maybe time snakes take? (check them being added to party)

            // maybe change BB event to use the dialog instead? "None shall escape. Prepare to die."
            // change BB to start at door and finish at fury spawn?
            os->AddObjective(Objective("Black Beast", 1))
                .AddStartEvent(EventType::DoorOpen, DoorID::DoA_foundry_r5_bb)
                .AddEndEvent(EventType::AgentUpdateAllegiance, 5221, 0x6E6F6E63); // all 3 are the same
            
            // 0x8101 0x273D 0x98D8 0xB91A 0x47B8 The Fury: Ah, you have finally arrived. My dark master informed me I might have visitors....
            os->AddObjective(Objective("Fury", 1))
                .AddStartEvent(EventType::DisplayDialogue, 4, (uint32_t)L"\x8101\x273D\x98DB\xB91A")
                .AddEndEvent(EventType::DoACompleteZone, Foundry);
        },
        [&]() {
            os->AddObjective(Objective("City"))
                .AddStartEvent(EventType::DoACompleteZone, Foundry)
                .AddEndEvent(EventType::DoACompleteZone, City);
            os->AddObjective(Objective("Outside", 1))
                .AddStartEvent(EventType::DoorOpen, DoorID::DoA_city_entrance)
                .AddEndEvent(EventType::DoorOpen, DoorID::DoA_city_wall);
            os->AddObjective(Objective("Inside", 1))
                .AddStartEvent(EventType::DoorOpen, DoorID::DoA_city_wall)
                .AddEndEvent(EventType::DoACompleteZone, City);

            // TODO: jadoth (starts at end of city, ends when chest spawns)
        }, 
        [&]() {
            os->AddObjective(Objective("Veil"))
                .AddStartEvent(EventType::DoACompleteZone, City)
                .AddEndEvent(EventType::DoACompleteZone, Veil);
            os->AddObjective(Objective("360", 1))
                .AddStartEvent(EventType::DoorOpen, DoorID::DoA_veil_360_left)
                .AddStartEvent(EventType::DoorOpen, DoorID::DoA_veil_360_middle)
                .AddStartEvent(EventType::DoorOpen, DoorID::DoA_veil_360_right);
            os->AddObjectiveAfter(Objective("Underlords", 1))
                .AddStartEvent(EventType::DoorOpen, DoorID::DoA_veil_ranger)
                .AddStartEvent(EventType::DoorOpen, DoorID::DoA_veil_derv);
            os->AddObjectiveAfter(Objective("Lords", 1))
                .AddStartEvent(EventType::DoorOpen, DoorID::DoA_veil_trench_gloom)
                .AddStartEvent(EventType::DoorOpen, DoorID::DoA_veil_trench_monk)
                .AddStartEvent(EventType::DoorOpen, DoorID::DoA_veil_trench_ele)
                .AddStartEvent(EventType::DoorOpen, DoorID::DoA_veil_trench_mes)
                .AddStartEvent(EventType::DoorOpen, DoorID::DoA_veil_trench_necro);
            os->AddObjectiveAfter(Objective("Tendrils", 1))
                .AddStartEvent(EventType::ServerMessage, 3, (uint32_t)L"\x817\x10D\x101")
                .AddEndEvent(EventType::DoACompleteZone, Veil);
        },
        [&]() {
            os->AddObjective(Objective("Gloom"))
                .AddStartEvent(EventType::DoACompleteZone, Veil)
                .AddEndEvent(EventType::DoACompleteZone, Gloom);

            os->AddObjective(Objective("Cave", 1))
                .AddStartEvent(EventType::DisplayDialogue, 4, (uint32_t)L"\x8101\x5765\x9846\xA72B")
                .AddEndEvent(EventType::DisplayDialogue, 4, (uint32_t)L"\x8101\x5767\xA547\xB2C2");
            
            // TODO: rift may not be possible from outside of range

            // TODO: deathbringer ?

            os->AddObjective(Objective("Darknesses", 1))
                .AddStartEvent(EventType::DisplayDialogue, 4, (uint32_t)L"\x8101\x273B\xB5DB\x8B13")
                .AddEndEvent(EventType::DoACompleteZone, Gloom);
        }
    };

    for (int i = 0; i < n_areas; ++i) {
        int idx = (starting_area + i) % n_areas;
        add_doa_obj[idx]();
    }

    os->objectives.front().SetStarted();
    AddObjectiveSet(os);
}
void ObjectiveTimerWindow::AddUrgozObjectiveSet()
{
    // Zone 1, Weakness = already open on start
    // Zone 2, Life Drain = Starts when door 45420 opens
    // Zone 3, Levers = Starts when door 11692 opens
    // Zone 4, Bridge = Starts when door 54552 opens
    // Zone 5, Wolves = Starts when door 1760 opens
    // Zone 6, Energy Drain = Starts when door 40330 opens
    // Zone 7, Exhaustion = Starts when door 29537 opens 60114? 54756?
    // Zone 8, Pillars = Starts when door 37191 opens
    // Zone 9, Blood Drinkers = Starts when door 35500 opens
    // Zone 10, Jons Fail Room = Starts when door 34278 opens
    // Zone 11, Urgoz = Starts when door 15529 opens
    // Urgoz = 3750
    // Objective for Urgoz = 357

    ObjectiveTimerWindow::ObjectiveSet* os = new ObjectiveSet;
    ::AsyncGetMapName(os->name, sizeof(os->name));
    os->AddObjective(Objective("Zone 1 | Weakness")).SetStarted();
    os->AddObjectiveAfterAll(Objective("Zone 2 | Life Drain")).AddStartEvent(EventType::DoorOpen, 45420);
    os->AddObjectiveAfterAll(Objective("Zone 3 | Levers")).AddStartEvent(EventType::DoorOpen, 11692);
    os->AddObjectiveAfterAll(Objective("Zone 4 | Bridge Wolves")).AddStartEvent(EventType::DoorOpen, 54552);
    os->AddObjectiveAfterAll(Objective("Zone 5 | More Wolves")).AddStartEvent(EventType::DoorOpen, 1760);
    os->AddObjectiveAfterAll(Objective("Zone 6 | Energy Drain")).AddStartEvent(EventType::DoorOpen, 40330);
    os->AddObjectiveAfterAll(Objective("Zone 7 | Exhaustion")).AddStartEvent(EventType::DoorOpen, 29537);
    os->AddObjectiveAfterAll(Objective("Zone 8 | Pillars")).AddStartEvent(EventType::DoorOpen, 37191);
    os->AddObjectiveAfterAll(Objective("Zone 9 | Blood Drinkers")).AddStartEvent(EventType::DoorOpen, 35500);
    os->AddObjectiveAfterAll(Objective("Zone 10 | Bridge")).AddStartEvent(EventType::DoorOpen, 34278);
    os->AddObjectiveAfterAll(Objective("Zone 11 | Urgoz"))
        .AddStartEvent(EventType::DoorOpen, 15529)
        .AddStartEvent(EventType::DoorOpen, 45631)
        .AddStartEvent(EventType::DoorOpen, 53071)
        .AddEndEvent(EventType::ServerMessage, 6, (uint32_t)L"\x6C9C\x0\x0\x0\x0\x2810")
        .AddEndEvent(EventType::ServerMessage, 6, (uint32_t)L"\x6C9C\x0\x0\x0\x0\x1488");

    AddObjectiveSet(os);
}
void ObjectiveTimerWindow::AddDeepObjectiveSet()
{
    ObjectiveTimerWindow::ObjectiveSet* os = new ObjectiveSet;
    ::AsyncGetMapName(os->name, sizeof(os->name));
    os->AddObjective(Objective("Room 1 | Soothing"))
        .SetStarted()
        .AddEndEvent(EventType::DoorOpen, DoorID::Deep_room_1_first)
        .AddEndEvent(EventType::DoorOpen, DoorID::Deep_room_1_second);
    os->AddObjective(Objective("Room 2 | Death"))
        .SetStarted()
        .AddEndEvent(EventType::DoorOpen, DoorID::Deep_room_2_first)
        .AddEndEvent(EventType::DoorOpen, DoorID::Deep_room_2_second);
    os->AddObjective(Objective("Room 3 | Surrender"))
        .SetStarted()
        .AddEndEvent(EventType::DoorOpen, DoorID::Deep_room_3_first)
        .AddEndEvent(EventType::DoorOpen, DoorID::Deep_room_3_second);
    os->AddObjective(Objective("Room 4 | Exposure"))
        .SetStarted()
        .AddEndEvent(EventType::DoorOpen, DoorID::Deep_room_4_first)
        .AddEndEvent(EventType::DoorOpen, DoorID::Deep_room_4_second);
    os->AddObjective(Objective("Room 5 | Pain"))
        .AddStartEvent(EventType::DoorOpen, DoorID::Deep_room_1_first)
        .AddStartEvent(EventType::DoorOpen, DoorID::Deep_room_1_second)
        .AddStartEvent(EventType::DoorOpen, DoorID::Deep_room_2_first)
        .AddStartEvent(EventType::DoorOpen, DoorID::Deep_room_2_second)
        .AddStartEvent(EventType::DoorOpen, DoorID::Deep_room_3_first)
        .AddStartEvent(EventType::DoorOpen, DoorID::Deep_room_3_second)
        .AddStartEvent(EventType::DoorOpen, DoorID::Deep_room_4_first)
        .AddStartEvent(EventType::DoorOpen, DoorID::Deep_room_4_second);
    
    os->AddObjectiveAfter(Objective("Room 6 | Lethargy")).AddStartEvent(EventType::DoorOpen, DoorID::Deep_room_5);
    os->AddObjectiveAfter(Objective("Room 7 | Depletion")).AddStartEvent(EventType::DoorOpen, DoorID::Deep_room_6);

    // 8 and 9 together because theres no boundary between
    os->AddObjectiveAfter(Objective("Room 8-9 | Failure/Shadows"))
        .AddStartEvent(EventType::DoorOpen, DoorID::Deep_room_7);
    
    os->AddObjectiveAfter(Objective("Room 10 | Scorpion"))
        .AddStartEvent(EventType::DisplayDialogue, kanaxai_room_dialogs::Room10);
    os->AddObjectiveAfter(Objective("Room 11 | Fear")).AddStartEvent(EventType::DoorOpen, DoorID::Deep_room_11);
    os->AddObjectiveAfter(Objective("Room 12 | Depletion"))
        .AddStartEvent(EventType::DisplayDialogue, kanaxai_room_dialogs::Room12);
    // 13 and 14 together because theres no boundary between
    os->AddObjectiveAfter(Objective("Room 13-14 | Decay/Torment"))
        .AddStartEvent(EventType::DisplayDialogue, kanaxai_room_dialogs::Room13); 
    os->AddObjectiveAfter(Objective("Room 15 | Kanaxai"))
        .AddStartEvent(EventType::DisplayDialogue, kanaxai_room_dialogs::Room15)
        .AddEndEvent(EventType::ServerMessage, 6, (uint32_t)L"\x6D4D\x0\x0\x0\x0\x2810")
        .AddEndEvent(EventType::ServerMessage, 6, (uint32_t)L"\x6D4D\x0\x0\x0\x0\x1488");
    AddObjectiveSet(os);
}
void ObjectiveTimerWindow::AddFoWObjectiveSet()
{
    ObjectiveSet* os = new ObjectiveSet;
    ::AsyncGetMapName(os->name, sizeof(os->name));
    
    os->AddQuestObjective("ToC", 309);
    os->AddQuestObjective("Wailing Lord", 310);
    os->AddQuestObjective("Griffons", 311);
    os->AddQuestObjective("Defend", 312);
    os->AddQuestObjective("Forge", 313);
    os->AddQuestObjective("Menzies", 314);
    os->AddQuestObjective("Restore", 315);
    os->AddQuestObjective("Khobay", 316);
    os->AddQuestObjective("ToS", 317);
    os->AddQuestObjective("Burning Forest", 318);
    os->AddQuestObjective("The Hunt", 319);
    AddObjectiveSet(os);
}
void ObjectiveTimerWindow::AddUWObjectiveSet()
{
    ObjectiveSet* os = new ObjectiveSet;
    ::AsyncGetMapName(os->name, sizeof(os->name));
    os->AddQuestObjective("Chamber", 146);
    os->AddQuestObjective("Restore", 147);
    os->AddQuestObjective("Escort", 148);
    os->AddQuestObjective("UWG", 149);
    os->AddQuestObjective("Vale", 150);
    os->AddQuestObjective("Waste", 151);
    os->AddQuestObjective("Pits", 152);
    os->AddQuestObjective("Planes", 153);
    os->AddQuestObjective("Mnts", 154);
    os->AddQuestObjective("Pools", 155);
    os->AddObjective(Objective("Dhuum"))
        .AddStartEvent(EventType::AgentUpdateAllegiance, GW::Constants::ModelID::UW::Dhuum, 0x6D6F6E31)
        .AddEndEvent(EventType::ObjectiveDone, 157);
    AddObjectiveSet(os);
}
void ObjectiveTimerWindow::AddToPKObjectiveSet()
{
    ObjectiveSet* os = new ObjectiveSet;

    // we could read out the name of the maps...
    os->AddObjective(Objective("The Underworld"))
        .SetStarted()
        .AddStartEvent(EventType::InstanceLoadInfo, (uint32_t)GW::Constants::MapID::The_Underworld_PvP)
        .AddEndEvent(EventType::CountdownStart, (uint32_t)GW::Constants::MapID::The_Underworld_PvP);
    os->AddObjective(Objective("Scarred Earth"))
        .AddStartEvent(EventType::InstanceLoadInfo, (uint32_t)GW::Constants::MapID::Scarred_Earth)
        .AddEndEvent(EventType::CountdownStart, (uint32_t)GW::Constants::MapID::Scarred_Earth);
    os->AddObjective(Objective("The Courtyard"))
        .AddStartEvent(EventType::InstanceLoadInfo, (uint32_t)GW::Constants::MapID::The_Courtyard)
        .AddEndEvent(EventType::CountdownStart, (uint32_t)GW::Constants::MapID::The_Courtyard);
    os->AddObjective(Objective("The Hall of Heroes"))
        .AddStartEvent(EventType::InstanceLoadInfo, (uint32_t)GW::Constants::MapID::The_Hall_of_Heroes)
        .AddEndEvent(EventType::CountdownStart, (uint32_t)GW::Constants::MapID::The_Hall_of_Heroes);

    ::AsyncGetMapName(os->name, sizeof(os->name), GW::Constants::MapID::Tomb_of_the_Primeval_Kings);
    AddObjectiveSet(os);
}

void ObjectiveTimerWindow::Update(float)
{
    if (current_objective_set && current_objective_set->active) {
        current_objective_set->Update();
    }
    if (runs_dirty && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
        SaveRuns(); // Save runs between map loads
    }
}
void ObjectiveTimerWindow::Draw(IDirect3DDevice9*)
{
    // Main objective timer window
    if (visible && !loading) {
        ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
            if (objective_sets.empty()) {
                ImGui::Text("Enter DoA, FoW, UW, Deep, Urgoz or a Dungeon to begin");
            } else {
                for (auto it = objective_sets.rbegin(); it != objective_sets.rend(); it++) {
                    bool show = (*it).second->Draw();
                    if (!show) {
                        objective_sets.erase(--(it.base()));
                        break;
                        // iterators go crazy, don't even bother, we're skipping a frame. NBD.
                        // if you really want to draw the rest make sure you extensively test this.
                    }
                }
            }
        }
        ImGui::End();
    }

    // Breakout objective set for current run
    if (show_current_run_window && current_objective_set) {
        ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
        char buf[256];
        sprintf(buf, "%s - %s###ObjectiveTimerCurrentRun", current_objective_set->name,
            current_objective_set->cached_time ? current_objective_set->cached_time : "--:--");

        if (ImGui::Begin(buf, &show_current_run_window, GetWinFlags())) {
            ImGui::PushID(static_cast<int>(current_objective_set->ui_id));
            for (Objective& objective : current_objective_set->objectives) {
                objective.Draw();
            }
            ImGui::PopID();
        }

        ImGui::End();
    }
}

ObjectiveTimerWindow::ObjectiveSet* ObjectiveTimerWindow::GetCurrentObjectiveSet()
{
    if (objective_sets.empty()) return nullptr;
    if (!current_objective_set || !current_objective_set->active) return nullptr;
    return current_objective_set;
}

void ObjectiveTimerWindow::DrawSettingInternal()
{
    clear_cached_times = ImGui::Checkbox("Show second decimal", &show_decimal);
    ImGui::Checkbox("Show 'Start' column", &show_start_column);
    ImGui::Checkbox("Show 'End' column", &show_end_column);
    ImGui::Checkbox("Show 'Time' column", &show_time_column);
    ImGui::Checkbox("Show run start date/time", &show_start_date_time);
    ImGui::Checkbox("Show current run in separate window", &show_current_run_window);
    if (ImGui::Checkbox("Save/Load runs to disk", &save_to_disk)) {
        SaveRuns();
    }
    ImGui::ShowHelp(
        "Keep a record or your runs in JSON format on disk, and load past runs from disk when starting GWToolbox.");
    ImGui::Checkbox("Show past runs", &show_past_runs);
    ImGui::ShowHelp("Display from previous days in the Objective Timer window.");
    ImGui::Checkbox("Automatic /age on completion", &auto_send_age);
    ImGui::ShowHelp(
        "As soon as final objective is complete, send /age command to game server to receive server-side completion time.");
    ComputeNColumns();
}

void ObjectiveTimerWindow::LoadSettings(CSimpleIni* ini)
{
    ToolboxWindow::LoadSettings(ini);
    show_decimal = ini->GetBoolValue(Name(), VAR_NAME(show_decimal), show_decimal);
    show_start_column = ini->GetBoolValue(Name(), VAR_NAME(show_start_column), show_start_column);
    show_end_column = ini->GetBoolValue(Name(), VAR_NAME(show_end_column), show_end_column);
    show_time_column = ini->GetBoolValue(Name(), VAR_NAME(show_time_column), show_time_column);
    show_current_run_window = ini->GetBoolValue(Name(), VAR_NAME(show_current_run_window), show_current_run_window);
    auto_send_age = ini->GetBoolValue(Name(), VAR_NAME(auto_send_age), auto_send_age);
    save_to_disk = ini->GetBoolValue(Name(), VAR_NAME(save_to_disk), save_to_disk);
    show_past_runs = ini->GetBoolValue(Name(), VAR_NAME(show_past_runs), show_past_runs);
    show_start_date_time = ini->GetBoolValue(Name(), VAR_NAME(show_start_date_time), show_start_date_time);
    ComputeNColumns();
    LoadRuns();
}
void ObjectiveTimerWindow::SaveSettings(CSimpleIni* ini)
{
    ToolboxWindow::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(show_decimal), show_decimal);
    ini->SetBoolValue(Name(), VAR_NAME(show_start_column), show_start_column);
    ini->SetBoolValue(Name(), VAR_NAME(show_end_column), show_end_column);
    ini->SetBoolValue(Name(), VAR_NAME(show_time_column), show_time_column);
    ini->SetBoolValue(Name(), VAR_NAME(show_current_run_window), show_current_run_window);
    ini->SetBoolValue(Name(), VAR_NAME(auto_send_age), auto_send_age);
    ini->SetBoolValue(Name(), VAR_NAME(show_start_date_time), show_start_date_time);
    ini->SetBoolValue(Name(), VAR_NAME(save_to_disk), save_to_disk);
    ini->SetBoolValue(Name(), VAR_NAME(show_past_runs), show_past_runs);
    SaveRuns();
}
void ObjectiveTimerWindow::LoadRuns()
{
    if (!save_to_disk) return;
    // Because this does a load of file reads and JSON decoding, its on a separate thread; it could delay rendering by
    // seconds
    if (run_loader.joinable()) run_loader.join();
    loading = true;
    run_loader = std::thread([]() {
        ObjectiveTimerWindow& instance = ObjectiveTimerWindow::Instance();
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

        // Output the list of names found
        for (auto it = obj_timer_files.rbegin();
             it != obj_timer_files.rend() && instance.objective_sets.size() < max_objectives_in_memory; it++) {
            try {
                std::ifstream file;
                std::wstring fn = Resources::GetPath(L"runs", *it);
                file.open(fn);
                if (file.is_open()) {
                    nlohmann::json os_json_arr;
                    file >> os_json_arr;
                    for (nlohmann::json::iterator json_it = os_json_arr.begin(); json_it != os_json_arr.end();
                         ++json_it) {
                        ObjectiveSet* os = ObjectiveSet::FromJson(&json_it.value());
                        if (instance.objective_sets.find(os->system_time) != instance.objective_sets.end())
                            continue; // Don't load in a run that already exists
                        os->StopObjectives();
                        os->need_to_collapse = true;
                        os->from_disk = true;
                        instance.objective_sets.emplace(os->system_time, os);
                    }
                    file.close();
                }
            } catch (const std::exception&) {
                Log::Error("Failed to load ObjectiveSets from json");
            }
        }
        instance.loading = false;
    });
}
void ObjectiveTimerWindow::SaveRuns()
{
    if (!save_to_disk || objective_sets.empty()) return;
    if (run_loader.joinable()) run_loader.join();
    loading = true;
    run_loader = std::thread([]() {
        ObjectiveTimerWindow& instance = ObjectiveTimerWindow::Instance();
        Resources::EnsureFolderExists(Resources::GetPath(L"runs"));
        std::map<std::wstring, std::vector<ObjectiveSet*>> objective_sets_by_file;
        wchar_t filename[36];
        struct tm* structtime;
        for (auto& os : instance.objective_sets) {
            if (os.second->from_disk) continue; // No need to re-save a run.
            time_t tt = (time_t)os.second->system_time;
            structtime = gmtime(&tt);
            if (!structtime) continue;
            swprintf(filename, 36, L"ObjectiveTimerRuns_%02d-%02d-%02d.json", structtime->tm_year + 1900,
                structtime->tm_mon + 1, structtime->tm_mday);
            objective_sets_by_file[filename].push_back(os.second);
        }
        bool error_saving = false;
        for (auto& it : objective_sets_by_file) {
            try {
                std::ofstream file;
                file.open(Resources::GetPath(L"runs", it.first));
                if (file.is_open()) {
                    nlohmann::json os_json_arr;
                    for (auto os : it.second) {
                        os_json_arr.push_back(os->ToJson());
                    }
                    file << os_json_arr << std::endl;
                    file.close();
                }
            } catch (const std::exception&) {
                Log::Error("Failed to save ObjectiveSets to json");
                error_saving = true;
            }
        }
        runs_dirty = false;
        instance.loading = false;
    });
}
void ObjectiveTimerWindow::ClearObjectiveSets()
{
    for (auto& os : objective_sets) {
        delete os.second;
    }
    objective_sets.clear();
}
void ObjectiveTimerWindow::StopObjectives()
{
    if (current_objective_set) {
        current_objective_set->StopObjectives();
    }
    current_objective_set = nullptr;
}


// =============================================================================

ObjectiveTimerWindow::Objective::Objective(const char* _name, int _indent)
    : indent(_indent)
    , start(TIME_UNKNOWN)
    , done(TIME_UNKNOWN)
    , duration(TIME_UNKNOWN)
{
    GuiUtils::StrCopy(name, _name, sizeof(name));
    PrintTime(cached_done, sizeof(cached_done), TIME_UNKNOWN);
    PrintTime(cached_start, sizeof(cached_start), TIME_UNKNOWN);
    PrintTime(cached_duration, sizeof(cached_duration), TIME_UNKNOWN);
}

ObjectiveTimerWindow::Objective& ObjectiveTimerWindow::Objective::AddStartEvent(
    EventType et, uint32_t id1, uint32_t id2)
{
    start_events.emplace_back<Event>({et, id1, id2});
    return *this;
}
ObjectiveTimerWindow::Objective& ObjectiveTimerWindow::Objective::AddEndEvent(
    EventType et, uint32_t id1, uint32_t id2)
{
    end_events.emplace_back<Event>({et, id1, id2});
    return *this;
}
ObjectiveTimerWindow::Objective& ObjectiveTimerWindow::Objective::SetStarted()
{
    if (start == TIME_UNKNOWN) {
        start = TimerWidget::Instance().GetTimer();
    }
    PrintTime(cached_start, sizeof(cached_start), start);
    status = Status::Started;
    return *this;
}
ObjectiveTimerWindow::Objective& ObjectiveTimerWindow::Objective::SetDone()
{
    if (done == TIME_UNKNOWN) {
        done = TimerWidget::Instance().GetTimer();
    }
    PrintTime(cached_done, sizeof(cached_done), done);

    // it's possible to have this called before the objective is "started".
    // This is for things that don't have a duration, and we leave start == TIME_UNKNOWN.
    if (start != TIME_UNKNOWN) {
        duration = done - start;
        PrintTime(cached_duration, sizeof(cached_duration), duration);
    }

    status = Status::Completed;
    runs_dirty = true;
    return *this;
}

bool ObjectiveTimerWindow::Objective::IsStarted() const { return start != TIME_UNKNOWN; }
bool ObjectiveTimerWindow::Objective::IsDone() const { return done != TIME_UNKNOWN; }

void ObjectiveTimerWindow::Objective::Update()
{
    unsigned long now = TimerWidget::Instance().GetTimer();

    if (start == TIME_UNKNOWN) {
        PrintTime(cached_duration, sizeof(cached_duration), TIME_UNKNOWN);
    } else if (done == TIME_UNKNOWN) {
        PrintTime(cached_duration, sizeof(cached_duration), now - start);
    }
    if (ObjectiveTimerWindow::Instance().clear_cached_times) {
        switch (status) {
            case Status::Completed: 
                PrintTime(cached_done, sizeof(cached_done), done);
                // fallthrough
            case Status::Started:
                PrintTime(cached_start, sizeof(cached_start), start);
                if (duration) {
                    PrintTime(cached_duration, sizeof(cached_duration), duration ? duration : now - start);
                }
                break;
            default: break;
        }
    }
}
void ObjectiveTimerWindow::Objective::Draw()
{
    switch (status) {
        case ObjectiveTimerWindow::Objective::Status::NotStarted:
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            break;
        case ObjectiveTimerWindow::Objective::Status::Started:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            break;
        case ObjectiveTimerWindow::Objective::Status::Completed:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
            break;
        case ObjectiveTimerWindow::Objective::Status::Failed:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
            break;
        default: 
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            break;
    }
    auto& style = ImGui::GetStyle();
    style.ButtonTextAlign.x = 0.0f;
    float label_width = GetLabelWidth();
    for (int i = 0; i < indent; ++i) {
        ImGui::Indent();
    }
    if (ImGui::Button(name, ImVec2(label_width - indent * style.IndentSpacing, 0))) {
        char buf[256];
        sprintf(buf, "[%s] ~ Start: %s ~ End: %s ~ Time: %s", name, cached_start, cached_done, cached_duration);
        GW::Chat::SendChat('#', buf);
    }
    style.ButtonTextAlign.x = 0.5f;
    ImGui::PopStyleColor();

    float ts_width = GetTimestampWidth();
    float offset = style.ItemSpacing.x + label_width + style.ItemSpacing.x;

    ImGui::PushItemWidth(ts_width);
    if (show_start_column) {
        ImGui::SameLine(offset);
        ImGui::Text(cached_start);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Start");
        offset += ts_width;
    }
    if (show_end_column) {
        ImGui::SameLine(offset);
        ImGui::Text(cached_done);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("End");
        offset += ts_width + style.ItemSpacing.x;
    }
    if (show_time_column) {
        ImGui::SameLine(offset);
        ImGui::Text(cached_duration);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Time");
    }
    for (int i = 0; i < indent; ++i) {
        ImGui::Unindent();
    }
}

void ObjectiveTimerWindow::ObjectiveSet::Update()
{
    if (!active) return;

    unsigned long now = TimerWidget::Instance().GetTimer();

    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
        PrintTime(cached_time, sizeof(cached_time), now, false);
    }

    for (Objective& obj : objectives) {
        obj.Update();
    }
}
void ObjectiveTimerWindow::ObjectiveSet::Event(EventType type, uint32_t id1, uint32_t id2)
{
    auto Match = [&](const Objective::Event& event) -> bool {
        if (type != event.type) return false;
        switch (type) {
            // for these, use id2 as a wchar_t*
            case EventType::ServerMessage:
            case EventType::DisplayDialogue: {
                const wchar_t* msg1 = (wchar_t*)id2;
                const wchar_t* msg2 = (wchar_t*)event.id2;
                for (uint32_t i = 0; i < id1 && i < event.id1; ++i) {
                    if (msg1[i] != 0 && msg1[i] != msg2[i]) return false;
                }
                return true;
            }

            default: 
                if (id1 != 0 && id1 != event.id1) return false;
                if (id2 != 0 && id2 != event.id2) return false;
                return true;
        }
    };

    bool just_set_something_done = false;
    size_t set_done_before = 0;
    
    for (size_t i = 0; i < objectives.size(); ++i) {
        ObjectiveTimerWindow::Objective& obj = objectives[i];
        if (obj.IsDone()) continue; // nothing to check

        if (!obj.IsStarted()) {
            for (auto& event : obj.start_events) {
                if (Match(event)) {
                    obj.SetStarted();
                    if (obj.starting_completes_all_previous) set_done_before = i;
                    if (obj.starting_completes_previous_objective && i > 0) {
                        Objective& other = objectives[i - 1];
                        if (!other.IsDone()) other.SetDone();
                    }
                    break;
                }  
            }
        }

        for (const Objective::Event& event : obj.end_events) {
            if (Match(event)) {
                obj.SetDone();
                just_set_something_done = true;
                break;
            }
        }
    }

    for (size_t i = 0; i < set_done_before; ++i) {
        Objective& obj = objectives[i];
        if (!obj.IsDone()) obj.SetDone();
    }

    if (just_set_something_done) {
        CheckSetDone();
    }
}
void ObjectiveTimerWindow::ObjectiveSet::CheckSetDone()
{
    bool done = true;
    for (const Objective& obj : objectives) {
        if (obj.done == TIME_UNKNOWN) {
            done = false;
            break;
        }
    }
    if (done) {
        active = false;
        if (ObjectiveTimerWindow::Instance().auto_send_age) {
            GW::Chat::SendChat('/', "age");
        }
        TimerWidget::Instance().SetRunCompleted();
    }
}

ObjectiveTimerWindow::ObjectiveSet::ObjectiveSet()
    : ui_id(cur_ui_id++)
{
    system_time = static_cast<DWORD>(time(NULL));
}

ObjectiveTimerWindow::ObjectiveSet* ObjectiveTimerWindow::ObjectiveSet::FromJson(nlohmann::json* json)
{
    ObjectiveSet* os = new ObjectiveSet;
    os->active = false;
    os->system_time = json->at("utc_start").get<DWORD>();
    std::string name = json->at("name").get<std::string>();
    snprintf(os->name, sizeof(os->name), "%s", name.c_str());
    os->instance_time = json->at("instance_start").get<DWORD>();
    PrintTime(os->cached_time, sizeof(cached_time), os->instance_time, false);
    nlohmann::json json_objs = json->at("objectives");
    for (nlohmann::json::iterator it = json_objs.begin(); it != json_objs.end(); ++it) {
        const nlohmann::json& o = it.value();
        name = o.at("name").get<std::string>();
        Objective obj(name.c_str());
        obj.status = o.at("status").get<Objective::Status>();
        obj.start = o.at("start").get<DWORD>();
        obj.duration = o.at("duration").get<DWORD>();
        obj.done = o.at("done").get<DWORD>();
        if (obj.done == 1 && obj.duration) obj.done = obj.start + obj.duration;
        switch (obj.status) {
            case Objective::Status::Completed: PrintTime(obj.cached_done, sizeof(obj.cached_done), obj.done);
            case Objective::Status::Failed:
            case Objective::Status::Started:
                PrintTime(obj.cached_start, sizeof(obj.cached_start), obj.start);
                if (obj.duration) PrintTime(obj.cached_duration, sizeof(obj.cached_duration), obj.duration);
                break;
            default: break;
        }
        os->objectives.emplace_back(obj);
    }
    os->StopObjectives();
    return os;
}
nlohmann::json ObjectiveTimerWindow::ObjectiveSet::ToJson()
{
    nlohmann::json json;
    json["name"] = name;
    json["instance_start"] = instance_time;
    json["utc_start"] = system_time;
    nlohmann::json json_objectives;
    for (const auto& o : objectives) {
        nlohmann::json obj_json;
        obj_json["name"] = o.name;
        obj_json["status"] = o.status;
        obj_json["start"] = o.start;
        obj_json["done"] = o.done;
        obj_json["duration"] = o.duration;
        json_objectives.push_back(obj_json);
    }
    json["objectives"] = json_objectives;
    return json;
}
bool ObjectiveTimerWindow::ObjectiveSet::Draw()
{
    char buf[256];
    if (!show_past_runs && from_disk) {
        struct tm timeinfo;
        GetStartTime(&timeinfo);
        time_t now = time(NULL);
        struct tm* nowinfo = localtime(&now);
        if (timeinfo.tm_yday != nowinfo->tm_yday || timeinfo.tm_year != nowinfo->tm_year) {
            return true; // Hide this objective set; its from a previous day
        }
    }
    if (show_start_date_time && !cached_start[0]) {
        struct tm timeinfo;
        GetStartTime(&timeinfo);
        time_t now = time(NULL);
        struct tm* nowinfo = localtime(&now);
        int cached_str_offset = 0;
        if (timeinfo.tm_yday != nowinfo->tm_yday || timeinfo.tm_year != nowinfo->tm_year) {
            char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
            cached_str_offset += snprintf(&cached_start[cached_str_offset], sizeof(cached_start) - cached_str_offset,
                "%s %02d, ", months[timeinfo.tm_mon], timeinfo.tm_mday);
        }
        snprintf(&cached_start[cached_str_offset], sizeof(cached_start) - cached_str_offset, "%02d:%02d - ",
            timeinfo.tm_hour, timeinfo.tm_min);
    }

    sprintf(buf, "%s%s - %s%s###header%u", show_start_date_time ? cached_start : "", name, cached_time,
        failed ? " [Failed]" : "", ui_id);

    bool is_open = true;
    bool is_collapsed = !ImGui::CollapsingHeader(buf, &is_open, ImGuiTreeNodeFlags_DefaultOpen);
    if (!is_open) return false;
    if (!is_collapsed) {
        ImGui::PushID(static_cast<int>(ui_id));
        for (Objective& objective : objectives) {
            objective.Draw();
        }
        ImGui::PopID();
    }
    if (need_to_collapse) {
        ImGui::GetCurrentWindow()->DC.StateStorage->SetInt(ImGui::GetID(buf), 0);
        need_to_collapse = false;
    }
    return true;
}

void ObjectiveTimerWindow::ObjectiveSet::GetStartTime(struct tm* timeinfo)
{
    time_t ts = (time_t)system_time;
    memcpy(timeinfo, localtime(&ts), sizeof(timeinfo[0]));
}
