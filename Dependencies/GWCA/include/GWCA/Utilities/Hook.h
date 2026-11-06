#pragma once

#include <cstddef>
#include <cstdint>

namespace GW
{
    struct HookEntry {};

    struct HookStatus {
        bool blocked = false;
        uint32_t altitude = 0;
    };
    static_assert(sizeof(HookStatus) == 8);
    static_assert(offsetof(HookStatus, altitude) == 4);

} // namespace GW
