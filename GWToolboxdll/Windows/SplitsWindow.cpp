#include "stdafx.h"

#include <algorithm>
#include <cctype>
#include <cwchar>
#include <functional>

#include <Windows/SplitsWindow.h>
#include <Windows/Splits/SCPresets.h>
#include <Modules/Resources.h>
#include <Utils/EncString.h>
#include <Utils/TextUtils.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Title.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/QuestMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/Packets/Opcodes.h>
#include <GWCA/Packets/StoC.h>

#include <Modules/WebSocketModule.h>

// ---------------------------------------------------------------------------
// JSON DTOs for resume.json / per-list run history (glaze reflection requires external linkage).
// ---------------------------------------------------------------------------
namespace SplitsWindowJson {
    struct SerializedSplit {
        double real_time    = 0.0;
        double game_time    = 0.0;
        double segment_real = 0.0;
        double segment_game = 0.0;
        // Only meaningful when status == "Started" — a Completed goal's own split fields above are its record.
        double start_real_time  = 0.0;
        double start_game_time  = 0.0;
        int    trigger_progress = 0;
        std::string status; // "Started" or "Completed" — NotStarted goals are std::nullopt, not this
    };

    struct SerializedResume {
        std::string list_name;
        double      real_time = 0.0;
        double      game_time = 0.0;
        std::optional<double> total_paused;
        std::optional<bool>   is_preset; // which folder list_name's file lives in — see SaveActiveList's Defaults\ split
        std::vector<std::optional<SerializedSplit>> goals;
    };

    struct SerializedRunSplit {
        double real_time = 0.0;
        double game_time = 0.0;
    };

    struct SerializedRunGoal {
        std::string label;
        std::string status; // "Completed" / "Failed" / "Started" / "NotStarted"
        double real_time = 0.0;
        double game_time = 0.0;
    };

    struct SerializedRun {
        double total_real = 0.0;
        std::vector<SerializedRunSplit> splits;
        std::optional<bool> failed;
        std::optional<int64_t> utc_start;
        std::optional<std::string> character_name;
        std::optional<std::vector<SerializedRunGoal>> goals;
        std::optional<double> total_paused;
    };
}
using namespace SplitsWindowJson;

// Shadow-step skills that should trigger auto-start the same as movement.
// Mirrors TimerLogic.cpp in GWChrono.
static const std::unordered_set<uint32_t> kShadowStepSkills = {
     769,  // Viper's Defense
     770,  // Return
     771,  // Aura of Displacement
     799,  // Beguiling Haze
     815,  // Scorpion Wire
     836,  // Ride the Lightning
     925,  // Recall
     952,  // Death's Charge
    1032,  // Heart of Shadow
    1040,  // Spirit Walk
    1044,  // Dark Prison
    1644,  // Wastrel's Collapse
    1646,  // Augury of Death
    1650,  // Shadow Walk
    1651,  // Death's Retreat
    1652,  // Shadow Prison
    1653,  // Swap
    1654,  // Shadow Meld
    2052,  // Shadow Fang
    2420,  // Ebon Escape
    3428,  // Shadow Theft
};

// Manual: maps with a "Time until mission start" ready-check dialog whose wait should pause game time. Vizunah Square/Unwaking Waters lock the party (PartyLock); Ascalon Academy's queue is solo and only sends the generic countdown packet — both funnel into in_mission_queue_.
static bool IsMissionQueueMap(GW::Constants::MapID map)
{
    static constexpr GW::Constants::MapID kQueueMaps[] = {
        GW::Constants::MapID::Vizunah_Square_Local_Quarter_outpost,
        GW::Constants::MapID::Vizunah_Square_Foreign_Quarter_outpost,
        GW::Constants::MapID::Unwaking_Waters_Luxon_outpost,
        GW::Constants::MapID::Unwaking_Waters_Kurzick_outpost,
        GW::Constants::MapID::Ascalon_City_pre_searing,
    };
    for (const auto m : kQueueMaps) {
        if (map == m) return true;
    }
    return false;
}

// =============================================================================
// LIFECYCLE
// =============================================================================

