#pragma once

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/GameEntities/Frame.h>

namespace GW::UI {
    // Find a multiline text label frame by its encoded label string.
    // Backed by a cache populated from frame create/destroy callbacks, so this avoids
    // chasing positional child frame ids that drift every time the GW UI changes.
    // With partial_match, returns the first frame whose encoded label starts with encoded_string.
    GW::MultiLineTextLabelFrame* GetMultilineTextFrameByEncodedString(const wchar_t* encoded_string, bool partial_match = false);
}
