#include "TrackerPersistence.h"

#include <Windows.h>

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <exception>
#include <limits>
#include <list>
#include <mutex>
#include <system_error>
#include <thread>
#include <utility>

#include <glaze/glaze.hpp>

namespace TrackerAdvanced {
    namespace {
        constexpr uint64_t max_json_file_size = 64ull * 1024ull * 1024ull;

        struct ScopedHandle {
            HANDLE value = INVALID_HANDLE_VALUE;

            ScopedHandle() = default;
            explicit ScopedHandle(const HANDLE handle) : value(handle) {}

            ~ScopedHandle()
            {
                if (value != INVALID_HANDLE_VALUE) {
                    CloseHandle(value);
                }
            }

            ScopedHandle(const ScopedHandle&) = delete;
            ScopedHandle(ScopedHandle&&) = delete;
            ScopedHandle& operator=(const ScopedHandle&) = delete;
            ScopedHandle& operator=(ScopedHandle&&) = delete;

            [[nodiscard]] bool Valid() const noexcept { return value != INVALID_HANDLE_VALUE; }
        };

        std::filesystem::path SiblingPathWithSuffix(const std::filesystem::path& path, const wchar_t* suffix)
        {
            auto sibling = path;
            sibling += suffix;
            return sibling;
        }

        std::string PathForMessage(const std::filesystem::path& path)
        {
            const auto utf8 = path.u8string();
            return std::string(utf8.begin(), utf8.end());
        }

