#pragma once
#include <GWCA/Runtime.h>

#include <cstdint>

#include <GWCA/Utilities/Export.h>

// Guards against headers and binary being from different releases -- struct offsets shift and nothing else catches it.
#define GWCA_ABI_VERSION 0x040C0000u

namespace GW {
    uint32_t GetAbiVersion();
    // Call before anything else: a mismatch means every struct offset is suspect.
    inline bool AbiVersionMatches()
    {
        return GetAbiVersion() == GWCA_ABI_VERSION;
    }
}

namespace GW {
    extern "C" GWCA_API uint32_t __cdecl GW_GetAbiVersion() noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW {
    inline uint32_t GetAbiVersion()
    {
        return GWCA_CALL_C(GW_GetAbiVersion);
    }
}
#endif
