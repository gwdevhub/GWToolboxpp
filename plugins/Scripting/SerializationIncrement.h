#pragma once

#include <io.h>
#include <Action.h>
#include <Condition.h>

#include <memory>

ActionPtr readV10Action(InputStream&);
ConditionPtr readV10Condition(InputStream&);
