#pragma once
#include <GWCA/Runtime.h>
#include "../Utilities/Export.h"

namespace GW {
    typedef struct Mat4x3f {
        float _11;
        float _12;
        float _13;
        float _14;
        float _21;
        float _22;
        float _23;
        float _24;
        float _31;
        float _32;
        float _33;
        float _34;
        uint32_t flags;
    } Mat4x3f;

    struct Vec2f {
        float x = 0.f;
        float y = 0.f;

        constexpr Vec2f(float _x, float _y)
            : x(_x), y(_y)
        {
        }

        constexpr Vec2f(int _x, int _y)
        {
            x = static_cast<float>(_x);
            y = static_cast<float>(_y);
        }

        Vec2f() = default;
    };

    struct Vec3f {
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;

        constexpr Vec3f(float _x, float _y, float _z = 0.f)
            : x(_x), y(_y), z(_z)
        {
        }

        constexpr Vec3f(GW::Vec2f v, float _z = 0.f)
            : x(v.x), y(v.y), z(_z)
        {
        }

        constexpr Vec3f(int _x, int _y, int _z)
        {
            x = static_cast<float>(_x);
            y = static_cast<float>(_y);
            z = static_cast<float>(_z);
        }

        Vec3f() = default;
    };
    static_assert(sizeof(Vec2f) == 8);
    static_assert(sizeof(Vec3f) == 12);

    constexpr Vec3f& operator+=(Vec3f& lhs, const Vec3f& rhs) {
        lhs.x += rhs.x;
        lhs.y += rhs.y;
        lhs.z += rhs.z;
        return lhs;
    }

    constexpr Vec3f operator+(Vec3f lhs, const Vec3f& rhs) {
        lhs += rhs;
        return lhs;
    }

    constexpr Vec3f& operator-=(Vec3f& lhs, const Vec3f& rhs) {
        lhs.x -= rhs.x;
        lhs.y -= rhs.y;
        lhs.z -= rhs.z;
        return lhs;
    }

    constexpr Vec3f operator-(Vec3f lhs, const Vec3f& rhs) {
        lhs -= rhs;
        return lhs;
    }

    constexpr Vec3f operator-(const Vec3f& v) {
        return {-v.x, -v.y, -v.z};
    }

    constexpr Vec3f& operator*=(Vec3f& lhs, float rhs) {
        lhs.x *= rhs;
        lhs.y *= rhs;
        lhs.z *= rhs;
        return lhs;
    }

    constexpr Vec3f operator*(Vec3f lhs, float rhs) {
        lhs *= rhs;
        return lhs;
    }

    constexpr Vec3f operator*(float lhs, Vec3f rhs) {
        rhs *= lhs;
        return rhs;
    }

    constexpr Vec3f& operator/=(Vec3f& lhs, float rhs) {
        lhs.x /= rhs;
        lhs.y /= rhs;
        lhs.z /= rhs;
        return lhs;
    }

    constexpr Vec3f operator/(Vec3f lhs, float rhs) {
        lhs /= rhs;
        return lhs;
    }

    constexpr Vec3f operator/(float lhs, Vec3f rhs) {
        rhs *= lhs;
        return rhs;
    }

    constexpr bool operator==(const Vec3f& lhs, const Vec3f& rhs) {
        return (lhs.x == rhs.x) && (lhs.y == rhs.y) && (lhs.z == rhs.z);
    }

    constexpr Vec2f& operator+=(Vec2f& lhs, Vec2f rhs)
    {
        lhs.x += rhs.x;
        lhs.y += rhs.y;
        return lhs;
    }

    constexpr Vec2f operator+(Vec2f lhs, Vec2f rhs) {
        lhs += rhs;
        return lhs;
    }

    constexpr Vec2f& operator-=(Vec2f& lhs, Vec2f rhs)
    {
        lhs.x -= rhs.x;
        lhs.y -= rhs.y;
        return lhs;
    }

    constexpr Vec2f operator-(Vec2f lhs, Vec2f rhs) {
        lhs -= rhs;
        return lhs;
    }

    constexpr Vec2f operator-(Vec2f v) {
        return {-v.x, -v.y};
    }

    constexpr Vec2f& operator*=(Vec2f& lhs, float rhs) {
        lhs.x *= rhs;
        lhs.y *= rhs;
        return lhs;
    }

    constexpr Vec2f operator*(Vec2f lhs, float rhs) {
        lhs *= rhs;
        return lhs;
    }

    constexpr Vec2f operator*(float lhs, Vec2f rhs) {
        rhs *= lhs;
        return rhs;
    }

    constexpr Vec2f& operator/=(Vec2f& lhs, float rhs) {
        lhs.x /= rhs;
        lhs.y /= rhs;
        return lhs;
    }

