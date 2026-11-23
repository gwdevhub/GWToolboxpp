#pragma once

#include <filesystem>

bool runningFromAppImage();
std::filesystem::path appImageFile();
std::filesystem::path launcherDirectory();
