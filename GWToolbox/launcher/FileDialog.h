#pragma once

#include <optional>
#include <string>

// The DLL the user picks, or nothing when they cancel or GTK isn't installed.
std::optional<std::string> ChooseDllFile(const std::string& start);
// The folder the user picks, or nothing when they cancel or GTK isn't installed.
std::optional<std::string> ChooseFolder(const std::string& start);
