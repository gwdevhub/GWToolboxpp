#include "stdafx.h"
#include "PortalConnections.h"

#include <fstream>
#include <sstream>

#include <glaze/glaze.hpp>

#include <Logger.h>
#include <Utils/TextUtils.h>
#include "PathingLog.h"

namespace Pathing {

    void PortalConnections::Add(const PortalConnection& conn)
    {
        connections.push_back(conn);
    }

    void PortalConnections::Remove(size_t index)
    {
        if (index < connections.size()) {
            connections.erase(connections.begin() + index);
        }
    }

    void PortalConnections::Clear()
    {
        connections.clear();
    }

    static glz::generic NpcToJson(const NpcEndpointData& npc)
    {
        glz::generic j;
        j["agent_id"]   = static_cast<double>(npc.agent_id);
        j["model_id"]   = static_cast<double>(npc.model_id);
        if (!npc.name.empty()) j["name"] = npc.name;
        j["agent_kind"] = static_cast<double>(static_cast<uint8_t>(npc.agent_kind));
        return j;
    }

    static bool SamePos(const GW::Vec2f& a, const GW::Vec2f& b)
    {
        return a.x == b.x && a.y == b.y;
    }

    static bool SameNpc(const NpcEndpointData& a, const NpcEndpointData& b)
    {
        return a.agent_id == b.agent_id && a.model_id == b.model_id
            && a.name == b.name && a.agent_kind == b.agent_kind;
    }

    // True when b is the exact mirror of a in every field. Only plain
    // bidirectional connections qualify, so collapsing one half on save and
    // re-synthesising it on load is lossless.
    static bool IsExactReverse(const PortalConnection& a, const PortalConnection& b)
    {
        if (a.IsOneWay() || b.IsOneWay()) return false;
        return a.from_map == b.to_map && a.to_map == b.from_map
            && SamePos(a.from_pos, b.to_pos) && SamePos(a.to_pos, b.from_pos)
            && SamePos(a.from_wm_pos, b.to_wm_pos) && SamePos(a.to_wm_pos, b.from_wm_pos)
            && a.from_type == b.to_type && a.to_type == b.from_type
            && a.campaign == b.campaign && a.no_draw == b.no_draw && a.notes == b.notes
            && SameNpc(a.from_npc, b.to_npc) && SameNpc(a.to_npc, b.from_npc);
    }

    static PortalConnection MakeReverse(const PortalConnection& c)
    {
        PortalConnection r;
        r.from_map = c.to_map;   r.from_pos = c.to_pos;   r.from_wm_pos = c.to_wm_pos;
        r.from_type = c.to_type; r.from_npc = c.to_npc;
        r.to_map = c.from_map;   r.to_pos = c.from_pos;   r.to_wm_pos = c.from_wm_pos;
        r.to_type = c.from_type; r.to_npc = c.from_npc;
        r.campaign = c.campaign; r.notes = c.notes;       r.no_draw = c.no_draw;
        r.one_way = false;
        return r;
    }

    static NpcEndpointData NpcFromJson(const glz::generic& j)
    {
        NpcEndpointData d;
        d.agent_id   = static_cast<uint32_t>(TextUtils::parseUint64FromJson(j, "agent_id", 0u));
        d.model_id   = static_cast<uint32_t>(TextUtils::parseUint64FromJson(j, "model_id", 0u));
        d.name       = TextUtils::parseStringFromJson(j, "name", {});
        d.agent_kind = static_cast<AgentKind>(
            TextUtils::parseIntFromJson(j, "agent_kind", static_cast<int>(AgentKind::Living)));
        return d;
    }

