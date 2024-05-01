#pragma once

#include <Condition.h>
#include <memory>

std::shared_ptr<Condition> readCondition(InputStream& stream);
std::shared_ptr<Condition> drawConditionSelector(float width);