// ---------------------------------------------------------------------------
// Initialize / Terminate
// ---------------------------------------------------------------------------
void SplitsWindow::Initialize()
{
    ToolboxWindow::Initialize();

    // Built now, not at the zone-transition tick — see doa_preset_cache_'s own comment.
    for (int i = 0; i < 4; ++i)
        doa_preset_cache_[static_cast<size_t>(i)] = SCPresets::BuildDoAPresetForZone(i);

    GW::UI::RegisterUIMessageCallback(
        &on_mission_complete_,
        GW::UI::UIMessage::kMissionComplete,
        [this](GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
            const auto map_id = static_cast<uint32_t>(GW::Map::GetMapID());
            engine_.NotifyMissionComplete(static_cast<GW::Constants::MapID>(map_id));
            // map_id is GetMapID() at the exact moment kMissionComplete fired — logged so a goal that silently never completes can be diagnosed against its own trigger.map_id (some missions report a transient "_cinematic" map_id here).
            PushDbgEvent("MissComplete", map_id, 0);
        });

    GW::UI::RegisterUIMessageCallback(
        &on_vanquish_complete_,
        GW::UI::UIMessage::kVanquishComplete,
        [this](GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
            const auto map_id = static_cast<uint32_t>(GW::Map::GetMapID());
            engine_.NotifyVanquishComplete(static_cast<GW::Constants::MapID>(map_id));
            PushDbgEvent("VqComplete", map_id, 0);
        });

    GW::UI::RegisterUIMessageCallback(
        &on_party_defeated_,
        GW::UI::UIMessage::kPartyDefeated,
        [this](GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
            // Latched rather than acted on directly, so ApplyTimerPolicy() is the one place all auto-fail/auto-start decisions live.
            pending_party_defeated_ = true;
        });

    // ObjectiveAdd: fires at mission start for each objective; type_flags 0x1 = bullet/sub-objective, 0x0 = base/primary objective — GoalEngine uses the base objective's completion to synthesize MissionComplete with a reliable map_id.
    GW::UI::RegisterUIMessageCallback(
        &on_objective_add_,
        GW::UI::UIMessage::kObjectiveAdd,
        [this](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
            const auto* p = static_cast<GW::UI::UIPacket::kObjectiveAdd*>(wparam);
            engine_.NotifyObjectiveAdd(p->objective_id, p->type);
            PushDbgEvent("ObjAdd", p->objective_id, p->type);
        });

    GW::UI::RegisterUIMessageCallback(
        &on_objective_done_,
        GW::UI::UIMessage::kObjectiveComplete,
        [this](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
            const auto* p = static_cast<GW::UI::UIPacket::kObjectiveComplete*>(wparam);
            const auto map_id = static_cast<uint32_t>(GW::Map::GetMapID());
            engine_.NotifyEvent(GoalTrigger::Type::ObjectiveDone, p->objective_id, map_id);
            PushDbgEvent("ObjDone", p->objective_id, map_id);
        });

    GW::UI::RegisterUIMessageCallback(
        &on_objective_started_,
        GW::UI::UIMessage::kObjectiveUpdated,
        [this](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
            const auto* p = static_cast<GW::UI::UIPacket::kObjectiveUpdated*>(wparam);
            engine_.NotifyEvent(GoalTrigger::Type::ObjectiveStarted, p->objective_id);
            PushDbgEvent("ObjStart", p->objective_id, 0);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ManipulateMapObject>(
        &on_door_,
        [this](GW::HookStatus*, const GW::Packet::StoC::ManipulateMapObject* p) {
            if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) return;
            if (p->animation_type == 16 && p->animation_stage == 2) {
                engine_.NotifyEvent(GoalTrigger::Type::DoorOpen, p->object_id);
                PushDbgEvent("DoorOpen", p->object_id, 0);
            } else if (p->animation_type == 3 && p->animation_stage == 2) {
                engine_.NotifyEvent(GoalTrigger::Type::DoorClose, p->object_id);
                PushDbgEvent("DoorClose", p->object_id, 0);
            }
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentUpdateAllegiance>(
        &on_agent_allegiance_,
        [this](GW::HookStatus*, const GW::Packet::StoC::AgentUpdateAllegiance* p) {
            const auto* agent = GW::Agents::GetAgentByID(p->agent_id);
            if (!agent) return;
            const auto* living = agent->GetAsAgentLiving();
            if (!living) return;
            engine_.NotifyEvent(GoalTrigger::Type::AgentUpdateAllegiance, living->player_number, p->allegiance_bits);
            PushDbgEvent("AgentAllg", living->player_number, p->allegiance_bits);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DoACompleteZone>(
        &on_doa_zone_,
        [this](GW::HookStatus*, const GW::Packet::StoC::DoACompleteZone* p) {
            if (p->message[0] != 0x8101) return;
            engine_.NotifyEvent(GoalTrigger::Type::DoACompleteZone, p->message[1]);
            PushDbgEvent("DoAZone", p->message[1], 0);
        });

    GW::UI::RegisterUIMessageCallback(
        &on_dungeon_reward_,
        GW::UI::UIMessage::kDungeonComplete,
        [this](GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
            engine_.NotifyEvent(GoalTrigger::Type::DungeonReward);
            PushDbgEvent("DungeonRwd", 0, 0);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageServer>(
        &on_server_message_,
        [this](GW::HookStatus*, GW::Packet::StoC::MessageServer*) {
            const auto* buff = &GW::GetGameContext()->world->message_buff;
            if (!buff || !buff->valid() || !buff->size()) return;
            const wchar_t* msg = buff->begin();
            const auto len = wcslen(msg);
            engine_.NotifyEvent(GoalTrigger::Type::ServerMessage, 0, 0, msg, len);
            // v1/v2 can't hold a full pattern — length + first wchar is just enough to sanity-check which one fired during live testing.
            PushDbgEvent("SrvMsg", static_cast<uint32_t>(len), len ? msg[0] : 0);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DisplayDialogue>(
        &on_display_dialogue_,
        [this](GW::HookStatus*, const GW::Packet::StoC::DisplayDialogue* p) {
            const auto len = wcslen(p->message);
            engine_.NotifyEvent(GoalTrigger::Type::DisplayDialogue, 0, 0, p->message, len);
            PushDbgEvent("DispDlg", static_cast<uint32_t>(len), len ? p->message[0] : 0);
        });

    // ToPK/Ascalon Academy countdown — gated to mission queue maps so unrelated countdowns don't pause Manual's game time.
    GW::StoC::RegisterPacketCallback(
        &on_countdown_start_, GAME_SMSG_INSTANCE_COUNTDOWN,
        [this](GW::HookStatus*, GW::Packet::StoC::PacketBase*) {
            const auto map_id = static_cast<uint32_t>(GW::Map::GetMapID());
            if (active_profile_idx_ == 0 && IsMissionQueueMap(static_cast<GW::Constants::MapID>(map_id)))
                in_mission_queue_ = true;
            engine_.NotifyEvent(GoalTrigger::Type::CountdownStart, map_id);
            PushDbgEvent("Countdown", map_id, 0);
        });

    // Running: shadow-step auto-start — filter to local player only.
    GW::UI::RegisterUIMessageCallback(
        &on_skill_activate_,
        GW::UI::UIMessage::kSkillActivated,
        [this](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
            const auto* p = static_cast<GW::UI::UIPacket::kAgentSkillPacket*>(wparam);
            if (p->agent_id == GW::Agents::GetControlledCharacterId())
                pending_skill_id_ = static_cast<uint32_t>(p->skill_id);
        });

    // Manual: party lock signals the mission-start ready-check queue is up/down, gated to mission queue maps so unrelated party locks don't affect game time.
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyLock>(
        &on_party_lock_,
        [this](GW::HookStatus*, const GW::Packet::StoC::PartyLock* p) {
            if (!p->unk2) {
                in_mission_queue_ = false;
            } else if (active_profile_idx_ == 0 && IsMissionQueueMap(last_map_)) {
                in_mission_queue_ = true;
            }
        });

    // Zone entry — fires for every new instance including same-map re-entry (district change, new character into same starting map, etc.).
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::InstanceLoadInfo>(
        &on_instance_load_info_,
        [this](GW::HookStatus*, const GW::Packet::StoC::InstanceLoadInfo* p) {
            pending_came_from_explorable_ = last_was_explorable_;
            last_was_explorable_          = (p->is_explorable != 0);
            last_map_                     = static_cast<GW::Constants::MapID>(p->map_id);
            in_mission_queue_             = false;
            pending_map_enter_            = true;
            if (NuzlockeDeathRulesEnabled()) nuzlocke_.OnInstanceLoad();
        });

    // Transfer packet fires before InstanceLoadInfo — clear queue state immediately on any server transfer (covers district changes where the map doesn't change).
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(
        &on_game_srv_transfer_,
        [this](GW::HookStatus*, const GW::Packet::StoC::GameSrvTransfer*) {
            in_mission_queue_ = false;
        });

    // DoA's zone rotation is spawn-dependent, not a fixed map_id lookup — latched independently of pending_map_enter_ and consumed by ApplySCAutoLoadPreset().
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::InstanceLoadFile>(
        &on_instance_load_file_,
        [this](GW::HookStatus*, const GW::Packet::StoC::InstanceLoadFile* p) {
            pending_doa_file_id_ = p->map_fileID;
            pending_doa_spawn_   = p->spawn_point;
            // v2 = spawn.x only (v1/v2 can't hold both floats) — enough to sanity-check DetectDoAStartingZone's input during live testing without a debugger.
            PushDbgEvent("InstLoadFile", p->map_fileID, static_cast<uint32_t>(static_cast<int32_t>(p->spawn_point.x)));
        });

    // --- Challenge/Nuzlocke debug events ---

    // Debug-log only — no player_id in the wparam-less UI message, but nothing here reads it for real tracking either (nuzlocke_.players is seeded from GetPlayerName(), never from this event).
    GW::UI::RegisterUIMessageCallback(
        &on_party_player_add_,
        GW::UI::UIMessage::kPartyAddPlayer,
        [this](GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
            PushDbgEvent("PlyAdd", 0, 0);
        });

    GW::UI::RegisterUIMessageCallback(
        &on_party_player_remove_,
        GW::UI::UIMessage::kPartyRemovePlayer,
        [this](GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
            PushDbgEvent("PlyRem", 0, 0);
        });

    // AgentState fires frequently for all agents — filter to dead-state transitions only (bit 0x10). Nuzlocke death tracking polls GetIsDead() instead (see NuzlockeUpdate); this hook now only feeds MobKill.
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentState>(
        &on_agent_state_,
        [this](GW::HookStatus*, const GW::Packet::StoC::AgentState* p) {
            if (!(p->state & 0x10)) return;
            // Model ID doubles as living->player_number for non-player agents — forward it so a MobKill goal can count it. Players excluded since their player_number is a login/party index, not a monster model.
            if (const auto* agent = GW::Agents::GetAgentByID(p->agent_id)) {
                if (const auto* living = agent->GetAsAgentLiving(); living && !living->IsPlayer())
                    engine_.NotifyEvent(GoalTrigger::Type::MobKill, living->player_number);
            }
            PushDbgEvent("AgentDied", p->agent_id, p->state);
        });

    // kQuestAdded fires on pickup, and again (re-announcing already-current log_state) as part of a full quest-log resync at zone transitions — not a live per-objective push. The IsCompleted() check here is a safety-net fallback (e.g. picked up already-complete); kQuestDetailsChanged below is the actual live "objective just finished" signal.
    GW::UI::RegisterUIMessageCallback(
        &on_quest_update_,
        GW::UI::UIMessage::kQuestAdded,
        [this](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
            const auto quest_id = *static_cast<GW::Constants::QuestID*>(wparam);
            auto* quest = GW::QuestMgr::GetQuest(quest_id);
            engine_.NotifyEvent(GoalTrigger::Type::QuestPickup, static_cast<uint32_t>(quest_id));
            if (quest && quest->IsCompleted())
                engine_.NotifyEvent(GoalTrigger::Type::QuestComplete, static_cast<uint32_t>(quest_id));
            PushDbgEvent("QuestUpd", static_cast<uint32_t>(quest_id), quest ? quest->log_state : 0);
        });

    // kQuestDetailsChanged fires live when a quest's own state changes (e.g. an objective completing) — unlike kQuestAdded, not tied to a zone-transition resync.
    GW::UI::RegisterUIMessageCallback(
        &on_quest_details_changed_,
        GW::UI::UIMessage::kQuestDetailsChanged,
        [this](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
            const auto quest_id = *static_cast<GW::Constants::QuestID*>(wparam);
            auto* quest = GW::QuestMgr::GetQuest(quest_id);
            if (quest && quest->IsCompleted())
                engine_.NotifyEvent(GoalTrigger::Type::QuestComplete, static_cast<uint32_t>(quest_id));
            PushDbgEvent("QuestDetail", static_cast<uint32_t>(quest_id), quest ? quest->log_state : 0);
        });

    // kQuestRemoved fires on turn-in AND abandon — doesn't distinguish the two, so an abandoned QuestComplete goal fires early. Acceptable for run-tracking; revisit if it causes false splits.
    GW::UI::RegisterUIMessageCallback(
        &on_quest_remove_,
        GW::UI::UIMessage::kQuestRemoved,
        [this](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
            const auto quest_id = *static_cast<GW::Constants::QuestID*>(wparam);
            engine_.NotifyEvent(GoalTrigger::Type::QuestComplete, static_cast<uint32_t>(quest_id));
            PushDbgEvent("QuestRem", static_cast<uint32_t>(quest_id), 0);
        });
}


// Defaulted here, not in the header, matching NuzlockeState's own out-of-line ctor/dtor below.
SplitsWindow::SplitsWindow()  = default;
SplitsWindow::~SplitsWindow() = default;

// Defaulted here, not in the header, since pending_hench_names/city_hench_names hold unique_ptr<GuiUtils::EncString> and EncString is only forward-declared in NuzlockeState.h.
NuzlockeState::NuzlockeState()  = default;
NuzlockeState::~NuzlockeState() = default;


void SplitsWindow::Terminate()
{
    GW::UI::RemoveUIMessageCallback(&on_mission_complete_);
    GW::UI::RemoveUIMessageCallback(&on_vanquish_complete_);
    GW::UI::RemoveUIMessageCallback(&on_party_defeated_);
    GW::UI::RemoveUIMessageCallback(&on_objective_add_);
    GW::UI::RemoveUIMessageCallback(&on_objective_done_);
    GW::UI::RemoveUIMessageCallback(&on_objective_started_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::ManipulateMapObject>(&on_door_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentUpdateAllegiance>(&on_agent_allegiance_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::DoACompleteZone>(&on_doa_zone_);
    GW::UI::RemoveUIMessageCallback(&on_dungeon_reward_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::MessageServer>(&on_server_message_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::DisplayDialogue>(&on_display_dialogue_);
    GW::StoC::RemoveCallback(GAME_SMSG_INSTANCE_COUNTDOWN, &on_countdown_start_);
    GW::UI::RemoveUIMessageCallback(&on_skill_activate_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::PartyLock>(&on_party_lock_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::InstanceLoadInfo>(&on_instance_load_info_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::GameSrvTransfer>(&on_game_srv_transfer_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::InstanceLoadFile>(&on_instance_load_file_);
    GW::UI::RemoveUIMessageCallback(&on_party_player_add_);
    GW::UI::RemoveUIMessageCallback(&on_party_player_remove_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentState>(&on_agent_state_);
    GW::UI::RemoveUIMessageCallback(&on_quest_update_);
    GW::UI::RemoveUIMessageCallback(&on_quest_details_changed_);
    GW::UI::RemoveUIMessageCallback(&on_quest_remove_);
    engine_.Detach();
    ToolboxWindow::Terminate();
}


void SplitsWindow::PushDbgEvent(const char* tag, const uint32_t v1, const uint32_t v2)
{
    if (!debug_log_events_) return;
    if (challenge_dbg_events_.size() >= 200)
        challenge_dbg_events_.erase(challenge_dbg_events_.begin());
    challenge_dbg_events_.push_back({tag, v1, v2});
}


// =============================================================================
// SETTINGS
// =============================================================================

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------
void SplitsWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWindow::LoadSettings(doc, legacy);

    // Establish data folders — profile subfolders created below
    const auto splits_path = Resources::GetPath(L"splits");
    const auto runs_path   = Resources::GetPath(L"splits\\runs");
    Resources::EnsureFolderExists(splits_path);
    Resources::EnsureFolderExists(runs_path);
    Resources::EnsureFolderExists(splits_path / L"manual");
    Resources::EnsureFolderExists(splits_path / L"running");
    Resources::EnsureFolderExists(splits_path / L"sc");
    Resources::EnsureFolderExists(splits_path / L"sc" / L"Defaults");
    Resources::EnsureFolderExists(runs_path   / L"manual");
    Resources::EnsureFolderExists(runs_path   / L"running");
    Resources::EnsureFolderExists(runs_path   / L"sc");
    Resources::EnsureFolderExists(runs_path   / L"sc" / L"Defaults");
    splits_folder_ = splits_path.wstring() + L"\\";
    runs_folder_   = runs_path.wstring() + L"\\";

    auto get_long = [&](const char* key, long def) -> long {
        long v = def;
        if (!doc.Get(Name(), key, v)) v = legacy->GetLongValue(Name(), key, def);
        return v;
    };
    key_start_ = get_long("key_start", 0);
    key_reset_ = get_long("key_reset", 0);
    key_split_ = get_long("key_split", 0);
    for (const auto& f : kColorFields) {
        Color& field = this->*f.member;
        if (!doc.Get(Name(), f.key, field))
            field = Colors::Load(legacy, Name(), f.key, field);
    }
    active_profile_idx_ = static_cast<int>(get_long("active_profile", 0));
    if (active_profile_idx_ < 0 || active_profile_idx_ >= kProfileCount) active_profile_idx_ = 0;
    for (int i = 0; i < kProfileCount; ++i)
        profiles_[i].LoadSettings(doc, legacy, kProfileSections[i]);

    for (const auto& f : kNuzlockeBoolFields)  doc.Get(Name(), f.key, nuzlocke_.*f.member);
    for (const auto& f : kNuzlockeLivesFields) nuzlocke_.*f.member = static_cast<int>(get_long(f.key, 1));
    for (const auto& f : kNuzlockePointFields) nuzlocke_.goal_points.*f.member = static_cast<int>(get_long(f.key, 0));

    doc.Get(Name(), "debug_log_events", debug_log_events_);

    engine_.Attach(&active_list_);

    // Crash-protection resume check
    const std::wstring resume_path = splits_folder_ + L"resume.json";
    std::ifstream rf(resume_path);
    if (rf.is_open()) {
        std::stringstream ss;
        ss << rf.rdbuf();
        rf.close();

        SerializedResume j;
        constexpr glz::opts opts{.error_on_unknown_keys = false};
        if (!glz::read<opts>(j, ss.str())) {
            if (!j.list_name.empty()) {
                // active_profile_idx_ is already restored above, so ActiveSplitsFolder() returns the correct profile subfolder.
                const std::wstring list_path = ActiveSplitsFolder() +
                    std::wstring(j.list_name.begin(), j.list_name.end()) + L".json";
                if (std::filesystem::exists(list_path)) {
                    pending_resume_name_ = j.list_name;
                    pending_resume_data_ = ss.str();
                    pending_resume_      = true;
                }
            }
        }
    }
}


void SplitsWindow::SaveSettings(SettingsDoc& doc)
{
    doc.Set(Name(), "key_start", key_start_);
    doc.Set(Name(), "key_reset", key_reset_);
    doc.Set(Name(), "key_split", key_split_);
    for (const auto& f : kColorFields)
        doc.Set(Name(), f.key, this->*f.member);
    doc.Set(Name(), "active_profile", active_profile_idx_);
    for (int i = 0; i < kProfileCount; ++i)
        profiles_[i].SaveSettings(doc, kProfileSections[i]);
    for (const auto& f : kNuzlockeBoolFields)  doc.Set(Name(), f.key, nuzlocke_.*f.member);
    for (const auto& f : kNuzlockeLivesFields) doc.Set(Name(), f.key, nuzlocke_.*f.member);
    for (const auto& f : kNuzlockePointFields) doc.Set(Name(), f.key, nuzlocke_.goal_points.*f.member);

    doc.Set(Name(), "debug_log_events", debug_log_events_);
    ToolboxWindow::SaveSettings(doc);
}


// =============================================================================
// GOAL LIST & SAVED-LIST MANAGEMENT
// =============================================================================

// ---------------------------------------------------------------------------
// Goal list management
// ---------------------------------------------------------------------------
void SplitsWindow::NewActiveList(const char* name)
{
    engine_.Detach();
    active_list_ = GoalList{};
    active_list_.name = name ? name : "New List";
    clock_.Reset();
    engine_.Attach(&active_list_);
    cached_saved_lists_dirty_ = true;
}

void SplitsWindow::SaveActiveList(bool clear_preset)
{
    if (splits_folder_.empty() || active_list_.name.empty()) return;
    // Own subfolder, not a filename prefix in the same folder — a preset and a user's own list can then share a display name (e.g. both called "Shards of Orr") with zero collision, in Explorer or in-app.
    const std::wstring folder = active_list_.is_preset ? ActiveSplitsFolder() + L"Defaults\\" : ActiveSplitsFolder();
    // An explicit user Save always produces a normal, user-owned list — presets are never persisted to disk themselves (always rebuilt live via ApplySCAutoLoadPreset). UpdateReferenceIfPB's internal auto-save passes false so it doesn't silently reclassify an in-progress farming session's list partway through (which used to split its own run history across two files — see RunHistoryFilePath).
    if (clear_preset) active_list_.is_preset = false;
    const std::wstring wname(active_list_.name.begin(), active_list_.name.end());
    active_list_.SaveToFile(folder + wname + L".json");
    cached_saved_lists_dirty_ = true;
}


void SplitsWindow::ReplaceActiveList(const std::function<void()>& populate)
{
    engine_.Detach();
    populate();
    engine_.Attach(&active_list_);
    LoadPB();
}


void SplitsWindow::LoadActiveList(const std::wstring& path)
{
    DeleteResumeState();
    ReplaceActiveList([&] { active_list_.LoadFromFile(path); });
    clock_.Reset();
    ResetRunFlags();
}


void SplitsWindow::SetActiveList(GoalList list)
{
    // Deliberately not NewActiveList() + mutating List()->goals afterward: NewActiveList() attaches the engine against an empty GoalList, so Attach()-time setup (starts_immediately, etc.) would silently never apply if goals were added after the fact.
    DeleteResumeState();
    // User-initiated, not tool-managed — never leave is_preset set on what's about to become an ordinary editable list.
    ReplaceActiveList([&] { active_list_ = std::move(list); active_list_.is_preset = false; });
    clock_.Reset();
    ResetRunFlags();
}


std::vector<std::pair<std::string, std::wstring>> SplitsWindow::GetSavedLists() const
{
    if (splits_folder_.empty()) return {};
    const std::wstring folder = ActiveSplitsFolder();
    if (cached_saved_lists_dirty_ || folder != cached_saved_lists_folder_) {
        // Presets now live in their own Defaults\ subfolder (see SaveActiveList), so the non-recursive directory scan below already excludes them without needing a name-based filter.
        cached_saved_lists_ = GoalList::ListSaved(folder);
        cached_saved_lists_folder_ = folder;
        cached_saved_lists_dirty_  = false;
    }
    return cached_saved_lists_;
}


std::wstring SplitsWindow::RunHistoryFilePath() const
{
    std::wstring safe_name(active_list_.name.begin(), active_list_.name.end());
    for (auto& c : safe_name) {
        if (c == L' ') c = L'_';
        else if (c == L'/' || c == L'\\' || c == L':' || c == L'*' ||
                 c == L'?' || c == L'"'  || c == L'<' || c == L'>' || c == L'|')
            c = L'-';
    }
    // Same Defaults\ subfolder split as SaveActiveList() — without it, a preset run and a user's own saved list sharing the same display name would collide onto one run-history file.
    const std::wstring folder = active_list_.is_preset ? ActiveRunsFolder() + L"Defaults\\" : ActiveRunsFolder();
    return folder + safe_name + L".json";
}


// ---------------------------------------------------------------------------
// Profile folder routing
// ---------------------------------------------------------------------------
std::wstring SplitsWindow::ActiveSplitsFolder() const
{
    return splits_folder_ + kProfileFolderNames[static_cast<size_t>(active_profile_idx_)];
}

std::wstring SplitsWindow::ActiveRunsFolder() const
{
    return runs_folder_ + kProfileFolderNames[static_cast<size_t>(active_profile_idx_)];
}


// =============================================================================
// PB / COMPARISON / RUN HISTORY
// =============================================================================

void SplitsWindow::LoadPB(bool refresh_comparisons)
{
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    pb_splits_.clear();       pb_splits_game_.clear();       pb_total_real_ = nan;
    if (refresh_comparisons) {
        avg_splits_.clear();      avg_splits_game_.clear();
        best_seg_splits_.clear(); best_seg_splits_game_.clear();
    }

    if (runs_folder_.empty() || active_list_.name.empty()) return;

    const std::wstring runs_path = RunHistoryFilePath();
    std::ifstream rf(runs_path);
    if (!rf.is_open()) return;

    std::stringstream ss;
    ss << rf.rdbuf();

    std::vector<SerializedRun> runs;
    constexpr glz::opts opts{.error_on_unknown_keys = false};
    if (glz::read<opts>(runs, ss.str())) return;

    // Arrays are indexed by non-header goals only (headers carry no split data).
    int goal_count = 0;
    for (const auto& g : active_list_.goals) if (!g.is_header) ++goal_count;
    if (goal_count == 0) return;

    // PB: fastest non-failed run that reached every goal.
    double best = std::numeric_limits<double>::infinity();
    const SerializedRun* best_run = nullptr;
    for (const auto& run : runs) {
        if (run.failed.value_or(false)) continue;
        if (static_cast<int>(run.splits.size()) < goal_count) continue;
        if (run.total_real < best) { best = run.total_real; best_run = &run; }
    }
    if (best_run) {
        pb_total_real_ = best;
        pb_splits_.resize(static_cast<size_t>(goal_count), nan);
        pb_splits_game_.resize(static_cast<size_t>(goal_count), nan);
        for (int i = 0; i < goal_count && i < static_cast<int>(best_run->splits.size()); ++i) {
            pb_splits_[static_cast<size_t>(i)]      = best_run->splits[static_cast<size_t>(i)].real_time;
            pb_splits_game_[static_cast<size_t>(i)] = best_run->splits[static_cast<size_t>(i)].game_time;
        }
    }

    // Unconditional, unlike Average/Sum of Best below — a just-finished run should show up here immediately, not be excluded from its own history.
    recent_runs_.clear();
    constexpr size_t kMaxRecentRuns = 5;
    const size_t take = std::min(kMaxRecentRuns, runs.size());
    recent_runs_.reserve(take);
    for (size_t i = 0; i < take; ++i) {
        const SerializedRun& run = runs[runs.size() - 1 - i]; // newest first
        RecentRun rr;
        rr.total_real = run.total_real;
        rr.failed     = run.failed.value_or(false);
        rr.utc_start  = run.utc_start.value_or(0);
        if (run.goals) {
            rr.goals.reserve(run.goals->size());
            for (const auto& g : *run.goals)
                rr.goals.push_back({g.label, g.real_time, g.game_time, g.status == "Completed"});
        }
        recent_runs_.push_back(std::move(rr));
    }

    if (!refresh_comparisons) return;

    // Average: mean of each goal index's time across every non-failed run that reached it — a run that only got to goal 3 of 5 still contributes to goals 0-2's average.
    {
        std::vector<double> sum_real(static_cast<size_t>(goal_count), 0.0);
        std::vector<double> sum_game(static_cast<size_t>(goal_count), 0.0);
        std::vector<int>    count(static_cast<size_t>(goal_count), 0);
        for (const auto& run : runs) {
            if (run.failed.value_or(false)) continue;
            for (int i = 0; i < goal_count && i < static_cast<int>(run.splits.size()); ++i) {
                sum_real[static_cast<size_t>(i)] += run.splits[static_cast<size_t>(i)].real_time;
                sum_game[static_cast<size_t>(i)] += run.splits[static_cast<size_t>(i)].game_time;
                ++count[static_cast<size_t>(i)];
            }
        }
        avg_splits_.resize(static_cast<size_t>(goal_count), nan);
        avg_splits_game_.resize(static_cast<size_t>(goal_count), nan);
        for (int i = 0; i < goal_count; ++i) {
            if (count[static_cast<size_t>(i)] > 0) {
                avg_splits_[static_cast<size_t>(i)]      = sum_real[static_cast<size_t>(i)] / count[static_cast<size_t>(i)];
                avg_splits_game_[static_cast<size_t>(i)] = sum_game[static_cast<size_t>(i)] / count[static_cast<size_t>(i)];
            }
        }
    }

    // Sum of Best: cumulative sum of each leg's fastest-ever segment, non-failed complete runs only (same restriction as Average) — the theoretical best if every best segment lined up in one run.
    {
        const auto inf = std::numeric_limits<double>::infinity();
        std::vector<double> best_seg_real(static_cast<size_t>(goal_count), inf);
        std::vector<double> best_seg_game(static_cast<size_t>(goal_count), inf);
        for (const auto& run : runs) {
            if (run.failed.value_or(false)) continue;
            if (static_cast<int>(run.splits.size()) < goal_count) continue;
            double prev_real = 0.0, prev_game = 0.0;
            for (int i = 0; i < goal_count; ++i) {
                const double seg_real = run.splits[static_cast<size_t>(i)].real_time - prev_real;
                const double seg_game = run.splits[static_cast<size_t>(i)].game_time - prev_game;
                if (seg_real < best_seg_real[static_cast<size_t>(i)]) best_seg_real[static_cast<size_t>(i)] = seg_real;
                if (seg_game < best_seg_game[static_cast<size_t>(i)]) best_seg_game[static_cast<size_t>(i)] = seg_game;
                prev_real = run.splits[static_cast<size_t>(i)].real_time;
                prev_game = run.splits[static_cast<size_t>(i)].game_time;
            }
        }
        best_seg_splits_.resize(static_cast<size_t>(goal_count), nan);
        best_seg_splits_game_.resize(static_cast<size_t>(goal_count), nan);
        double cum_real = 0.0, cum_game = 0.0;
        for (int i = 0; i < goal_count; ++i) {
            if (std::isinf(best_seg_real[static_cast<size_t>(i)])) break; // no successful run ever reached this leg
            cum_real += best_seg_real[static_cast<size_t>(i)];
            cum_game += best_seg_game[static_cast<size_t>(i)];
            best_seg_splits_[static_cast<size_t>(i)]      = cum_real;
            best_seg_splits_game_[static_cast<size_t>(i)] = cum_game;
        }
    }
}


const std::vector<double>& SplitsWindow::CompareSplits() const
{
    using CM = SplitsProfile::ComparisonMode;
    switch (ActiveProfile().comparison_mode) {
        case CM::Average:   return avg_splits_;
        case CM::SumOfBest: return best_seg_splits_;
        default:            return pb_splits_;
    }
}


const std::vector<double>& SplitsWindow::CompareSplitsGame() const
{
    using CM = SplitsProfile::ComparisonMode;
    switch (ActiveProfile().comparison_mode) {
        case CM::Average:   return avg_splits_game_;
        case CM::SumOfBest: return best_seg_splits_game_;
        default:            return pb_splits_game_;
    }
}


void SplitsWindow::UpdateReferenceIfPB()
{
    if (std::isnan(pb_total_real_) || pb_splits_.empty()) return;

    double ref_total = std::numeric_limits<double>::infinity();
    if (active_list_.reference.has_value() && !active_list_.reference->splits.empty())
        ref_total = active_list_.reference->splits.back();

    if (pb_total_real_ >= ref_total) return;

    GoalReference& ref = active_list_.reference.emplace();
    ref.splits = pb_splits_;

    SaveActiveList(/*clear_preset=*/false);
}


// ---------------------------------------------------------------------------
// SaveCompletedRun / FailRun / SaveRunToHistory
// ---------------------------------------------------------------------------
void SplitsWindow::SaveCompletedRun()
{
    run_complete_ = true;
    clock_.Pause();
    SaveRunToHistory(/*failed=*/false);
    LoadPB(/*refresh_comparisons=*/false);
    UpdateReferenceIfPB();
    DeleteResumeState();
    if (ActiveProfile().auto_send_age)
        GW::Chat::SendChat('/', L"age");
}


void SplitsWindow::FailRun(const char* reason)
{
    // RealTime()>0, not just IsRunning() — Running's own auto-pause-on-leaving-explorable can fire earlier this same tick (e.g. a wrong turn into a town), so a run that's genuinely in progress but just paused this instant must still be failable.
    if (run_complete_ || run_failed_ || (!clock_.IsRunning() && clock_.RealTime() <= 0.0)) return;
    engine_.FailRun(clock_);
    clock_.Pause();
    run_failed_ = true;
    WebSocketModule::Instance().Send("reset", (std::string("Splits: Reset - ") + reason).c_str());
    SaveRunToHistory(/*failed=*/true);
    DeleteResumeState();
}


void SplitsWindow::SaveRunToHistory(bool failed)
{
    if (runs_folder_.empty() || active_list_.name.empty()) return;

    const std::wstring runs_path = RunHistoryFilePath();

    std::vector<SerializedRun> runs;
    {
        std::ifstream rf(runs_path);
        if (rf.is_open()) {
            std::stringstream ss;
            ss << rf.rdbuf();
            constexpr glz::opts opts{.error_on_unknown_keys = false};
            if (glz::read<opts>(runs, ss.str())) runs.clear();
        }
    }

    auto status_name = [](GoalStatus s) -> std::string {
        switch (s) {
            case GoalStatus::Started:   return "Started";
            case GoalStatus::Completed: return "Completed";
            case GoalStatus::Failed:    return "Failed";
            default:                    return "NotStarted";
        }
    };

    SerializedRun run;
    run.total_real      = clock_.RealTime();
    run.failed          = failed;
    run.utc_start       = run_start_unix_;
    run.character_name  = run_char_name_;
    run.total_paused    = total_paused_real_;

    // Headers carry no split data — PB/history arrays index only non-header goals.
    std::vector<SerializedRunGoal> rgoals;
    for (const auto& g : active_list_.goals) {
        if (g.is_header) continue;
        run.splits.push_back(SerializedRunSplit{g.split.real_time, g.split.game_time});
        rgoals.push_back(SerializedRunGoal{g.label, status_name(g.status),
                                            g.split.real_time, g.split.game_time});
    }
    run.goals = std::move(rgoals);

    runs.push_back(std::move(run));
    constexpr size_t kMaxRuns = 200;
    if (runs.size() > kMaxRuns)
        runs.erase(runs.begin(), runs.end() - static_cast<long>(kMaxRuns));

    std::ofstream f(runs_path);
    if (f.is_open())
        f << glz::write<glz::opts{.prettify = true}>(runs).value_or(std::string{});
}


// =============================================================================
// CRASH PROTECTION / RESUME
// =============================================================================

// ---------------------------------------------------------------------------
// Crash protection
// ---------------------------------------------------------------------------
void SplitsWindow::SaveResumeState()
{
    if (splits_folder_.empty() || !clock_.IsRunning() || active_list_.name.empty()) return;

    SerializedResume j;
    j.list_name    = active_list_.name;
    j.real_time    = clock_.RealTime();
    j.game_time    = clock_.GameTime();
    j.total_paused = total_paused_real_;
    if (active_list_.is_preset) j.is_preset = true;

    j.goals.reserve(active_list_.goals.size());
    for (const auto& g : active_list_.goals) {
        // Started (not just Completed) goals need saving too — otherwise a crash mid-goal (e.g. an SC room already entered, a partial MobKill count) silently reverts to NotStarted on resume.
        if (g.status != GoalStatus::Completed && g.status != GoalStatus::Started) {
            j.goals.emplace_back(std::nullopt);
        } else {
            SerializedSplit js{
                g.split.real_time, g.split.game_time, g.split.segment_real, g.split.segment_game,
                g.start_real_time, g.start_game_time, g.trigger_progress,
                g.status == GoalStatus::Completed ? "Completed" : "Started"};
            j.goals.push_back(std::move(js));
        }
    }

    std::ofstream f(splits_folder_ + L"resume.json");
    if (f.is_open()) f << glz::write<glz::opts{.prettify = true}>(j).value_or(std::string{});
}

void SplitsWindow::DeleteResumeState()
{
    pending_resume_ = false;
    pending_resume_name_.clear();
    pending_resume_data_.clear();
    if (splits_folder_.empty()) return;
    std::error_code ec;
    std::filesystem::remove(splits_folder_ + L"resume.json", ec);
}


void SplitsWindow::ApplyResume()
{
    if (!pending_resume_) return;
    pending_resume_ = false;

    SerializedResume j;
    constexpr glz::opts opts{.error_on_unknown_keys = false};
    const bool parse_failed = static_cast<bool>(glz::read<opts>(j, pending_resume_data_));
    pending_resume_data_.clear();
    if (parse_failed) return;

    if (j.list_name.empty()) return;
    const double real_time = j.real_time;
    const double game_time = j.game_time;

    const bool was_preset = j.is_preset.value_or(false);
    const std::wstring list_path = (was_preset ? ActiveSplitsFolder() + L"Defaults\\" : ActiveSplitsFolder()) +
        std::wstring(j.list_name.begin(), j.list_name.end()) + L".json";

    // LoadFromFile always reads is_preset back as false (SaveActiveList clears it before writing — see its own comment), so restore it from the resume snapshot instead of trusting the file body.
    ReplaceActiveList([&] { active_list_.LoadFromFile(list_path); active_list_.is_preset = was_preset; });

    for (size_t i = 0; i < j.goals.size() && i < active_list_.goals.size(); ++i) {
        const auto& jg = j.goals[i];
        if (!jg.has_value()) continue;
        auto& g              = active_list_.goals[i];
        g.trigger_progress   = jg->trigger_progress;
        if (jg->status == "Started") {
            g.status          = GoalStatus::Started;
            g.start_real_time = jg->start_real_time;
            g.start_game_time = jg->start_game_time;
        } else {
            g.status             = GoalStatus::Completed;
            g.split.real_time    = jg->real_time;
            g.split.game_time    = jg->game_time;
            g.split.segment_real = jg->segment_real;
            g.split.segment_game = jg->segment_game;
        }
    }

    clock_.Restore(real_time, game_time);
    engine_.ForceStarted();
    total_paused_real_   = j.total_paused.value_or(0.0);
    pending_map_enter_            = false;
    pending_came_from_explorable_ = false;
}


void SplitsWindow::DiscardResume()
{
    pending_resume_ = false;
    pending_resume_data_.clear();
    DeleteResumeState();
}


// =============================================================================
// RUN LIFECYCLE CONTROLS
// =============================================================================

void SplitsWindow::ResetRunFlags()
{
    run_complete_              = false;
    run_failed_                = false;
    running_awaiting_movement_ = false;
    running_load_paused_       = false;
    pending_skill_id_          = 0;
    in_mission_queue_          = false;
    manually_paused_           = false;
    manual_pause_accum_        = 0.0;
    total_paused_real_         = 0.0;
    pending_map_enter_            = false;
    pending_came_from_explorable_ = false;
}


void SplitsWindow::BeginRun(const char* reason)
{
    run_char_name_.clear();
    if (const wchar_t* wname = GW::PlayerMgr::GetPlayerName())
        run_char_name_ = TextUtils::WStringToString(wname);
    run_start_unix_ = static_cast<int64_t>(time(nullptr));
    WebSocketModule::Instance().Send("reset", "Splits: Reset - run starting");
    WebSocketModule::Instance().Send("start", (std::string("Splits: Start - ") + reason).c_str());
    clock_.Start();
}


// ---------------------------------------------------------------------------
// Controls
// ---------------------------------------------------------------------------
void SplitsWindow::StartRun()
{
    if (clock_.IsRunning()) {
        // Pause — leave all run/goal state untouched.
        clock_.Pause();
        manually_paused_    = true;
        manual_pause_accum_ = 0.0;
        return;
    }

    if (manually_paused_) {
        // Resume — keep all progress; just fold the pause into the running total and continue.
        total_paused_real_ += manual_pause_accum_;
        manually_paused_ = false;
        clock_.Resume();
        return;
    }

    // Fresh start.
    engine_.Attach(&active_list_);
    // Full refresh (not just the PB-only rescan SaveCompletedRun() does) so Average/Sum of Best pick up whatever run just finished, since the run about to start should compare against it.
    LoadPB();
    BeginRun("manually started");
    engine_.ForceStarted();
}

void SplitsWindow::ResetRun()
{
    if (clock_.IsRunning() || run_complete_ || run_failed_)
        WebSocketModule::Instance().Send("reset", "Splits: Reset - run reset");
    DeleteResumeState();
    ResetRunFlags();
    engine_.Reset();
    clock_.Reset();
    // last_map_ deliberately left untouched: re-entry only fires when InstanceLoadInfo arrives, so no spurious MapEnter re-trigger on reset.
    // Full refresh, unlike SaveCompletedRun()'s PB-only rescan.
    LoadPB();
    if (NuzlockeDeathRulesEnabled()) nuzlocke_.ResetProgress();
}

void SplitsWindow::TriggerManualSplit()
{
    engine_.TriggerManual(clock_);
}

void SplitsWindow::SwitchProfile(int idx)
{
    if (idx < 0 || idx >= kProfileCount || idx == active_profile_idx_) return;

    if (!active_list_.name.empty())
        profiles_[active_profile_idx_].last_list_name = active_list_.name;

    active_profile_idx_ = idx;

    // Reset run state without clearing last_map_ — resetting it to None would trigger a spurious just_entered_map next tick, immediately re-loading presets.
    DeleteResumeState();
    run_complete_       = false;
    run_failed_         = false;
    manually_paused_    = false;
    manual_pause_accum_ = 0.0;
    total_paused_real_  = 0.0;
    engine_.Detach();
    engine_.Reset();
    clock_.Reset();

    active_list_ = GoalList{};

    const SplitsProfile& p = ActiveProfile();
    if (!p.last_list_name.empty() && !splits_folder_.empty()) {
        const std::wstring path = ActiveSplitsFolder() +
            std::wstring(p.last_list_name.begin(), p.last_list_name.end()) + L".json";
        if (std::filesystem::exists(path))
            LoadActiveList(path);
    }
}


// =============================================================================
// TICK LOOP & TIMER POLICY
// =============================================================================

// ---------------------------------------------------------------------------
// Update — called every frame
// ---------------------------------------------------------------------------
void SplitsWindow::Update(float delta)
{
    if (NuzlockeDeathRulesEnabled()) nuzlocke_.Update(last_was_explorable_);

    const auto instance_type   = GW::Map::GetInstanceType();
    const bool is_explorable   = (instance_type == GW::Constants::InstanceType::Explorable);
    const bool is_loading      = (instance_type == GW::Constants::InstanceType::Loading);
    const bool in_cinematic    = GW::Map::GetIsInCinematic();
    const bool is_running      = ActiveProfile().sequential_route;

    // Consume bus-sourced map-entry flags (set by InstanceLoadInfo / GameSrvTransfer callbacks).
    const bool just_entered_map     = pending_map_enter_;
    const bool came_from_explorable = pending_came_from_explorable_;
    pending_map_enter_            = false;
    pending_came_from_explorable_ = false;

    // Before engine_.Update() below, so a freshly-swapped-in preset is already attached in time for this same tick's Pass 1/autostart checks.
    ApplySCAutoLoadPreset(just_entered_map);

    // Accumulate wall-clock time while manually paused (clock_.RealTime() is frozen during a manual pause, so it can't measure how long the pause lasted).
    if (manually_paused_)
        manual_pause_accum_ += static_cast<double>(delta);
    // Manual: pause game time during loading, cinematics, and mission-start queues. Running: game time is controlled entirely by the clock pause below (explorable only).
    const bool time_paused = is_loading || in_cinematic
        || (active_profile_idx_ == 0 && in_mission_queue_);

    clock_.AddRealTime(static_cast<double>(delta));

    if (!time_paused)
        clock_.AddGameTime(static_cast<double>(delta));

    const GW::Agent* controlled = GW::Agents::GetControlledCharacter();
    const GW::AgentLiving* controlled_living = controlled ? controlled->GetAsAgentLiving() : nullptr;
    int player_level = 0;
    if (controlled_living)
        player_level = static_cast<int>(controlled_living->level);

    // Running: clock only ticks in explorable areas (no loading, no town time).
    if (is_running) {
        if (!is_explorable && clock_.IsRunning() && !running_load_paused_) {
            clock_.Pause();
            running_load_paused_ = true;
        }
        // Arm movement detector whenever in explorable and clock isn't active — matches GWChrono's continuous check so the player doesn't need to re-zone to arm.
        if (is_explorable && !clock_.IsRunning() && !run_complete_ && !run_failed_)
            running_awaiting_movement_ = true;
    }

    // Running: movement or shadow step in an explorable starts or resumes the clock.
    if (is_running && !run_complete_ && !run_failed_) {
        if (running_awaiting_movement_ && is_explorable) {
            const uint32_t skill = pending_skill_id_;
            pending_skill_id_    = 0;

            bool triggered = skill != 0 && kShadowStepSkills.count(skill) != 0;
            if (!triggered) {
                triggered = controlled_living &&
                    (controlled_living->GetIsMoving() ||
                     controlled_living->model_state == 204 ||
                     controlled_living->move_x != 0.f ||
                     controlled_living->move_y != 0.f);
            }

            if (triggered) {
                running_awaiting_movement_ = false;
                if (running_load_paused_) {
                    // Mid-run resume after a town or loading screen.
                    clock_.Resume();
                    running_load_paused_ = false;
                } else {
                    // Initial run start.
                    run_complete_ = false;
                    run_failed_   = false;
                    BeginRun("movement detected");
                    engine_.ForceStarted();
                }
            }
        } else {
            pending_skill_id_ = 0;
        }
    }

    // Needs both: IsRunning() catches the exact tick Start() just fired (RealTime() hasn't accumulated yet that tick); RealTime()>0 catches the exact tick Pause() just fired on leaving an explorable (IsRunning() already flipped false, but real_elapsed_ survives a pause).
    const bool fire_map_enter = !is_running
        ? just_entered_map
        : (just_entered_map && (clock_.IsRunning() || clock_.RealTime() > 0.0));

    // Synchronous last_was_explorable_, not the live-polled is_explorable above (see GoalEngine::Update's own comment).
    const int fired = engine_.Update(clock_, last_map_, fire_map_enter,
                                     came_from_explorable, last_was_explorable_,
                                     player_level);
    // TEMPORARY diagnostic for the MissionComplete-not-firing investigation — see GoalEngine::debug_notes_.
    for (const auto& n : engine_.debug_notes_) PushDbgEvent(n.tag, n.v1, n.v2);
    engine_.debug_notes_.clear();

    if (clock_.IsRunning()) {
        resume_save_timer_ += static_cast<float>(delta);
        if (fired > 0 || resume_save_timer_ >= 1.0f) {
            SaveResumeState();
            resume_save_timer_ = 0.f;
        }
        if (fired > 0)
            WebSocketModule::Instance().Send("split", "Splits: Split - goal complete");
    }

    ApplyTimerPolicy(just_entered_map);

    // Running: also check completion when a split just fired with the clock paused (final outpost entry).
    if (!run_complete_ && !run_failed_ && !active_list_.goals.empty() &&
        (clock_.IsRunning() || (is_running && fired > 0))) {
        bool all_done = true;
        for (const auto& g : active_list_.goals) {
            if (g.is_header) continue;
            if (g.status != GoalStatus::Completed) { all_done = false; break; }
        }
        if (all_done) SaveCompletedRun();
    }

    // Keybind edge detection
    auto poll_key = [](int vk, bool& prev) -> bool {
        if (vk <= 0) { prev = false; return false; }
        const bool held  = (GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool fired2 = held && !prev;
        prev = held;
        return fired2;
    };
    if (poll_key(key_start_, key_start_prev_)) StartRun();
    if (poll_key(key_reset_, key_reset_prev_)) ResetRun();
    if (poll_key(key_split_, key_split_prev_)) TriggerManualSplit();
}


// ---------------------------------------------------------------------------
// Timer policy: auto-fail (party wipe, incomplete rezone) and Manual profile auto-start. See the header doc comment on ApplyTimerPolicy() for why this stays separate from GoalEngine's fire/complete switch.
// ---------------------------------------------------------------------------
void SplitsWindow::ApplyTimerPolicy(const bool just_entered_map)
{
    // ---- Auto-fail conditions ----
    const bool party_defeated = pending_party_defeated_;
    pending_party_defeated_    = false;
    if (party_defeated && ActiveProfile().stop_on_party_defeated && clock_.IsRunning())
        FailRun(); // default reason: "party defeated"

    // A VQ/Mission/Bonus goal was attempted and abandoned (rezoned out of its target map without completing it). Always drained so the flag can't go stale across ticks even when this behavior is turned off.
    if (engine_.ConsumeIncompleteRezone() && ActiveProfile().auto_fail_on_rezone && clock_.IsRunning())
        FailRun("left the area without finishing the objective");

    const bool is_running = ActiveProfile().sequential_route;

    // Wrong turn — Running only, no toggle unlike rezone above. RealTime()>0 too, not just IsRunning() — see FailRun()'s own comment, same auto-pause-same-tick race.
    if (engine_.ConsumeWrongMapEntered() && is_running && (clock_.IsRunning() || clock_.RealTime() > 0.0))
        FailRun("wrong turn");

    // run done + new loading screen = forget old run, reset, let auto-start try again
    if (!is_running && (run_complete_ || run_failed_) && ActiveProfile().auto_reset_on_complete && just_entered_map)
        ResetRun();

    // ---- Manual/SC profiles: auto-start the clock ----
    // Auto-starts on the first goal firing; for Mission/Bonus/Vanquish/Dungeon/Titles/MobKill first goals, also starts on the earliest sign of an attempt (not just completion) so the clock covers the whole thing. Excludes manually_paused_ so a user pause isn't immediately undone.
    // SC=OT here: OT's own timer auto-starts at map-load into the relevant explorable, same MapEnter-based early-start rule as Manual's Mission/Bonus/Vanquish, matched against the goal's own trigger.map_id (Dungeons) or its owning header's map_id (Elite Areas, see below).
    // NOTE: intentionally separate from GoalEngine's fire/complete switch (Pass 2) — trigger firing and clock policy are independent pieces, so some duplication is expected. A new trigger type with a real attempt-to-complete gap needs a progress rule here too (see the matching note on GoalTrigger::Type).
    if (!is_running &&
        !clock_.IsRunning() && !run_complete_ && !run_failed_ && !manually_paused_) {
        bool should_start = false;
        // Elite Areas checkpoints carry no map_id of their own — only the auto-created area header does. Remembered as we walk past each header so the first non-header goal can fall back to it.
        GW::Constants::MapID owning_header_map = GW::Constants::MapID::None;
        for (const auto& g : active_list_.goals) {
            if (g.is_header) {
                owning_header_map = g.trigger.map_id;
                continue;
            }
            if (g.status == GoalStatus::Started || g.status == GoalStatus::Completed) {
                should_start = true;
            } else if (just_entered_map && last_was_explorable_) {
                // last_was_explorable_ (set synchronously from InstanceLoadInfo) rather than the polled is_explorable/GetInstanceType(), which can still reflect the previous instance for a frame at the transition boundary and was silently failing this check. Also correctly excludes a mission map_id entered as an outpost/staging instance before the explorable unlocks.
                using TT = GoalTrigger::Type;
                const auto tt = g.trigger.type;
                if (tt == TT::MissionComplete || tt == TT::MissionBonus || tt == TT::VanquishComplete ||
                    tt == TT::DungeonReward) {
                    if (last_map_ == g.trigger.map_id) should_start = true;
                } else if (owning_header_map != GW::Constants::MapID::None && last_map_ == owning_header_map) {
                    should_start = true;
                }
            } else if (g.trigger.type == GoalTrigger::Type::ReachTitleRank) {
                // No map/zone signal to key off — start the instant any progress toward the title is detected, rather than waiting for the full rank to complete.
                const GW::Title* title = GW::PlayerMgr::GetTitleTrack(g.trigger.title_id);
                if (title && title->current_points > 0) should_start = true;
            } else if (g.trigger.type == GoalTrigger::Type::MobKill) {
                // Same reasoning as Mission/Bonus/Vanquish above: start on the first kill, not after the full trigger.param2 count completes.
                if (g.trigger_progress > 0) should_start = true;
            }
            break; // only check the first non-header goal
        }
        if (should_start) BeginRun("first goal fired");
    }
}


void SplitsWindow::ApplySCAutoLoadPreset(const bool just_entered_map)
{
    if (active_profile_idx_ != 2) return;

    // Domain of Anguish, keyed off InstanceLoadFile's file_id (219215, same magic number as OT's CheckIsMapLoaded), not the map_id path below — DoA's zone rotation is spawn-dependent, so it can't be a static per-map lookup. Checked independently of just_entered_map since InstanceLoadFile can arrive on a different Update() tick than InstanceLoadInfo (server packet order isn't guaranteed), so this is its own one-shot latch instead.
    if (pending_doa_file_id_ == 219215) {
        const GW::Vec2f spawn = pending_doa_spawn_;
        pending_doa_file_id_  = 0; // consume regardless of outcome (incl. Mallyx/no swap)
        if (!clock_.IsRunning()) {
            const int starting_zone = SCPresets::DetectDoAStartingZone(spawn);
            if (starting_zone != -1) { // -1 = Mallyx, not DoA
                // Copy from doa_preset_cache_, not a fresh SCPresets::BuildDoAPresetList(spawn) — building it here risks still being in progress when Room 1's door-close event fires.
                GoalList doa_preset = doa_preset_cache_[static_cast<size_t>(starting_zone)];
                if (active_list_.name != doa_preset.name &&
                    (active_list_.name.empty() || active_list_.is_preset)) {
                    SetActiveList(std::move(doa_preset));
                    active_list_.is_preset = true;
                    // starts_immediately (see BuildDoAPresetForZone) only sets Room 1's own status — Pass 2 still needs engine_.started_ true to actually evaluate its end trigger, and just_entered_map may not be true this exact tick.
                    engine_.ForceStarted();
                }
            }
        }
        return;
    }

    if (!just_entered_map || clock_.IsRunning()) return;

    auto preset = SCPresets::BuildPresetForMap(last_map_);
    if (!preset) return; // not a tracked dungeon/elite area

    // Compare by anchor map_id (the header's, or the single goal's — every preset type stamps this as its dungeon/area identity), not by name or is_preset: a renamed or explicitly-loaded list for the SAME dungeon still counts as correct, but one for a DIFFERENT dungeon gets swapped regardless of how it was loaded.
    auto anchor_map_id = [](const GoalList& list) {
        return list.goals.empty() ? GW::Constants::MapID::None : list.goals.front().trigger.map_id;
    };
    if (anchor_map_id(*preset) != GW::Constants::MapID::None && anchor_map_id(active_list_) == anchor_map_id(*preset))
        return; // already the right dungeon/area

    SetActiveList(std::move(*preset));
    // SetActiveList always clears is_preset, but this path is auto-detection, not a user pick, so it must stay swappable for the next dungeon/area the player walks into.
    active_list_.is_preset = true;
}


// =============================================================================
// DRAW
// =============================================================================

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------
void SplitsWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) return;
    window_.Draw(*this);
}


// ---------------------------------------------------------------------------
// Settings UI
// ---------------------------------------------------------------------------
void SplitsWindow::DrawSettingsInternal()
{
    window_.DrawSettings(*this);

    // Manual-only feature start to finish — hidden entirely outside Manual rather than shown disabled/inert, so there's no confusion about whether it's doing anything for Running/SC.
    if (active_profile_idx_ == 0) {
        ImGui::Separator();
        ImGui::TextUnformatted("Nuzlocke");
        ImGui::Indent();

        // Death Rules and Points are independent modules — enabling one doesn't require the other. Death Rules draws its roster below the goal list; Points has no section of its own, shown left-aligned in the header clock row.
        ImGui::Checkbox("Death Rules", &nuzlocke_.death_rules_enabled);
        if (nuzlocke_.death_rules_enabled) {
            ImGui::Indent();
            ImGui::SetNextItemWidth(120.f);
            ImGui::InputInt("Hero lives", &nuzlocke_.hero_lives);
            ImGui::SetNextItemWidth(120.f);
            ImGui::InputInt("Henchman lives", &nuzlocke_.hench_lives);
            if (nuzlocke_.hero_lives  < 1) nuzlocke_.hero_lives  = 1;
            if (nuzlocke_.hench_lives < 1) nuzlocke_.hench_lives = 1;

            ImGui::SetNextItemWidth(120.f);
            ImGui::InputInt("Player lives", &nuzlocke_.player_lives);
            if (nuzlocke_.player_lives < 1) nuzlocke_.player_lives = 1;

            ImGui::Checkbox("Merge same-named henchmen across campaigns", &nuzlocke_.merge_hench_by_name);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "Off: a henchman name reused by a different NPC/build in another campaign\n"
                    "or outpost (e.g. two different \"Eve\"s) tracks as a separate entry.\n"
                    "On: any henchman sharing that display name is folded into one entry,\n"
                    "sharing the same life count.\n"
                    "Only affects henchmen tracked from here on, not ones already seen this session.");
            }
            ImGui::Unindent();
        }

        ImGui::Checkbox("Points", &nuzlocke_.points_enabled);
        if (nuzlocke_.points_enabled) {
            ImGui::Indent();
            ImGui::TextDisabled("Leave at 0 for goal types you don't want scored.");
            ImGui::SetNextItemWidth(100.f); ImGui::InputInt("Manual##nuzlocke_pts", &nuzlocke_.goal_points.manual);
            ImGui::SetNextItemWidth(100.f); ImGui::InputInt("Missions",     &nuzlocke_.goal_points.missions);
            ImGui::SetNextItemWidth(100.f); ImGui::InputInt("Explorables",  &nuzlocke_.goal_points.explorables);
            ImGui::SetNextItemWidth(100.f); ImGui::InputInt("Towns",       &nuzlocke_.goal_points.towns);
            ImGui::SetNextItemWidth(100.f); ImGui::InputInt("Titles",      &nuzlocke_.goal_points.titles);
            ImGui::SetNextItemWidth(100.f); ImGui::InputInt("Reach Level", &nuzlocke_.goal_points.reach_level);
            ImGui::SetNextItemWidth(100.f); ImGui::InputInt("Quest",       &nuzlocke_.goal_points.quest);
            ImGui::SetNextItemWidth(100.f); ImGui::InputInt("Skill Learnt", &nuzlocke_.goal_points.skill_learnt);
            ImGui::Unindent();
        }

        ImGui::Unindent();
    }

    ImGui::Separator();
    if (ImGui::CollapsingHeader("Party / Quest Debug Log")) {
        // Off by default, same idea as OT's show_debug_events but an in-UI list instead of Log::Info. Nothing is captured into challenge_dbg_events_ at all while off (see PushDbgEvent), not just hidden.
        ImGui::Checkbox("Log events", &debug_log_events_);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Records every party/quest/preset event below as it fires.\nUse for debugging and to confirm hooks are actually firing during live testing.");
        if (ImGui::Button("Clear##cdbg")) challenge_dbg_events_.clear();
        ImGui::SameLine();
        ImGui::TextDisabled("(%zu events)", challenge_dbg_events_.size());
        ImGui::TextDisabled("PlyAdd/Rem: v1=player_number  |  HeroAdd: v1=agent_id v2=hero_id  |  HenchAdd: v1=agent_id v2=profession  |  AgentDied: v1=agent_id v2=state  |  QuestUpd: v1=quest_id v2=log_state");
        ImGui::TextDisabled("ObjAdd: v1=objective_id v2=type_flags(0x1=bullet)  |  ObjDone: v1=objective_id v2=map_id  |  ObjStart: v1=objective_id");
        ImGui::TextDisabled("DoorOpen/Close: v1=object_id  |  AgentAllg: v1=player_number v2=allegiance_bits  |  DungeonRwd: (no params)");
        ImGui::TextDisabled("DoAZone: v1=zone message word  |  Countdown: v1=map_id  |  InstLoadFile: v1=file_id v2=spawn.x");
        ImGui::TextDisabled("SrvMsg/DispDlg: v1=pattern length v2=first wchar (not the full pattern)");
        ImGui::TextDisabled("MissComplete/MissBonus/VqComplete: v1=map_id (GetMapID() at that instant \xe2\x80\x94 compare against the goal's own map_id if it's not firing)");
        ImGui::BeginChild("##cdbglog", {0, 160}, true, ImGuiWindowFlags_HorizontalScrollbar);
        for (const auto& e : challenge_dbg_events_) {
            ImGui::Text("%-10s  v1=%-6u (0x%04X)  v2=%-6u (0x%04X)",
                e.tag, e.v1, e.v1, e.v2, e.v2);
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.f)
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
    }
}


// ---------------------------------------------------------------------------
// Nuzlocke — behavior lives in NuzlockeState (Windows/Splits/Nuzlocke.cpp); these are thin wrappers gated on profile/active-list state NuzlockeState doesn't own.
// ---------------------------------------------------------------------------
void SplitsWindow::DrawNuzlockeSection()
{
    if (NuzlockeDeathRulesEnabled()) nuzlocke_.Draw();
}

int SplitsWindow::NuzlockeTotalPoints() const
{
    return nuzlocke_.TotalPoints(active_list_);
}
