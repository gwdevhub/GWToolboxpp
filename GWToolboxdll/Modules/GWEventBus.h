#pragma once

#include <functional>
#include <vector>

#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Utilities/Hook.h>

#include <ToolboxModule.h>

// ---------------------------------------------------------------------------
// GWEvent — unified event fired by GWEventBus to all subscribers.
// ---------------------------------------------------------------------------
struct GWEvent {
    enum class Type : uint8_t {
        // UIMessage-sourced
        MissionComplete,        // id1 = map_id
        MissionBonus,           // id1 = map_id  (kObjectiveComplete in GW terms)
        VanquishComplete,       // id1 = map_id
        PartyDefeated,

        // StoC-sourced
        ObjectiveDone,          // id1 = objective_id
        ObjectiveStarted,       // id1 = objective_id
        DoorOpen,               // id1 = object_id
        DoorClose,              // id1 = object_id
        AgentUpdateAllegiance,  // id1 = player_number, id2 = allegiance_bits
        DoACompleteZone,        // id1 = zone word (msg[1])
        DungeonReward,
        ServerMessage,          // str/str_len — pointer valid only during callback
        DisplayDialogue,        // str/str_len — pointer valid only during callback
        CountdownStart,         // id1 = map_id
        SkillActivate,          // id1 = agent_id, id2 = skill_id
        PartyLock,              // id1 = lock_state (unk2: 0=unlock, non-zero=lock)

        // Instance lifecycle
        InstanceLoadInfo,       // id1 = map_id, id2 = is_explorable
        InstanceLoadFile,       // id1 = map_fileID, spawn = DoA spawn point
        InstanceTimer,          // map instance timer started
        GameSrvTransfer,        // id1 = map_id, id2 = is_explorable
    };

    Type           type    = {};
    uint32_t       id1     = 0;
    uint32_t       id2     = 0;
    const wchar_t* str     = nullptr;
    size_t         str_len = 0;
    GW::Vec2f      spawn   = {}; // InstanceLoadFile only
};

using GWBusCallback = std::function<void(const GWEvent&)>;

// ---------------------------------------------------------------------------
// GWEventBus — owns all GW packet/UIMessage hooks and broadcasts to subscribers.
// Consolidates the duplicated hook registrations in ObjectiveTimerWindow and
// SplitsWindow into a single source of truth.
// ---------------------------------------------------------------------------
class GWEventBus : public ToolboxModule {
    GWEventBus()  = default;
    ~GWEventBus() override = default;

public:
    static GWEventBus& Instance()
    {
        static GWEventBus instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "GW Event Bus"; }
    bool HasSettings() override { return false; }

    void Initialize() override;
    void SignalTerminate() override;

    // Re-registering the same owner replaces its callback. Do not call from within a callback.
    void Subscribe(const void* owner, GWBusCallback cb);
    void Unsubscribe(const void* owner);

private:
    void Emit(const GWEvent& e) const;

    std::vector<std::pair<const void*, GWBusCallback>> subscribers_;

    GW::HookEntry on_mission_complete_;
    GW::HookEntry on_mission_bonus_;
    GW::HookEntry on_vanquish_complete_;
    GW::HookEntry on_party_defeated_;
    GW::HookEntry on_objective_done_;
    GW::HookEntry on_objective_started_;
    GW::HookEntry on_door_;
    GW::HookEntry on_agent_allegiance_;
    GW::HookEntry on_doa_zone_;
    GW::HookEntry on_dungeon_reward_;
    GW::HookEntry on_server_message_;
    GW::HookEntry on_display_dialogue_;
    GW::HookEntry on_countdown_start_;
    GW::HookEntry on_skill_activate_;
    GW::HookEntry on_party_lock_;
    GW::HookEntry on_instance_load_info_;
    GW::HookEntry on_instance_load_file_;
    GW::HookEntry on_instance_timer_;
    GW::HookEntry on_game_srv_transfer_;
};
