#pragma once

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>

#include <glaze/glaze.hpp>

// JSON settings document: in-memory map of section -> { key -> value }.
// On disk each section is its own <section>.json inside a folder (toolbox), or all
// sections share a single file (plugin SDK, and the one-time legacy GWToolbox.json seed).
// Sections of unknown/disabled modules are preserved verbatim across load/save cycles.
class SettingsDoc {
public:
    using Section = std::map<std::string, glz::raw_json, std::less<>>;

    // Single-file form. Missing file is an empty doc, not an error.
    bool LoadFile(const std::filesystem::path& path);
    bool SaveFile(const std::filesystem::path& path) const;

    // Folder form: one prettified <section>.json per section. Missing folder is an empty doc.
    bool LoadFolder(const std::filesystem::path& folder);
    bool SaveFolder(const std::filesystem::path& folder) const;
    bool SaveFolder() const { return SaveFolder(location_on_disk); }

    [[nodiscard]] bool Empty() const { return sections.empty(); }

    // Lenient whole-struct read: unknown keys ignored, missing keys keep current values.
    template <typename T>
    bool GetStruct(const std::string_view section, T& out) const
    {
        const auto* sec = FindSection(section);
        if (!sec) {
            return false;
        }
        const auto buffer = glz::write_json(*sec);
        if (!buffer) {
            return false;
        }
        return !glz::read<lenient_opts>(out, *buffer);
    }

    // Merges the struct's keys into the section, preserving keys it doesn't own.
    template <typename T>
    void SetStruct(const std::string_view section, const T& value)
    {
        const auto buffer = glz::write_json(value);
        if (!buffer) {
            return;
        }
        Section staged;
        if (glz::read_json(staged, *buffer)) {
            return;
        }
        auto& sec = GetOrCreateSection(section);
        for (auto& [key, raw] : staged) {
            sec.insert_or_assign(key, std::move(raw));
        }
    }

    // Returns false (out untouched) on missing key or parse failure.
    template <typename T>
    bool Get(const std::string_view section, const std::string_view key, T& out) const
    {
        const auto* raw = FindKey(section, key);
        if (!raw) {
            return false;
        }
        T staged{};
        if (glz::read<lenient_opts>(staged, raw->str)) {
            return false;
        }
        out = std::move(staged);
        return true;
    }

    template <typename T>
    void Set(const std::string_view section, const std::string_view key, const T& value)
    {
        auto buffer = glz::write_json(value);
        if (!buffer) {
            return;
        }
        GetOrCreateSection(section).insert_or_assign(std::string(key), glz::raw_json{std::move(*buffer)});
    }

    [[nodiscard]] bool Has(std::string_view section, std::string_view key) const;
    [[nodiscard]] bool HasSection(std::string_view section) const;
    void EraseSection(std::string_view section);
    void EraseKey(std::string_view section, std::string_view key);

    std::filesystem::path location_on_disk;

private:
    static constexpr glz::opts lenient_opts{.error_on_unknown_keys = false};

    [[nodiscard]] const Section* FindSection(std::string_view section) const;
    [[nodiscard]] const glz::raw_json* FindKey(std::string_view section, std::string_view key) const;
    Section& GetOrCreateSection(std::string_view section);

    std::map<std::string, Section, std::less<>> sections;
};

// GW-encoded wstrings can hold lone surrogates that don't survive utf8 conversion; persisted as
// space-separated hex code units (same codec as GuiUtils::ArrayToIni, so legacy INI values match).
struct SettingWString {
    std::wstring value;

    SettingWString() = default;
    SettingWString(std::wstring v) : value(std::move(v)) {}

    operator std::wstring&() { return value; }
    operator const std::wstring&() const { return value; }
};

template <>
struct glz::meta<SettingWString> {
    static constexpr auto read_wstr = [](SettingWString& s, const std::string& input) {
        s.value.clear();
        s.value.reserve((input.size() + 1) / 5);
        const char* p = input.c_str();
        while (*p) {
            char* end = nullptr;
            const auto parsed = strtoul(p, &end, 16);
            if (end == p || parsed > 0xFFFF) break;
            s.value.push_back(static_cast<wchar_t>(parsed));
            if (*end != ' ') break;
            p = end + 1;
        }
    };
    static constexpr auto write_wstr = [](const SettingWString& s) -> std::string {
        std::string out;
        out.reserve(s.value.size() * 5);
        char buf[8];
        for (size_t i = 0; i < s.value.size(); i++) {
            snprintf(buf, sizeof(buf), i ? " %04x" : "%04x", static_cast<unsigned>(s.value[i]) & 0xFFFFu);
            out += buf;
        }
        return out;
    };
    static constexpr auto value = glz::custom<read_wstr, write_wstr>;
};
