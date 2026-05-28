#pragma once

#include <cstdint>
#include <filesystem>
#include <unordered_map>

namespace HealthLogCache {
    void Load();
    const std::unordered_map<uint32_t, uint32_t>& GetMap();
    bool TryGetMaxHp(uint32_t player_number, uint32_t& max_hp_out);
    const std::filesystem::path& GetPath();
}
