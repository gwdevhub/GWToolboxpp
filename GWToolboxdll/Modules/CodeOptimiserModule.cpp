#include "stdafx.h"

#include "CodeOptimiserModule.h"
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

namespace {
    // Benchmarks:
    // 100MB -> GW: 240 ms, this: 45 ms
    // 1MB -> GW: <3 ms, this: <1 ms

    typedef uint32_t(__cdecl* ComputeCRC32_pt)(uint32_t crc_init, const void* data, uint32_t bytes);
    ComputeCRC32_pt ComputeCRC32_func = nullptr;

    //https://github.com/komrad36/CRC
    static constexpr uint32_t P = 0xEDB88320U;

    static uint32_t g_tbl[256 * 16];

    void compute_tabular_method_tables(uint32_t* pTbl, uint32_t kNumTables)
    {
        uint32_t i = 0;

        // initialize tbl0, the first 256 elements of pTbl,
        // using naive CRC to compute pTbl[i] = CRC(i)
        for (; i < 256; ++i)
        {
            uint32_t R = i;
            for (int j = 0; j < 8; ++j)
            {
                R = R & 1 ? (R >> 1) ^ P : R >> 1;
            }
            pTbl[i] = R;
        }

        // initialize remaining tables by taking the previous
        // table's entry for i and churning through 8 more zero bits
        for (; i < kNumTables * 256; ++i)
        {
            const uint32_t R = pTbl[i - 256];
            pTbl[i] = (R >> 8) ^ pTbl[uint8_t(R)];
        }
    }

    uint32_t ComputeCRC32(uint32_t crc_init, const void* data, uint32_t bytes) {
        GW::Hook::EnterHook();
        uint32_t R = ~crc_init;

        const uint8_t* M8 = (const uint8_t*)data;

        while (((uintptr_t)M8 & 0x3) && bytes) {
            R = (R >> 8) ^ g_tbl[(R ^ *M8++) & 0xFF];
            bytes--;
        }

        const uint32_t* M32 = (const uint32_t*)M8;
        while (bytes >= 16) {
            R ^= *M32++;
            const uint32_t R2 = *M32++;
            const uint32_t R3 = *M32++;
            const uint32_t R4 = *M32++;
            R = g_tbl[0 * 256 + uint8_t(R4 >> 24)] ^
                g_tbl[1 * 256 + uint8_t(R4 >> 16)] ^
                g_tbl[2 * 256 + uint8_t(R4 >> 8)] ^
                g_tbl[3 * 256 + uint8_t(R4 >> 0)] ^
                g_tbl[4 * 256 + uint8_t(R3 >> 24)] ^
                g_tbl[5 * 256 + uint8_t(R3 >> 16)] ^
                g_tbl[6 * 256 + uint8_t(R3 >> 8)] ^
                g_tbl[7 * 256 + uint8_t(R3 >> 0)] ^
                g_tbl[8 * 256 + uint8_t(R2 >> 24)] ^
                g_tbl[9 * 256 + uint8_t(R2 >> 16)] ^
                g_tbl[10 * 256 + uint8_t(R2 >> 8)] ^
                g_tbl[11 * 256 + uint8_t(R2 >> 0)] ^
                g_tbl[12 * 256 + uint8_t(R >> 24)] ^
                g_tbl[13 * 256 + uint8_t(R >> 16)] ^
                g_tbl[14 * 256 + uint8_t(R >> 8)] ^
                g_tbl[15 * 256 + uint8_t(R >> 0)];
            bytes -= 16;
        }

        M8 = (const uint8_t*)M32;
        while (bytes--) {
            R = (R >> 8) ^ g_tbl[(R ^ *M8++) & 0xFF];
        }
        GW::Hook::LeaveHook();
        return ~R;
    }

}

void CodeOptimiserModule::Initialize() {
    ComputeCRC32_func = (ComputeCRC32_pt)GW::Scanner::Find("\xf7\xd6\x85", "xxx", -0xF);
    if (ComputeCRC32_func) {
        compute_tabular_method_tables(g_tbl, 16);
        GW::Hook::CreateHook((void**)&ComputeCRC32_func, ComputeCRC32, nullptr);
        GW::Hook::EnableHooks(ComputeCRC32_func);
    }
#ifdef _DEBUG
    ASSERT(ComputeCRC32_func);
#endif
}

void CodeOptimiserModule::SignalTerminate() {
    GW::Hook::RemoveHook(ComputeCRC32_func);
}
