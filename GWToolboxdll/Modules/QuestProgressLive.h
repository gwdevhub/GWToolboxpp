#pragma once

// Live GWCA adapters for Batch 2C. Not linked into QuestProgressTests.

#include <Modules/QuestProgressService.h>
#include <Modules/QuestSessionIdentity.h>

struct LiveQuestView;

namespace QuestProgress {

// Sample account/character UUID + metadata when world context is available.
// Returns Unbound when not world-ready; Ephemeral when character UUID is zero.
SessionIdentity SampleLiveSessionIdentity(bool world_ready);

// Copy LiveQuestView into owned reducer snapshot (no borrowed GWCA pointers).
QuestSnapshot ToQuestSnapshot(const LiveQuestView& view);

} // namespace QuestProgress
