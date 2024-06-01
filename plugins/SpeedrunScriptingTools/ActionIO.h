#pragma once

#include <Action.h>
#include <memory>

std::shared_ptr<Action> readV8Action(InputStream& stream);
std::shared_ptr<Action> readAction(InputStream& stream);

std::shared_ptr<Action> drawActionSelector(float width);
std::string_view toString(ActionType type);
