#pragma once

#include <optional>
#include <sstream>

enum class TriggerPacket{None, InstanceLoad, CastSpell, Count};

void drawTriggerPacketSelector(TriggerPacket& trigger, float width);
std::istringstream& operator>>(std::istringstream& in, TriggerPacket& packet);
