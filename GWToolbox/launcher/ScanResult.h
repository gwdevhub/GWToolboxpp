#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ScanTarget {
    std::string prefix;
    std::string wine;
    uint32_t pid = 0;
    std::string kind;
    std::string character;
    bool injected = false;
};

struct ScanError {
    std::string prefix;
    std::string message;
};

struct ScanResult {
    std::vector<ScanTarget> targets;
    std::vector<ScanError> errors;
};

ScanResult ScanGuildWars();
