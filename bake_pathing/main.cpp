#include "DatArchive.h"
#include "PlacedMaps.h"

#include <Utils/ArenaNetFileParser.h>
#include <Windows/Pathfinding/PathingMapDataLoader.h>
#include <Windows/Pathfinding/maps_constant_data.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
    using Planes = std::vector<Pathing::detail::ParsedPlane>;
    using Tiles = std::unordered_set<uint64_t>;
    using TileSets = std::array<Tiles, 6>;
    using Traps = std::unordered_set<uint64_t>;
    constexpr double cell_size = 32.0;
    constexpr double unit_size = 96.0;
    constexpr std::array<const char*, 6> names = {
        "standable", "creditable", "standable_glitched", "creditable_glitched", "standable_any", "creditable_any"};
    constexpr std::array<const char*, 6> magic = {"CSM1", "CCM1", "CSG1", "CCG1", "CSA1", "CCA1"};
    constexpr std::array<const char*, 6> array_names = {"kBits", "kCredit", "kBitsG", "kCreditG", "kBitsA", "kCreditA"};

    struct Map {
        int id, continent, sx, sy, ex, ey;
        uint32_t file_id;
    };

    struct Gate {
        double x, y, radius_sq;
    };

    struct Result {
        Map map;
        TileSets masks;
        bool success = false;
        double elapsed_ms = 0.;
    };

    uint64_t Key(int x, int y)
    {
        return static_cast<uint64_t>(static_cast<uint32_t>(y)) << 32 | static_cast<uint32_t>(x);
    }

    int X(uint64_t key) { return static_cast<int32_t>(key); }
    int Y(uint64_t key) { return static_cast<int32_t>(key >> 32); }

    std::vector<Map> Maps()
    {
        std::unordered_map<int, uint32_t> ids;
        for (const auto& [fid, records] : constant_maps_info) {
            if (!fid) continue;
            for (const auto& record : records) {
                const auto id = static_cast<int>(record.map_id);
                if (id <= 0) continue;
                auto& selected = ids[id];
                if (!selected || static_cast<uint32_t>(fid) < selected) selected = static_cast<uint32_t>(fid);
            }
        }
        std::istringstream input{std::string(placed_maps)};
        std::vector<Map> result;
        std::string value;
        while (input >> value) {
            Map m{};
            std::replace(value.begin(), value.end(), ':', ' ');
            std::istringstream fields(value);
            if (!(fields >> m.id >> m.continent >> m.sx >> m.sy >> m.ex >> m.ey)) throw std::runtime_error("invalid placed map");
            if (const auto it = ids.find(m.id); it != ids.end()) {
                m.file_id = it->second;
                result.push_back(m);
            }
        }
        return result;
    }

    const ArenaNetFileParser::Chunk* Chunk(ArenaNetFileParser::ArenaNetFile& file, ArenaNetFileParser::ChunkType type)
    {
        return file.FindChunk(type);
    }

    bool ParsePlanes(const ArenaNetFileParser::Chunk* chunk, Planes& planes)
    {
        if (!chunk || chunk->chunk_size < 21) return false;
        auto* bytes = reinterpret_cast<const uint8_t*>(chunk) + sizeof(*chunk);
        const size_t size = chunk->chunk_size;
        size_t off = 12;
        uint8_t tag;
        uint32_t length;
        if (!Pathing::detail::read_tag(bytes, off, size, tag, length) || tag != 7 || off + 5 + length > size) return false;
        off += 5 + length;
        if (!Pathing::detail::read_tag(bytes, off, size, tag, length) || tag != 8) return false;
        off += 5;
        uint32_t count;
        if (!Pathing::detail::read_u32(bytes, off, size, count) || count > 256) return false;
        off += 4;
        planes.resize(count);
        for (auto& plane : planes) {
            if (!Pathing::detail::parse_plane(bytes, off, size, plane)) return false;
        }
        return true;
    }

    std::vector<Gate> PortalGates(ArenaNetFileParser::ArenaNetFile& file)
    {
        const auto* filename_chunk = Chunk(file, ArenaNetFileParser::ChunkType::Map_PropFilenames);
        const auto* prop_chunk = Chunk(file, ArenaNetFileParser::ChunkType::Map_PropInfo);
        if (!filename_chunk || !prop_chunk || filename_chunk->chunk_size < 5 || prop_chunk->chunk_size < 12) return {};
        const auto* names_bytes = reinterpret_cast<const uint8_t*>(filename_chunk) + sizeof(*filename_chunk);
        const auto* props = reinterpret_cast<const uint8_t*>(prop_chunk) + sizeof(*prop_chunk);
        std::vector<uint32_t> file_ids;
        for (size_t i = 0; i < (filename_chunk->chunk_size - 5) / 6; i++) {
            uint16_t a, b;
            memcpy(&a, names_bytes + 5 + i * 6, 2);
            memcpy(&b, names_bytes + 7 + i * 6, 2);
            file_ids.push_back(a > 0xff && b > 0xff ? (a - 0xff00ff) + static_cast<uint32_t>(b) * 0xff00 : 0);
        }
        uint16_t count;
        memcpy(&count, props + 10, 2);
        size_t off = 12;
        std::vector<Gate> gates;
        for (uint16_t i = 0; i < count && off + 48 <= prop_chunk->chunk_size; i++) {
            uint16_t index;
            memcpy(&index, props + off, 2);
            if (index < file_ids.size()) {
                const auto fid = file_ids[index];
                if (Pathing::IsPortalModelFileId(fid)) {
                    float x, y, r;
                    memcpy(&x, props + off + 2, 4);
                    memcpy(&y, props + off + 6, 4);
                    memcpy(&r, props + off + 42, 4);
                    const double radius = r > 0.f ? r : 400.;
                    gates.push_back({x, y, radius * radius});
                }
            }
            off += 48 + props[off + 47] * 8;
        }
        return gates;
    }

    std::pair<double, double> Centre(const Pathing::detail::ParsedTrapezoid& trap)
    {
        return {(trap.XTL + trap.XTR + trap.XBL + trap.XBR) * .25, (trap.YT + trap.YB) * .5};
    }

    bool Crosses(const std::vector<Gate>& gates, double ax, double ay, double bx, double by)
    {
        for (const auto& gate : gates) {
            const auto dx = bx - ax, dy = by - ay;
            const auto span = dx * dx + dy * dy;
            const auto t = span ? std::clamp(((gate.x - ax) * dx + (gate.y - ay) * dy) / span, 0., 1.) : 0.;
            const auto ox = ax + dx * t - gate.x, oy = ay + dy * t - gate.y;
            if (ox * ox + oy * oy < gate.radius_sq) return true;
        }
        return false;
    }

    Traps Flood(const Planes& planes, const std::vector<uint64_t>& seeds, const std::vector<Gate>& gates)
    {
        Traps reached;
        std::vector<uint64_t> queue;
        for (const auto seed : seeds) if (reached.insert(seed).second) queue.push_back(seed);
        std::vector<std::unordered_map<uint16_t, size_t>> portal_pairs(planes.size());
        for (size_t pi = 0; pi < planes.size(); pi++) {
            for (size_t i = 0; i < planes[pi].portals.size(); i++)
                portal_pairs[pi][planes[pi].portals[i].shared_id] = i;
        }
        for (size_t head = 0; head < queue.size(); head++) {
            const auto key = queue[head];
            const auto pi = static_cast<size_t>(Y(key));
            const auto ti = static_cast<size_t>(X(key));
            const auto& plane = planes[pi];
            const auto& trap = plane.trapezoids[ti];
            const auto [ax, ay] = Centre(trap);
            const auto expand = [&](size_t other_plane, size_t other_trap) {
                if (other_plane >= planes.size() || other_trap >= planes[other_plane].trapezoids.size()) return;
                const auto next = Key(static_cast<int>(other_trap), static_cast<int>(other_plane));
                if (reached.contains(next)) return;
                const auto [bx, by] = Centre(planes[other_plane].trapezoids[other_trap]);
                if (Crosses(gates, ax, ay, bx, by)) return;
                reached.insert(next);
                queue.push_back(next);
            };
            for (auto index : trap.neighbors) expand(pi, index);
            for (auto index : {trap.portal_left, trap.portal_right}) {
                if (index >= plane.portals.size()) continue;
                const auto& portal = plane.portals[index];
                if ((portal.flags & 4) || portal.neighbor_plane >= planes.size()) continue;
                const auto dest = portal.neighbor_plane;
                const auto it = portal_pairs[dest].find(portal.shared_id);
                if (it == portal_pairs[dest].end()) continue;
                const auto& peer = planes[dest].portals[it->second];
                for (uint32_t i = 0; i < peer.trapezoid_count; i++) {
                    const auto offset = static_cast<size_t>(peer.trapezoid_index_start) + i;
                    if (offset < planes[dest].portal_trapezoid_indices.size())
                        expand(dest, planes[dest].portal_trapezoid_indices[offset]);
                }
            }
        }
        return reached;
    }

    std::array<Traps, 2> Reachable(const Planes& planes, const std::vector<Gate>& gates)
    {
        Traps normal, seen, largest;
        for (size_t pi = 0; pi < planes.size(); pi++) {
            for (size_t ti = 0; ti < planes[pi].trapezoids.size(); ti++) {
                const auto key = Key(static_cast<int>(ti), static_cast<int>(pi));
                if (seen.contains(key)) continue;
                const auto piece = Flood(planes, {key}, gates);
                seen.insert(piece.begin(), piece.end());
                if (piece.size() > largest.size()) largest = piece;
            }
        }
        normal.insert(largest.begin(), largest.end());
        for (size_t i = 0; i < gates.size(); i++) {
            std::vector<uint64_t> seeds;
            for (size_t pi = 0; pi < planes.size(); pi++) {
                for (size_t ti = 0; ti < planes[pi].trapezoids.size(); ti++) {
                    const auto [x, y] = Centre(planes[pi].trapezoids[ti]);
                    const auto dx = x - gates[i].x, dy = y - gates[i].y;
                    if (dx * dx + dy * dy < gates[i].radius_sq)
                        seeds.push_back(Key(static_cast<int>(ti), static_cast<int>(pi)));
                }
            }
            std::vector<Gate> other_gates;
            for (size_t j = 0; j < gates.size(); j++) if (j != i) other_gates.push_back(gates[j]);
            const auto piece = Flood(planes, seeds, other_gates);
            normal.insert(piece.begin(), piece.end());
        }
        const std::vector<uint64_t> seeds(normal.begin(), normal.end());
        const auto glitched = normal.empty() ? normal : Flood(planes, seeds, {});
        return {std::move(normal), std::move(glitched)};
    }

    using Polygon = std::vector<std::pair<double, double>>;

    Polygon Clip(const Polygon& source, int axis, double limit, bool above)
    {
        Polygon out;
        for (size_t i = 0; i < source.size(); i++) {
            const auto& a = source[i];
            const auto& b = source[(i + 1) % source.size()];
            const auto ca = axis ? a.second : a.first, cb = axis ? b.second : b.first;
            const bool a_in = above ? ca >= limit : ca <= limit;
            const bool b_in = above ? cb >= limit : cb <= limit;
            if (a_in) out.push_back(a);
            if (a_in != b_in) {
                const auto u = (ca - limit) / (ca - cb);
                out.emplace_back(a.first + (b.first - a.first) * u, a.second + (b.second - a.second) * u);
            }
        }
        return out;
    }

    bool Intersects(Polygon poly, double x0, double y0, double x1, double y1)
    {
        poly = Clip(poly, 0, x0, true);
        poly = Clip(poly, 0, x1, false);
        poly = Clip(poly, 1, y0, true);
        poly = Clip(poly, 1, y1, false);
        for (size_t i = 0; i < poly.size(); i++) {
            const auto& a = poly[i];
            const auto& b = poly[(i + 1) % poly.size()];
            if (a.first < x1 && a.second > y0) return true;
            if ((a.first + b.first) * .5 < x1 && (a.second + b.second) * .5 > y0) return true;
        }
        return false;
    }

    void Rasterize(const Map& map, const Planes& planes, const std::array<Traps, 2>& reachable,
                   float min_x, float max_y, TileSets& tiles)
    {
        const auto anchor_x = map.sx - min_x / unit_size;
        const auto anchor_y = map.sy + max_y / unit_size + 1.;
        for (size_t pi = 0; pi < planes.size(); pi++) {
            for (size_t ti = 0; ti < planes[pi].trapezoids.size(); ti++) {
                const auto& t = planes[pi].trapezoids[ti];
                const auto key = Key(static_cast<int>(ti), static_cast<int>(pi));
                const bool normal = reachable[0].contains(key), glitched = reachable[1].contains(key);
                const auto left = std::min(t.XTL, t.XBL) / unit_size + anchor_x;
                const auto right = std::max(t.XTR, t.XBR) / unit_size + anchor_x;
                const auto top = -t.YT / unit_size + anchor_y;
                const auto bottom = -t.YB / unit_size + anchor_y;
                Polygon poly = {{t.XTL / unit_size + anchor_x, top}, {t.XTR / unit_size + anchor_x, top},
                                {t.XBR / unit_size + anchor_x, bottom}, {t.XBL / unit_size + anchor_x, bottom}};
                for (int cy = static_cast<int>(std::ceil(top / cell_size)) - 1;
                     cy <= static_cast<int>(std::floor(bottom / cell_size)); cy++) {
                    for (int cx = static_cast<int>(std::ceil(left / cell_size)) - 1;
                         cx <= static_cast<int>(std::floor(right / cell_size)); cx++) {
                        const auto tile = Key(cx, cy);
                        if (tiles[4].contains(tile) && (!normal || tiles[0].contains(tile)) &&
                            (!glitched || tiles[2].contains(tile))) continue;
                        if (!Intersects(poly, cx * cell_size, cy * cell_size,
                                        (cx + 1) * cell_size, (cy + 1) * cell_size)) continue;
                        tiles[4].insert(tile);
                        if (normal) tiles[0].insert(tile);
                        if (glitched) tiles[2].insert(tile);
                    }
                }
            }
        }
        const auto x0 = static_cast<int>(std::floor(map.sx / cell_size)) - 1;
        const auto y0 = static_cast<int>(std::ceil(map.sy / cell_size)) - 2;
        const auto x1 = static_cast<int>(std::ceil(map.ex / cell_size)) + 1;
        const auto y1 = static_cast<int>(std::ceil(map.ey / cell_size)) + 1;
        for (int kind : {0, 2, 4}) {
            for (const auto key : tiles[kind]) {
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        const int cx = X(key) + dx, cy = Y(key) + dy;
                        if (x0 <= cx && cx < x1 && y0 <= cy && cy < y1) tiles[kind + 1].insert(Key(cx, cy));
                    }
                }
            }
        }
    }

    Result Bake(const Map& map, const DatArchive& dat)
    {
        const auto started = std::chrono::steady_clock::now();
        Result result{};
        result.map = map;
        std::vector<uint8_t> bytes;
        if (!dat.ReadFile(map.file_id, 1, bytes)) return result;
        ArenaNetFileParser::ArenaNetFile asset;
        if (!asset.parse(bytes)) return result;
        const auto* info = Chunk(asset, ArenaNetFileParser::ChunkType::Map_Info);
        if (!info || info->chunk_size < 21) return result;
        const auto* raw = reinterpret_cast<const uint8_t*>(info) + sizeof(*info);
        float min_x, max_y;
        memcpy(&min_x, raw + 5, 4);
        memcpy(&max_y, raw + 17, 4);
        Planes planes;
        if (!ParsePlanes(Chunk(asset, ArenaNetFileParser::ChunkType::Map_Pathfinding), planes)) return result;
        const auto gates = PortalGates(asset);
        const auto reachable = Reachable(planes, gates);
        Rasterize(map, planes, reachable, min_x, max_y, result.masks);
        result.success = true;
        result.elapsed_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        return result;
    }

    struct Mask {
        int continent, x0, y0, width, height;
        const char* kind;
        const char* signature;
        std::vector<uint8_t> bits;
    };

    Mask Pack(int continent, int kind, const Tiles& tiles)
    {
        Mask mask{continent, INT_MAX, INT_MAX, 0, 0, names[kind], magic[kind], {}};
        int x1 = INT_MIN, y1 = INT_MIN;
        for (const auto key : tiles) {
            mask.x0 = std::min(mask.x0, X(key));
            mask.y0 = std::min(mask.y0, Y(key));
            x1 = std::max(x1, X(key));
            y1 = std::max(y1, Y(key));
        }
        if (tiles.empty()) return mask;
        mask.width = x1 - mask.x0 + 1;
        mask.height = y1 - mask.y0 + 1;
        mask.bits.resize((static_cast<size_t>(mask.width) * mask.height + 7) / 8);
        for (const auto key : tiles) {
            const auto bit = static_cast<size_t>(Y(key) - mask.y0) * mask.width + X(key) - mask.x0;
            mask.bits[bit >> 3] |= static_cast<uint8_t>(1u << (bit & 7));
        }
        return mask;
    }

    void Write(const std::filesystem::path& folder, const Mask& mask)
    {
        std::ofstream file(folder / (std::string(mask.kind) + "_L" + std::to_string(mask.continent) + ".bin"), std::ios::binary);
        if (!file) throw std::runtime_error("cannot write mask");
        file.write(mask.signature, 4);
        const int32_t header[5] = {mask.continent, mask.x0, mask.y0, mask.width, mask.height};
        file.write(reinterpret_cast<const char*>(header), sizeof(header));
        file.write(reinterpret_cast<const char*>(mask.bits.data()), static_cast<std::streamsize>(mask.bits.size()));
        if (!file) throw std::runtime_error("failed writing mask");
    }

    void WriteHeader(const std::filesystem::path& folder, const std::vector<Mask>& masks)
    {
        std::ofstream file(folder / "CartographyData.h");
        if (!file) throw std::runtime_error("cannot write CartographyData.h");
        file << "#pragma once\n\n#include <cstdint>\n\nnamespace CartographyData {\n"
             << "    struct Mask { int x0, y0, width, height; const uint8_t* bits; int byte_count; };\n"
             << "    struct Continent { int id; Mask standable, creditable, standable_glitched, creditable_glitched, standable_any, creditable_any; };\n";
        for (const auto& mask : masks) {
            file << "    inline constexpr uint8_t " << array_names[(&mask - masks.data()) % 6] << mask.continent << "[] = {\n";
            for (size_t i = 0; i < mask.bits.size(); i++) {
                if (i % 32 == 0) file << "        ";
                file << static_cast<unsigned>(mask.bits[i]) << ',';
                file << (i % 32 == 31 || i + 1 == mask.bits.size() ? '\n' : ' ');
            }
            file << "    };\n";
        }
        file << "    inline constexpr Continent kContinents[] = {\n";
        for (size_t i = 0; i < masks.size(); i += 6) {
            file << "        {" << masks[i].continent;
            for (size_t k = 0; k < 6; k++) {
                const auto& m = masks[i + k];
                file << ", {" << m.x0 << ',' << m.y0 << ',' << m.width << ',' << m.height << ','
                     << array_names[k] << m.continent << ',' << m.bits.size() << '}';
            }
            file << "},\n";
        }
        file << "    };\n}\n";
        if (!file) throw std::runtime_error("failed writing CartographyData.h");
    }
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 3 && (argc != 5 || std::wstring_view(argv[3]) != L"--map")) {
        std::wcerr << L"Usage: bake_pathing.exe <Gw.dat> <output folder> [--map <map id>]\n";
        return 1;
    }
    try {
        const DatArchive dat(argv[1]);
        if (!dat.Valid()) throw std::runtime_error("could not open GW DAT index");
        auto maps = Maps();
        if (argc == 5) {
            const int wanted = std::stoi(argv[4]);
            std::erase_if(maps, [wanted](const Map& map) { return map.id != wanted; });
            if (maps.empty()) throw std::runtime_error("map has no bakeable world-map file ID");
        }
        std::map<int, TileSets> continents;
        for (size_t start = 0; start < maps.size(); start += 10) {
            std::vector<std::future<Result>> batch;
            for (size_t i = start; i < std::min(start + 10, maps.size()); i++) {
                batch.emplace_back(std::async(std::launch::async, [&dat, map = maps[i]] { return Bake(map, dat); }));
            }
            for (auto& job : batch) {
                auto baked = job.get();
                if (!baked.success) throw std::runtime_error("map " + std::to_string(baked.map.id) + " failed to bake");
                if (argc == 5)
                    std::wcout << L"map " << baked.map.id << L": " << baked.elapsed_ms << L" ms\n";
                auto& aggregate = continents[baked.map.continent];
                for (size_t kind = 0; kind < 6; kind++)
                    aggregate[kind].insert(baked.masks[kind].begin(), baked.masks[kind].end());
            }
            std::wcout << std::min(start + 10, maps.size()) << L'/' << maps.size() << L" maps baked\n";
        }
        std::vector<Mask> masks;
        for (const auto& [continent, data] : continents) {
            for (size_t kind = 0; kind < data.size(); kind++) masks.push_back(Pack(continent, static_cast<int>(kind), data[kind]));
        }
        const std::filesystem::path folder(argv[2]);
        std::filesystem::create_directories(folder);
        for (const auto& mask : masks) Write(folder, mask);
        WriteHeader(folder, masks);
        return 0;
    }
    catch (const std::exception& error) {
        std::wcerr << error.what() << L'\n';
        return 1;
    }
}
