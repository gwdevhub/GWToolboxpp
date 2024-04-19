#pragma once

#include <optional>
#include <sstream>

enum class Trigger{None, InstanceLoad, HardModePing, Hotkey, Count};

void drawTriggerSelector(Trigger& trigger, float width, long& hotkeyData, long& hotkeyMod);
std::istringstream& operator>>(std::istringstream& in, Trigger& trigger);
