#include "stdafx.h"

#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/Utilities/Scanner.h>

#include <Defines.h>
#include <Logger.h>

#include "CrashFixesModule.h"

namespace {
    GW::MemoryPatcher blockmap_dims_x_assert_patch;
    GW::MemoryPatcher blockmap_dims_y_assert_patch;
}

void CrashFixesModule::Initialize()
{
    ToolboxModule::Initialize();

    const auto y_check_address = GW::Scanner::FindAssertion("ChCliApi.cpp", "blockMapDims.x == missionRect.x1 - missionRect.x0 && blockMapDims.y == missionRect.y1 - missionRect.y0", 0, -5);
    DEBUG_ASSERT(y_check_address && *(uint8_t*)y_check_address == 0x74); // je <matched>
    if (y_check_address) {
        blockmap_dims_y_assert_patch.SetPatch(y_check_address, "\xeb", 1);
        blockmap_dims_y_assert_patch.TogglePatch(true);

        const auto x_check_address = y_check_address - 10;
        DEBUG_ASSERT(*(uint8_t*)x_check_address == 0x75); // jne <report>
        blockmap_dims_x_assert_patch.SetPatch(x_check_address, "\x90\x90", 2);
        blockmap_dims_x_assert_patch.TogglePatch(true);
    }
    Log::Log("[CrashFixes] blockmap_dims_x_assert_patch = %p, blockmap_dims_y_assert_patch = %p\n",
             blockmap_dims_x_assert_patch.GetAddress(), blockmap_dims_y_assert_patch.GetAddress());
}

void CrashFixesModule::Terminate()
{
    ToolboxModule::Terminate();
    blockmap_dims_x_assert_patch.Reset();
    blockmap_dims_y_assert_patch.Reset();
}