    bool PortalConnections::Save(const std::string& path) const
    {
        try {
            glz::generic j;
            glz::generic::array_t arr;
            arr.reserve(connections.size());
            std::vector<const PortalConnection*> emitted;
            for (const auto& c : connections) {
                // Bidirectional pairs are stored once: skip c if its exact
                // mirror was already written. Load re-synthesises the reverse.
                if (!c.IsOneWay()) {
                    bool dup = false;
                    for (const auto* e : emitted) {
                        if (IsExactReverse(*e, c)) { dup = true; break; }
                    }
                    if (dup) continue;
                }
                glz::generic entry;
                entry["from_map"]   = static_cast<double>(static_cast<uint32_t>(c.from_map));
                entry["from_x"]     = static_cast<double>(c.from_pos.x);
                entry["from_y"]     = static_cast<double>(c.from_pos.y);
                entry["from_wm_x"]  = static_cast<double>(c.from_wm_pos.x);
                entry["from_wm_y"]  = static_cast<double>(c.from_wm_pos.y);
                if (c.from_type != ConnectionType::Portal)
                    entry["from_type"] = static_cast<double>(static_cast<uint32_t>(c.from_type));
                entry["to_map"]     = static_cast<double>(static_cast<uint32_t>(c.to_map));
                entry["to_x"]       = static_cast<double>(c.to_pos.x);
                entry["to_y"]       = static_cast<double>(c.to_pos.y);
                entry["to_wm_x"]    = static_cast<double>(c.to_wm_pos.x);
                entry["to_wm_y"]    = static_cast<double>(c.to_wm_pos.y);
                if (c.to_type != ConnectionType::Portal)
                    entry["to_type"] = static_cast<double>(static_cast<uint32_t>(c.to_type));
                entry["campaign"]   = static_cast<double>(c.campaign);
                if (!c.notes.empty()) entry["notes"]   = c.notes;
                if (c.no_draw)        entry["no_draw"]  = c.no_draw;
                if (c.one_way)        entry["one_way"]  = c.one_way;
                if (c.from_type == ConnectionType::NPC) entry["from_npc"] = NpcToJson(c.from_npc);
                if (c.to_type   == ConnectionType::NPC) entry["to_npc"]   = NpcToJson(c.to_npc);
                arr.emplace_back(std::move(entry));
                emitted.push_back(&c);
            }
            const size_t written = arr.size();
            j["connections"] = std::move(arr);

            auto str = glz::write<glz::opts{.prettify = true}>(j).value_or(std::string{});
            std::ofstream f(path);
            if (!f.is_open()) return false;
            f << str;
            PATH_LOG_NOTICE("Saved %d portal connections to %s", (int)written, path.c_str());
            return true;
        }
        catch (...) {
            PATH_LOG_ERROR("Failed to save portal connections");
            return false;
        }
    }

    bool PortalConnections::Load(const std::string& path)
    {
        std::ifstream f(path);
        if (!f.is_open()) {
            PATH_LOG_ERROR("Could not open portal connections file (%s)",
                path.empty() ? "<empty path>" : path.c_str());
            return false;
        }
        std::ostringstream oss;
        oss << f.rdbuf();
        const std::string buf = oss.str();
        return LoadFromMemory(buf.data(), buf.size(), path);
    }

    bool PortalConnections::LoadFromMemory(const char* data, size_t size, const std::string& source)
    {
        try {
            const std::string buf(data, size);

            glz::generic j;
            if (auto ec = glz::read_json(j, buf); ec) {
                PATH_LOG_ERROR("Failed to parse portal connections JSON (%s)", source.c_str());
                return false;
            }
            if (!j.contains("connections") || !j.at("connections").is_array()) {
                connections.clear();
                PATH_LOG_NOTICE("Portal connections file has no \"connections\" array (%s)", source.c_str());
                return true;
            }

            connections.clear();
            const auto& arr = j.at("connections").get_array();
            for (const auto& entry : arr) {
                PortalConnection c;
                c.from_map    = static_cast<GW::Constants::MapID>(
                    TextUtils::parseUint64FromJson(entry, "from_map", 0u));
                c.from_pos    = { TextUtils::parseFloatFromJson(entry, "from_x", 0.f),
                                  TextUtils::parseFloatFromJson(entry, "from_y", 0.f) };
                c.from_wm_pos = { TextUtils::parseFloatFromJson(entry, "from_wm_x", 0.f),
                                  TextUtils::parseFloatFromJson(entry, "from_wm_y", 0.f) };
                c.to_map      = static_cast<GW::Constants::MapID>(
                    TextUtils::parseUint64FromJson(entry, "to_map", 0u));
                c.to_pos      = { TextUtils::parseFloatFromJson(entry, "to_x", 0.f),
                                  TextUtils::parseFloatFromJson(entry, "to_y", 0.f) };
                c.to_wm_pos   = { TextUtils::parseFloatFromJson(entry, "to_wm_x", 0.f),
                                  TextUtils::parseFloatFromJson(entry, "to_wm_y", 0.f) };
                c.campaign    = static_cast<uint32_t>(
                    TextUtils::parseUint64FromJson(entry, "campaign", 0u));
                c.notes       = TextUtils::parseStringFromJson(entry, "notes", {});
                c.no_draw     = TextUtils::parseBoolFromJson(entry, "no_draw", false);
                c.one_way     = TextUtils::parseBoolFromJson(entry, "one_way", false);

                // Per-endpoint types. Either key may be omitted (defaults to
                // Portal=0). Backward compat: pre-split files carry one "type".
                if (entry.contains("from_type") || entry.contains("to_type")) {
                    c.from_type = static_cast<ConnectionType>(
                        TextUtils::parseIntFromJson(entry, "from_type", 0));
                    c.to_type   = static_cast<ConnectionType>(
                        TextUtils::parseIntFromJson(entry, "to_type", 0));
                } else {
                    auto t = static_cast<ConnectionType>(
                        TextUtils::parseIntFromJson(entry, "type", 0));
                    c.from_type = t;
                    c.to_type   = t;
                }

                if (entry.contains("from_npc")) c.from_npc = NpcFromJson(entry.at("from_npc"));
                if (entry.contains("to_npc"))   c.to_npc   = NpcFromJson(entry.at("to_npc"));

                // Backward compat: old flat NPC fields → from_npc
                if (!entry.contains("from_npc") && !entry.contains("to_npc")
                    && entry.contains("npc_agent_id")) {
                    c.from_npc.agent_id = static_cast<uint32_t>(
                        TextUtils::parseUint64FromJson(entry, "npc_agent_id", 0u));
                    c.from_npc.model_id = static_cast<uint32_t>(
                        TextUtils::parseUint64FromJson(entry, "npc_model_id", 0u));
                    c.from_npc.name = TextUtils::parseStringFromJson(entry, "npc_name", {});
                }

                connections.push_back(c);
            }

            // Bidirectional connections are stored once; materialise the reverse
            // edge for any plain connection whose mirror isn't already present.
            // NOTE: 5 source links (mixed Portal/NPC around map 625, e.g. 553->625 type 0/2) are authored single-direction, so the in-memory count exceeds the file's by 5. Harmless (pathfinder already treats non-one-way as bidirectional) — flagged for data cleanup.
            const size_t loaded = connections.size();
            for (size_t i = 0; i < loaded; ++i) {
                if (connections[i].IsOneWay()) continue;
                bool has_reverse = false;
                for (size_t k = 0; k < connections.size(); ++k) {
                    if (k != i && IsExactReverse(connections[i], connections[k])) {
                        has_reverse = true;
                        break;
                    }
                }
                if (!has_reverse) connections.push_back(MakeReverse(connections[i]));
            }
            PATH_LOG_NOTICE("Loaded %d portal connections from %s", (int)connections.size(), source.c_str());
            return true;
        }
        catch (...) {
            PATH_LOG_ERROR("Failed to load portal connections from %s", source.c_str());
            return false;
        }
    }

