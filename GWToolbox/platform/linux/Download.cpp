#include "Download.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <system_error>
#include <thread>

#include "Command.h"

namespace fs = std::filesystem;

namespace {

constexpr auto kDownloadTimeout = std::chrono::seconds(600);
constexpr auto kPollInterval = std::chrono::milliseconds(200);

}

int downloadWithProgress(const std::string& url, const fs::path& partial, const std::uintmax_t expected_size, const std::function<void(int)>& progress)
{
    std::atomic<bool> finished{false};
    std::thread poller([&] {
        int last = -1;
        while (!finished) {
            std::error_code ec;
            const std::uintmax_t size = fs::file_size(partial, ec);
            if (!ec && expected_size > 0) {
                const int percent = static_cast<int>(std::min<std::uintmax_t>(100, size * 100 / expected_size));
                if (percent != last) {
                    last = percent;
                    progress(percent);
                }
            }
            std::this_thread::sleep_for(kPollInterval);
        }
    });

    const CommandOutput download = runCommand({"curl", "-fsSL", "-o", partial.string(), url}, currentEnvironment(), kDownloadTimeout);
    finished = true;
    poller.join();
    return download.exit_code;
}
