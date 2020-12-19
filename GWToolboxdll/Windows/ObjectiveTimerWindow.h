#pragma once

#include <GWCA/Constants/Maps.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Packets/StoC.h>

#include <Defines.h>
#include <ToolboxWindow.h>

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
    class Objective {
    public:
        uint32_t id = 0;
        char     name[126] = "";
        DWORD    start = 0;
        DWORD    done = 0;
        DWORD    duration = 0;
        char cached_done[16] = "";
        char cached_start[16] = "";
        char cached_duration[16] = "";
        enum Status {
            NotStarted,
            Started,
            Completed,
            Failed
        } status = NotStarted;
       
        Objective(uint32_t id, const char* name = "");

        bool IsStarted() const;
        bool IsDone() const;
        void SetStarted();
        void SetDone();
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

        void CheckSetDone();
        bool Draw(); // returns false when should be deleted
        void StopObjectives();
        static ObjectiveSet* FromJson(nlohmann::json* json);
        nlohmann::json ToJson();
        void Update();
        void GetStartTime(struct tm* timeinfo);
        // todo: print to file
        // an internal id to ensure interface consistency
        const unsigned int ui_id = 0;

        bool single_instance = true;
    private:

        static unsigned int cur_ui_id;
    };

    std::map<DWORD, ObjectiveSet *> objective_sets;

    Objective* GetCurrentObjective(uint32_t obj_id);
    ObjectiveSet* GetCurrentObjectiveSet();
    bool monitor_doors = false;
    bool show_current_run_window = false;
    bool clear_cached_times = false;
    bool auto_send_age = false;
    ObjectiveSet* current_objective_set = nullptr;

    void AddDoAObjectiveSet(GW::Vec2f spawn);
    void AddFoWObjectiveSet();
    void AddUWObjectiveSet();
    void AddSlaversObjectiveSet();
    void AddDungeonObjectiveSet(int levels);
    void AddObjectiveSet(ObjectiveSet* os);
    void DoorOpened(uint32_t door_id);
    
    void AddDeepObjectiveSet();
    void AddUrgozObjectiveSet();
    void AddToPKObjectiveSet();
    void ClearObjectiveSets();

    static void OnAgentUpdateAllegiance(GW::HookStatus *, GW::Packet::StoC::AgentUpdateAllegiance *packet);
    static void OnObjectiveDone(GW::HookStatus *, GW::Packet::StoC::ObjectiveDone *packet);
    static void OnUpdateObjectiveName(GW::HookStatus *, GW::Packet::StoC::ObjectiveUpdateName *packet);
    static void OnManipulateMapObject(GW::HookStatus *, GW::Packet::StoC::ManipulateMapObject *packet);
    static void OnDisplayDialogue(GW::HookStatus *,GW::Packet::StoC::DisplayDialogue *packet);
    static void OnMessageServer(GW::HookStatus *, GW::Packet::StoC::MessageServer *);
    static void OnDoACompleteZone(GW::HookStatus *,GW::Packet::StoC::DoACompleteZone *packet);
    static void OnCountdownStart(GW::HookStatus *, GW::Packet::StoC::PacketBase *);
    static void OnDungeonReward(GW::HookStatus *, GW::Packet::StoC::DungeonReward *);
    // Called via PartyDefeated packet, but can also be called without args to kill any running objectives.
    static void StopObjectives(GW::HookStatus *status = nullptr, GW::Packet::StoC::PacketBase *packet = nullptr);
    // Called when leaving, loading and spawning in a map; Packet types handled within
    static void OnMapChanged(GW::HookStatus *, GW::Packet::StoC::PacketBase *packet);

    GW::HookEntry PartyDefeated_Entry;
    GW::HookEntry GameSrvTransfer_Entry;
    GW::HookEntry InstanceLoadFile_Entry;
    GW::HookEntry ObjectiveAdd_Entry;
    GW::HookEntry ObjectiveUpdateName_Entry;
    GW::HookEntry ObjectiveDone_Entry;
    GW::HookEntry AgentUpdateAllegiance_Entry;
    GW::HookEntry DoACompleteZone_Entry;
    GW::HookEntry DisplayDialogue_Entry;
    GW::HookEntry MessageServer_Entry;
    GW::HookEntry InstanceLoadInfo_Entry;
    GW::HookEntry ManipulateMapObject_Entry;
    GW::HookEntry DungeonReward_Entry;
    GW::HookEntry CountdownStart_Enty;
};
