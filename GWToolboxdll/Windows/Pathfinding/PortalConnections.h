#pragma once

#include <vector>
#include <string>
#include <GWCA/Constants/Maps.h>
#include <GWCA/GameContainers/GamePos.h>

namespace Pathing {

    enum class ConnectionType : uint32_t {
        Portal = 0,     // bidirectional walkable portal (green)
        Disabled,       // blocked connection (red, infinite cost)
        NPC,            // one-way: talk to NPC to teleport (blue)
        Dummy,          // one-way: arbitrary marker position (yellow)
    };

    // Types whose connections default to one-way when first created (position
    // markers / NPC teleports have no inherent return path). This only seeds the
    // one_way flag in the editor — actual directionality is the flag alone (see
    // IsOneWay), so a Dummy/NPC connection can still be made bidirectional.
    inline bool DefaultsToOneWay(ConnectionType t) {
        return t == ConnectionType::Dummy || t == ConnectionType::NPC;
    }

    // Kind of agent stored in an NPC endpoint. The "NPC" name is kept for the
    // ConnectionType enum and JSON for backwards compat, but the storage now
    // supports both AgentLiving (NPCs) and AgentGadget (signposts, statues, etc).
    enum class AgentKind : uint8_t {
        Living = 0,  // AgentLiving — model_id stores player_number
        Gadget = 1,  // AgentGadget — model_id stores gadget_id
    };

    // NPC endpoint data (used when endpoint type == NPC)
    struct NpcEndpointData {
        uint32_t agent_id = 0;   // runtime agent ID (current instance, unstable)
        uint32_t model_id = 0;   // stable type id: player_number (Living) or gadget_id (Gadget)
        std::string name;
        AgentKind agent_kind = AgentKind::Living; // default = backwards compat
    };

    struct PortalConnection {
        GW::Constants::MapID from_map = GW::Constants::MapID::None;
        GW::Vec2f from_pos{};       // game coords on from_map
        GW::Vec2f from_wm_pos{};    // world map coords
        ConnectionType from_type = ConnectionType::Portal;
        NpcEndpointData from_npc;

        GW::Constants::MapID to_map = GW::Constants::MapID::None;
        GW::Vec2f to_pos{};         // game coords on to_map
        GW::Vec2f to_wm_pos{};      // world map coords
        ConnectionType to_type = ConnectionType::Portal;
        NpcEndpointData to_npc;

        uint32_t campaign = 0;      // campaign/continent ID
        std::string notes;
        bool no_draw = false;      // don't draw connection line (e.g. underground maps)
        bool one_way = false;      // sole source of truth for directionality (false = bidirectional)

        bool IsOneWay() const {
            return one_way;
        }
    };

    class PortalConnections {
    public:
        void Add(const PortalConnection& conn);
        void Remove(size_t index);
        bool Save(const std::string& path) const;
        bool Load(const std::string& path);
        // Parse connections from an in-memory JSON buffer (e.g. an embedded resource).
        bool LoadFromMemory(const char* data, size_t size, const std::string& source = "<memory>");
        void Clear();

        const std::vector<PortalConnection>& GetAll() const { return connections; }
        size_t Count() const { return connections.size(); }

        // Get all connections involving a specific map
        std::vector<const PortalConnection*> GetConnectionsForMap(GW::Constants::MapID map) const;

        // Get connections between two specific maps
        std::vector<const PortalConnection*> GetConnectionsBetween(
            GW::Constants::MapID a, GW::Constants::MapID b) const;

        // Check if a connection with similar positions already exists
        bool HasConnectionAt(GW::Constants::MapID from, const GW::Vec2f& from_pos,
                            GW::Constants::MapID to, const GW::Vec2f& to_pos,
                            float threshold = 500.f) const;

        // Check if any connection exists between two maps (ignoring positions)
        bool HasConnection(GW::Constants::MapID from, GW::Constants::MapID to) const;

        // Cost multiplier for a connection type
        static float GetCostMultiplier(ConnectionType type);

    private:
        std::vector<PortalConnection> connections;
    };

} // namespace Pathing
