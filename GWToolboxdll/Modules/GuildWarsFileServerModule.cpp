#include "stdafx.h"

#include "GuildWarsFileServerModule.h"
#include "GwDatModule.h"

#include <condition_variable>
#include <stdexcept>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Utilities/Scanner.h>

#include <Utils/GuildWarsFileServer.h>

namespace {
    using State = GuildWarsFileServerModule::State;
    using File = GuildWarsFileServerModule::File;
    using FilePtr = GuildWarsFileServerModule::FilePtr;
    using Callback = GuildWarsFileServerModule::Callback;

    struct Request {
        State state = State::Pending;
        FilePtr file;
        std::string error;
        std::vector<Callback> callbacks;
        uint64_t last_used = 0;
    };

    struct Completion {
        Callback callback;
        FilePtr file;
        std::string error;
    };

    constexpr size_t maximum_cache_bytes = 128 * 1024 * 1024;
    constexpr size_t maximum_requests = 4096;
    std::mutex mutex;
    std::condition_variable_any wake;
    std::unordered_map<uint32_t, Request> requests;
    std::deque<uint32_t> pending;
    std::deque<uint32_t> native_pending;
    std::unordered_set<uint32_t> native_requested;
    std::deque<Completion> completions;
    std::jthread worker;
    std::atomic<bool> finished = true;
    bool accepting = false;
    size_t cache_bytes = 0;
    uint64_t usage_clock = 0;

    using SubmitFiles = uint32_t(__cdecl*)(uint32_t, const uint32_t*, uint32_t);
    SubmitFiles submit_files = nullptr;
    const uint32_t* downloads_disabled = nullptr;

    void RunWorker(std::stop_token stop)
    {
        WSADATA winsock{};
        const auto network_started = WSAStartup(MAKEWORD(2, 2), &winsock) == 0;
        auto revisions = std::unordered_map<uint32_t, uint32_t>{};
        auto downloaded = std::unordered_map<uint32_t, std::weak_ptr<const File>>{};
        while (!stop.stop_requested()) {
            uint32_t base_id;
            {
                auto lock = std::unique_lock(mutex);
                if (!wake.wait(lock, stop, [] { return !pending.empty(); }))
                    break;
                base_id = pending.front();
                pending.pop_front();
            }
            FilePtr file;
            std::string error;
            try {
                if (!network_started)
                    throw std::runtime_error("Could not initialize Winsock");
                if (revisions.empty()) {
                    std::vector<uint8_t> manifest;
                    // The client's reserved dependency-table alias pins downloads to its own asset revision.
                    if (!GwDatModule::ReadFile(0x1001, manifest))
                        throw std::runtime_error("The client's asset manifest is not available in Gw.dat");
                    revisions = GuildWarsFileServer::ParseManifest(manifest);
                }
                const auto found = revisions.find(base_id);
                const auto file_id = found == revisions.end() ? base_id : found->second;
                file = downloaded[file_id].lock();
                if (!file) {
                    auto result = std::make_shared<File>();
                    result->file_id = file_id;
                    result->data = GuildWarsFileServer::Download(file_id, stop);
                    file = std::move(result);
                    downloaded[file_id] = file;
                }
            }
            catch (const std::exception& exception) {
                error = exception.what();
            }
            {
                const auto lock = std::scoped_lock(mutex);
                if (stop.stop_requested())
                    break;
                auto& request = requests.at(base_id);
                request.file = file;
                request.error = error;
                request.state = file ? State::Ready : State::Failed;
                request.last_used = ++usage_clock;
                if (file)
                    cache_bytes += file->data.size();
                for (auto& callback : request.callbacks)
                    completions.push_back({std::move(callback), file, error});
                request.callbacks.clear();
                while (cache_bytes > maximum_cache_bytes) {
                    auto oldest = requests.end();
                    for (auto it = requests.begin(); it != requests.end(); ++it) {
                        if (it->first != base_id && it->second.state == State::Ready &&
                            (oldest == requests.end() || it->second.last_used < oldest->second.last_used))
                            oldest = it;
                    }
                    if (oldest == requests.end())
                        break;
                    cache_bytes -= oldest->second.file->data.size();
                    requests.erase(oldest);
                }
            }
            if (!error.empty())
                Log::Log("[file-server] File %u: %s", base_id, error.c_str());
        }
        if (network_started)
            WSACleanup();
        finished.store(true, std::memory_order_release);
    }
}

