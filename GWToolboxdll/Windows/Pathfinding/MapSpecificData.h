#pragma once
#include <vector>
#include <string>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>

namespace MapSpecific {    
    class Teleport {
    public:
        enum class direction { one_way = 0, both_ways };
    
        Teleport(const GW::GamePos &enter, const GW::GamePos &exit, Teleport::direction directionality) :
            m_enter(enter), m_exit(exit), m_directionality(directionality) {}
    
        GW::GamePos m_enter, m_exit;
        direction m_directionality;
    };
    typedef std::vector<Teleport> Teleports;
    
    typedef struct {
        Teleport *tp1;
        Teleport *tp2;
        float distance;
    } teleport_node;
        
    class MapSpecificData {
    public:
        MapSpecificData() {};    
        MapSpecificData(GW::Constants::MapID id)
        {
            switch (id) {
                case GW::Constants::MapID::Salt_Flats: {
                    m_teleports.push_back({{-14614.00f, -1161.00f, 11}, {-18387.00f, 214.00f, 4}, Teleport::direction::both_ways});
                    m_teleports.push_back({{-1438.00f, 7532.00f, 54}, {-3299.00f, 3869.00f, 55}, Teleport::direction::both_ways});
                    m_teleports.push_back({{14437.00f, 19515.00f, 23}, {10846.00f, 18889.00f, 24}, Teleport::direction::both_ways});
                }
                case GW::Constants::MapID::Prophets_Path: {
                    m_teleports.push_back({{-6828.00f, -10891.00f, 9}, {-4630.00f, -7482.00f, 8}, Teleport::direction::both_ways});
                    m_teleports.push_back({{6465.00f, -782.00f, 6}, {2315.00f, -1533.00f, 7}, Teleport::direction::both_ways});
                    m_teleports.push_back({{-8113.00f, 9181.00f, 11}, {-5077.00f, 6024.00f, 5}, Teleport::direction::both_ways});
                    m_teleports.push_back({{9929.00f, 14956.00f, 13}, {6330.00f, 13056.00f, 4}, Teleport::direction::both_ways});
                } break;
                case GW::Constants::MapID::Isle_of_Jade:
                case GW::Constants::MapID::Isle_of_Jade_mission:
                case GW::Constants::MapID::Isle_of_Jade_outpost: {
                    m_teleports.push_back({{6796.00f, 735.00f, 12}, {2465.00f, 803.00f, 28}, Teleport::direction::both_ways}); // entrance / exit
                    m_teleports.push_back({{-3710.00f, 674.00f, 5}, {596.00f, 709.00f, 26}, Teleport::direction::both_ways});  // entrance / exit
                } break;
            }
        }

    public:
        Teleports m_teleports;
    };
}
