#include "PetModule.h"

#include "../stdafx.h"
#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <ToolboxModule.h>
#include "Defines.h"

namespace {
    bool display_window = false;
    GW::Constants::MapID map_id = GW::Constants::MapID::None;
    GW::Constants::MapID previous_map_id = GW::Constants::MapID::None;
    GW::Constants::InstanceType instance_type = GW::Constants::InstanceType::Loading;
    GW::Constants::InstanceType previous_instance_type = GW::Constants::InstanceType::Loading;
}

void PetModule::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);
    LOAD_BOOL(keep_pet_window_open);
}

void PetModule::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    SAVE_BOOL(keep_pet_window_open);
}

void PetModule::DrawSettingsInternal()
{
    ToolboxModule::DrawSettingsInternal();
    ImGui::Checkbox("Keep Pet Command Window Open", &keep_pet_window_open);
}

void PetModule::Update(const float)
{
    if (!keep_pet_window_open || GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) {
        return;
    }

    if (GW::Map::GetInstanceType() != instance_type || GW::Map::GetMapID() != map_id) {
        previous_instance_type = instance_type;
        previous_map_id = map_id;

        instance_type = GW::Map::GetInstanceType();
        map_id = GW::Map::GetMapID();

        display_window = instance_type == GW::Constants::InstanceType::Explorable;
    }

    if (display_window) {
        Keypress(GW::UI::ControlAction_OpenPetCommander);
        display_window = !display_window;
    }
}

