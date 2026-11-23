#pragma once

#include <chrono>
#include <string>
#include <vector>

struct CommandOutput {
    int exit_code = -1;
    std::string text;
};

CommandOutput runCommand(const std::vector<std::string>& argv, const std::vector<std::string>& environment, std::chrono::seconds timeout);
std::vector<std::string> currentEnvironment();
