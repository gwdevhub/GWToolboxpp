#pragma once

#include <Characteristic.h>
#include <memory>

CharacteristicPtr readCharacteristic(InputStream& stream);
CharacteristicPtr drawCharacteristicSelector(float width);
