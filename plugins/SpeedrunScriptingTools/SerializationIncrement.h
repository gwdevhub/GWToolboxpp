#pragma once

#include <io.h>
#include <Action.h>
#include <Condition.h>

#include <memory>

std::shared_ptr<Action> readV8Action(InputStream&);
std::shared_ptr<Condition> readV8Condition(InputStream&);
