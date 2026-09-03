#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace TrackerAdvanced {
    class TrackerPersistence final {
    public:
        using JobId = uint64_t;

        enum class JobType : uint8_t { Read, Write, ListJsonFiles, Delete };

        enum class ResultStatus : uint8_t {
            Succeeded,
            SucceededWithWarning,
            NotFound,
            Failed,
            Superseded
        };

        enum class ReadSource : uint8_t { None, Primary, Backup };

        struct Result {
            JobId id = 0;
            JobType type = JobType::Read;
            ResultStatus status = ResultStatus::Failed;
            std::string key;
            std::filesystem::path path;
            ReadSource read_source = ReadSource::None;
            std::string json;
            std::vector<std::filesystem::path> files;
            std::string message;

            [[nodiscard]] bool Succeeded() const noexcept
            {
                return status == ResultStatus::Succeeded
                    || status == ResultStatus::SucceededWithWarning;
            }
            [[nodiscard]] bool HasWarning() const noexcept
            {
                return status == ResultStatus::SucceededWithWarning;
            }
            [[nodiscard]] bool WasNotFound() const noexcept
            {
                return status == ResultStatus::NotFound;
            }
            [[nodiscard]] bool UsedBackup() const noexcept { return read_source == ReadSource::Backup; }
        };

        TrackerPersistence();
        ~TrackerPersistence();

        TrackerPersistence(const TrackerPersistence&) = delete;
        TrackerPersistence(TrackerPersistence&&) = delete;
        TrackerPersistence& operator=(const TrackerPersistence&) = delete;
        TrackerPersistence& operator=(TrackerPersistence&&) = delete;

        [[nodiscard]] JobId EnqueueRead(std::filesystem::path path, std::string key = {});
        [[nodiscard]] JobId EnqueueWrite(
            std::filesystem::path path,
            std::string json,
            std::string coalesce_key,
            bool preserve_backup = false);
        [[nodiscard]] JobId EnqueueListJsonFiles(std::filesystem::path directory, std::string key = {});
        [[nodiscard]] JobId EnqueueDelete(std::filesystem::path path, std::string key = {});

        [[nodiscard]] std::vector<Result> DrainResults();

        // Accepted work is drained before the worker exits.
        void RequestShutdown() noexcept;
        void ShutdownAndWait() noexcept;
        [[nodiscard]] bool RequestShutdownIfIdle() noexcept;
        [[nodiscard]] bool IsIdle() const noexcept;
        [[nodiscard]] bool IsShutdownRequested() const noexcept;
        [[nodiscard]] bool IsShutdownComplete() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
} // namespace TrackerAdvanced
