#include "stdafx.h"
#include "MapNames.h"

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/GameEntities/Map.h>

namespace MapNames {

static std::unordered_map<int, std::string> s_cache;
static bool s_initialized = false;

struct DecodeParam { int map_id; };

static void OnDecoded(void* param, const wchar_t* decoded)
{
    auto* p = static_cast<DecodeParam*>(param);
    if (decoded && decoded[0]) {
        char buf[256] = {};
        WideCharToMultiByte(CP_UTF8, 0, decoded, -1, buf, sizeof(buf), nullptr, nullptr);
        if (buf[0]) s_cache[p->map_id] = buf;
    }
    delete p;
}

void Init()
{
    if (s_initialized) return;
    s_initialized = true;

    for (int id = 1; id < static_cast<int>(GW::Constants::MapID::Count); ++id) {
        const auto* info = GW::Map::GetMapInfo(static_cast<GW::Constants::MapID>(id));
        if (!info || !info->name_id) continue;

        wchar_t enc[8] = {};
        if (!GW::UI::UInt32ToEncStr(info->name_id, enc, _countof(enc))) continue;

        GW::UI::AsyncDecodeStr(enc, OnDecoded, new DecodeParam{id});
    }
}

const std::string& Get(GW::Constants::MapID id)
{
    auto it = s_cache.find(static_cast<int>(id));
    if (it != s_cache.end()) return it->second;

    static std::unordered_map<int, std::string> s_fallback;
    auto& fb = s_fallback[static_cast<int>(id)];
    if (fb.empty()) {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "Map %d", static_cast<int>(id));
        fb = tmp;
    }
    return fb;
}

std::vector<std::pair<int, std::string>> Filter(
    const char* query,
    std::initializer_list<GW::RegionType> types)
{
    std::string q = query ? query : "";
    for (char& c : q) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    std::vector<std::pair<int, std::string>> result;
    result.reserve(128);

    for (const auto& [id, name] : s_cache) {
        if (types.size() > 0) {
            const auto* info = GW::Map::GetMapInfo(static_cast<GW::Constants::MapID>(id));
            if (!info) continue;
            bool type_ok = false;
            for (const auto t : types) {
                if (info->type == t) { type_ok = true; break; }
            }
            if (!type_ok) continue;
        }

        if (q.empty()) {
            result.emplace_back(id, name);
        } else {
            std::string lower = name;
            for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (lower.find(q) != std::string::npos)
                result.emplace_back(id, name);
        }
    }

    std::sort(result.begin(), result.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    return result;
}

} // namespace MapNames
