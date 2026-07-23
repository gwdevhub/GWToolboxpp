#pragma once

#include <string>

#include <GWCA/Constants/Maps.h>

// ---------------------------------------------------------------------------
// MapNames — async-decoded cache of GW map names.
// ---------------------------------------------------------------------------
namespace MapNames {

    void Init();
    const std::string& Get(GW::Constants::MapID id);

} // namespace MapNames
