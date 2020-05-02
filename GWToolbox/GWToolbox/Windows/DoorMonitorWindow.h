#pragma once

#include <Windows.h>
#include <vector>
#include <Defines.h>
#include <time.h>
#include <ctime>

#include <GWCA/Utilities/Hook.h>

#include "ToolboxWindow.h"

class DoorMonitorWindow : public ToolboxWindow {
    DoorMonitorWindow() {};
    ~DoorMonitorWindow() {};
public:
    class DoorObject {
    public:
        DoorObject(uint32_t object_id_) : object_id(object_id_) {};



        uint32_t object_id = 0; // object_id
        uint32_t initial_state = 0;
        time_t first_load = 0; // When did this door first get sent to the client?
        time_t first_open = 0; // First time the door was opened
        time_t last_open = 0; // Most recent time the door was opened
        time_t first_close = 0; // First time the door was closed
        time_t last_close = 0; // Most recent time the door was closed
    public:

        static void DoorAnimation(uint32_t object_id, uint32_t animation_type, uint32_t animation_stage) {
            if (animation_type != 16 && animation_type != 3)
                return; // Not opening or closing door.
            DoorObject* d = GetDoor(object_id);
            time_t now = time(nullptr);
            if (!d->initial_state) {
                // First load of this door.
                d->initial_state = animation_stage;
                d->first_load = now;
            }
            //if (animation_stage == 2) {
                // Door animating.
                if (animation_type == 16) {
                    // Opening
                    if (!d->first_open)
                        d->first_open = now;
                    else
                        d->last_open = now;
                }
                else if (animation_type == 3) {
                    // Closing
                    if (!d->first_close)
                        d->first_close = now;
                    else
                        d->last_close = now;
                }
           // }
        };
    };

    static DoorMonitorWindow& Instance() {
        static DoorMonitorWindow instance;
        return instance;
    }
    static DoorObject* GetDoor(uint32_t object_id) {
        std::map<uint32_t, DoorObject*>::iterator it = Instance().doors.find(object_id);
        if (it != Instance().doors.end())
            return it->second;
        DoorObject* d = new DoorObject(object_id);
        Instance().doors.emplace(object_id, d);
        return d;
    };
    const char* Name() const override { return "Door Monitor"; }

    void Initialize() override;

    //void Update(float delta) override;
    void Draw(IDirect3DDevice9* pDevice) override;
    //void DrawSettingInternal() override;

    //void LoadSettings(CSimpleIni* ini) override;
    //void SaveSettings(CSimpleIni* ini) override;

private:
    bool in_zone = false;

	GW::HookEntry InstanceLoadInfo_Callback;
	GW::HookEntry ManipulateMapObject_Callback;
 
public:
    std::map<uint32_t, DoorObject*> doors;
};