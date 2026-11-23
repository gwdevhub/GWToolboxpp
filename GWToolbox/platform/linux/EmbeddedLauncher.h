#pragma once

#include <filesystem>
#include <optional>
#include <string>

std::optional<std::filesystem::path> extractWindowsLauncher(std::string& error);
