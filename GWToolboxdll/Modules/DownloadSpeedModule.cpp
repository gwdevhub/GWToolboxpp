#include "stdafx.h"

#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/Utilities/Scanner.h>

#include "DownloadSpeedModule.h"

namespace {
    constexpr uint32_t maximum_chunk_size = 0x8000;
    GW::MemoryPatcher chunk_patches[4];
}

void DownloadSpeedModule::Initialize()
{
    ToolboxModule::Initialize();

    const auto file_client_begin = GW::Scanner::FindAssertion("FcSrv.cpp", "dnsIndex < arrsize(s_dns2dc)", 0, 0);
    const auto file_client_end = GW::Scanner::FindAssertion("FcSrv.cpp", "s_currSrv != this", 0, 0);
    if (!file_client_begin || !file_client_end || file_client_begin >= file_client_end) {
        Log::Error("Download Speed: could not locate the file client assertions");
        return;
    }

    const auto find_operand = [file_client_begin, file_client_end](const char* pattern, const char* mask, const int operand_offset) -> uintptr_t {
        const auto instruction = GW::Scanner::FindInRange(pattern, mask, 0, file_client_begin, file_client_end);
        if (!instruction || GW::Scanner::FindInRange(pattern, mask, 0, instruction + 1, file_client_end)) {
            return 0;
        }
        return instruction + operand_offset;
    };

    const auto foreground_default = find_operand("\xc7\x40\x78\x00\x40\x00\x00", "x?xxxxx", 3);
    const auto background_default = find_operand("\xc7\x40\x7c\x00\x02\x00\x00", "x?xxxxx", 3);
    const auto minimum_comparison = find_operand("\x3d\x00\x02\x00\x00", "xxxxx", 1);
    const auto minimum_assignment = find_operand("\xb8\x00\x02\x00\x00", "xxxxx", 1);
    if (!foreground_default || !background_default || !minimum_comparison || !minimum_assignment) {
        Log::Error("Download Speed: could not uniquely locate the chunk-size constants");
        return;
    }

    const uintptr_t operands[] = {foreground_default, background_default, minimum_comparison, minimum_assignment};
    for (size_t i = 0; i < _countof(chunk_patches); ++i) {
        chunk_patches[i].SetPatch(operands[i], reinterpret_cast<const char*>(&maximum_chunk_size), sizeof(maximum_chunk_size));
        chunk_patches[i].TogglePatch(true);
    }
}

void DownloadSpeedModule::Terminate()
{
    for (auto& patch : chunk_patches) {
        patch.Reset();
    }
    ToolboxModule::Terminate();
}
