// No stdafx: this file is also compiled into plugin DLLs (see cmake/gwtoolboxdll_plugins.cmake).
#include <Utils/SettingsDoc.h>

#include <fstream>
#include <iterator>
#include <string>

namespace {
    // Canonical section key, also its <section>.json filename stem, so forbidden chars are stripped.
    std::string SanitiseSection(const std::string_view section)
    {
        std::string out;
        out.reserve(section.size());
        for (const char c : section) {
            if (std::string_view(R"(<>:"/\|?*)").find(c) == std::string_view::npos && static_cast<unsigned char>(c) >= 0x20) {
                out += c;
            }
        }
        return out;
    }

    std::filesystem::path SectionFilename(const std::string_view section)
    {
        const std::string name = SanitiseSection(section);
        auto filename = std::filesystem::path(std::u8string(name.begin(), name.end()));
        filename += L".json";
        return filename;
    }
}

bool SettingsDoc::LoadFile(const std::filesystem::path& path)
{
    sections.clear();
    location_on_disk = path;
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return true;
    }
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    const std::string buffer{std::istreambuf_iterator(file), {}};
    if (buffer.empty()) {
        return true;
    }
    if (glz::read<lenient_opts>(sections, buffer)) {
        sections.clear();
        return false;
    }
    return true;
}

bool SettingsDoc::SaveFile(const std::filesystem::path& path) const
{
    std::string buffer;
    if (glz::write<glz::opts{.prettify = true}>(sections, buffer)) {
        return false;
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    return file.good();
}

bool SettingsDoc::LoadFolder(const std::filesystem::path& folder)
{
    sections.clear();
    location_on_disk = folder;
    std::error_code ec;
    if (!std::filesystem::is_directory(folder, ec)) {
        return true;
    }
    bool ok = true;
    for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
        if (!entry.is_regular_file(ec) || entry.path().extension() != L".json") {
            continue;
        }
        std::ifstream file(entry.path(), std::ios::binary);
        if (!file) {
            ok = false;
            continue;
        }
        const std::string buffer{std::istreambuf_iterator(file), {}};
        Section section;
        if (!buffer.empty() && glz::read<lenient_opts>(section, buffer)) {
            ok = false;
            continue;
        }
        const auto stem = entry.path().stem().u8string();
        sections.insert_or_assign(std::string(stem.begin(), stem.end()), std::move(section));
    }
    return ok;
}

bool SettingsDoc::SaveFolder(const std::filesystem::path& folder) const
{
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    bool ok = true;
    for (const auto& [name, section] : sections) {
        std::string buffer;
        if (glz::write<glz::opts{.prettify = true}>(section, buffer)) {
            ok = false;
            continue;
        }
        std::ofstream file(folder / SectionFilename(name), std::ios::binary | std::ios::trunc);
        if (!file) {
            ok = false;
            continue;
        }
        file.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        ok = file.good() && ok;
    }
    return ok;
}

bool SettingsDoc::Has(const std::string_view section, const std::string_view key) const
{
    return FindKey(section, key) != nullptr;
}

bool SettingsDoc::HasSection(const std::string_view section) const
{
    return FindSection(section) != nullptr;
}

void SettingsDoc::EraseSection(const std::string_view section)
{
    const auto found = sections.find(SanitiseSection(section));
    if (found != sections.end()) {
        sections.erase(found);
    }
}

void SettingsDoc::EraseKey(const std::string_view section, const std::string_view key)
{
    const auto found = sections.find(SanitiseSection(section));
    if (found == sections.end()) {
        return;
    }
    const auto key_found = found->second.find(key);
    if (key_found != found->second.end()) {
        found->second.erase(key_found);
    }
}

const SettingsDoc::Section* SettingsDoc::FindSection(const std::string_view section) const
{
    const auto found = sections.find(SanitiseSection(section));
    return found != sections.end() ? &found->second : nullptr;
}

const glz::raw_json* SettingsDoc::FindKey(const std::string_view section, const std::string_view key) const
{
    const auto* sec = FindSection(section);
    if (!sec) {
        return nullptr;
    }
    const auto found = sec->find(key);
    return found != sec->end() ? &found->second : nullptr;
}

SettingsDoc::Section& SettingsDoc::GetOrCreateSection(const std::string_view section)
{
    std::string name = SanitiseSection(section);
    const auto found = sections.find(name);
    if (found != sections.end()) {
        return found->second;
    }
    return sections.emplace(std::move(name), Section{}).first->second;
}
