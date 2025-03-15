#pragma once
#include <vector>
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
                case GW::Constants::MapID::Prophets_Path: {
                    m_teleports.push_back({{-8112.00f, 9118.00f, 0}, {-5076.00f, 6013.00f, 0}, Teleport::direction::both_ways});
                    m_teleports.push_back({{2297.00f, -1544.00f, 0}, {6379.00f, -668.00f, 0}, Teleport::direction::both_ways});
                    m_teleports.push_back({{6311.00f, 13050.00f, 0}, {9935.00f, 14937.00f, 0}, Teleport::direction::both_ways});
                    m_teleports.push_back({{-6843.00f, -10917.00f, 0}, {-4651.00f, -7494.00f, 0}, Teleport::direction::both_ways});
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
