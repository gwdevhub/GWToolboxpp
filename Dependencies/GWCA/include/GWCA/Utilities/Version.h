#pragma once

#include <cstdint>

#include <GWCA/Utilities/Export.h>

// Guards against headers and binary being from different releases -- struct offsets shift and nothing else catches it.
#define GWCA_ABI_VERSION 0x04080500u

extern "C" {
    // The version the binary was built at, against GWCA_ABI_VERSION which is what the caller compiled against.
    GWCA_API uint32_t gwca_abi_version(void);
}

namespace GW {
    // Call before anything else: a mismatch means every struct offset is suspect.
    inline bool AbiVersionMatches()
    {
        return gwca_abi_version() == GWCA_ABI_VERSION;
    }
}
