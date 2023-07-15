#pragma once

#include <filesystem>

// is_directory without catch; returns false on failure
bool PathIsDirectorySafe(const std::filesystem::path& path, bool* out);
// directory_iterator without catch; returns false on failure
bool PathDirectoryIteratorSafe(const std::filesystem::path& path, std::filesystem::directory_iterator* out);
// exists without catch; returns false on failure
bool PathExistsSafe(const std::filesystem::path& path, bool* out);

// Get absolute path of current running executable
bool PathGetExeFullPath(std::filesystem::path& out);
// Get basename of current running executable
bool PathGetExeFileName(std::wstring& out);

bool PathGetProgramDirectory(std::filesystem::path& out);

bool PathGetDocumentsPath(std::filesystem::path& out, const wchar_t* suffix);

// create_directories without catch; returns false on failure
bool PathCreateDirectorySafe(const std::filesystem::path& path);

// Performs manually recursive copy. Useful for figuring out which file is in use when using std::filesystem::copy on a directory fails.
bool PathSafeCopy(const std::filesystem::path& from, const std::filesystem::path& to, bool copy_if_target_is_newer = false);

// Performs manually recursive remove. Useful for figuring out which file is in use when std::filesystem::remove_all fails.
// This function will be MUCH slower than doing remove_all
bool PathRecursiveRemove(const std::filesystem::path& from);

bool PathGetComputerName(std::filesystem::path& out);
