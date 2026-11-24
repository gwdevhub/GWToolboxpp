#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

int downloadWithProgress(const std::string& url, const std::filesystem::path& partial, std::uintmax_t expected_size, const std::function<void(int)>& progress);
