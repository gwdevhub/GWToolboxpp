#include "stdafx.h"

#include <ToolboxIni.h>
#include <Modules/Resources.h>
#include <Utils/TextUtils.h>
#include <Widgets/HealthLogCache.h>

namespace {
    constexpr const wchar_t* HEALTHLOG_INI_FILENAME = L"healthlog.ini";
    constexpr const char* HEALTHLOG_INI_SECTION = "health";

    ToolboxIni* healthlog_ini = nullptr;
    std::unordered_map<uint32_t, uint32_t> healthlog_max_hp;
    bool healthlog_loaded = false;
    std::filesystem::path healthlog_path;

    std::vector<std::filesystem::path> GetHealthLogCandidates()
    {
        const auto primary_path = Resources::GetSettingFile(HEALTHLOG_INI_FILENAME);
        const auto fallback_path = Resources::GetPath(HEALTHLOG_INI_FILENAME);
        const auto cwd_path = std::filesystem::current_path() / HEALTHLOG_INI_FILENAME;
        const auto exe_path = Resources::GetExePath().parent_path() / HEALTHLOG_INI_FILENAME;
        return {primary_path, fallback_path, cwd_path, exe_path};
    }
}

namespace HealthLogCache {
    void Load()
    {
        if (healthlog_loaded) {
            return;
        }
        healthlog_loaded = true;

        if (healthlog_ini == nullptr) {
            healthlog_ini = new ToolboxIni(false, false, false);
        }

        const auto candidates = GetHealthLogCandidates();
        const auto fallback_path = Resources::GetPath(HEALTHLOG_INI_FILENAME);
        std::filesystem::path path;
        for (const auto& candidate : candidates) {
            if (std::filesystem::exists(candidate)) {
                path = candidate;
                break;
            }
        }
        healthlog_path = path;
        if (path.empty()) {
            return;
        }

        healthlog_max_hp.clear();
        if (healthlog_ini->LoadFile(path) != SI_OK) {
            return;
        }

        TNamesDepend keys;
        healthlog_ini->GetAllKeys(HEALTHLOG_INI_SECTION, keys);
        if (keys.empty() && path != fallback_path && std::filesystem::exists(fallback_path)) {
            healthlog_ini->LoadFile(fallback_path);
            healthlog_ini->GetAllKeys(HEALTHLOG_INI_SECTION, keys);
            healthlog_path = fallback_path;
        }

        for (const auto& key : keys) {
            int id = 0;
            if (!TextUtils::ParseInt(key.pItem, &id) || id <= 0) {
                continue;
            }
            const long hp = healthlog_ini->GetLongValue(HEALTHLOG_INI_SECTION, key.pItem, 0);
            if (hp > 0) {
                healthlog_max_hp[static_cast<uint32_t>(id)] = static_cast<uint32_t>(hp);
            }
        }
    }

    const std::unordered_map<uint32_t, uint32_t>& GetMap()
    {
        Load();
        return healthlog_max_hp;
    }

    bool TryGetMaxHp(uint32_t player_number, uint32_t& max_hp_out)
    {
        Load();
        const auto it = healthlog_max_hp.find(player_number);
        if (it == healthlog_max_hp.end()) {
            return false;
        }
        max_hp_out = it->second;
        return true;
    }

    const std::filesystem::path& GetPath()
    {
        return healthlog_path;
    }
}
