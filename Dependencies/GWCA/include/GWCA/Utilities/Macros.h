#pragma once

#include <cstddef>

#define Verify(expr) (expr)

// Game layouts require compiler-supported offsets even for records containing inherited members.
#if defined(__GNUC__) || defined(__clang__)
#define GWCA_OFFSETOF(type, member) \
    ([] { \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Winvalid-offsetof\"") \
        constexpr auto offset = offsetof(type, member); \
        _Pragma("GCC diagnostic pop") \
        return offset; \
    }())
#else
#define GWCA_OFFSETOF(type, member) offsetof(type, member)
#endif
