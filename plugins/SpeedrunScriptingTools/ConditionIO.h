#pragma once

#include <Condition.h>
#include <memory>

ConditionPtr readCondition(InputStream& stream);
ConditionPtr drawConditionSelector(float width);
