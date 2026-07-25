#pragma once

#include <Modules/QuestProgressDomain.h>

namespace QuestProgress {

// Pure deterministic reduce(prev, input) → next. No I/O, no global mutable state, no GWCA.
ReducerOutput Reduce(const ReducerInput& input);

} // namespace QuestProgress