    constexpr Vec2f operator/(Vec2f lhs, float rhs) {
        lhs /= rhs;
        return lhs;
    }

    constexpr Vec2f operator/(float lhs, Vec2f rhs) {
        rhs *= lhs;
        return rhs;
    }

    constexpr bool operator==(Vec2f lhs, Vec2f rhs) {
        return (lhs.x == rhs.x) && (lhs.y == rhs.y);
    }

    constexpr float GetSquareDistance(const Vec3f& p1, const Vec3f& p2) {
        float dx = p1.x - p2.x;
        float dy = p1.y - p2.y;
        float dz = p1.z - p2.z;
        return dx * dx + dy * dy + dz * dz;
    }

    constexpr float GetSquareDistance(const Vec2f& p1, const Vec2f& p2) {
        float dx = p1.x - p2.x;
        float dy = p1.y - p2.y;
        return dx * dx + dy * dy;
    }

    float GetDistance3D(Vec3f p1, Vec3f p2);
    float GetDistance2D(const Vec2f& p1, const Vec2f& p2);
    inline float GetDistance(Vec3f p1, Vec3f p2) { return GetDistance3D(p1, p2); }
    inline float GetDistance(const Vec2f& p1, const Vec2f& p2) { return GetDistance2D(p1, p2); }

    constexpr float GetSquaredNorm(const Vec3f& p) {
        return (p.x * p.x) + (p.y * p.y) + (p.z * p.z);
    }

    constexpr float GetSquaredNorm(const Vec2f& p) {
        return (p.x * p.x) + (p.y * p.y);
    }

    float GetNorm3D(Vec3f p);
    float GetNorm2D(Vec2f p);
    inline float GetNorm(Vec3f p) { return GetNorm3D(p); }
    inline float GetNorm(Vec2f p) { return GetNorm2D(p); }

    inline Vec3f Normalize(Vec3f v) {
        float n = GetNorm(v);
        v.x /= n;
        v.y /= n;
        v.z /= n;
        return v;
    }

    inline Vec2f Normalize(Vec2f v) {
        float n = GetNorm(v);
        v.x /= n;
        v.y /= n;
        return v;
    }

    constexpr Vec2f Rotate(Vec2f v, float cos, float sin) {
        Vec2f res;
        res.x = (v.x * cos) - (v.y * sin);
        res.y = (v.x * sin) + (v.y * cos);
        return res;
    }

    void RotateNative(Vec2f value, float rotation, Vec2f* result);
    inline Vec2f Rotate(Vec2f value, float rotation)
    {
        Vec2f result;
        RotateNative(value, rotation, &result);
        return result;
    }

    struct GamePos {
        float    x;
        float    y;
        uint32_t zplane;

        GamePos(float _x, float _y, uint32_t _zplane = 0)
            : x(_x), y(_y), zplane(_zplane)
        {
        }

        GamePos() : GamePos(0.f, 0.f, 0)
        {
        }

        GamePos(Vec2f v)
            : GamePos(v.x, v.y, 0)
        {
        }

        operator Vec2f() const {
            return Vec2f(x, y);
        }
    };

    constexpr bool operator==(const GamePos& lhs, const GamePos& rhs) {
        return (lhs.x == rhs.x) && (lhs.y == rhs.y) && (lhs.zplane == rhs.zplane);
    }
}

namespace GW {
    extern "C" GWCA_API float __cdecl GW_GetDistance3D(Vec3f p1, Vec3f p2) noexcept;
    extern "C" GWCA_API float __cdecl GW_GetDistance2D(const Vec2f& p1, const Vec2f& p2) noexcept;
    extern "C" GWCA_API float __cdecl GW_GetNorm3D(Vec3f p) noexcept;
    extern "C" GWCA_API float __cdecl GW_GetNorm2D(Vec2f p) noexcept;
    extern "C" GWCA_API void __cdecl GW_RotateNative(Vec2f value, float rotation, Vec2f* result) noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW {
    inline float GetDistance3D(Vec3f p1, Vec3f p2)
    {
        return GWCA_CALL_C(GW_GetDistance3D, p1, p2);
    }

    inline float GetDistance2D(const Vec2f& p1, const Vec2f& p2)
    {
        return GWCA_CALL_C(GW_GetDistance2D, p1, p2);
    }

    inline float GetNorm3D(Vec3f p)
    {
        return GWCA_CALL_C(GW_GetNorm3D, p);
    }

    inline float GetNorm2D(Vec2f p)
    {
        return GWCA_CALL_C(GW_GetNorm2D, p);
    }

    inline void RotateNative(Vec2f value, float rotation, Vec2f* result)
    {
        return GWCA_CALL_C(GW_RotateNative, value, rotation, result);
    }
}
#endif
