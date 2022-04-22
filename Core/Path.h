#pragma once

#include <filesystem>

// @Cleanup: Add return values
void PathGetExeFullPath(wchar_t *path, size_t length);
void PathGetExeFileName(wchar_t *path, size_t length);

void PathGetProgramDirectory(wchar_t *path, size_t length);

bool PathGetDocumentsDirectory(wchar_t *path, size_t length);
bool PathGetDocumentsPath(wchar_t* path, size_t length, const wchar_t* suffix);
bool PathGetAppDataDirectory(wchar_t* path, size_t length);
bool PathGetAppDataPath(wchar_t* path, size_t length, const wchar_t* suffix);

bool PathCreateDirectory(const wchar_t *path);

bool PathCompose(wchar_t *dest, size_t length, const wchar_t *left, const wchar_t *right);

std::filesystem::path PathGetComputerName();
bool PathMoveDataAndCreateSymlink(bool create_symlink = true);
