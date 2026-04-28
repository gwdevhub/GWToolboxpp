#pragma once

#include <cstdint>
#include <optional>
#include <vector>
#include <GWCA/GameContainers/GamePos.h>

struct IDirect3DDevice9;

namespace RenderingUtils {
    void clearDrawingList();
    void addCircleToDraw(GW::GamePos center, float radius, unsigned int color, bool filled, std::optional<int> msToShow = std::nullopt);
    void addEllipseToDraw(GW::GamePos center, GW::Vec2f ADirection, float a, float b, unsigned int color, bool filled, std::optional<int> msToShow = std::nullopt);
    void addSingletonPolyline(std::vector<GW::GamePos>&& polyline, unsigned int color);
    void clearSingletonPolyline();
    void draw(IDirect3DDevice9* device);
} // namespace RenderingUtils
