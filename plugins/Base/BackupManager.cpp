#include <BackupManager.h>

#include <filesystem>
#include <fstream>
#include <chrono>

namespace {
    constexpr auto maxBackupsPerPlugin = 9;

    std::filesystem::path getLargestEntry(std::filesystem::path dir)
    {
        std::uintmax_t maxFileSize = 0;
        std::filesystem::path largestFile = "";
        for (const auto& entry : std::filesystem::directory_iterator{dir}) {
            if (const auto sz = std::filesystem::file_size(entry.path()); sz > maxFileSize) {
                maxFileSize = sz;
                largestFile = entry.path();
            }
        }
        return largestFile;
    }
    std::filesystem::path getRecentlyEditedEntry(std::filesystem::path dir)
    {
        std::filesystem::file_time_type mostRecentEdit = std::chrono::time_point<std::chrono::file_clock>::min();
        std::filesystem::path file = "";
        for (const auto& entry : std::filesystem::directory_iterator{dir}) {
            if (const auto editTime = std::filesystem::last_write_time(entry.path()); editTime > mostRecentEdit) {
                mostRecentEdit = editTime;
                file = entry.path();
            }
        }
        return file;
    }
    std::filesystem::path getOldestEntry(std::filesystem::path dir)
    {
        std::filesystem::file_time_type leastRecentEdit = std::chrono::time_point<std::chrono::file_clock>::max();
        std::filesystem::path file = "";
        for (const auto& entry : std::filesystem::directory_iterator{dir}) {
            if (const auto editTime = std::filesystem::last_write_time(entry.path()); editTime < leastRecentEdit) {
                leastRecentEdit = editTime;
                file = entry.path();
            }
        }
        return file;
    }
} // namespace

void BackupManager::initialize(std::wstring_view pluginFolderPath)
{
    basePath = std::filesystem::path{pluginFolderPath} / L"backups";
    std::filesystem::create_directory(basePath);
}

void BackupManager::save(std::wstring_view pluginName, std::filesystem::path contentPath) 
{
    const auto pluginBackupFolder = basePath / pluginName;
    std::filesystem::create_directory(pluginBackupFolder);

    if (backupCount(pluginName) >= maxBackupsPerPlugin) {
        const auto oldestEntry = getOldestEntry(pluginBackupFolder);
        if (oldestEntry.wstring().ends_with(L".txt")) 
            std::filesystem::remove(oldestEntry);
    }
    
    const auto filePath = [&] {
        int backupIndex = 0;
        while (true) 
        {
            const auto filePath = pluginBackupFolder / (std::to_wstring(backupIndex++) + L".txt");
            if (!std::filesystem::exists(filePath)) return filePath;
        }
    }();
    
    std::filesystem::copy(contentPath, filePath);
}

int BackupManager::backupCount(std::wstring_view pluginName)
{
    const auto pluginBackupFolder = basePath / pluginName;
    if (!std::filesystem::exists(pluginBackupFolder)) return 0;

    size_t count = 0;
    for ([[maybe_unused]] const auto& entry : std::filesystem::directory_iterator{pluginBackupFolder})
        ++count;
    return count;
}

std::vector<std::filesystem::path> BackupManager::list(std::wstring_view pluginName)
{
    const auto pluginBackupFolder = basePath / pluginName;
    std::vector<std::filesystem::path> result;
    if (!std::filesystem::exists(pluginBackupFolder)) return result;
    
    for (const auto& entry : std::filesystem::directory_iterator{pluginBackupFolder})
    {
        result.push_back(entry.path());
    }
    return result;
}

std::filesystem::path BackupManager::load(std::wstring_view pluginName, BackupManager::LoadType type, std::optional<int> index)
{
    const auto pluginBackupFolder = basePath / pluginName;
    if (!std::filesystem::exists(pluginBackupFolder)) return {};

    std::filesystem::path fileToLoad = "";
    switch (type) 
    {
        case BackupManager::LoadType::Largest:
            fileToLoad = getLargestEntry(pluginBackupFolder);
            break;
        case BackupManager::LoadType::Latest:
            fileToLoad = getRecentlyEditedEntry(pluginBackupFolder);
            break;
        case BackupManager::LoadType::Index:
            fileToLoad = index.has_value() ? pluginBackupFolder / (std::to_wstring(*index) + L".txt") : "";
            break;
        default:
            break;
    }
    return fileToLoad;
}
