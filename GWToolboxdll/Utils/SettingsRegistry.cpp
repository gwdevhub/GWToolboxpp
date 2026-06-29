#include "stdafx.h"

#include <Logger.h>
#include <ToolboxIni.h>
#include <ToolboxModule.h>
#include <Utils/SettingsRegistry.h>

namespace {
    std::vector<SettingsRegistry::Entry> entries;

    std::string Prettify(const std::string_view key)
    {
        std::string label(key);
        std::ranges::replace(label, '_', ' ');
        if (!label.empty()) {
            label[0] = static_cast<char>(toupper(static_cast<unsigned char>(label[0])));
        }
        return label;
    }
} // namespace

namespace SettingsRegistry {
    void detail::AddEntry(ToolboxModule* module, const std::string_view key, const Type type, void* ptr, const bool from_struct, const std::string_view description)
    {
        const auto found = std::ranges::find_if(entries, [module, key](const Entry& e) {
            return e.module == module && e.key == key;
        });
        Entry& entry = found != entries.end() ? *found : entries.emplace_back();
        entry.module = module;
        entry.section = module->Name();
        entry.key = key;
        entry.label = Prettify(key);
        entry.description = description;
        entry.type = type;
        entry.ptr = ptr;
        entry.from_struct = from_struct;
    }

    void detail::LogUnsupportedMember(ToolboxModule* module, const std::string_view key)
    {
        Log::Log("SettingsRegistry: no entry for unsupported member type %s.%.*s", module->Name(), static_cast<int>(key.size()), key.data());
    }

    void RegisterField(ToolboxModule* module, const std::string_view key, bool* ptr, const std::string_view description)
    {
        detail::AddEntry(module, key, Type::Bool, ptr, false, description);
    }

    void RegisterField(ToolboxModule* module, const std::string_view key, int* ptr, const std::string_view description)
    {
        detail::AddEntry(module, key, Type::Int, ptr, false, description);
    }

    void RegisterField(ToolboxModule* module, const std::string_view key, unsigned int* ptr, const std::string_view description)
    {
        detail::AddEntry(module, key, Type::Uint, ptr, false, description);
    }

    void RegisterField(ToolboxModule* module, const std::string_view key, float* ptr, const std::string_view description)
    {
        detail::AddEntry(module, key, Type::Float, ptr, false, description);
    }

    void RegisterField(ToolboxModule* module, const std::string_view key, std::string* ptr, const std::string_view description)
    {
        detail::AddEntry(module, key, Type::String, ptr, false, description);
    }

    void RegisterField(ToolboxModule* module, const std::string_view key, Colors::SettingColor* ptr, const std::string_view description)
    {
        detail::AddEntry(module, key, Type::Color, &ptr->value, false, description);
    }

    void RegisterField(ToolboxModule* module, const std::string_view key, std::array<float, 2>* ptr, const std::string_view description)
    {
        detail::AddEntry(module, key, Type::Float2, ptr, false, description);
    }

    void Describe(ToolboxModule* module, const std::string_view key, const std::string_view label, const std::string_view description)
    {
        const auto found = std::ranges::find_if(entries, [module, key](const Entry& e) {
            return e.module == module && e.key == key;
        });
        if (found == entries.end()) {
            Log::Log("SettingsRegistry::Describe: no entry for %s.%.*s", module->Name(), static_cast<int>(key.size()), key.data());
            return;
        }
        if (!label.empty()) {
            found->label = label;
        }
        found->description = description;
    }

    void Unregister(ToolboxModule* module)
    {
        std::erase_if(entries, [module](const Entry& e) { return e.module == module; });
    }

    const std::vector<Entry>& GetEntries()
    {
        return entries;
    }

