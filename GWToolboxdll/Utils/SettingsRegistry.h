#pragma once

#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <glaze/glaze.hpp>

#include <Color.h>
#include <Utils/SettingsDoc.h>

class ToolboxModule;
class ToolboxIni;

// Runtime descriptor table of every registered leaf setting.
// Drives settings search, /tb_setting chat commands and the generic legacy-INI read fallback.
namespace SettingsRegistry {
    enum class Type : uint8_t { Bool, Int, Uint, Float, String, Color, Float2 };

    struct Entry {
        ToolboxModule* module = nullptr;
        std::string section;     // module->Name() at registration time == JSON section == legacy INI section
        std::string key;         // member name == JSON key == legacy INI key
        std::string label;       // prettified key for search/UI
        std::string description; // optional, for search + chat command help
        Type type = Type::Bool;
        void* ptr = nullptr;     // live value; Color entries point at the raw ::Color
        bool from_struct = false;
    };

    namespace detail {
        void AddEntry(ToolboxModule* module, std::string_view key, Type type, void* ptr, bool from_struct, std::string_view description = {});
        void LogUnsupportedMember(ToolboxModule* module, std::string_view key);

        template <typename M>
        void RegisterMember(ToolboxModule* module, const std::string_view key, M& member)
        {
            if constexpr (std::same_as<M, bool>) {
                AddEntry(module, key, Type::Bool, &member, true);
            }
            else if constexpr (std::same_as<M, int>) {
                AddEntry(module, key, Type::Int, &member, true);
            }
            else if constexpr (std::same_as<M, unsigned int>) {
                AddEntry(module, key, Type::Uint, &member, true);
            }
            else if constexpr (std::same_as<M, float>) {
                AddEntry(module, key, Type::Float, &member, true);
            }
            else if constexpr (std::same_as<M, std::string>) {
                AddEntry(module, key, Type::String, &member, true);
            }
            else if constexpr (std::same_as<M, Colors::SettingColor>) {
                AddEntry(module, key, Type::Color, &member.value, true);
            }
            else if constexpr (std::same_as<M, std::array<float, 2>>) {
                AddEntry(module, key, Type::Float2, &member, true);
            }
            else if constexpr (std::is_enum_v<M> && sizeof(M) == sizeof(int)) {
                AddEntry(module, key, Type::Int, &member, true);
            }
            else {
                // Still persisted by glaze via the struct round-trip, just not searchable/chat-settable.
                LogUnsupportedMember(module, key);
            }
        }
    }

    // Struct path: registers one Entry per supported member (member name == legacy INI key) for
    // search, chat commands and the INI fallback. Persistence is the module's job: its
    // LoadSettings/SaveSettings overrides call doc.GetStruct/SetStruct(Name(), settings) so every
    // module keeps a dedicated debugging point.
    template <typename T>
    void Register(ToolboxModule* module, T& settings)
    {
        static_assert(glz::reflectable<T>, "Settings struct must be a plain glaze-reflectable aggregate");
        auto tie = glz::to_tie(settings);
        [&]<size_t... I>(std::index_sequence<I...>) {
            (detail::RegisterMember(module, glz::reflect<T>::keys[I], glz::get<I>(tie)), ...);
        }(std::make_index_sequence<glz::reflect<T>::size>{});
    }

    // Field path: for individually registered values (e.g. base-class members).
    void RegisterField(ToolboxModule* module, std::string_view key, bool* ptr, std::string_view description = {});
    void RegisterField(ToolboxModule* module, std::string_view key, int* ptr, std::string_view description = {});
    void RegisterField(ToolboxModule* module, std::string_view key, unsigned int* ptr, std::string_view description = {});
    void RegisterField(ToolboxModule* module, std::string_view key, float* ptr, std::string_view description = {});
    void RegisterField(ToolboxModule* module, std::string_view key, std::string* ptr, std::string_view description = {});
    void RegisterField(ToolboxModule* module, std::string_view key, Colors::SettingColor* ptr, std::string_view description = {});
    void RegisterField(ToolboxModule* module, std::string_view key, std::array<float, 2>* ptr, std::string_view description = {});

    // Override the auto-prettified label and attach a description on an already-registered entry, so
    // settings search, the locate-highlight and /tb_setting match the text the user actually sees on screen.
    void Describe(ToolboxModule* module, std::string_view key, std::string_view label, std::string_view description = {});

    // Called from ToolboxModule::Terminate.
    void Unregister(ToolboxModule* module);

    const std::vector<Entry>& GetEntries();

    // For every entry of module whose key is absent from the JSON doc, read the legacy INI value.
    void LoadFromIniFallback(ToolboxModule* module, ToolboxIni* ini, const SettingsDoc& doc);

    // Persistence for field-path entries (struct persistence lives in module overrides).
    void SaveFieldsToDoc(ToolboxModule* module, SettingsDoc& doc);
    void LoadFieldsFromDoc(ToolboxModule* module, const SettingsDoc& doc);

    // Reads one entry's value from the doc into the live value; false if the doc has none.
    bool LoadEntryFromDoc(const Entry& entry, const SettingsDoc& doc);
}
