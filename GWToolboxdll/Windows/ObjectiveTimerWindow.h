#pragma once

#include <GWCA/Constants/Maps.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Packets/StoC.h>

#include <Defines.h>
#include <ToolboxWindow.h>

/*
each objective can have a duration (start and end) or a single timestamp
- e.g. quests in fow vs doors in urgoz
  (when start of an item is the same as end of previous, we don't need to keep both)

each objective can have sub-objectives. (?)

each list of objectives can be either sequential or independent
*/

class ObjectiveTimerWindow : public ToolboxWindow {
    ObjectiveTimerWindow() {};
    ~ObjectiveTimerWindow();
public:
    static ObjectiveTimerWindow& Instance() {
        static ObjectiveTimerWindow instance;
        return instance;
    }

    const char* Name() const override { return "Objectives"; }
    const char* Icon() const override { return ICON_FA_BULLSEYE; }

    void Initialize() override;

    void Update(float delta) override;
    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawSettingInternal() override;

    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void LoadRuns();
    void SaveRuns();

private:
    std::thread run_loader;
    bool loading = false;

    // TODO: many of those are not hooked up
    enum class EventType {
        ServerMessage, 
        Dialog, 
        InstanceLoadFile, // id is mapfile
        InstanceLoadInfo, // id is mapid
        GameSrvTransfer, // start of map load. Use as map finished.
        DoorOpen, 
        DoorClose,
        ObjectiveStarted,
        ObjectiveDone,
        AgentTurnsFriendly,
        AgentTurnsHostile,
        DoACompleteZone,
        CountdownStart,
        DungeonReward
    };

    class Objective
    {
    public:
        char name[126] = "";
        int indent = 0; // TODO: take in constructor after we remove ID
        bool starting_completes_previous_objectives = false;

        uint32_t id = 0; // TODO: remove

        struct Event {
            EventType type;
            uint32_t id;
        };
        std::vector<Event> start_events;
        std::vector<Event> end_events;

        DWORD    start = 0;
        DWORD    done = 0;
        DWORD    duration = 0;
        char cached_done[16] = "";
        char cached_start[16] = "";
        char cached_duration[16] = "";
        enum class Status {
            NotStarted,
            Started,
            Completed,
            Failed
        } status = Status::NotStarted;
       
        Objective(const char* name, uint32_t id = 0);

        Objective& AddStartEvent(EventType et, uint32_t _id);
        Objective& AddEndEvent(EventType et, uint32_t _id);
        Objective& SetStarted();
        Objective& SetDone();

        bool IsStarted() const;
        bool IsDone() const;
        void Draw();
        void Update();
    };

    class ObjectiveSet {
    public:
        ObjectiveSet();

        DWORD system_time;
        DWORD instance_time = (DWORD)-1;
        char cached_time[16] = { 0 };
        char cached_start[16] = { 0 };

        bool active = true;
        bool failed = false;
        bool from_disk = false;
        bool need_to_collapse = false;
        char name[256] = { 0 };

        std::vector<Objective> objectives;

        Objective& AddObjective(Objective&& obj) { 
            objectives.push_back(std::move(obj)); 
            return objectives.back();
        }
        Objective& AddObjectiveAfter(Objective&& obj) {
            obj.starting_completes_previous_objectives = true;
            return AddObjective(std::move(obj));
        }

        void Event(EventType type, uint32_t id);
        void CheckSetDone();
        bool Draw(); // returns false when should be deleted
        void StopObjectives();
        static ObjectiveSet* FromJson(nlohmann::json* json);
        nlohmann::json ToJson();
        void Update();
        void GetStartTime(struct tm* timeinfo);
        
        const unsigned int ui_id = 0; // an internal id to ensure interface consistency

    private:
        static unsigned int cur_ui_id;
    };

    std::map<DWORD, ObjectiveSet*> objective_sets;

    Objective* GetCurrentObjective(uint32_t obj_id);
    ObjectiveSet* GetCurrentObjectiveSet();
    bool show_current_run_window = false;
    bool clear_cached_times = false;
    bool auto_send_age = false;
    ObjectiveSet* current_objective_set = nullptr;

    void AddObjectiveSet(ObjectiveSet* os);
    void AddDoAObjectiveSet(GW::Vec2f spawn);
    void AddFoWObjectiveSet();
    void AddUWObjectiveSet();
    void AddSlaversObjectiveSet();
    void AddDungeonObjectiveSet(int levels);
    void DoorOpened(uint32_t door_id);
    
    void AddDeepObjectiveSet();
    void AddUrgozObjectiveSet();
    void AddToPKObjectiveSet();
    void ClearObjectiveSets();

    static void OnAgentUpdateAllegiance(GW::HookStatus *, GW::Packet::StoC::AgentUpdateAllegiance *packet);
    static void OnDisplayDialogue(GW::HookStatus *,GW::Packet::StoC::DisplayDialogue *packet);
    static void OnMessageServer(GW::HookStatus *, GW::Packet::StoC::MessageServer *);
    static void OnCountdownStart(GW::HookStatus *, GW::Packet::StoC::PacketBase *);
    // Called via PartyDefeated packet, but can also be called without args to kill any running objectives.
    static void StopObjectives(GW::HookStatus *status = nullptr, GW::Packet::StoC::PacketBase *packet = nullptr);
    static void OnMapChanged(GW::HookStatus*, GW::Packet::StoC::InstanceLoadInfo* packet);
};
