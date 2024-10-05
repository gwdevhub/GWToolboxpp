#pragma once

#include <Characteristic.h>
#include <memory>

CharacteristicPtr makeCharacteristic(CharacteristicType type);
CharacteristicPtr readCharacteristic(InputStream& stream);
CharacteristicPtr drawCharacteristicSelector(float width);
std::optional<CharacteristicType> drawCharacteristicSubMenu();
