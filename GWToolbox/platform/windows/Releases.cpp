#include "stdafx.h"

#include <RestClient.h>

#include <algorithm>
#include <chrono>
#include <thread>

#include "Releases.h"
#include "Sha256.h"

namespace {

constexpr auto kReleasesUrl = "https://api.github.com/repos/gwdevhub/GWToolboxpp/releases?per_page=30";
constexpr auto kDownloadPoll = std::chrono::milliseconds(50);

class AsyncFileDownloader : public AsyncRestClient {
public:
    AsyncFileDownloader() : m_DownloadLength{0} {}

    AsyncFileDownloader(const AsyncFileDownloader&) = delete;

    size_t GetDownloadCount() const { return m_DownloadLength.load(std::memory_order_relaxed); }

private:
    void OnContent(const char* bytes, const size_t count) override
    {
        AsyncRestClient::OnContent(bytes, count);
        m_DownloadLength += count;
    }

    std::atomic<size_t> m_DownloadLength;
};

}

bool FetchReleaseListing(std::string& listing, std::string& error)
{
    RestClient client;
    client.SetUrl(kReleasesUrl);
    client.SetFollowLocation(true);
    client.SetVerifyPeer(true);
    client.SetTimeoutSec(5);
    client.SetUserAgent("curl/7.71.1");
    client.Execute();
    if (!client.IsSuccessful()) {
        error = std::format("The GWToolbox release list couldn't be downloaded (status {}).", client.GetStatusCode());
        return false;
    }
    listing = client.GetContent();
    return true;
}

bool FileMatchesRelease(const std::filesystem::path& file, const std::uintmax_t size, const std::string& sha256)
{
    std::error_code ec;
    if (std::filesystem::file_size(file, ec) != size || ec || sha256.empty()) {
        return false;
    }
    return Sha256OfFile(file) == sha256;
}

bool DownloadVerified(const std::string& url, const std::uintmax_t size, const std::string& sha256, const std::function<void(int)>& progress, std::string& content, std::string& error)
{
    if (sha256.empty()) {
        error = "GitHub did not provide a SHA-256 digest for this download.";
        return false;
    }

    AsyncFileDownloader downloader;
    downloader.SetUrl(url.c_str());
    downloader.SetVerifyPeer(true);
    downloader.SetFollowLocation(true);
    downloader.SetUserAgent("curl/7.71.1");
    downloader.ExecuteAsync();
    while (!downloader.IsCompleted()) {
        if (size > 0) {
            progress(static_cast<int>(std::min<std::uintmax_t>(100, downloader.GetDownloadCount() * 100 / size)));
        }
        std::this_thread::sleep_for(kDownloadPoll);
    }

    if (!downloader.IsSuccessful()) {
        error = std::format("Failed to download the update (status {}).", downloader.GetStatusCode());
        return false;
    }

    content = std::move(downloader.GetContent());
    if (Sha256Hex(content) != sha256) {
        error = "The download's SHA-256 digest didn't match the GitHub release metadata.";
        return false;
    }
    progress(100);
    return true;
}
