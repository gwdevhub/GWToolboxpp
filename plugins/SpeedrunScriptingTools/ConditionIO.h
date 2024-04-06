#pragma once

#include <Condition.h>
#include <memory>

std::shared_ptr<Condition> readCondition(std::istringstream& stream);
std::shared_ptr<Condition> drawConditionSelector(float width);
