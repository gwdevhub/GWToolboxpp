#pragma once

#include <cstdio>

inline int g_test_passed = 0;
inline int g_test_failed = 0;

inline void Expect(bool condition, const char* name)
{
    if (condition) {
        ++g_test_passed;
        std::printf("PASS %s\n", name);
    }
    else {
        ++g_test_failed;
        std::printf("FAIL %s\n", name);
    }
}

void RunBatch2BStoreTests();
