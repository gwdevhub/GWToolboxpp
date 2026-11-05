#include "stdafx.h"

#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/Utilities/Scanner.h>

#include "DownloadSpeedModule.h"

namespace {
    GW::MemoryPatcher chunk_patches[4];
}

void DownloadSpeedModule::Initialize()
{
    ToolboxModule::Initialize();

    const auto file_client = GW::Scanner::FindAssertion("FcSrv.cpp", "dnsIndex < arrsize(s_dns2dc)", 0, 0);
    if (!file_client) {
        Log::Error("Download Speed: could not locate the file client assertion");
        return;
    }
    const auto scan_end = file_client + 0x1000;

    struct Scan {
        const char* pattern;
        const char* mask;
        uintptr_t address = 0;
    };
    Scan scans[] = {
        {"\xc7\x40\x78\x00\x40\x00\x00", "xxxxxxx"},
        {"\xc7\x40\x7c\x00\x02\x00\x00", "xxxxxxx"},
        {"\x8b\x44\x96\x78\xd1\xe8", "xxxxxx"},
        {"\x8b\x44\x96\x78\x03\xc0", "xxxxxx"}
    };
    for (auto& scan : scans) {
        scan.address = GW::Scanner::FindInRange(scan.pattern, scan.mask, 0, file_client, scan_end);
        if (!scan.address || GW::Scanner::FindInRange(scan.pattern, scan.mask, 0, scan.address + 1, scan_end)) {
            Log::Error("Download Speed: could not uniquely locate the file client chunk-size instructions");
            return;
        }
    }

    chunk_patches[0].SetPatch(scans[0].address + 3, "\x00\x80\x00\x00", 4);
    chunk_patches[1].SetPatch(scans[1].address + 3, "\x00\x80\x00\x00", 4);
    chunk_patches[2].SetPatch(scans[2].address, "\xb8\x00\x80\x00\x00\x90", 6);
    chunk_patches[3].SetPatch(scans[3].address, "\xb8\x00\x80\x00\x00\x90", 6);
    for (auto& patch : chunk_patches) {
        patch.TogglePatch(true);
    }
}

void DownloadSpeedModule::Terminate()
{
    for (auto& patch : chunk_patches) {
        patch.Reset();
    }
    ToolboxModule::Terminate();
}
