#pragma once

#include <string_view>
#include <unordered_map>

const std::unordered_map<uint16_t, std::string_view>& getModelNames();
