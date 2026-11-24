#include "DllDownload.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <optional>
#include <regex>
#include <string>
#include <string_view>

#include "Messages.h"

namespace {

std::optional<Json> matchingAsset(const Json& release, const std::function<bool(const std::string&)>& isAsset)
{
    if (!release.contains("assets")) {
        return std::nullopt;
    }
    for (const Json& asset : release["assets"]) {
        if (isAsset(asset.value("name", std::string()))) {
            return asset;
        }
    }
    return std::nullopt;
}

std::string stringField(const Json& object, const char* key)
{
    return object.contains(key) && object[key].is_string() ? object[key].get<std::string>() : std::string();
}

bool isDllAsset(const std::string& name)
{
    return name == "GWToolboxdll.dll" || name == "GWToolbox.dll";
}

}

std::string Sha256FromDigest(const std::string& digest)
{
    constexpr std::string_view prefix = "sha256:";
    if (!digest.starts_with(prefix)) {
        return {};
    }
    std::string hex = digest.substr(prefix.size());
    if (hex.size() != 64 || !std::ranges::all_of(hex, [](const char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; })) {
        return {};
    }
    std::ranges::transform(hex, hex.begin(), [](const char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    return hex;
}

bool ReleaseFromListing(const std::string& listing, const std::function<bool(const std::string&)>& isAsset, const std::string& missing, DllRelease& release, std::string& error)
{
    const Json releases = Json::parse(listing, nullptr, false);
    if (!releases.is_array()) {
        error = "The GWToolbox release list couldn't be read.";
        return false;
    }
    for (const Json& candidate : releases) {
        const auto asset = matchingAsset(candidate, isAsset);
        if (candidate.value("draft", false) || !asset) {
            continue;
        }
        const std::string tag = stringField(candidate, "tag_name");
        release.version = std::regex_replace(tag, std::regex(R"([^0-9.])"), "");
        release.changelog = "GWToolbox " + tag + "\n\n" + stringField(candidate, "body");
        release.url = stringField(*asset, "browser_download_url");
        release.size = asset->value("size", std::uintmax_t(0));
        release.sha256 = Sha256FromDigest(stringField(*asset, "digest"));
        if (release.url.empty()) {
            error = "The release has no download address.";
            return false;
        }
        return true;
    }
    error = missing;
    return false;
}

bool DllReleaseFromListing(const std::string& listing, DllRelease& release, std::string& error)
{
    return ReleaseFromListing(listing, isDllAsset, "No GWToolboxdll.dll was found in the GWToolbox releases.", release, error);
}