        std::string WideToUtf8(const std::wstring_view value)
        {
            if (value.empty()) {
                return {};
            }
            const auto required = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
            if (required <= 0) {
                return {};
            }
            std::string output(static_cast<size_t>(required), '\0');
            WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), output.data(), required, nullptr, nullptr);
            return output;
        }

        std::string WindowsError(const DWORD error)
        {
            LPWSTR buffer = nullptr;
            const auto length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
            std::wstring message;
            if (length && buffer) {
                message.assign(buffer, length);
                while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n')) {
                    message.pop_back();
                }
            }
            if (buffer) {
                LocalFree(buffer);
            }
            auto result = WideToUtf8(message);
            if (result.empty()) {
                result = "Unknown Windows error";
            }
            result += " (Win32 error " + std::to_string(error) + ")";
            return result;
        }

        enum class FileReadStatus : uint8_t {
            Succeeded,
            NotFound,
            Failed
        };

        struct FileRead {
            FileReadStatus status = FileReadStatus::Failed;
            std::string json;
            std::string error;

            [[nodiscard]] bool Succeeded() const noexcept
            {
                return status == FileReadStatus::Succeeded;
            }

            [[nodiscard]] bool WasNotFound() const noexcept
            {
                return status == FileReadStatus::NotFound;
            }
        };

        FileRead ReadAndValidateJson(const std::filesystem::path& path)
        {
            const ScopedHandle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
            if (!file.Valid()) {
                const auto error = GetLastError();
                return {
                    .status = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND
                        ? FileReadStatus::NotFound
                        : FileReadStatus::Failed,
                    .error = "Could not open '" + PathForMessage(path) + "': " + WindowsError(error),
                };
            }

            LARGE_INTEGER file_size{};
            if (!GetFileSizeEx(file.value, &file_size)) {
                const auto error = GetLastError();
                return {.error = "Could not determine the size of '" + PathForMessage(path) + "': " + WindowsError(error)};
            }
            if (file_size.QuadPart < 0 || static_cast<uint64_t>(file_size.QuadPart) > max_json_file_size || static_cast<uint64_t>(file_size.QuadPart) > std::numeric_limits<size_t>::max()) {
                return {.error = "'" + PathForMessage(path) + "' exceeds the 64 MiB JSON file limit."};
            }

            std::string json(static_cast<size_t>(file_size.QuadPart), '\0');
            size_t total_read = 0;
            while (total_read < json.size()) {
                const auto remaining = json.size() - total_read;
                const auto chunk_size = static_cast<DWORD>(std::min<size_t>(remaining, std::numeric_limits<DWORD>::max()));
                DWORD bytes_read = 0;
                if (!ReadFile(file.value, json.data() + total_read, chunk_size, &bytes_read, nullptr)) {
                    const auto error = GetLastError();
                    return {.error = "Could not read '" + PathForMessage(path) + "': " + WindowsError(error)};
                }
                if (!bytes_read) {
                    return {.error = "Unexpected end of file while reading '" + PathForMessage(path) + "'."};
                }
                total_read += bytes_read;
            }

            if (const auto error = glz::validate_json(json); error) {
                return {.error = "'" + PathForMessage(path) + "' contains invalid JSON: " + glz::format_error(error, json)};
            }
            return {.status = FileReadStatus::Succeeded, .json = std::move(json)};
        }

        bool WriteAll(const HANDLE file, const std::string& bytes, std::string& error)
        {
            size_t total_written = 0;
            while (total_written < bytes.size()) {
                const auto remaining = bytes.size() - total_written;
                const auto chunk_size = static_cast<DWORD>(std::min<size_t>(remaining, std::numeric_limits<DWORD>::max()));
                DWORD bytes_written = 0;
                if (!WriteFile(file, bytes.data() + total_written, chunk_size, &bytes_written, nullptr)) {
                    error = WindowsError(GetLastError());
                    return false;
                }
                if (!bytes_written) {
                    error = "WriteFile completed without writing any bytes.";
                    return false;
                }
                total_written += bytes_written;
            }
            return true;
        }

        TrackerPersistence::Result WriteJson(
            const TrackerPersistence::JobId id,
            const std::filesystem::path& path,
            const std::string& json,
            const std::string& key,
            const bool preserve_backup)
        {
            TrackerPersistence::Result result{.id = id, .type = TrackerPersistence::JobType::Write, .key = key, .path = path};

            if (json.size() > max_json_file_size) {
                result.message = "Refused to write more than 64 MiB of JSON to '" + PathForMessage(path) + "'.";
                return result;
            }
            if (const auto error = glz::validate_json(json); error) {
                result.message = "Refused to write invalid JSON to '" + PathForMessage(path) + "': " + glz::format_error(error, json);
                return result;
            }

            const auto parent = path.parent_path();
            if (!parent.empty()) {
                std::error_code directory_error;
                std::filesystem::create_directories(parent, directory_error);
                if (directory_error) {
                    result.message = "Could not create directory '" + PathForMessage(parent) + "': " + directory_error.message();
                    return result;
                }
            }

            const auto temporary = SiblingPathWithSuffix(path, L".tmp");
            const auto backup = SiblingPathWithSuffix(path, L".bak");
            std::string temporary_error;
            {
                const ScopedHandle file(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
                if (!file.Valid()) {
                    result.message = "Could not create temporary file '" + PathForMessage(temporary) + "': " + WindowsError(GetLastError());
                    return result;
                }

                if (!WriteAll(file.value, json, temporary_error)) {
                    temporary_error = "Could not write temporary file '" + PathForMessage(temporary) + "': " + temporary_error;
                }
                else if (!FlushFileBuffers(file.value)) {
                    temporary_error = "Could not flush temporary file '" + PathForMessage(temporary) + "': " + WindowsError(GetLastError());
                }
            }
            if (!temporary_error.empty()) {
                result.message = std::move(temporary_error);
                DeleteFileW(temporary.c_str());
                return result;
            }

            const auto attributes = GetFileAttributesW(path.c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES) {
                if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
                    result.message = "Cannot replace '" + PathForMessage(path) + "' because it is a directory.";
                    DeleteFileW(temporary.c_str());
                    return result;
                }

                const auto primary = ReadAndValidateJson(path);
                if (preserve_backup) {
                    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                        result.message = "Could not replace invalid application data at '" + PathForMessage(path) + "' while preserving its backup: " + WindowsError(GetLastError());
                        DeleteFileW(temporary.c_str());
                        return result;
                    }
                }
                else if (primary.Succeeded()) {
                    if (!ReplaceFileW(path.c_str(), temporary.c_str(), backup.c_str(), REPLACEFILE_WRITE_THROUGH | REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr)) {
                        result.message = "Could not atomically replace '" + PathForMessage(path) + "' while preserving '" + PathForMessage(backup) + "': " + WindowsError(GetLastError());
                        DeleteFileW(temporary.c_str());
                        return result;
                    }
                }
                else {
                    const auto existing_backup = ReadAndValidateJson(backup);
                    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                        result.message = "Could not replace invalid primary JSON at '" + PathForMessage(path) + "': " + WindowsError(GetLastError());
                        DeleteFileW(temporary.c_str());
                        return result;
                    }

                    result.message = "Replaced invalid primary JSON at '" + PathForMessage(path) + "'. Primary error: " + primary.error;
                    if (existing_backup.Succeeded()) {
                        result.message += " The validated backup was preserved.";
                    }
                    else if (!CopyFileW(path.c_str(), backup.c_str(), FALSE)) {
                        result.message += " The backup was also invalid or missing, and a replacement backup could not be created: " + WindowsError(GetLastError());
                    }
                    else {
                        result.message += " The backup was recreated from the committed JSON.";
                    }
                }
            }
            else {
                const auto attributes_error = GetLastError();
                if (attributes_error != ERROR_FILE_NOT_FOUND && attributes_error != ERROR_PATH_NOT_FOUND) {
                    result.message = "Could not inspect destination '" + PathForMessage(path) + "': " + WindowsError(attributes_error);
                    DeleteFileW(temporary.c_str());
                    return result;
                }
                if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                    result.message = "Could not move temporary file into '" + PathForMessage(path) + "': " + WindowsError(GetLastError());
                    DeleteFileW(temporary.c_str());
                    return result;
                }
                const auto existing_backup = ReadAndValidateJson(backup);
                if (!existing_backup.Succeeded() && !CopyFileW(path.c_str(), backup.c_str(), FALSE)) {
                    result.message = "Saved '" + PathForMessage(path) + "', but could not create its initial backup '" + PathForMessage(backup) + "': " + WindowsError(GetLastError());
                }
            }

            result.status = result.message.empty()
                ? TrackerPersistence::ResultStatus::Succeeded
                : TrackerPersistence::ResultStatus::SucceededWithWarning;
            return result;
        }

        TrackerPersistence::Result ReadJson(const TrackerPersistence::JobId id, const std::filesystem::path& path, const std::string& key)
        {
            TrackerPersistence::Result result{.id = id, .type = TrackerPersistence::JobType::Read, .key = key, .path = path};

            auto primary = ReadAndValidateJson(path);
            if (primary.Succeeded()) {
                result.status = TrackerPersistence::ResultStatus::Succeeded;
                result.read_source = TrackerPersistence::ReadSource::Primary;
                result.json = std::move(primary.json);
                return result;
            }

            const auto backup_path = SiblingPathWithSuffix(path, L".bak");
            auto backup = ReadAndValidateJson(backup_path);
            if (backup.Succeeded()) {
                result.status = TrackerPersistence::ResultStatus::Succeeded;
                result.read_source = TrackerPersistence::ReadSource::Backup;
                result.json = std::move(backup.json);
                result.message = "Primary read failed; recovered from '" + PathForMessage(backup_path) + "'. Primary error: " + primary.error;
                return result;
            }

            if (primary.WasNotFound() && backup.WasNotFound()) {
                result.status = TrackerPersistence::ResultStatus::NotFound;
            }
            result.message = "Could not read primary or backup JSON. Primary: " + primary.error + " Backup: " + backup.error;
            return result;
        }

        TrackerPersistence::Result ListJsonFiles(const TrackerPersistence::JobId id, const std::filesystem::path& directory, const std::string& key)
        {
            TrackerPersistence::Result result{.id = id, .type = TrackerPersistence::JobType::ListJsonFiles, .key = key, .path = directory};

            std::error_code error;
            const auto exists = std::filesystem::exists(directory, error);
            if (error) {
                result.message = "Could not inspect profiles directory '" + PathForMessage(directory) + "': " + error.message();
                return result;
            }
            if (!exists) {
                result.status = TrackerPersistence::ResultStatus::Succeeded;
                return result;
            }
            if (!std::filesystem::is_directory(directory, error)) {
                result.message = error ? "Could not inspect profiles directory '" + PathForMessage(directory) + "': " + error.message() : "'" + PathForMessage(directory) + "' is not a directory.";
                return result;
            }

            for (std::filesystem::directory_iterator iterator(directory, error), end; iterator != end && !error; iterator.increment(error)) {
                std::error_code entry_error;
                if (!iterator->is_regular_file(entry_error) || entry_error) {
                    continue;
                }
                const auto extension = iterator->path().extension().wstring();
                if (_wcsicmp(extension.c_str(), L".json") == 0) {
                    result.files.push_back(iterator->path());
                }
            }
            if (error) {
                result.files.clear();
                result.message = "Could not enumerate profiles directory '" + PathForMessage(directory) + "': " + error.message();
                return result;
            }

            std::ranges::sort(result.files, [](const auto& left, const auto& right) {
                return _wcsicmp(left.filename().c_str(), right.filename().c_str()) < 0;
            });
            result.status = TrackerPersistence::ResultStatus::Succeeded;
            return result;
        }

        bool DeleteIfPresent(const std::filesystem::path& path, std::string& error)
        {
            if (DeleteFileW(path.c_str())) {
                return true;
            }
            const auto delete_error = GetLastError();
            if (delete_error == ERROR_FILE_NOT_FOUND || delete_error == ERROR_PATH_NOT_FOUND) {
                return true;
            }
            if (!error.empty()) {
                error += ' ';
            }
            error += "Could not delete '" + PathForMessage(path) + "': " + WindowsError(delete_error);
            return false;
        }

        TrackerPersistence::Result DeleteJson(const TrackerPersistence::JobId id, const std::filesystem::path& path, const std::string& key)
        {
            TrackerPersistence::Result result{.id = id, .type = TrackerPersistence::JobType::Delete, .key = key, .path = path};
            const auto backup = SiblingPathWithSuffix(path, L".bak");
            const auto temporary = SiblingPathWithSuffix(path, L".tmp");

            auto committed = DeleteIfPresent(path, result.message);
            committed = DeleteIfPresent(backup, result.message) && committed;
            const auto temporary_removed = DeleteIfPresent(temporary, result.message);
            if (committed) {
                result.status = temporary_removed
                    ? TrackerPersistence::ResultStatus::Succeeded
                    : TrackerPersistence::ResultStatus::SucceededWithWarning;
            }
            return result;
        }
    } // namespace

    struct TrackerPersistence::Impl {
        struct Job {
            const JobId id;
            const JobType type;
            const std::filesystem::path path;
            const std::string key;
            const std::string json;
            const bool preserve_backup;
        };

        mutable std::mutex mutex;
        std::condition_variable wake;
        std::list<std::shared_ptr<const Job>> jobs;
        std::deque<Result> results;
        JobId next_id = 1;
        bool active = false;
        bool shutdown_requested = false;
        bool shutdown_complete = false;
        std::jthread worker;

        Impl()
            : worker([this] {
                  Run();
              })
        {}

        ~Impl()
        {
            RequestShutdown();
            if (worker.joinable()) {
                worker.join();
            }
        }

        JobId NextId()
        {
            const auto id = next_id++;
            if (!next_id) {
                next_id = 1;
            }
            return id;
        }

        JobId RejectJob(const JobType type, std::filesystem::path path, std::string key, std::string message)
        {
            const auto id = NextId();
            results.push_back({.id = id, .type = type, .status = ResultStatus::Failed, .key = std::move(key), .path = std::move(path), .message = std::move(message)});
            return id;
        }

        JobId Enqueue(
            const JobType type,
            std::filesystem::path path,
            std::string key,
            std::string json,
            const bool preserve_backup = false)
        {
            std::scoped_lock lock(mutex);
            if (shutdown_requested) {
                return RejectJob(type, std::move(path), std::move(key), "Persistence worker is shutting down; the job was not queued.");
            }
            if (type == JobType::Write && key.empty()) {
                return RejectJob(type, std::move(path), std::move(key), "Write jobs require a non-empty coalescing key.");
            }

            const auto id = NextId();
            if (type == JobType::Write) {
                for (auto iterator = jobs.begin(); iterator != jobs.end();) {
                    const auto& queued = **iterator;
                    if (queued.type != JobType::Write || queued.key != key) {
                        ++iterator;
                        continue;
                    }
                    results.push_back({.id = queued.id, .type = queued.type, .status = ResultStatus::Superseded, .key = queued.key, .path = queued.path, .message = "A newer queued write replaced this job."});
                    iterator = jobs.erase(iterator);
                }
            }

            jobs.push_back(std::make_shared<const Job>(Job{
                .id = id,
                .type = type,
                .path = std::move(path),
                .key = std::move(key),
                .json = std::move(json),
                .preserve_backup = preserve_backup,
            }));
            wake.notify_one();
            return id;
        }

        void RequestShutdown() noexcept
        {
            {
                std::scoped_lock lock(mutex);
                shutdown_requested = true;
            }
            wake.notify_one();
        }

        void ShutdownAndWait() noexcept
        {
            RequestShutdown();
            if (worker.joinable()) {
                worker.join();
            }
        }

        bool RequestShutdownIfIdle() noexcept
        {
            {
                std::scoped_lock lock(mutex);
                if (active || !jobs.empty() || !results.empty()) {
                    return false;
                }
                shutdown_requested = true;
            }
            wake.notify_one();
            return true;
        }

        void Run()
        {
            for (;;) {
                std::shared_ptr<const Job> job;
                {
                    std::unique_lock lock(mutex);
                    wake.wait(lock, [this] {
                        return shutdown_requested || !jobs.empty();
                    });
                    if (jobs.empty()) {
                        if (shutdown_requested) {
                            shutdown_complete = true;
                            return;
                        }
                        continue;
                    }
                    job = std::move(jobs.front());
                    jobs.pop_front();
                    active = true;
                }

                Result result{.id = job->id, .type = job->type, .key = job->key, .path = job->path};
                try {
                    switch (job->type) {
                        case JobType::Read:
                            result = ReadJson(job->id, job->path, job->key);
                            break;
                        case JobType::Write:
                            result = WriteJson(
                                job->id,
                                job->path,
                                job->json,
                                job->key,
                                job->preserve_backup);
                            break;
                        case JobType::ListJsonFiles:
                            result = ListJsonFiles(job->id, job->path, job->key);
                            break;
                        case JobType::Delete:
                            result = DeleteJson(job->id, job->path, job->key);
                            break;
                    }
                } catch (const std::exception& exception) {
                    result.message = "Persistence job failed unexpectedly: " + std::string(exception.what());
                } catch (...) {
                    result.message = "Persistence job failed with an unknown exception.";
                }

                {
                    std::scoped_lock lock(mutex);
                    results.push_back(std::move(result));
                    active = false;
                }
            }
        }
    };

    TrackerPersistence::TrackerPersistence() : impl_(std::make_unique<Impl>()) {}

    TrackerPersistence::~TrackerPersistence() = default;

    TrackerPersistence::JobId TrackerPersistence::EnqueueRead(std::filesystem::path path, std::string key)
    {
        return impl_->Enqueue(JobType::Read, std::move(path), std::move(key), {});
    }

    TrackerPersistence::JobId TrackerPersistence::EnqueueWrite(
        std::filesystem::path path,
        std::string json,
        std::string coalesce_key,
        const bool preserve_backup)
    {
        return impl_->Enqueue(
            JobType::Write,
            std::move(path),
            std::move(coalesce_key),
            std::move(json),
            preserve_backup);
    }

    TrackerPersistence::JobId TrackerPersistence::EnqueueListJsonFiles(std::filesystem::path directory, std::string key)
    {
        return impl_->Enqueue(JobType::ListJsonFiles, std::move(directory), std::move(key), {});
    }

    TrackerPersistence::JobId TrackerPersistence::EnqueueDelete(std::filesystem::path path, std::string key)
    {
        return impl_->Enqueue(JobType::Delete, std::move(path), std::move(key), {});
    }

    std::vector<TrackerPersistence::Result> TrackerPersistence::DrainResults()
    {
        std::scoped_lock lock(impl_->mutex);
        std::vector<Result> drained;
        drained.reserve(impl_->results.size());
        while (!impl_->results.empty()) {
            drained.push_back(std::move(impl_->results.front()));
            impl_->results.pop_front();
        }
        return drained;
    }

    void TrackerPersistence::RequestShutdown() noexcept
    {
        impl_->RequestShutdown();
    }

    void TrackerPersistence::ShutdownAndWait() noexcept
    {
        impl_->ShutdownAndWait();
    }

    bool TrackerPersistence::RequestShutdownIfIdle() noexcept
    {
        return impl_->RequestShutdownIfIdle();
    }

    bool TrackerPersistence::IsIdle() const noexcept
    {
        std::scoped_lock lock(impl_->mutex);
        return !impl_->active && impl_->jobs.empty();
    }

    bool TrackerPersistence::IsShutdownRequested() const noexcept
    {
        std::scoped_lock lock(impl_->mutex);
        return impl_->shutdown_requested;
    }

    bool TrackerPersistence::IsShutdownComplete() const noexcept
    {
        std::scoped_lock lock(impl_->mutex);
        return impl_->shutdown_complete;
    }
} // namespace TrackerAdvanced
