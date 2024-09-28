#pragma once

#include <io.h>
#include <Action.h>
#include <Condition.h>

#include <memory>

ActionPtr readV8Action(InputStream&);
ConditionPtr readV8Condition(InputStream&);