    std::vector<const PortalConnection*> PortalConnections::GetConnectionsForMap(
        GW::Constants::MapID map) const
    {
        std::vector<const PortalConnection*> result;
        for (const auto& c : connections) {
            if (c.from_map == map || c.to_map == map) {
                result.push_back(&c);
            }
        }
        return result;
    }

    std::vector<const PortalConnection*> PortalConnections::GetConnectionsBetween(
        GW::Constants::MapID a, GW::Constants::MapID b) const
    {
        std::vector<const PortalConnection*> result;
        for (const auto& c : connections) {
            if ((c.from_map == a && c.to_map == b) || (c.from_map == b && c.to_map == a)) {
                result.push_back(&c);
            }
        }
        return result;
    }

    bool PortalConnections::HasConnectionAt(GW::Constants::MapID from, const GW::Vec2f& from_pos,
                                            GW::Constants::MapID to, const GW::Vec2f& to_pos,
                                            float threshold) const
    {
        float thresh_sq = threshold * threshold;
        for (const auto& c : connections) {
            bool maps_match = (c.from_map == from && c.to_map == to) ||
                              (c.from_map == to && c.to_map == from);
            if (!maps_match) continue;

            const auto& cf = (c.from_map == from) ? c.from_pos : c.to_pos;
            const auto& ct = (c.from_map == from) ? c.to_pos : c.from_pos;
            float df = (cf.x - from_pos.x) * (cf.x - from_pos.x) + (cf.y - from_pos.y) * (cf.y - from_pos.y);
            float dt = (ct.x - to_pos.x) * (ct.x - to_pos.x) + (ct.y - to_pos.y) * (ct.y - to_pos.y);
            if (df < thresh_sq && dt < thresh_sq) return true;
        }
        return false;
    }

    bool PortalConnections::HasConnection(GW::Constants::MapID from, GW::Constants::MapID to) const
    {
        for (const auto& c : connections) {
            if ((c.from_map == from && c.to_map == to) || (c.from_map == to && c.to_map == from))
                return true;
        }
        return false;
    }

    float PortalConnections::GetCostMultiplier(ConnectionType type)
    {
        switch (type) {
            case ConnectionType::Portal:    return 1.0f;
            case ConnectionType::Disabled:  return 1000000.f;
            case ConnectionType::NPC:       return 1.0f;
            case ConnectionType::Dummy:     return 1.0f;
            default: return 1.0f;
        }
    }

} // namespace Pathing
