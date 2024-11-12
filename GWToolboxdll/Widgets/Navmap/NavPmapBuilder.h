#pragma once

#include <GWCA/GameEntities/Pathing.h>
#include "Navigation/Navigation.h"

class NavPMapBuilder
{
public:
    bool Build(GW::PathingMapArray * path_map);

    NavmeshBuildSettings nbs;
    bool show_success_message = true;

private:
    std::vector<float> _vertices;
    std::vector<int> _indices;

public:
    Navmesh * nm = nullptr;
};
