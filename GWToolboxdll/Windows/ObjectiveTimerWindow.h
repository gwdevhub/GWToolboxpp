#pragma once

#include <GWCA/Constants/Maps.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Packets/StoC.h>

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
    ObjectiveTimerWindow() = default;
    ~ObjectiveTimerWindow() {
        if (run_loader.joinable()) {
            run_loader.join();
        }
        ClearObjectiveSets();
    }

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

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void LoadRuns();
    void SaveRuns();

private:
    std::thread run_loader;
    bool loading = false;

    bool map_load_pending = false;
    GW::Packet::StoC::InstanceLoadInfo* InstanceLoadInfo = 0;
    GW::Packet::StoC::InstanceLoadFile* InstanceLoadFile = 0;
    GW::Packet::StoC::InstanceTimer* InstanceTimer = 0;
    // Checks that we've received all of the packets needed to start an objective set, then triggers necessary events
    void CheckIsMapLoaded();

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

    class ObjectiveSet;

    class Objective
    {
    public:
        char name[126] = "";
        int indent = 0;
        int starting_completes_n_previous_objectives = 0; // use -1 for all

        ObjectiveSet* parent;

        struct Event {
            EventType type;
            uint32_t id1 = 0;
            uint32_t id2 = 0;
        };
        std::vector<Event> start_events;
        std::vector<Event> end_events;
        std::vector<Objective*> children;

        DWORD    start = 0;
        DWORD    done = 0;
        DWORD start_time_point = 0;
        DWORD done_time_point = 0;

        enum class Status {
            NotStarted,
            Started,
            Completed,
            Failed
        } status = Status::NotStarted;
        const char* GetStartTimeStr();
        const char* GetEndTimeStr();
        const char* GetDurationStr();
        DWORD GetDuration();

        Objective(const char* name);

        Objective* AddStartEvent(EventType et, uint32_t id1 = 0, uint32_t id2 = 0);
        Objective* AddStartEvent(EventType et, uint32_t count, const wchar_t* msg);
        Objective* AddEndEvent(EventType et, uint32_t id1 = 0, uint32_t id2 = 0);
        Objective* AddEndEvent(EventType et, uint32_t count, const wchar_t* msg);
        Objective* SetStarted();
        Objective* SetDone();
        Objective* AddChild(Objective* child);
        static Objective* FromJson(const nlohmann::json& json);
        nlohmann::json ToJson();

        bool IsStarted() const;
        bool IsDone() const;
        void Draw();
        void Update();
    private:
        char cached_done[16] = "";
        char cached_start[16] = "";
        char cached_duration[16] = "";
        DWORD    duration = 0;
    };

    class ObjectiveSet {
    public:
        ObjectiveSet();
        ~ObjectiveSet();

        DWORD system_time;
        // Time point that this objective set was created in ms (i.e. run started)
        DWORD run_start_time_point = 0;
        DWORD duration = (DWORD)-1;
        DWORD GetDuration();
        const char* GetDurationStr();
        const char* GetStartTimeStr();

        bool active = true;
        bool failed = false;
        bool from_disk = false;
        bool need_to_collapse = false;
        char name[256] = { 0 };

        std::vector<Objective*> objectives;

        Objective* AddObjective(Objective* obj, int starting_completes_num_previous = 0) {
            obj->starting_completes_n_previous_objectives = starting_completes_num_previous;
            obj->parent = this;
            objectives.push_back(obj);
            return objectives.back();
        }
        Objective* AddObjectiveAfter(Objective* obj) {
            return AddObjective(obj, 1);
        }
        Objective* AddObjectiveAfterAll(Objective* obj) {
            return AddObjective(obj, -1);
        }
        void AddQuestObjective(const char* obj_name, uint32_t id)
        {
            AddObjective(new Objective(obj_name))
                ->AddStartEvent(EventType::ObjectiveStarted, id)
                ->AddEndEvent(EventType::ObjectiveDone, id);
        }

        void Event(EventType type, uint32_t id1, uint32_t id2);
        void CheckSetDone();
        bool Draw(); // returns false when should be deleted
        void StopObjectives();
        static ObjectiveSet* FromJson(const nlohmann::json& json);
        nlohmann::json ToJson();
        void Update();
        void GetStartTime(struct tm* timeinfo);

        const unsigned int ui_id = 0; // an internal id to ensure interface consistency

    private:
        static unsigned int cur_ui_id;
        char cached_start[16] = { 0 };
        char cached_time[16] = { 0 };

    };

    std::map<DWORD, ObjectiveSet*> objective_sets;

    ObjectiveSet* GetCurrentObjectiveSet();
    bool show_current_run_window = false;
    bool clear_cached_times = false;
    bool auto_send_age = false;
    bool show_detailed_objectives = true; // currently only for doa
    bool show_debug_events = false;
    ObjectiveSet* current_objective_set = nullptr;

    void Event(EventType type, uint32_t id1 = 0, uint32_t id2 = 0);
    void Event(EventType type, uint32_t count, const wchar_t* msg);

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
