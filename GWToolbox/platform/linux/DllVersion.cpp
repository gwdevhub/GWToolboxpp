#include "DllVersion.h"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

constexpr std::uint32_t kRtVersion = 16;
constexpr std::uint32_t kFixedInfoSignature = 0xFEEF04BD;

std::optional<std::uint32_t> readU32(const Bytes& data, std::size_t at)
{
    if (at + 4 > data.size()) {
        return std::nullopt;
    }
    return data[at] | (data[at + 1] << 8) | (data[at + 2] << 16) | (static_cast<std::uint32_t>(data[at + 3]) << 24);
}

std::optional<std::uint16_t> readU16(const Bytes& data, std::size_t at)
{
    if (at + 2 > data.size()) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(data[at] | (data[at + 1] << 8));
}

struct Section {
    std::uint32_t virtual_address;
    std::uint32_t span;
    std::uint32_t raw_offset;
};

std::optional<std::size_t> rvaToOffset(const std::vector<Section>& sections, std::uint32_t rva)
{
    for (const Section& section : sections) {
        if (rva >= section.virtual_address && rva - section.virtual_address < section.span) {
            return static_cast<std::size_t>(rva - section.virtual_address) + section.raw_offset;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> resourceEntryData(const Bytes& data, std::size_t directory, std::uint32_t wanted_id, bool& is_directory,
                                             std::size_t& first_offset)
{
    const auto named = readU16(data, directory + 12);
    const auto ids = readU16(data, directory + 14);
    if (!named || !ids) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < *named + *ids; ++i) {
        const std::size_t entry = directory + 16 + i * 8;
        const auto name = readU32(data, entry);
        const auto offset = readU32(data, entry + 4);
        if (!name || !offset) {
            return std::nullopt;
        }
        if (i == 0 && wanted_id == 0) {
            first_offset = *offset & 0x7FFFFFFF;
            is_directory = (*offset & 0x80000000) != 0;
            return std::size_t{0};
        }
        if (*name == wanted_id) {
            is_directory = (*offset & 0x80000000) != 0;
            first_offset = *offset & 0x7FFFFFFF;
            return std::size_t{0};
        }
    }
    return std::nullopt;
}

}

std::string PeFileVersion(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary);
    const Bytes data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    const auto pe_offset = readU32(data, 0x3C);
    if (!pe_offset || *pe_offset + 24 > data.size() || data[*pe_offset] != 'P' || data[*pe_offset + 1] != 'E') {
        return {};
    }
    const std::size_t coff = *pe_offset + 4;
    const auto section_count = readU16(data, coff + 2);
    const auto optional_size = readU16(data, coff + 16);
    if (!section_count || !optional_size) {
        return {};
    }
    const std::size_t optional = coff + 20;
    const auto magic = readU16(data, optional);
    if (!magic) {
        return {};
    }
    const std::size_t directories = optional + (*magic == 0x20B ? 112 : 96);
    const auto resource_rva = readU32(data, directories + 2 * 8);
    if (!resource_rva || *resource_rva == 0) {
        return {};
    }

    std::vector<Section> sections;
    const std::size_t table = optional + *optional_size;
    for (std::size_t i = 0; i < *section_count; ++i) {
        const std::size_t header = table + i * 40;
        const auto span = readU32(data, header + 8);
        const auto address = readU32(data, header + 12);
        const auto size = readU32(data, header + 16);
        const auto raw = readU32(data, header + 20);
        if (!span || !address || !size || !raw) {
            return {};
        }
        sections.push_back({*address, *span > *size ? *span : *size, *raw});
    }

    const auto resource_base = rvaToOffset(sections, *resource_rva);
    if (!resource_base) {
        return {};
    }

    bool is_directory = false;
    std::size_t offset = 0;
    if (!resourceEntryData(data, *resource_base, kRtVersion, is_directory, offset) || !is_directory) {
        return {};
    }
    std::size_t level = *resource_base + offset;
    if (!resourceEntryData(data, level, 0, is_directory, offset) || !is_directory) {
        return {};
    }
    level = *resource_base + offset;
    if (!resourceEntryData(data, level, 0, is_directory, offset) || is_directory) {
        return {};
    }
    const std::size_t entry = *resource_base + offset;
    const auto data_rva = readU32(data, entry);
    if (!data_rva) {
        return {};
    }
    const auto version_at = rvaToOffset(sections, *data_rva);
    if (!version_at) {
        return {};
    }

    for (std::size_t at = *version_at; at + 52 <= data.size() && at < *version_at + 256; at += 4) {
        const auto signature = readU32(data, at);
        if (signature && *signature == kFixedInfoSignature) {
            const auto most = readU32(data, at + 8);
            const auto least = readU32(data, at + 12);
            if (!most || !least) {
                return {};
            }
            return std::to_string(*most >> 16) + "." + std::to_string(*most & 0xFFFF) + "." + std::to_string(*least >> 16) + "." +
                   std::to_string(*least & 0xFFFF);
        }
    }
    return {};
}
