#pragma once

#include <string>

#include "ScanResult.h"

struct InjectOutcome {
    bool ok = false;
    std::string message;
};

InjectOutcome injectTarget(const ScanTarget& target);
