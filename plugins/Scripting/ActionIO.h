#pragma once

#include <Action.h>
#include <memory>

ActionPtr readAction(InputStream& stream);

ActionPtr drawActionSelector(float width);
std::string_view toString(ActionType type);
