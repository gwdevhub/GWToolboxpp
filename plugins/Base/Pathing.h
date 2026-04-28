#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <Windows.h>

#include <GWCA/GameEntities/Pathing.h>
#include <GWCA/GameEntities/Agent.h>

struct PathPoint {
    GW::GamePos pos = {};
    const GW::PathingTrapezoid* t = nullptr;
};
struct OutcomeChances {
    float success = 0.f;
    float partial = 0.f;
    float failure = 0.f;
};

const GW::PathingTrapezoid* findTrapezoid(const GW::GamePos& pos, const GW::PathingMapArray* path_map);
OutcomeChances dcPrediction(const PathPoint& playerPathPoint, const GW::Agent* target, const GW::PathingMapArray* path_map);
OutcomeChances sohPrediction(const PathPoint& playerPathPoint, const PathPoint& sohSpot);
void InitializePathing();
