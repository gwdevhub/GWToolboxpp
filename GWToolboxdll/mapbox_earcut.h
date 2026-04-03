#pragma once

#include <mapbox/earcut.hpp>
#include <GWCA/GameContainers/GamePos.h>

namespace mapbox::util {
    template <>
    struct nth<0, GW::GamePos> {
        static auto get(const GW::GamePos& t) { return t.x; }
    };

    template <>
    struct nth<1, GW::GamePos> {
        static auto get(const GW::GamePos& t) { return t.y; }
    };

    template <>
    struct nth<0, GW::Vec2f> {
        static auto get(const GW::Vec2f& t) { return t.x; }
    };

    template <>
    struct nth<1, GW::Vec2f> {
        static auto get(const GW::Vec2f& t) { return t.y; }
    };

    template <>
    struct nth<0, GW::Vec3f> {
        static auto get(const GW::Vec3f& t) { return t.x; }
    };

    template <>
    struct nth<1, GW::Vec3f> {
        static auto get(const GW::Vec3f& t) { return t.y; }
    };
} // namespace mapbox::util
