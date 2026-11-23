#include "stdafx.h"

#include <bcrypt.h>

#include <RestClient.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <regex>
#include <thread>

#include "Releases.h"

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

std::string hashHex(const unsigned char* digest, const size_t length)
{
    std::string hex(length * 2, '\0');
    for (size_t i = 0; i < length; ++i) {
        sprintf_s(hex.data() + i * 2, 3, "%02x", digest[i]);
    }
    return hex;
}

std::string sha256OfFile(const std::filesystem::path& path)
{
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        return {};
    }

    std::string result;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) == 0) {
        std::ifstream file(path, std::ios::binary);
        bool ok = file.is_open();
        char buffer[64 * 1024];
        while (ok) {
            file.read(buffer, sizeof(buffer));
            const std::streamsize n = file.gcount();
            if (n <= 0) {
                break;
            }
            ok = BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer), static_cast<ULONG>(n), 0) == 0;
        }

        unsigned char digest[32];
        if (ok && file.eof() && !file.bad() && BCryptFinishHash(hash, digest, sizeof(digest), 0) == 0) {
            result = hashHex(digest, sizeof(digest));
        }
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return result;
}

std::string sha256OfContent(const std::string_view content)
{
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        return {};
    }

    std::string result;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) == 0) {
        auto remaining = content;
        bool ok = true;
        while (ok && !remaining.empty()) {
            const auto count = std::min<size_t>(remaining.size(), 64 * 1024);
            ok = BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(remaining.data())), static_cast<ULONG>(count), 0) == 0;
            remaining.remove_prefix(count);
        }

        unsigned char digest[32];
        if (ok && BCryptFinishHash(hash, digest, sizeof(digest), 0) == 0) {
            result = hashHex(digest, sizeof(digest));
        }
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return result;
}

bool sha256Matches(const std::string_view actual, const std::string_view expected)
{
    return std::ranges::equal(actual, expected, [](const char left, const char right) {
        return std::tolower(static_cast<unsigned char>(left)) == std::tolower(static_cast<unsigned char>(right));
    });
}

}

bool FetchReleases(std::vector<Release>& releases, std::string& error)
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

    constexpr glz::opts opts{.error_on_unknown_keys = false};
    if (const auto parse_error = glz::read<opts>(releases, client.GetContent()); parse_error) {
        error = "The GWToolbox release list couldn't be read.";
        return false;
    }
    return true;
}

FoundAsset FindNewestAsset(const std::vector<Release>& releases, const std::vector<std::string_view>& names)
{
    for (const Release& release : releases) {
        for (const Asset& asset : release.assets) {
            if (std::ranges::find(names, asset.name) != names.end()) {
                return {&release, &asset};
            }
        }
    }
    return {};
}

std::string Sha256FromDigest(const std::optional<std::string>& digest)
{
    constexpr std::string_view prefix = "sha256:";
    if (!digest || !digest->starts_with(prefix)) {
        return {};
    }
    const std::string hex = digest->substr(prefix.size());
    if (hex.size() != 64 || !std::ranges::all_of(hex, [](const char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; })) {
        return {};
    }
    return hex;
}

std::string VersionFromTag(const std::string& tag)
{
    return std::regex_replace(tag, std::regex(R"([^0-9.])"), "");
}

bool FileMatchesRelease(const std::filesystem::path& file, const std::uintmax_t size, const std::string& sha256)
{
    std::error_code ec;
    if (std::filesystem::file_size(file, ec) != size || ec || sha256.empty()) {
        return false;
    }
    return sha256Matches(sha256OfFile(file), sha256);
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
    if (!sha256Matches(sha256OfContent(content), sha256)) {
        error = "The download's SHA-256 digest didn't match the GitHub release metadata.";
        return false;
    }
    progress(100);
    return true;
}