void GuildWarsFileServerModule::Initialize()
{
    ToolboxModule::Initialize();
    const auto assertion = GW::Scanner::FindAssertion("DnApi.cpp", "count && DownloadIsEnabled()", 0, 0);
    const auto function = assertion ? GW::Scanner::ToFunctionStart(assertion) : 0;
    const auto guard = function ? GW::Scanner::FindInRange("\x83\x3d????\x00", "xx????x", 2, function, function + 0x30) : 0;
    if (guard) {
        uintptr_t address = 0;
        memcpy(&address, reinterpret_cast<const void*>(guard), sizeof(address));
        if (GW::Scanner::IsValidPtr(address)) {
            downloads_disabled = reinterpret_cast<const uint32_t*>(address);
            submit_files = reinterpret_cast<SubmitFiles>(function);
        }
    }
    if (!submit_files)
        Log::Log("[file-server] Client download submission unavailable; standalone downloads remain enabled");
    {
        const auto lock = std::scoped_lock(mutex);
        accepting = true;
        finished.store(false, std::memory_order_release);
        worker = std::jthread(RunWorker);
    }
}

bool GuildWarsFileServerModule::FetchFile(uint32_t base_file_id, Callback callback)
{
    const auto lock = std::scoped_lock(mutex);
    if (!accepting || base_file_id <= 0x1fff || base_file_id >= 0x80000000)
        return false;
    auto found = requests.find(base_file_id);
    if (found == requests.end()) {
        if (requests.size() >= maximum_requests) {
            auto oldest = requests.end();
            for (auto it = requests.begin(); it != requests.end(); ++it) {
                if (it->second.state != State::Pending &&
                    (oldest == requests.end() || it->second.last_used < oldest->second.last_used))
                    oldest = it;
            }
            if (oldest == requests.end())
                return false;
            if (oldest->second.file)
                cache_bytes -= oldest->second.file->data.size();
            requests.erase(oldest);
        }
        found = requests.emplace(base_file_id, Request{}).first;
        pending.push_back(base_file_id);
        if (native_requested.insert(base_file_id).second)
            native_pending.push_back(base_file_id);
        wake.notify_one();
    }
    auto& request = found->second;
    request.last_used = ++usage_clock;
    if (callback) {
        if (request.state == State::Pending)
            request.callbacks.push_back(std::move(callback));
        else
            completions.push_back({std::move(callback), request.file, request.error});
    }
    return true;
}

GuildWarsFileServerModule::State GuildWarsFileServerModule::GetFile(uint32_t base_file_id, FilePtr& file)
{
    const auto lock = std::scoped_lock(mutex);
    file.reset();
    if (!accepting)
        return State::Failed;
    const auto found = requests.find(base_file_id);
    if (found == requests.end())
        return State::NotRequested;
    found->second.last_used = ++usage_clock;
    file = found->second.file;
    return found->second.state;
}

void GuildWarsFileServerModule::Update(float)
{
    std::deque<Completion> ready;
    std::vector<uint32_t> client_files;
    {
        const auto lock = std::scoped_lock(mutex);
        if (!accepting)
            return;
        for (size_t i = 0; i < 32 && !completions.empty(); ++i) {
            ready.push_back(std::move(completions.front()));
            completions.pop_front();
        }
        if (submit_files && downloads_disabled && !*downloads_disabled && GW::GetGameContext() && GW::Map::GetIsMapLoaded()) {
            for (size_t i = 0; i < 16 && !native_pending.empty(); ++i) {
                client_files.push_back(native_pending.front());
                native_pending.pop_front();
            }
        }
    }
    if (!client_files.empty())
        submit_files(static_cast<uint32_t>(client_files.size()), client_files.data(), 0);
    for (auto& completion : ready)
        completion.callback(std::move(completion.file), completion.error);
}

void GuildWarsFileServerModule::SignalTerminate()
{
    const auto lock = std::scoped_lock(mutex);
    accepting = false;
    worker.request_stop();
    wake.notify_all();
    pending.clear();
    native_pending.clear();
    completions.clear();
}

bool GuildWarsFileServerModule::CanTerminate()
{
    return finished.load(std::memory_order_acquire);
}

void GuildWarsFileServerModule::Terminate()
{
    if (worker.joinable())
        worker.join();
    {
        const auto lock = std::scoped_lock(mutex);
        requests.clear();
        native_requested.clear();
        cache_bytes = 0;
    }
    submit_files = nullptr;
    downloads_disabled = nullptr;
    ToolboxModule::Terminate();
}
