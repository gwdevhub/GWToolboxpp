#pragma once

#include <Enums.h>

#include <commonIncludes.h>
#include <unordered_map>
#include <string>
#include <optional>

class ScriptVariableManager {
public:
    static ScriptVariableManager& getInstance()
    {
        static ScriptVariableManager info;
        return info;
    }

    void set(const std::string& name, int value) { variables[name] = value; }
    std::optional<int> get(const std::string& name) const 
    {
        if (variables.contains(name)) 
        {
            return variables.at(name);
        }
        return std::nullopt;
    }
    void clear() { variables.clear(); }

private:
    ScriptVariableManager() = default;
    ScriptVariableManager(const ScriptVariableManager&) = delete;
    ScriptVariableManager(ScriptVariableManager&&) = delete;

    std::unordered_map<std::string, int> variables;
};
