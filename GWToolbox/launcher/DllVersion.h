#pragma once

#include <filesystem>
#include <string>

// The file version of a PE file, as major.minor.build.revision, read from its version resource. Empty when the file has none.
std::string PeFileVersion(const std::filesystem::path& file);