    void LoadFromIniFallback(ToolboxModule* module, ToolboxIni* ini, const SettingsDoc& doc)
    {
        if (!ini) {
            return;
        }
        for (const auto& e : entries) {
            if (e.module != module || doc.Has(e.section, e.key)) {
                continue;
            }
            const auto* section = e.section.c_str();
            const auto* key = e.key.c_str();
            switch (e.type) {
                case Type::Bool: {
                    auto& val = *static_cast<bool*>(e.ptr);
                    val = ini->GetBoolValue(section, key, val);
                    break;
                }
                case Type::Int: {
                    auto& val = *static_cast<int*>(e.ptr);
                    val = static_cast<int>(ini->GetLongValue(section, key, val));
                    break;
                }
                case Type::Uint: {
                    auto& val = *static_cast<unsigned int*>(e.ptr);
                    val = static_cast<unsigned int>(ini->GetLongValue(section, key, static_cast<long>(val)));
                    break;
                }
                case Type::Float: {
                    auto& val = *static_cast<float*>(e.ptr);
                    val = static_cast<float>(ini->GetDoubleValue(section, key, val));
                    break;
                }
                case Type::String: {
                    auto& val = *static_cast<std::string*>(e.ptr);
                    val = ini->GetValue(section, key, val.c_str());
                    break;
                }
                case Type::Color: {
                    auto& val = *static_cast<Color*>(e.ptr);
                    val = Colors::Load(ini, section, key, val);
                    break;
                }
                case Type::Float2: {
                    auto& val = *static_cast<std::array<float, 2>*>(e.ptr);
                    val[0] = static_cast<float>(ini->GetDoubleValue(section, (e.key + "[0]").c_str(), val[0]));
                    val[1] = static_cast<float>(ini->GetDoubleValue(section, (e.key + "[1]").c_str(), val[1]));
                    break;
                }
            }
        }
    }

    void SaveFieldsToDoc(ToolboxModule* module, SettingsDoc& doc)
    {
        for (const auto& e : entries) {
            if (e.module != module || e.from_struct) {
                continue;
            }
            switch (e.type) {
                case Type::Bool:
                    doc.Set(e.section, e.key, *static_cast<bool*>(e.ptr));
                    break;
                case Type::Int:
                    doc.Set(e.section, e.key, *static_cast<int*>(e.ptr));
                    break;
                case Type::Uint:
                    doc.Set(e.section, e.key, *static_cast<unsigned int*>(e.ptr));
                    break;
                case Type::Float:
                    doc.Set(e.section, e.key, *static_cast<float*>(e.ptr));
                    break;
                case Type::String:
                    doc.Set(e.section, e.key, *static_cast<std::string*>(e.ptr));
                    break;
                case Type::Color:
                    doc.Set(e.section, e.key, Colors::SettingColor(*static_cast<Color*>(e.ptr)));
                    break;
                case Type::Float2:
                    doc.Set(e.section, e.key, *static_cast<std::array<float, 2>*>(e.ptr));
                    break;
            }
        }
    }

    bool LoadEntryFromDoc(const Entry& e, const SettingsDoc& doc)
    {
        switch (e.type) {
            case Type::Bool:
                return doc.Get(e.section, e.key, *static_cast<bool*>(e.ptr));
            case Type::Int:
                return doc.Get(e.section, e.key, *static_cast<int*>(e.ptr));
            case Type::Uint:
                return doc.Get(e.section, e.key, *static_cast<unsigned int*>(e.ptr));
            case Type::Float:
                return doc.Get(e.section, e.key, *static_cast<float*>(e.ptr));
            case Type::String:
                return doc.Get(e.section, e.key, *static_cast<std::string*>(e.ptr));
            case Type::Color: {
                Colors::SettingColor staged(*static_cast<Color*>(e.ptr));
                if (!doc.Get(e.section, e.key, staged)) {
                    return false;
                }
                *static_cast<Color*>(e.ptr) = staged.value;
                return true;
            }
            case Type::Float2:
                return doc.Get(e.section, e.key, *static_cast<std::array<float, 2>*>(e.ptr));
        }
        return false;
    }

    void LoadFieldsFromDoc(ToolboxModule* module, const SettingsDoc& doc)
    {
        for (const auto& e : entries) {
            if (e.module == module && !e.from_struct) {
                LoadEntryFromDoc(e, doc);
            }
        }
    }
}
