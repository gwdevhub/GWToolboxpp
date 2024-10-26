#pragma once

#include <Enums.h>

#include <commonIncludes.h>
#include <unordered_map>
#include <string>
#include <optional>

struct Variable 
{
    int value = 0;
    bool preserveThroughInstanceLoad = false;
};
class ScriptVariableManager {
public:
    static ScriptVariableManager& getInstance()
    {
        static ScriptVariableManager info;
        return info;
    }

    void set(const std::string& name, Variable var) { variables[name] = var; }
    std::optional<Variable> get(const std::string& name) const 
    {
        if (variables.contains(name)) 
        {
            return variables.at(name);
        }
        return std::nullopt;
    }
    void clear() { std::erase_if(variables, [](const auto& elem){return !elem.second.preserveThroughInstanceLoad;}); }

private:
    ScriptVariableManager() = default;
    ScriptVariableManager(const ScriptVariableManager&) = delete;
    ScriptVariableManager(ScriptVariableManager&&) = delete;

    std::unordered_map<std::string, Variable> variables;
};
