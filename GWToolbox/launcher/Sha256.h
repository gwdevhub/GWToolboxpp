#pragma once

#include <filesystem>
#include <string>
#include <string_view>

std::string Sha256Hex(std::string_view content);
std::string Sha256OfFile(const std::filesystem::path& file);
