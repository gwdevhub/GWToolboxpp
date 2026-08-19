#include "stdafx.h"

#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/Utilities/Scanner.h>

#include <Defines.h>
#include <Logger.h>

#include "CrashFixesModule.h"

namespace {
    // Tripped on every map reveal packet by any map whose AreaInfo rect disagrees with its pathmap dimensions;
    // Kodonur Crossroads and Sunjiang Horde do as of build 38848.
    constexpr char blockmap_dims_assertion[] =
        "blockMapDims.x == missionRect.x1 - missionRect.x0 && blockMapDims.y == missionRect.y1 - missionRect.y0";

    constexpr char skip_assert_call[] = "\x83\xC4\x04\x90\x90"; // ADD ESP,4 (the pushed line number), then pad

    GW::MemoryPatcher blockmap_dims_assert_patch;
}

void CrashFixesModule::Initialize()
{
    ToolboxModule::Initialize();

    // Not FindAssertion: it needs PUSH <line>, MOV EDX,<file>, MOV ECX,<msg> contiguous, and here the compiler
    // emits the MOV EDX before the PUSH, so that pattern never matches. Anchor on the message reference instead.
    const auto message_operand = GW::Scanner::FindUseOfString(blockmap_dims_assertion);
    const auto call_address = message_operand ? message_operand + 4 : 0;
    if (call_address && *(uint8_t*)call_address == 0xE8) {
        // The handler isn't noreturn here; the instructions after the call reload the rect pointers and fall into
        // the paint loop, so skipping the call resumes the path the code already intended.
        blockmap_dims_assert_patch.SetPatch(call_address, skip_assert_call, 5);
        blockmap_dims_assert_patch.TogglePatch(true);
    }
    DEBUG_ASSERT(blockmap_dims_assert_patch.GetAddress());

    Log::Log("[CrashFixes] blockmap_dims_assert_patch = %p\n", blockmap_dims_assert_patch.GetAddress());
}

void CrashFixesModule::Terminate()
{
    ToolboxModule::Terminate();
    blockmap_dims_assert_patch.Reset();
}
