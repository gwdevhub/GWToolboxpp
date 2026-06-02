#pragma once

#include <string>
#include <vector>
#include <utility>
#include <initializer_list>

#include <GWCA/Constants/Maps.h>
#include <GWCA/GameEntities/Map.h>

// ---------------------------------------------------------------------------
// MapNames — async-decoded cache of GW map names.
// ---------------------------------------------------------------------------
namespace MapNames {

    void Init();
    const std::string& Get(GW::Constants::MapID id);

    std::vector<std::pair<int, std::string>> Filter(
        const char* query,
        std::initializer_list<GW::RegionType> types = {});

} // namespace MapNames
