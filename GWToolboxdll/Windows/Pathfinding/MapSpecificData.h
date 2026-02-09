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
				case GW::Constants::MapID::Diviners_Ascent: {
					m_teleports.push_back({ { -14192.00f, 19497.00f, 17 }, { -9624.00f, 19097.00f, 18 }, Teleport::direction::both_ways });
					m_teleports.push_back({ { 541.00f, 2562.00f, 23 }, { 423.00f, -2505.00f, 22 }, Teleport::direction::both_ways });
				} break;

                case GW::Constants::MapID::Prophets_Path: {
					m_teleports.push_back({ {-6828.00f, -10891.00f, 9}, {-4630.00f, -7482.00f, 8}, Teleport::direction::both_ways });
					m_teleports.push_back({ {6465.00f, -782.00f, 6}, {2315.00f, -1533.00f, 7}, Teleport::direction::both_ways });
					m_teleports.push_back({ {-8113.00f, 9181.00f, 11}, {-5077.00f, 6024.00f, 5}, Teleport::direction::both_ways });
					m_teleports.push_back({ {9929.00f, 14956.00f, 13}, {6330.00f, 13056.00f, 4}, Teleport::direction::both_ways });
                } break;

				case GW::Constants::MapID::The_Scar: {
					m_teleports.push_back({ { 5807.00f, -8777.00f, 9 }, { 3426.00f, -4641.00f, 10 }, Teleport::direction::both_ways });
					m_teleports.push_back({ { 15973.00f, -12318.00f, 12 }, { 15606.00f, -8047.00f, 13 }, Teleport::direction::both_ways });
					m_teleports.push_back({ { -7003.00f, 11503.00f, 14 }, { -11239.00f, 10515.00f, 16 }, Teleport::direction::both_ways });
					m_teleports.push_back({ { -10700.00f, -15141.00f, 17 }, { -9460.00f, -12226.00f, 19 }, Teleport::direction::both_ways });
					m_teleports.push_back({ { -9545.00f, -4615.00f, 15 }, { -5002.00f, -6079.00f, 11 }, Teleport::direction::both_ways });
				} break;

				case GW::Constants::MapID::Salt_Flats: {
                    m_teleports.push_back({ { -14618.00f, -1157.00f, 11 }, { -18391.00f, 213.00f, 4 }, Teleport::direction::both_ways});
                    m_teleports.push_back({ { -1455.00f, 7539.00f, 54 }, { -3326.00f, 3859.00f, 55 }, Teleport::direction::both_ways});
                    m_teleports.push_back({ { 14421.00f, 19508.00f, 23 }, { 10851.00f, 18902.00f, 24 }, Teleport::direction::both_ways});
				} break;

				case GW::Constants::MapID::Skyward_Reach: {
					m_teleports.push_back({ { -6075.00f, -7864.00f, 6 }, { -9354.00f, -11209.00f, 7 }, Teleport::direction::both_ways });
					m_teleports.push_back({ { 9891.00f, -14560.00f, 17 }, { 7069.00f, -11204.00f, 18 }, Teleport::direction::both_ways });
					m_teleports.push_back({ { 4416.00f, 17926.00f, 1 }, { 1410.00f, 15063.00f, 14 }, Teleport::direction::both_ways });
					m_teleports.push_back({ { 13219.00f, -1557.00f, 15 }, { 12171.00f, 3409.00f, 16 }, Teleport::direction::both_ways });
				} break;

				case GW::Constants::MapID::Vulture_Drifts: {
					m_teleports.push_back({ { -11338.00f, 661.00f, 9 }, { -14466.00f, 2419.00f, 10 }, Teleport::direction::both_ways });
					m_teleports.push_back({ { 2365.00f, -9348.00f, 11 }, { -1686.00f, -9896.00f, 12 }, Teleport::direction::both_ways });
					m_teleports.push_back({ { 9544.00f, 319.00f, 17 }, { 12384.00f, 2683.00f, 18 }, Teleport::direction::both_ways });
                } break;
            }
        }

    public:
        Teleports m_teleports;
    };
}
