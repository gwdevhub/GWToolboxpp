#include "stdafx.h"

#include <cwchar>

#include <Modules/GWEventBus.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Packets/Opcodes.h>
#include <GWCA/Packets/StoC.h>

void GWEventBus::Subscribe(const void* owner, GWBusCallback cb)
{
    for (auto& [o, fn] : subscribers_) {
        if (o == owner) { fn = std::move(cb); return; }
    }
    subscribers_.emplace_back(owner, std::move(cb));
}

void GWEventBus::Unsubscribe(const void* owner)
{
    auto it = std::find_if(subscribers_.begin(), subscribers_.end(),
        [owner](const auto& p) { return p.first == owner; });
    if (it != subscribers_.end()) subscribers_.erase(it);
}

void GWEventBus::Emit(const GWEvent& e) const
{
    // Snapshot rather than iterate subscribers_ directly: a callback that Subscribes or
    // Unsubscribes (e.g. a module tearing itself down mid-event) would otherwise mutate
    // the vector out from under this loop. Only ~2 subscribers exist today, so the copy
    // is free; it buys correctness regardless of what a callback does instead of relying
    // on subscribers never reentering.
    const auto subscribers_snapshot = subscribers_;
    for (const auto& [_, cb] : subscribers_snapshot)
        cb(e);
}

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------
void GWEventBus::Initialize()
{
    ToolboxModule::Initialize();

    // --- UIMessage-sourced events ---

    GW::UI::RegisterUIMessageCallback(
        &on_mission_complete_,
        GW::UI::UIMessage::kMissionComplete,
        [this](GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
            Emit({GWEvent::Type::MissionComplete,
                  static_cast<uint32_t>(GW::Map::GetMapID())});
        });

    // ObjectiveAdd: fires at mission start for each objective; type_flags 0x1 = bullet/sub-objective,
    // 0x0 = base/primary objective. GoalEngine uses the base objective's completion to
    // synthesize both MissionComplete and MissionBonus with a reliable map_id.
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveAdd>(
        &on_objective_add_,
        [this](GW::HookStatus*, const GW::Packet::StoC::ObjectiveAdd* p) {
            Emit({GWEvent::Type::ObjectiveAdd, p->objective_id, p->type});
        });

    GW::UI::RegisterUIMessageCallback(
        &on_vanquish_complete_,
        GW::UI::UIMessage::kVanquishComplete,
        [this](GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
            Emit({GWEvent::Type::VanquishComplete,
                  static_cast<uint32_t>(GW::Map::GetMapID())});
        });

    GW::UI::RegisterUIMessageCallback(
        &on_party_defeated_,
        GW::UI::UIMessage::kPartyDefeated,
        [this](GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
            Emit({GWEvent::Type::PartyDefeated});
        });

    // --- StoC-sourced events ---

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveDone>(
        &on_objective_done_,
        [this](GW::HookStatus*, const GW::Packet::StoC::ObjectiveDone* p) {
            Emit({GWEvent::Type::ObjectiveDone, p->objective_id,
                  static_cast<uint32_t>(GW::Map::GetMapID())});
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveUpdateName>(
        &on_objective_started_,
        [this](GW::HookStatus*, const GW::Packet::StoC::ObjectiveUpdateName* p) {
            Emit({GWEvent::Type::ObjectiveStarted, p->objective_id});
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ManipulateMapObject>(
        &on_door_,
        [this](GW::HookStatus*, const GW::Packet::StoC::ManipulateMapObject* p) {
            if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) return;
            if (p->animation_type == 16 && p->animation_stage == 2)
                Emit({GWEvent::Type::DoorOpen, p->object_id});
            else if (p->animation_type == 3 && p->animation_stage == 2)
                Emit({GWEvent::Type::DoorClose, p->object_id});
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentUpdateAllegiance>(
        &on_agent_allegiance_,
        [this](GW::HookStatus*, const GW::Packet::StoC::AgentUpdateAllegiance* p) {
            const auto* agent = GW::Agents::GetAgentByID(p->agent_id);
            if (!agent) return;
            const auto* living = agent->GetAsAgentLiving();
            if (!living) return;
            Emit({GWEvent::Type::AgentUpdateAllegiance,
                  living->player_number, p->allegiance_bits});
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DoACompleteZone>(
        &on_doa_zone_,
        [this](GW::HookStatus*, const GW::Packet::StoC::DoACompleteZone* p) {
            if (p->message[0] == 0x8101)
                Emit({GWEvent::Type::DoACompleteZone, p->message[1]});
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DungeonReward>(
        &on_dungeon_reward_,
        [this](GW::HookStatus*, GW::Packet::StoC::DungeonReward*) {
            Emit({GWEvent::Type::DungeonReward});
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageServer>(
        &on_server_message_,
        [this](GW::HookStatus*, GW::Packet::StoC::MessageServer*) {
            const auto* buff = &GW::GetGameContext()->world->message_buff;
            if (!buff || !buff->valid() || !buff->size()) return;
            const wchar_t* msg = buff->begin();
            GWEvent e;
            e.type    = GWEvent::Type::ServerMessage;
            e.str     = msg;
            e.str_len = wcslen(msg);
            Emit(e);
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DisplayDialogue>(
        &on_display_dialogue_,
        [this](GW::HookStatus*, const GW::Packet::StoC::DisplayDialogue* p) {
            GWEvent e;
            e.type    = GWEvent::Type::DisplayDialogue;
            e.str     = p->message;
            e.str_len = wcslen(p->message);
            Emit(e);
        });

    GW::StoC::RegisterPacketCallback(
        &on_countdown_start_, GAME_SMSG_INSTANCE_COUNTDOWN,
        [this](GW::HookStatus*, GW::Packet::StoC::PacketBase*) {
            Emit({GWEvent::Type::CountdownStart,
                  static_cast<uint32_t>(GW::Map::GetMapID())});
        });

    // Fires for all players — subscribers filter by controlled character as needed.
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::SkillActivate>(
        &on_skill_activate_,
        [this](GW::HookStatus*, const GW::Packet::StoC::SkillActivate* p) {
            Emit({GWEvent::Type::SkillActivate, p->agent_id, p->skill_id});
        });

    // Fires on party lock/unlock — subscribers gate to relevant maps as needed.
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyLock>(
        &on_party_lock_,
        [this](GW::HookStatus*, const GW::Packet::StoC::PartyLock* p) {
            Emit({GWEvent::Type::PartyLock, p->unk2});
        });

    // --- Instance lifecycle events ---

    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::InstanceLoadInfo>(
        &on_instance_load_info_,
        [this](GW::HookStatus*, const GW::Packet::StoC::InstanceLoadInfo* p) {
            Emit({GWEvent::Type::InstanceLoadInfo, p->map_id,
                  static_cast<uint32_t>(p->is_explorable)});
        });

    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::InstanceLoadFile>(
        &on_instance_load_file_,
        [this](GW::HookStatus*, const GW::Packet::StoC::InstanceLoadFile* p) {
            GWEvent e;
            e.type  = GWEvent::Type::InstanceLoadFile;
            e.id1   = p->map_fileID;
            e.spawn = p->spawn_point;
            Emit(e);
        });

    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::InstanceTimer>(
        &on_instance_timer_,
        [this](GW::HookStatus*, GW::Packet::StoC::InstanceTimer*) {
            Emit({GWEvent::Type::InstanceTimer});
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(
        &on_game_srv_transfer_,
        [this](GW::HookStatus*, const GW::Packet::StoC::GameSrvTransfer* p) {
            Emit({GWEvent::Type::GameSrvTransfer, p->map_id,
                  static_cast<uint32_t>(p->is_explorable)});
        });

    // --- Party composition events ---

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyPlayerAdd>(
        &on_party_player_add_,
        [this](GW::HookStatus*, const GW::Packet::StoC::PartyPlayerAdd* p) {
            Emit({GWEvent::Type::PartyPlayerAdd, p->player_id});
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyPlayerRemove>(
        &on_party_player_remove_,
        [this](GW::HookStatus*, const GW::Packet::StoC::PartyPlayerRemove* p) {
            Emit({GWEvent::Type::PartyPlayerRemove, p->player_id});
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyHeroAdd>(
        &on_party_hero_add_,
        [this](GW::HookStatus*, const GW::Packet::StoC::PartyHeroAdd* p) {
            Emit({GWEvent::Type::PartyHeroAdd, p->agent_id, p->hero_id});
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyHeroRemove>(
        &on_party_hero_remove_,
        [this](GW::HookStatus*, const GW::Packet::StoC::PartyHeroRemove* p) {
            Emit({GWEvent::Type::PartyHeroRemove, p->agent_id});
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyHenchmanAdd>(
        &on_party_henchman_add_,
        [this](GW::HookStatus*, const GW::Packet::StoC::PartyHenchmanAdd* p) {
            Emit({GWEvent::Type::PartyHenchmanAdd, p->agent_id,
                  static_cast<uint32_t>(p->profession), p->name_enc,
                  wcsnlen(p->name_enc, 8)});
        });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyHenchmanRemove>(
        &on_party_henchman_remove_,
        [this](GW::HookStatus*, const GW::Packet::StoC::PartyHenchmanRemove* p) {
            Emit({GWEvent::Type::PartyHenchmanRemove, p->agent_id});
        });

    // AgentState fires frequently for all agents — filter to dead-state transitions only.
    // The dead flag is bit 0x10 of the state bitmap (value 16).
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentState>(
        &on_agent_state_,
        [this](GW::HookStatus*, const GW::Packet::StoC::AgentState* p) {
            if (p->state & 0x10)
                Emit({GWEvent::Type::AgentDied, p->agent_id, p->state});
        });

    // QuestAdd fires when a quest is added or its log_state changes (active→complete etc.).
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::QuestAdd>(
        &on_quest_update_,
        [this](GW::HookStatus*, const GW::Packet::StoC::QuestAdd* p) {
            Emit({GWEvent::Type::QuestUpdate,
                  static_cast<uint32_t>(p->quest_id),
                  static_cast<uint32_t>(p->log_state)});
        });

    // QuestRemove fires when a quest leaves the log — turn-in (the common case) or abandon.
    // QuestAdd does not refire with a completed log_state, so this is the only observable
    // signal for quest completion.
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::QuestRemove>(
        &on_quest_remove_,
        [this](GW::HookStatus*, const GW::Packet::StoC::QuestRemove* p) {
            Emit({GWEvent::Type::QuestRemoved,
                  static_cast<uint32_t>(p->quest_id)});
        });
}

// ---------------------------------------------------------------------------
// SignalTerminate
// ---------------------------------------------------------------------------
void GWEventBus::SignalTerminate()
{
    GW::UI::RemoveUIMessageCallback(&on_mission_complete_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::ObjectiveAdd>(&on_objective_add_);
    GW::UI::RemoveUIMessageCallback(&on_vanquish_complete_);
    GW::UI::RemoveUIMessageCallback(&on_party_defeated_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::ObjectiveDone>(&on_objective_done_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::ObjectiveUpdateName>(&on_objective_started_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::ManipulateMapObject>(&on_door_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentUpdateAllegiance>(&on_agent_allegiance_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::DoACompleteZone>(&on_doa_zone_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::DungeonReward>(&on_dungeon_reward_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::MessageServer>(&on_server_message_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::DisplayDialogue>(&on_display_dialogue_);
    GW::StoC::RemoveCallback(GAME_SMSG_INSTANCE_COUNTDOWN, &on_countdown_start_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::SkillActivate>(&on_skill_activate_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::PartyLock>(&on_party_lock_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::InstanceLoadInfo>(&on_instance_load_info_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::InstanceLoadFile>(&on_instance_load_file_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::InstanceTimer>(&on_instance_timer_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::GameSrvTransfer>(&on_game_srv_transfer_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::PartyPlayerAdd>(&on_party_player_add_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::PartyPlayerRemove>(&on_party_player_remove_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::PartyHeroAdd>(&on_party_hero_add_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::PartyHeroRemove>(&on_party_hero_remove_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::PartyHenchmanAdd>(&on_party_henchman_add_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::PartyHenchmanRemove>(&on_party_henchman_remove_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentState>(&on_agent_state_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::QuestAdd>(&on_quest_update_);
    GW::StoC::RemoveCallback<GW::Packet::StoC::QuestRemove>(&on_quest_remove_);
    ToolboxModule::SignalTerminate();
}
