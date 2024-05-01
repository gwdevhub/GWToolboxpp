#pragma once

#include <utils.h>

enum class Trigger{None, InstanceLoad, HardModePing, Hotkey, Count};

void drawTriggerSelector(Trigger& trigger, float width, long& hotkeyData, long& hotkeyMod);
InputStream& operator>>(InputStream& in, Trigger& trigger);
