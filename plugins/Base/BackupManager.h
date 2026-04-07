#pragma once

#include <string>
#include <optional>
#include <filesystem>

class BackupManager 
{
public:
    static BackupManager& getInstance()
    {
        static BackupManager info;
        return info;
    }
    BackupManager(const BackupManager&) = delete;
    BackupManager(BackupManager&&) = delete;

    enum class LoadType {
        Largest,
        Latest,
        Index
    };
    void initialize(std::wstring_view pluginFolderPath);
    void save(std::wstring_view pluginName, std::filesystem::path contentPath);
    std::filesystem::path load(std::wstring_view pluginName, LoadType type, std::optional<int> index = {});
    int backupCount(std::wstring_view pluginName);
    std::vector<std::filesystem::path> list(std::wstring_view pluginName);

private:
    BackupManager() = default;

    std::filesystem::path basePath;
};
