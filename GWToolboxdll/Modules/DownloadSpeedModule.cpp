#include "stdafx.h"

#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/Utilities/Scanner.h>

#include "DownloadSpeedModule.h"

namespace {
    GW::MemoryPatcher foreground_chunk_patch;
    GW::MemoryPatcher background_chunk_patch;
    GW::MemoryPatcher decrease_chunk_patch;
    GW::MemoryPatcher increase_chunk_patch;

    bool patches_applied = false;
}

void DownloadSpeedModule::Initialize()
{
    ToolboxModule::Initialize();

    struct Scan {
        const char* pattern;
        const char* mask;
        uintptr_t address = 0;
    };
    Scan scans[] = {
        {"\x66\xc7\x40\x64\xf1\xf1\xc7\x40\x74\x00\x40\x00\x00\xc7\x40\x78\x00\x40\x00\x00\xc7\x40\x7c\x00\x02\x00\x00", "xxxxxxxxxxxxxxxxxxxxxxxxxxx"},
        {"\x8b\x44\x96\x78\xd1\xe8\x3d\x00\x02\x00\x00\x73\x2c\xb8\x00\x02\x00\x00", "xxxxxxxxxxxx?xxxxx"},
        {"\x8b\x44\x96\x78\x03\xc0\x3d\x00\x80\x00\x00\x76\x05\xb8\x00\x80\x00\x00", "xxxxxxxxxxxx?xxxxx"}
    };
    for (auto& scan : scans) {
        scan.address = GW::Scanner::Find(scan.pattern, scan.mask);
        if (!scan.address || GW::Scanner::FindNth(scan.pattern, scan.mask, 1)) {
            Log::Error("Download Speed: could not uniquely locate the file client chunk-size instructions");
            return;
        }
    }

    foreground_chunk_patch.SetPatch(scans[0].address + 16, "\x00\x80\x00\x00", 4);
    background_chunk_patch.SetPatch(scans[0].address + 23, "\x00\x80\x00\x00", 4);
    decrease_chunk_patch.SetPatch(scans[1].address, "\xb8\x00\x80\x00\x00\x90", 6);
    increase_chunk_patch.SetPatch(scans[2].address, "\xb8\x00\x80\x00\x00\x90", 6);

    patches_applied = foreground_chunk_patch.TogglePatch(true)
        && background_chunk_patch.TogglePatch(true)
        && decrease_chunk_patch.TogglePatch(true)
        && increase_chunk_patch.TogglePatch(true);
    if (!patches_applied) {
        foreground_chunk_patch.Reset();
        background_chunk_patch.Reset();
        decrease_chunk_patch.Reset();
        increase_chunk_patch.Reset();
        Log::Error("Download Speed: failed to apply the file client chunk-size patches");
    }
}

void DownloadSpeedModule::Terminate()
{
    foreground_chunk_patch.Reset();
    background_chunk_patch.Reset();
    decrease_chunk_patch.Reset();
    increase_chunk_patch.Reset();
    patches_applied = false;
    ToolboxModule::Terminate();
}

void DownloadSpeedModule::DrawSettingsInternal()
{
    ImGui::TextUnformatted(patches_applied ? "Maximum download chunk size: 32 KiB" : "Download speed patch unavailable for this game client.");
    ImGui::TextWrapped("Existing connections switch on their next adaptive chunk-size update. New connections start at the maximum size.");
}
