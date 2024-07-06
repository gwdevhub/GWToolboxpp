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
