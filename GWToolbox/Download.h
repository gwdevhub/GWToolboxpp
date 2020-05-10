#pragma once

#include <string>

bool Download(std::string& content, const wchar_t *url);
bool Download(const wchar_t *path_to_file, const wchar_t *url);

bool DownloadFiles();
