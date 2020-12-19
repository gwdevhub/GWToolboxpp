#pragma once

#include <GWCA/Constants/Maps.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Packets/StoC.h>

#include <Defines.h>
#include <ToolboxWindow.h>
#include <vector>

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
        ServerMessage,      // id1=msg length, id2=pointer to msg

        // dialog from owner. Can be in chat or middle of screen.
        DisplayDialogue,    // id1=msg length, id2=pointer to msg

        InstanceLoadInfo,   // id1 is mapid
        InstanceEnd,        // id1 is mapid we're leaving. Use as "end of instance".
        DoorOpen,           // id=object_id
        DoorClose,          // id=object_id
        ObjectiveStarted,   // id=objective_id
        ObjectiveDone,      // id=objective_id

        AgentUpdateAllegiance, // id1 = agent model id, id2 = allegiance_bits
        DoACompleteZone,    // id1 = second wchar of message (doa "id")
        CountdownStart,     // id1 = mapid
        DungeonReward
    };

    class Objective
    {
    public:
        char name[126] = "";
        int indent = 0;
        bool starting_completes_previous_objective = false;
        bool starting_completes_all_previous = false;

        struct Event {
            EventType type;
            uint32_t id1 = 0;
            uint32_t id2 = 0;
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
       
        Objective(const char* name, int indent = 0);

        Objective& AddStartEvent(EventType et, uint32_t id1 = 0, uint32_t id2 = 0);
        Objective& AddEndEvent(EventType et, uint32_t id1 = 0, uint32_t id2 = 0);
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
            obj.starting_completes_previous_objective = true;
            return AddObjective(std::move(obj));
        }
        Objective& AddObjectiveAfterAll(Objective&& obj) {
            obj.starting_completes_all_previous = true;
            return AddObjective(std::move(obj));
        }
        void AddQuestObjective(const char* obj_name, uint32_t id)
        {
            AddObjective(Objective(obj_name))
                .AddStartEvent(EventType::ObjectiveStarted, id)
                .AddEndEvent(EventType::ObjectiveDone, id);
        }

        void Event(EventType type, uint32_t id1, uint32_t id2);
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

    ObjectiveSet* GetCurrentObjectiveSet();
    bool show_current_run_window = false;
    bool clear_cached_times = false;
    bool auto_send_age = false;
    bool show_detailed_objectives = true; // currently only for doa
    ObjectiveSet* current_objective_set = nullptr;

    void Event(EventType type, uint32_t id1 = 0, uint32_t id2 = 0);

    void AddObjectiveSet(ObjectiveSet* os);
    void AddObjectiveSet(GW::Constants::MapID map_id);
    void AddDoAObjectiveSet(GW::Vec2f spawn);
    void AddFoWObjectiveSet();
    void AddUWObjectiveSet();
    void AddDeepObjectiveSet();
    void AddUrgozObjectiveSet();
    void AddToPKObjectiveSet();
    void AddDungeonObjectiveSet(const std::vector<GW::Constants::MapID>& levels);

    void ClearObjectiveSets();
    void StopObjectives(); // called on partydefeated or back to outpost
};
