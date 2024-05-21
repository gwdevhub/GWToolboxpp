#include "DailyQuests.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/Managers/MapMgr.h>
#include <Utils/GuiUtils.h>

NicholasCycleData::NicholasCycleData(const wchar_t* enc_name, int quantity, MapID map_id) : quantity(quantity), map_id(map_id) {
    name_english = new GuiUtils::EncString(enc_name);
    name_english->language(GW::Constants::Language::English);
    name_translated = new GuiUtils::EncString(enc_name);
    _map_name = new GuiUtils::EncString();
}

NicholasCycleData::~NicholasCycleData() {
    delete name_english;
    delete name_translated;
    delete _map_name;
}

// Lazy initialisation: if toolbox is injected at gw startup (e.g. by gwlauncher), then
// the constructor is called before GWCA initialises, so GetMapInfo is unavailable.
// As a workaround, lazy-initialize map_name.
GuiUtils::EncString* NicholasCycleData::map_name() {
    if (_map_name->encoded().empty()) {
        if (GW::AreaInfo* areaInfo = GW::Map::GetMapInfo(map_id)) {
            _map_name->reset(areaInfo->name_id);
        }
    }

    return _map_name;
}
