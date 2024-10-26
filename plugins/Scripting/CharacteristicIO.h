#pragma once

#include <Characteristic.h>

#include <memory>
#include <unordered_set>

CharacteristicPtr makeCharacteristic(CharacteristicType type);
CharacteristicPtr readCharacteristic(InputStream& stream);
CharacteristicPtr drawCharacteristicSelector(float width);
std::optional<CharacteristicType> drawCharacteristicSubMenu(std::unordered_set<CharacteristicType> toSkip = {});
