#include "stdafx.h"

#include <Windows/Pathfinding/OpenTyriaPathfinder.h>

#include <cstring>
#include <cmath>
#include <cassert>
#include <cfloat>

namespace OpenTyria {

    namespace {
        inline Vec2f To2(const GW::GamePos& p) { return {p.x, p.y}; }

        inline Vec2f V2Sub(Vec2f a, Vec2f b) { return {a.x - b.x, a.y - b.y}; }
        inline Vec2f V2Add(Vec2f a, Vec2f b) { return {a.x + b.x, a.y + b.y}; }
        inline float V2Cross(Vec2f a, Vec2f b) { return a.x * b.y - a.y * b.x; }
        inline float V2Dot(Vec2f a, Vec2f b) { return a.x * b.x + a.y * b.y; }
        inline Vec2f V2Div(Vec2f v, float s) { return {v.x / s, v.y / s}; }
        inline bool V2IsZero(Vec2f v) { return v.x == 0.f && v.y == 0.f; }
        inline bool V2Equal(Vec2f a, Vec2f b) { return a.x == b.x && a.y == b.y; }
        inline float fclampf(float v, float lo, float hi) { return fmaxf(fminf(v, hi), lo); }

        // Fast 1/x lookup — verbatim from OpenTyria. Mirrors the in-game routine
        // including its divide-by-zero behavior (returns ~1.7e38 for x=0).
        float PathReciproc(float value)
        {
            static const uint32_t LookupTable[256] = {
                0x007F8040, 0x007E823D, 0x007D8631, 0x007C8C16, 0x007B93E6, 0x007A9D9D, 0x0079A934, 0x0078B6A6,
                0x0077C5EE, 0x0076D705, 0x0075E9E8, 0x0074FE91, 0x007414FA, 0x00732D1F, 0x007246FB, 0x00716289,
                0x00707FC4, 0x006F9EA8, 0x006EBF2F, 0x006DE156, 0x006D0518, 0x006C2A70, 0x006B515A, 0x006A79D1,
                0x0069A3D2, 0x0068CF59, 0x0067FC60, 0x00672AE4, 0x00665AE2, 0x00658C54, 0x0064BF38, 0x0063F389,
                0x00632943, 0x00626063, 0x006198E5, 0x0060D2C6, 0x00600E01, 0x005F4A93, 0x005E887A, 0x005DC7B0,
                0x005D0834, 0x005C4A01, 0x005B8D14, 0x005AD16A, 0x005A1700, 0x00595DD3, 0x0058A5DF, 0x0057EF21,
                0x00573997, 0x0056853D, 0x0055D210, 0x0055200D, 0x00546F32, 0x0053BF7C, 0x005310E7, 0x00526371,
                0x0051B717, 0x00510BD7, 0x005061AE, 0x004FB899, 0x004F1095, 0x004E69A0, 0x004DC3B8, 0x004D1ED9,
                0x004C7B02, 0x004BD830, 0x004B3660, 0x004A9590, 0x0049F5BF, 0x004956E8, 0x0048B90B, 0x00481C24,
                0x00478032, 0x0046E532, 0x00464B22, 0x0045B201, 0x004519CB, 0x0044827F, 0x0043EC1A, 0x0043569B,
                0x0042C1FF, 0x00422E45, 0x00419B6A, 0x0041096D, 0x0040784B, 0x003FE803, 0x003F5892, 0x003EC9F8,
                0x003E3C31, 0x003DAF3C, 0x003D2318, 0x003C97C2, 0x003C0D39, 0x003B837B, 0x003AFA86, 0x003A7258,
                0x0039EAF0, 0x0039644D, 0x0038DE6C, 0x0038594B, 0x0037D4EA, 0x00375147, 0x0036CE5F, 0x00364C32,
                0x0035CABE, 0x00354A01, 0x0034C9FA, 0x00344AA7, 0x0033CC07, 0x00334E19, 0x0032D0DA, 0x0032544A,
                0x0031D867, 0x00315D2F, 0x0030E2A2, 0x003068BE, 0x002FEF82, 0x002F76EB, 0x002EFEFA, 0x002E87AB,
                0x002E1100, 0x002D9AF5, 0x002D258A, 0x002CB0BD, 0x002C3C8D, 0x002BC8FA, 0x002B5601, 0x002AE3A1,
                0x002A71DA, 0x002A00AA, 0x00299010, 0x0029200B, 0x0028B099, 0x002841BA, 0x0027D36C, 0x002765AE,
                0x0026F880, 0x00268BDF, 0x00261FCC, 0x0025B445, 0x00254948, 0x0024DED5, 0x002474EB, 0x00240B89,
                0x0023A2AD, 0x00233A57, 0x0022D286, 0x00226B39, 0x0022046E, 0x00219E25, 0x0021385D, 0x0020D315,
                0x00206E4C, 0x00200A01, 0x001FA633, 0x001F42E1, 0x001EE00A, 0x001E7DAE, 0x001E1BCB, 0x001DBA61,
                0x001D596E, 0x001CF8F3, 0x001C98ED, 0x001C395D, 0x001BDA41, 0x001B7B99, 0x001B1D63, 0x001ABF9F,
                0x001A624D, 0x001A056A, 0x0019A8F7, 0x00194CF3, 0x0018F15D, 0x00189634, 0x00183B77, 0x0017E126,
                0x00178740, 0x00172DC4, 0x0016D4B2, 0x00167C08, 0x001623C7, 0x0015CBEC, 0x00157478, 0x00151D6A,
                0x0014C6C2, 0x0014707D, 0x00141A9D, 0x0013C51F, 0x00137005, 0x00131B4C, 0x0012C6F4, 0x001272FC,
                0x00121F65, 0x0011CC2C, 0x00117953, 0x001126D7, 0x0010D4B8, 0x001082F7, 0x00103191, 0x000FE087,
                0x000F8FD8, 0x000F3F83, 0x000EEF87, 0x000E9FE5, 0x000E509C, 0x000E01AA, 0x000DB310, 0x000D64CC,
                0x000D16DF, 0x000CC948, 0x000C7C05, 0x000C2F18, 0x000BE27E, 0x000B9638, 0x000B4A45, 0x000AFEA5,
                0x000AB356, 0x000A6859, 0x000A1DAC, 0x0009D350, 0x00098944, 0x00093F88, 0x0008F61A, 0x0008ACFB,
                0x0008642A, 0x00081BA6, 0x0007D36F, 0x00078B84, 0x000743E6, 0x0006FC93, 0x0006B58B, 0x00066ECD,
                0x0006285A, 0x0005E231, 0x00059C50, 0x000556B9, 0x0005116A, 0x0004CC63, 0x000487A3, 0x0004432A,
                0x0003FEF8, 0x0003BB0C, 0x00037766, 0x00033405, 0x0002F0E9, 0x0002AE12, 0x00026B7F, 0x0002292F,
                0x0001E723, 0x0001A559, 0x000163D3, 0x0001228E, 0x0000E18B, 0x0000A0C9, 0x00006048, 0x00002008,
            };
            uint32_t u32val;
            memcpy(&u32val, &value, sizeof(u32val));
            uint32_t tmp1 = (u32val & 0xFF800000) + 0x817FFFFF;
            uint32_t tmp2 = LookupTable[(u32val & 0x7FFFFF) >> 15];
            uint32_t tmp3 = tmp2 - tmp1;
            memcpy(&value, &tmp3, sizeof(value));
            return value;
        }

        float PathSqrt(float value)
        {
            static const uint32_t LookupTable[256] = {
                0x0035B99D, 0x00366D95, 0x003720DC, 0x0037D374, 0x00388560, 0x003936A0, 0x0039E737, 0x003A9728,
                0x003B4673, 0x003BF51A, 0x003CA320, 0x003D5086, 0x003DFD4D, 0x003EA978, 0x003F5509, 0x003FFFFF,
                0x0040AA5E, 0x00415427, 0x0041FD5C, 0x0042A5FD, 0x00434E0D, 0x0043F58C, 0x00449C7D, 0x004542E1,
                0x0045E8B8, 0x00468E05, 0x004732C9, 0x0047D705, 0x00487ABB, 0x00491DEB, 0x0049C098, 0x004A62C1,
                0x004B0469, 0x004BA591, 0x004C463A, 0x004CE664, 0x004D8612, 0x004E2544, 0x004EC3FC, 0x004F623A,
                0x004FFFFF, 0x00509D4E, 0x00513A26, 0x0051D689, 0x00527277, 0x00530DF3, 0x0053A8FC, 0x00544394,
                0x0054DDBC, 0x00557774, 0x005610BE, 0x0056A99B, 0x0057420B, 0x0057DA0F, 0x005871A9, 0x005908D8,
                0x00599F9F, 0x005A35FD, 0x005ACBF5, 0x005B6185, 0x005BF6B0, 0x005C8B76, 0x005D1FD8, 0x005DB3D7,
                0x005E4773, 0x005EDAAD, 0x005F6D86, 0x005FFFFF, 0x00609219, 0x006123D4, 0x0061B530, 0x0062462F,
                0x0062D6D2, 0x00636719, 0x0063F704, 0x00648694, 0x006515CB, 0x0065A4A8, 0x0066332D, 0x0066C15A,
                0x00674F2F, 0x0067DCAD, 0x006869D6, 0x0068F6A8, 0x00698326, 0x006A0F50, 0x006A9B26, 0x006B26A8,
                0x006BB1D8, 0x006C3CB7, 0x006CC743, 0x006D517F, 0x006DDB6A, 0x006E6506, 0x006EEE52, 0x006F7750,
                0x006FFFFF, 0x00708861, 0x00711076, 0x0071983E, 0x00721FBA, 0x0072A6EA, 0x00732DCF, 0x0073B469,
                0x00743AB9, 0x0074C0C0, 0x0075467D, 0x0075CBF2, 0x0076511E, 0x0076D602, 0x00775A9F, 0x0077DEF5,
                0x00786304, 0x0078E6CE, 0x00796A52, 0x0079ED90, 0x007A708A, 0x007AF33F, 0x007B75B1, 0x007BF7DF,
                0x007C79CA, 0x007CFB72, 0x007D7CD8, 0x007DFDFB, 0x007E7EDE, 0x007EFF7F, 0x007F7FDF, 0x007FFFFF,
                0x00807FC0, 0x0080FF01, 0x00817DC6, 0x0081FC0F, 0x008279DE, 0x0082F734, 0x00837412, 0x0083F07B,
                0x00846C6E, 0x0084E7EE, 0x008562FB, 0x0085DD98, 0x008657C4, 0x0086D182, 0x00874AD2, 0x0087C3B6,
                0x00883C2E, 0x0088B43D, 0x00892BE2, 0x0089A31F, 0x008A19F6, 0x008A9066, 0x008B0672, 0x008B7C19,
                0x008BF15E, 0x008C6641, 0x008CDAC2, 0x008D4EE4, 0x008DC2A7, 0x008E360B, 0x008EA912, 0x008F1BBC,
                0x008F8E0B, 0x00900000, 0x0090719A, 0x0090E2DB, 0x009153C4, 0x0091C456, 0x00923490, 0x0092A475,
                0x00931405, 0x00938341, 0x0093F228, 0x009460BD, 0x0094CF00, 0x00953CF1, 0x0095AA92, 0x009617E2,
                0x009684E3, 0x0096F196, 0x00975DFA, 0x0097CA11, 0x009835DB, 0x0098A159, 0x00990C8C, 0x00997773,
                0x0099E211, 0x009A4C64, 0x009AB66F, 0x009B2031, 0x009B89AB, 0x009BF2DE, 0x009C5BCA, 0x009CC470,
                0x009D2CD0, 0x009D94EC, 0x009DFCC2, 0x009E6454, 0x009ECBA3, 0x009F32AF, 0x009F9978, 0x00A00000,
                0x00A06645, 0x00A0CC4A, 0x00A1320E, 0x00A19792, 0x00A1FCD6, 0x00A261DC, 0x00A2C6A2, 0x00A32B2B,
                0x00A38F75, 0x00A3F382, 0x00A45753, 0x00A4BAE6, 0x00A51E3E, 0x00A5815A, 0x00A5E43B, 0x00A646E1,
                0x00A6A94D, 0x00A70B7E, 0x00A76D77, 0x00A7CF35, 0x00A830BC, 0x00A89209, 0x00A8F31F, 0x00A953FD,
                0x00A9B4A4, 0x00AA1513, 0x00AA754D, 0x00AAD550, 0x00AB351D, 0x00AB94B4, 0x00ABF417, 0x00AC5345,
                0x00ACB23E, 0x00AD1103, 0x00AD6F95, 0x00ADCDF3, 0x00AE2C1D, 0x00AE8A15, 0x00AEE7DB, 0x00AF456E,
                0x00AFA2D0, 0x00B00000, 0x00B05CFE, 0x00B0B9CC, 0x00B11669, 0x00B172D6, 0x00B1CF13, 0x00B22B20,
                0x00B286FD, 0x00B2E2AC, 0x00B33E2B, 0x00B3997C, 0x00B3F49F, 0x00B44F93, 0x00B4AA5A, 0x00B504F3,
            };
            uint32_t u32val;
            memcpy(&u32val, &value, sizeof(u32val));
            uint32_t tmp1 = ((u32val >> 24) + 0x3F) << 23;
            uint32_t tmp2 = LookupTable[(u32val >> 16) & 0xFF];
            uint32_t tmp3 = tmp1 + tmp2;
            memcpy(&value, &tmp3, sizeof(value));
            return value;
        }

        inline float V2Dist(Vec2f a, Vec2f b)
        {
            float dx = a.x - b.x, dy = a.y - b.y;
            return PathSqrt(dx * dx + dy * dy);
        }

        inline Vec2f V2Unit(Vec2f v)
        {
            float n = V2Dot(v, v);
            return V2Div(v, PathSqrt(n));
        }

        // Min-heap by cost — verbatim port of GmPathHeap.c.
        void HeapPush(std::vector<PathHeapNode>& heap, PathHeapNode node)
        {
            heap.push_back(node);
            if (heap.size() < 2) return;
            auto* nodes = heap.data();
            size_t hole = heap.size() - 1;
            while (hole > 0) {
                size_t parent_idx = (hole - 1) >> 1;
                if (nodes[parent_idx].cost < node.cost) break;
                nodes[hole] = nodes[parent_idx];
                hole = parent_idx;
            }
            nodes[hole] = node;
        }

        bool HeapPop(std::vector<PathHeapNode>& heap, PathHeapNode& out)
        {
            if (heap.empty()) return false;
            size_t len = heap.size() - 1;
            auto* nodes = heap.data();
            PathHeapNode last_node = nodes[len];
            out = nodes[0];
            if (len == 0) {
                heap.pop_back();
                return true;
            }
            size_t idx = 0, hole = 0;
            const size_t last_full_depth_idx = len >> 1;
            while (idx < last_full_depth_idx) {
                idx = idx * 2 + 2;
                if (nodes[idx - 1].cost < nodes[idx].cost) --idx;
                if (last_node.cost <= nodes[idx].cost) break;
                nodes[hole] = nodes[idx];
                hole = idx;
            }
            if (idx == last_full_depth_idx && (len % 2) == 0) {
                if (nodes[len - 1].cost < last_node.cost) {
                    nodes[hole] = nodes[len - 1];
                    hole = len - 1;
                }
            }
            nodes[hole] = last_node;
            heap.pop_back();
            return true;
        }
    } // anonymous namespace

    TrapezoidPathfinder::TrapezoidPathfinder(const Pathing::PathingMapData* data)
        : m_data(data)
    {
        if (!m_data) return;
        // Assign a global 0..N-1 index to each trapezoid, walking planes in stored order.
        size_t total = 0;
        for (const auto& plane : m_data->planes) {
            for (uint32_t i = 0; i < plane.trapezoid_count; ++i) {
                trap_index.emplace(&plane.trapezoids[i], static_cast<uint32_t>(total++));
            }
        }
        total_traps = total;
    }

    uint32_t TrapezoidPathfinder::IndexOf(const GW::PathingTrapezoid* t) const
    {
        auto it = trap_index.find(t);
        return it == trap_index.end() ? UINT32_MAX : it->second;
    }

    size_t TrapezoidPathfinder::PlaneCount() const
    {
        return m_data ? m_data->planes.size() : 0;
    }

    uint32_t TrapezoidPathfinder::PlaneZAt(size_t i) const
    {
        return (m_data && i < m_data->planes.size()) ? m_data->planes[i].zplane : 0;
    }

    bool TrapezoidPathfinder::IsPlaneBlocked(uint32_t plane_idx) const
    {
        // O(1) lookup. The live game array is indexed by plane id with a
        // refcount value (non-zero = blocked). DAT-loaded maps have a null
        // pointer → all planes treated as unblocked.
        if (!m_data || !m_data->blockedPlanesPtr) return false;
        const auto& bp = *m_data->blockedPlanesPtr;
        if (plane_idx >= bp.size()) return false;
        return bp.get(plane_idx) && *bp.get(plane_idx) != 0;
    }


    namespace {
        // GWTB convention: pos.zplane is a direct index into mapData->planes[],
        // NOT a value to match against the plane's stored .zplane field. The
        // loader normalizes so planes[0] is the ground plane (raw zplane may be
        // UINT_MAX). See `FindTrapezoid` in Pathing.cpp for the existing AStar
        // path which uses the same convention.
        const Pathing::CopiedPathingMap* PlaneByZ(const Pathing::PathingMapData* data, uint32_t zplane_idx)
        {
            if (!data || zplane_idx >= data->planes.size()) return nullptr;
            return &data->planes[zplane_idx];
        }

        // Forward declarations within the anonymous namespace so helpers can
        // call each other without strict ordering.
        bool IsPointInTrap(const GW::GamePos& pos, const GW::PathingTrapezoid* t);

        const GW::PathingTrapezoid* WalkBspToTrap(const Pathing::CopiedPathingMap& plane, const GW::GamePos& pos)
        {
            const GW::Node* node = plane.root_node;
            while (node) {
                switch (node->type) {
                case 0: { // XNode
                    const auto* x = static_cast<const GW::XNode*>(node);
                    Vec2f tmp = V2Sub(To2(pos), {x->pos.x, x->pos.y});
                    float res = V2Cross({x->dir.x, x->dir.y}, tmp);
                    node = res < 0 ? x->left : x->right;
                    break;
                }
                case 1: { // YNode
                    const auto* y = static_cast<const GW::YNode*>(node);
                    if (pos.y < y->pos.y)        node = y->below;
                    else if (pos.y > y->pos.y)   node = y->above;
                    else if (pos.x < y->pos.x)   node = y->below;
                    else                          node = y->above;
                    break;
                }
                case 2: // SinkNode
                    return static_cast<const GW::SinkNode*>(node)->trapezoid;
                default:
                    return nullptr;
                }
            }
            return nullptr;
        }

        const GW::PathingTrapezoid* FindTrapezoid(const Pathing::PathingMapData* data, const GW::GamePos& pos)
        {
            const auto* plane = PlaneByZ(data, pos.zplane);
            if (!plane || !plane->root_node) return nullptr;
            return WalkBspToTrap(*plane, pos);
        }

        // Distance from point to a single line segment (squared not-needed —
        // we want absolute units to match GWTB's tolerance).
        float DistToSegment(Vec2f a, Vec2f b, Vec2f p)
        {
            Vec2f ab = V2Sub(b, a);
            float len2 = V2Dot(ab, ab);
            if (len2 == 0.f) {
                Vec2f d = V2Sub(p, a);
                return PathSqrt(V2Dot(d, d));
            }
            float t = V2Dot(V2Sub(p, a), ab) / len2;
            t = fclampf(t, 0.f, 1.f);
            Vec2f proj{a.x + t * ab.x, a.y + t * ab.y};
            Vec2f d = V2Sub(p, proj);
            return PathSqrt(V2Dot(d, d));
        }

        // Mirrors GWTB's FindClosestTrapezoid (Pathing.cpp:396): when no plane's
        // BSP returns a containing trap (e.g. for portal positions on the edge
        // of the walkable mesh), walk every plane's BSP with a tolerance and
        // snap to the trap with the smallest edge distance.
        const GW::PathingTrapezoid* FindClosestTrapezoid(const Pathing::PathingMapData* data, const GW::GamePos& pos)
        {
            const GW::PathingTrapezoid* exact = FindTrapezoid(data, pos);
            if (exact && IsPointInTrap(pos, exact)) return exact;

            constexpr float tolerance = 50.f;
            const GW::PathingTrapezoid* best = nullptr;
            float best_dist = INFINITY;
            std::vector<const GW::Node*> open;

            for (const auto& plane : data->planes) {
                if (!plane.root_node) continue;
                open.clear();
                open.push_back(plane.root_node);
                int cnt = 50000;
                while (!open.empty() && cnt--) {
                    const GW::Node* n = open.back();
                    open.pop_back();
                    if (!n) continue;
                    switch (n->type) {
                    case 0: {
                        const auto* xn = static_cast<const GW::XNode*>(n);
                        float dir_norm = PathSqrt(xn->dir.x * xn->dir.x + xn->dir.y * xn->dir.y);
                        if (dir_norm <= 0.f) dir_norm = 1.f;
                        float d = ((pos.y - xn->pos.y) * xn->dir.x - (pos.x - xn->pos.x) * xn->dir.y) / dir_norm;
                        if (d > tolerance) {
                            open.push_back(xn->right);
                        } else if (d < -tolerance) {
                            open.push_back(xn->left);
                        } else {
                            open.push_back(xn->left);
                            open.push_back(xn->right);
                        }
                        break;
                    }
                    case 1: {
                        const auto* yn = static_cast<const GW::YNode*>(n);
                        float d = pos.y - yn->pos.y;
                        if (d > tolerance) {
                            open.push_back(yn->above);
                        } else if (d < -tolerance) {
                            open.push_back(yn->below);
                        } else {
                            open.push_back(yn->above);
                            open.push_back(yn->below);
                        }
                        break;
                    }
                    case 2: {
                        const auto* sn = static_cast<const GW::SinkNode*>(n);
                        const GW::PathingTrapezoid* t = sn->trapezoid;
                        if (!t) break;
                        if (IsPointInTrap(pos, t)) return t;
                        Vec2f tl{t->XTL, t->YT}, tr{t->XTR, t->YT};
                        Vec2f bl{t->XBL, t->YB}, br{t->XBR, t->YB};
                        Vec2f p{pos.x, pos.y};
                        float d = std::min({
                            DistToSegment(tl, tr, p),
                            DistToSegment(tr, br, p),
                            DistToSegment(br, bl, p),
                            DistToSegment(bl, tl, p),
                        });
                        if (d < best_dist) { best_dist = d; best = t; }
                        break;
                    }
                    }
                }
            }
            return best;
        }

        // Trapezoid-containment test. Mirrors GWTB's IsOnPathingTrapezoid
        // (Pathing.cpp:63) which uses correct geometry:
        //   cross(edge_vec, point - edge_start)
        // The OpenTyria source has a subtly broken version using cross(pos, edge_vec)
        // — that only works if every edge passes through the origin, which obviously
        // isn't the case for world-coord trapezoids. The OT bug never bites in
        // normal flow (IsPointInTrap is only called on the unreachable-fallback
        // path), but it bit our containment-validation calls.
        bool IsPointInTrap(const GW::GamePos& pos, const GW::PathingTrapezoid* t)
        {
            if (t->YT < pos.y || t->YB > pos.y) return false;
            if (t->XBL > pos.x && t->XTL > pos.x) return false;
            if (t->XBR < pos.x && t->XTR < pos.x) return false;
            // a=top-left, b=bottom-left → ab = left edge from top to bottom
            // d=top-right, c=bottom-right → cd = right edge from bottom to top
            Vec2f a{t->XTL, t->YT}, b{t->XBL, t->YB};
            Vec2f c{t->XBR, t->YB}, d{t->XTR, t->YT};
            Vec2f ab = V2Sub(b, a), cd = V2Sub(d, c);
            Vec2f pa = V2Sub({pos.x, pos.y}, a);
            Vec2f pc = V2Sub({pos.x, pos.y}, c);
            constexpr float tolerance = -1.0f;
            if (V2Cross(ab, pa) < tolerance) return false;
            if (V2Cross(cd, pc) < tolerance) return false;
            return true;
        }

        void GetClosestPointOnLineSegment(Vec2f pos, Vec2f p1, Vec2f p2, float* closest_point_dist, Vec2f* closest_point)
        {
            Vec2f p = V2Sub(p2, p1);
            Vec2f v = V2Sub(pos, p1);
            if (p.x == 0.f && p.y == 0.f) {
                float norm = V2Dot(v, v);
                if (norm < *closest_point_dist) { *closest_point_dist = norm; *closest_point = p1; }
                return;
            }
            float coeff = V2Dot(v, p) / V2Dot(p, p);
            Vec2f proj;
            if (coeff >= 0.f && coeff <= 1.f) {
                proj.x = coeff * p.x + p1.x;
                proj.y = coeff * p.y + p1.y;
            } else if (coeff >= 1.f) {
                proj = p2;
            } else {
                proj = p1;
            }
            Vec2f oproj = V2Sub(pos, proj);
            float norm = V2Dot(oproj, oproj);
            if (norm < *closest_point_dist) { *closest_point_dist = norm; *closest_point = oproj; }
        }

        float DistToTrap(const GW::GamePos& pos, const GW::PathingTrapezoid* t)
        {
            if (IsPointInTrap(pos, t)) return 0.f;
            float dist = INFINITY;
            Vec2f point;
            Vec2f tl{t->XTL, t->YT}, tr{t->XTR, t->YT}, bl{t->XBL, t->YB}, br{t->XBR, t->YB};
            GetClosestPointOnLineSegment(To2(pos), tl, tr, &dist, &point);
            GetClosestPointOnLineSegment(To2(pos), tr, br, &dist, &point);
            GetClosestPointOnLineSegment(To2(pos), br, bl, &dist, &point);
            GetClosestPointOnLineSegment(To2(pos), bl, tl, &dist, &point);
            return dist;
        }

        void FindPointOnNextTrap(float min_left_x, float max_right_x, float next_y, Vec2f src, Vec2f dst, Vec2f* result)
        {
            float percent = -1.f;
            if (src.y != dst.y) {
                float divisor = PathReciproc(dst.y - src.y);
                percent = (next_y - src.y) * divisor;
            }
            if (percent < 0.f) {
                float src_x_clamp = fclampf(src.x, min_left_x, max_right_x);
                float dst_x_clamp = fclampf(dst.x, min_left_x, max_right_x);
                result->x = src_x_clamp * 0.8999999761581421f + dst_x_clamp * 0.1000000238418579f;
                result->y = next_y;
            } else {
                float new_x = (dst.x - src.x) * percent + src.x;
                result->x = fclampf(new_x, min_left_x, max_right_x);
                result->y = next_y;
            }
        }

        bool FindIntersectionPoint(Vec2f s1, Vec2f d1, Vec2f s2, Vec2f d2, float* t1, float* t2)
        {
            float d = V2Cross(d1, d2);
            if (d == 0.f) return false;
            Vec2f v = V2Sub(s1, s2);
            *t1 = V2Cross(d2, v) / d;
            *t2 = V2Cross(d1, v) / d;
            return true;
        }

        void PickNextPoint(Vec2f point1, Vec2f point2, Vec2f cur_pos, Vec2f dst_pos, Vec2f* result)
        {
            Vec2f cur_to_dst = V2Sub(dst_pos, cur_pos);
            Vec2f p1_to_p2 = V2Sub(point2, point1);
            float t1, t2;
            if (FindIntersectionPoint(cur_pos, cur_to_dst, point1, p1_to_p2, &t1, &t2) && t2 >= 0) {
                t2 = fminf(t2, 1.f);
                result->x = point1.x + p1_to_p2.x * t2;
                result->y = point1.y + p1_to_p2.y * t2;
            } else {
                float norm2 = V2Dot(p1_to_p2, p1_to_p2);
                float divisor = PathReciproc(norm2);
                t1 = V2Dot(point1, p1_to_p2) * divisor;
                t2 = V2Dot(point2, p1_to_p2) * divisor;
                t1 = fclampf(t1, 0.f, 1.f);
                t2 = fclampf(t2, 0.f, 1.f);
                float t = t2 * 0.1f + t1 * 0.9f;
                result->x = point1.x + p1_to_p2.x * t;
                result->y = point1.y + p1_to_p2.y * t;
            }
        }

        float V2PreciseCross(Vec2f a, Vec2f b)
        {
            if (V2IsZero(a) || V2IsZero(b)) return 0.f;
            float c = V2Cross(a, b);
            if (c > -0.01f && c < 0.01f) return 0.f;
            if (c <= -1.f || c >= 1.f) return c;
            Vec2f au = V2Unit(a), bu = V2Unit(b);
            c = V2Cross(au, bu);
            if (c <= -0.01f || c >= 0.01f) return c;
            return 0.f;
        }
    } // anonymous namespace

    // ─────────────────────────────────────────────────────────────────────────
    // The body of the algorithm. Kept inside the class so it can reach
    // m_data, nodes, prioq, steps, IndexOf().
    // ─────────────────────────────────────────────────────────────────────────

    namespace {
        struct PathFindPoint {
            GW::GamePos                pos;
            const GW::PathingTrapezoid* trap;
        };

        struct PathFindBound {
            PathFindPoint point;
            size_t        step_id;
            Vec2f         vec;
        };
    }

    // Helper struct local to Search — holds funnel/string-pull state. Free
    // function so it can take refs into the pathfinder's vectors.
    struct PathBuildHelper {
        TrapezoidPathfinder*       owner;
        std::vector<PathFindNode>* nodes;
        std::vector<PathBuildStep>* steps;
        std::vector<Waypoint>*     waypoints;
        PathFindBound left_bound;
        PathFindBound right_bound;
        PathFindPoint curr_start_point;
        uint16_t      current_plane;
        GW::GamePos   last_pos;
        PathFindPoint src_point;
        PathFindPoint dst_point;
    };

    namespace {
        void AddNode(
            std::vector<PathFindNode>& nodes,
            std::vector<PathHeapNode>& prioq,
            uint32_t trap_idx,
            PathFindNode* parent,
            const GW::PathingTrapezoid* trap,
            const GW::GamePos& pos,
            float current_cost,
            float estimated_cost)
        {
            // Raw pointer — skip MSVC debug bounds checks; `nodes` isn't resized during a search.
            PathFindNode& n = nodes.data()[trap_idx];
            n.closed = false;
            n.cost_to_node = current_cost;
            n.pos = pos;
            n.trap = trap;
            n.next = parent;
            HeapPush(prioq, {estimated_cost, trap_idx});
        }

        void VisitTrap(
            TrapezoidPathfinder& pf,
            std::vector<PathFindNode>& nodes,
            std::vector<PathHeapNode>& prioq,
            PathFindNode* curr_node,
            Vec2f dst_pos,
            const GW::PathingTrapezoid* neighbor,
            const GW::GamePos& cross_pos,
            float max_cost)
        {
            float dist_to_trap = V2Dist(To2(curr_node->pos), To2(cross_pos));
            float cost_to_trap = curr_node->cost_to_node + dist_to_trap;
            if (max_cost <= cost_to_trap) return;

            Vec2f bottom = {fclampf(dst_pos.x, neighbor->XBL, neighbor->XBR), neighbor->YB};
            float estimated_dest_bottom = V2Dist(bottom, dst_pos);
            Vec2f top = {fclampf(dst_pos.x, neighbor->XTL, neighbor->XTR), neighbor->YT};
            float estimated_dest_top = V2Dist(top, dst_pos);
            float best_estimate = fminf(estimated_dest_bottom, estimated_dest_top);
            if (neighbor->YB <= dst_pos.y && dst_pos.y <= neighbor->YT)
                best_estimate -= 0.009999999776482582f;

            float new_estimated_cost = cost_to_trap + best_estimate;
            uint32_t nidx = pf.IndexOf(neighbor);
            if (nidx == UINT32_MAX) return;
            AddNode(nodes, prioq, nidx, curr_node, neighbor, cross_pos, cost_to_trap, new_estimated_cost);
        }

        void VisitAbove(
            TrapezoidPathfinder& pf,
            std::vector<PathFindNode>& nodes,
            std::vector<PathHeapNode>& prioq,
            PathFindNode* curr_node,
            Vec2f dst_pos,
            const GW::PathingTrapezoid* neighbor,
            float max_cost)
        {
            const auto* trap = curr_node->trap;
            float xl = fmaxf(neighbor->XBL, trap->XTL);
            float xr = fminf(neighbor->XBR, trap->XTR);
            GW::GamePos cross_pos{};
            cross_pos.zplane = curr_node->pos.zplane;
            Vec2f cp;
            FindPointOnNextTrap(xl, xr, neighbor->YB, To2(curr_node->pos), dst_pos, &cp);
            cross_pos.x = cp.x; cross_pos.y = cp.y;
            VisitTrap(pf, nodes, prioq, curr_node, dst_pos, neighbor, cross_pos, max_cost);
        }

        void VisitBelow(
            TrapezoidPathfinder& pf,
            std::vector<PathFindNode>& nodes,
            std::vector<PathHeapNode>& prioq,
            PathFindNode* curr_node,
            Vec2f dst_pos,
            const GW::PathingTrapezoid* neighbor,
            float max_cost)
        {
            const auto* trap = curr_node->trap;
            float xl = fmaxf(neighbor->XTL, trap->XBL);
            float xr = fminf(neighbor->XTR, trap->XBR);
            GW::GamePos cross_pos{};
            cross_pos.zplane = curr_node->pos.zplane;
            Vec2f cp;
            FindPointOnNextTrap(xl, xr, neighbor->YT, To2(curr_node->pos), dst_pos, &cp);
            cross_pos.x = cp.x; cross_pos.y = cp.y;
            VisitTrap(pf, nodes, prioq, curr_node, dst_pos, neighbor, cross_pos, max_cost);
        }

        const GW::Portal* PortalByIdx(const Pathing::PathingMapData* data, uint16_t plane_id, uint16_t portal_id)
        {
            const auto* plane = PlaneByZ(data, plane_id);
            if (!plane || portal_id >= plane->portal_count) return nullptr;
            return &plane->portals[portal_id];
        }

        void VisitPortalSide(
            TrapezoidPathfinder& pf,
            const Pathing::PathingMapData* data,
            std::vector<PathFindNode>& nodes,
            std::vector<PathHeapNode>& prioq,
            PathFindNode* curr_node,
            Vec2f dst_pos,
            uint16_t portal_id,
            float max_cost,
            bool left_side)
        {
            const GW::Portal* portal = PortalByIdx(data, static_cast<uint16_t>(curr_node->pos.zplane), portal_id);
            if (!portal || !portal->pair) return;
            if (portal->flags & 0x4) return;
            if (pf.IsPlaneBlocked(portal->neighbor_plane)) return;

            const auto* curr_trap = curr_node->trap;
            const GW::Portal* pair = portal->pair;
            for (uint32_t idx = 0; idx < pair->count; ++idx) {
                const GW::PathingTrapezoid* portal_trap = pair->trapezoids[idx];
                uint32_t pidx = pf.IndexOf(portal_trap);
                if (pidx == UINT32_MAX) continue;
                if (nodes.data()[pidx].closed) continue;

                Vec2f point1, point2;
                if (left_side) {
                    if (portal_trap->YT <= curr_trap->YT) point1 = {portal_trap->XTR, portal_trap->YT};
                    else                                  point1 = {curr_trap->XTL, curr_trap->YT};
                    if (curr_trap->YB <= portal_trap->YB) point2 = {portal_trap->XBR, portal_trap->YB};
                    else                                   point2 = {curr_trap->XBL, curr_trap->YB};
                } else {
                    if (portal_trap->YT <= curr_trap->YT) point1 = {portal_trap->XTL, portal_trap->YT};
                    else                                  point1 = {curr_trap->XTR, curr_trap->YT};
                    if (curr_trap->YB <= portal_trap->YB) point2 = {portal_trap->XBL, portal_trap->YB};
                    else                                   point2 = {curr_trap->XBR, curr_trap->YB};
                }

                if (point2.y <= point1.y) {
                    GW::GamePos crossing_pos{};
                    crossing_pos.zplane = pair->portal_plane;
                    Vec2f cp;
                    PickNextPoint(point1, point2, To2(curr_node->pos), dst_pos, &cp);
                    crossing_pos.x = cp.x; crossing_pos.y = cp.y;
                    VisitTrap(pf, nodes, prioq, curr_node, dst_pos, portal_trap, crossing_pos, max_cost);
                }
            }
        }

        // Reverse the linked-list of PathFindNodes from head→null to head→...→null
        // becoming null←...←head, returning the original tail as the new head.
        PathFindNode* ReversePath(PathFindNode* head)
        {
            PathFindNode* prev = nullptr;
            while (head) {
                PathFindNode* next = head->next;
                head->next = prev;
                prev = head;
                head = next;
            }
            return prev;
        }

        void BuildAddWaypointAndReduce(PathBuildHelper& h, PathFindPoint new_point, Vec2f from_pos)
        {
            auto& steps = *h.steps;
            auto& waypoints = *h.waypoints;
            for (size_t idx = 0; idx < steps.size(); ++idx) {
                PathBuildStep step = steps[idx];
                Vec2f best_pos;
                if (!V2IsZero(step.dir)) {
                    float t, s;
                    Vec2f to_point = V2Sub(To2(new_point.pos), from_pos);
                    FindIntersectionPoint(step.pos, step.dir, from_pos, to_point, &t, &s);
                    if (t > 1.01f)        best_pos = V2Add(step.pos, step.dir);
                    else if (t < -0.01f)  best_pos = step.pos;
                    else                   best_pos = {step.pos.x + t * step.dir.x, step.pos.y + t * step.dir.y};
                } else {
                    best_pos = step.pos;
                }
                if (!V2Equal(To2(h.last_pos), best_pos) || h.last_pos.zplane != step.plane) {
                    h.last_pos.x = best_pos.x;
                    h.last_pos.y = best_pos.y;
                    h.last_pos.zplane = step.plane;
                    if (!waypoints.empty() && V2Equal(best_pos, To2(waypoints.back().pos))) {
                        waypoints.pop_back();
                    }
                    Waypoint wp;
                    wp.pos.x = best_pos.x; wp.pos.y = best_pos.y;
                    wp.pos.zplane = step.plane;
                    wp.trap_id = h.owner->IndexOf(step.next_trap);
                    waypoints.push_back(wp);
                }
            }
            steps.clear();
            if (!V2Equal(To2(h.last_pos), To2(new_point.pos))) {
                h.last_pos = new_point.pos;
                if (!waypoints.empty() && V2Equal(To2(new_point.pos), To2(waypoints.back().pos))) {
                    waypoints.pop_back();
                }
                Waypoint wp;
                wp.pos = new_point.pos;
                wp.trap_id = h.owner->IndexOf(new_point.trap);
                waypoints.push_back(wp);
            }
        }

        void BuildAddWaypoint(PathBuildHelper& h, Vec2f left, Vec2f right, PathFindNode** node, std::vector<PathFindNode>& nodes)
        {
            PathFindBound* bound_used;
            if (V2Equal(To2(h.left_bound.point.pos), left)) {
                bound_used = &h.left_bound;
                h.right_bound.point = h.left_bound.point;
                h.right_bound.step_id = h.left_bound.step_id;
                *node = &nodes[h.owner->IndexOf(h.left_bound.point.trap)];
            } else if (V2Equal(To2(h.right_bound.point.pos), right)) {
                bound_used = &h.right_bound;
                h.left_bound.point = h.right_bound.point;
                h.left_bound.step_id = h.right_bound.step_id;
                *node = &nodes[h.owner->IndexOf(h.right_bound.point.trap)];
            } else {
                float dx = left.x - h.curr_start_point.pos.x;
                float dy = left.y - h.curr_start_point.pos.y;
                float lval = dx * dx + dy * dy;
                dx = right.x - h.curr_start_point.pos.x;
                dy = right.y - h.curr_start_point.pos.y;
                float rval = dx * dx + dy * dy;
                bound_used = (lval < rval) ? &h.left_bound : &h.right_bound;
            }
            h.steps->resize(bound_used->step_id);
            BuildAddWaypointAndReduce(h, bound_used->point, To2(h.curr_start_point.pos));
            h.curr_start_point = bound_used->point;
            h.right_bound.vec = {0, 0};
            h.left_bound.vec = {0, 0};
            h.current_plane = static_cast<uint16_t>(bound_used->point.pos.zplane);
        }

        void BuildProcessNext(PathBuildHelper& h, Vec2f left, Vec2f right, const GW::PathingTrapezoid* next_trap, PathFindNode** node, std::vector<PathFindNode>& nodes)
        {
            Vec2f to_left = V2Sub(left, To2(h.curr_start_point.pos));
            Vec2f to_right = V2Sub(right, To2(h.curr_start_point.pos));

            PathFindBound new_left;
            if (V2PreciseCross(to_left, h.left_bound.vec) < 0.f) {
                to_left = h.left_bound.vec;
                new_left = h.left_bound;
            } else {
                new_left.vec = to_left;
                new_left.point.pos.x = left.x; new_left.point.pos.y = left.y;
                new_left.point.pos.zplane = h.current_plane;
                new_left.point.trap = next_trap;
                new_left.step_id = h.steps->size();
            }

            PathFindBound new_right;
            if (V2PreciseCross(to_right, h.right_bound.vec) > 0.f) {
                to_right = h.right_bound.vec;
                new_right = h.right_bound;
            } else {
                new_right.vec = to_right;
                new_right.point.pos.x = right.x; new_right.point.pos.y = right.y;
                new_right.point.pos.zplane = h.current_plane;
                new_right.point.trap = next_trap;
                new_right.step_id = h.steps->size();
            }

            if (V2IsZero(h.left_bound.vec) || V2PreciseCross(to_left, to_right) <= 0.f) {
                h.left_bound = new_left;
                h.right_bound = new_right;
                *node = (*node)->next;
            } else {
                Vec2f ltmp = {new_left.point.pos.x, new_left.point.pos.y};
                Vec2f rtmp = {new_right.point.pos.x, new_right.point.pos.y};
                BuildAddWaypoint(h, ltmp, rtmp, node, nodes);
            }
        }

        bool AddLast(PathBuildHelper& h, PathFindNode** node, PathFindPoint new_point, std::vector<PathFindNode>& nodes)
        {
            Vec2f to_new = V2Sub(To2(new_point.pos), To2(h.curr_start_point.pos));
            if (V2PreciseCross(to_new, h.left_bound.vec) >= 0.f) {
                if (V2PreciseCross(to_new, h.right_bound.vec) <= 0.f) {
                    BuildAddWaypointAndReduce(h, new_point, To2(h.curr_start_point.pos));
                    return true;
                } else {
                    h.right_bound.vec = {0, 0};
                    h.left_bound.point = h.right_bound.point;
                    *node = &nodes[h.owner->IndexOf(h.right_bound.point.trap)];
                    h.steps->resize(h.right_bound.step_id);
                    BuildAddWaypointAndReduce(h, h.right_bound.point, To2(h.curr_start_point.pos));
                    h.curr_start_point = h.right_bound.point;
                    h.current_plane = static_cast<uint16_t>(h.right_bound.point.pos.zplane);
                    return false;
                }
            } else {
                h.left_bound.vec = {0, 0};
                h.right_bound.point = h.left_bound.point;
                *node = &nodes[h.owner->IndexOf(h.left_bound.point.trap)];
                h.steps->resize(h.left_bound.step_id);
                BuildAddWaypointAndReduce(h, h.left_bound.point, To2(h.curr_start_point.pos));
                h.curr_start_point = h.left_bound.point;
                h.current_plane = static_cast<uint16_t>(h.left_bound.point.pos.zplane);
                return false;
            }
        }

        bool GetPortalAndCheckTrapAdjacent(
            const Pathing::PathingMapData* data,
            uint16_t plane_id, uint32_t portal_id,
            const GW::PathingTrapezoid* trap, const GW::Portal** result)
        {
            if (portal_id == 0xFFFF) return false;
            const GW::Portal* portal = PortalByIdx(data, plane_id, static_cast<uint16_t>(portal_id));
            if (!portal || !portal->pair) return false;
            const GW::Portal* pair = portal->pair;
            for (uint32_t i = 0; i < pair->count; ++i) {
                if (pair->trapezoids[i] == trap) { *result = portal; return true; }
            }
            return false;
        }

        void CreateWaypoints(
            TrapezoidPathfinder& pf,
            const Pathing::PathingMapData* data,
            std::vector<PathFindNode>& nodes,
            std::vector<PathBuildStep>& steps,
            std::vector<Waypoint>& waypoints,
            PathFindPoint src_point, PathFindPoint dst_point)
        {
            // OpenTyria used 0xBB8 (3000) as an assert-only safeguard. Bumped to
            // 100000 so we never bail mid-path on large explorables and emit a
            // truncated waypoint chain (which would underestimate distance).
            constexpr int MAX_LOOP = 100000;
            PathBuildHelper h{};
            h.owner = &pf;
            h.nodes = &nodes;
            h.steps = &steps;
            h.waypoints = &waypoints;
            h.left_bound.point = src_point;
            h.right_bound.point = src_point;
            h.curr_start_point = src_point;
            h.current_plane = static_cast<uint16_t>(src_point.pos.zplane);
            h.src_point = src_point;
            h.dst_point = dst_point;

            steps.clear();
            uint32_t src_idx = pf.IndexOf(src_point.trap);
            if (src_idx == UINT32_MAX) return;
            PathFindNode* curr_node = &nodes[src_idx];

            for (int loop = 0;; ++loop) {
                if (loop >= MAX_LOOP) return; // bail out instead of asserting

                const GW::PathingTrapezoid* curr_trap = curr_node->trap;
                PathFindNode* next_node = curr_node->next;
                if (!next_node || curr_trap == dst_point.trap) {
                    if (AddLast(h, &curr_node, dst_point, nodes)) break;
                    continue;
                }
                const GW::PathingTrapezoid* next_trap = next_node->trap;
                if (next_trap == curr_trap->top_left || next_trap == curr_trap->top_right) {
                    Vec2f left{fmaxf(curr_trap->XTL, next_trap->XBL), curr_trap->YT};
                    Vec2f right{fminf(curr_trap->XTR, next_trap->XBR), curr_trap->YT};
                    BuildProcessNext(h, left, right, next_trap, &curr_node, nodes);
                } else if (next_trap == curr_trap->bottom_left || next_trap == curr_trap->bottom_right) {
                    Vec2f left{fmaxf(curr_trap->XBL, next_trap->XTL), curr_trap->YB};
                    Vec2f right{fminf(curr_trap->XBR, next_trap->XTR), curr_trap->YB};
                    BuildProcessNext(h, right, left, next_trap, &curr_node, nodes);
                } else {
                    const GW::Portal* portal = nullptr;
                    if (GetPortalAndCheckTrapAdjacent(data, h.current_plane, curr_trap->portal_right, next_trap, &portal)) {
                        Vec2f top, bottom;
                        if (next_trap->YT <= curr_trap->YT) top = {next_trap->XTL, next_trap->YT};
                        else                                 top = {curr_trap->XTR, curr_trap->YT};
                        if (curr_trap->YB <= next_trap->YB) bottom = {next_trap->XBL, next_trap->YB};
                        else                                 bottom = {curr_trap->XBR, curr_trap->YB};
                        h.current_plane = portal->neighbor_plane;
                        PathBuildStep step;
                        step.pos = top;
                        step.dir = {bottom.x - top.x, bottom.y - top.y};
                        step.plane = h.current_plane;
                        step.next_trap = next_trap;
                        steps.push_back(step);
                        BuildProcessNext(h, top, bottom, next_trap, &curr_node, nodes);
                    } else if (GetPortalAndCheckTrapAdjacent(data, h.current_plane, curr_trap->portal_left, next_trap, &portal)) {
                        Vec2f top, bottom;
                        if (next_trap->YT <= curr_trap->YT) top = {next_trap->XTR, next_trap->YT};
                        else                                 top = {curr_trap->XTL, curr_trap->YT};
                        if (curr_trap->YB <= next_trap->YB) bottom = {next_trap->XBR, next_trap->YB};
                        else                                 bottom = {curr_trap->XBL, curr_trap->YB};
                        h.current_plane = portal->neighbor_plane;
                        PathBuildStep step;
                        step.pos = top;
                        step.dir = {bottom.x - top.x, bottom.y - top.y};
                        step.plane = h.current_plane;
                        step.next_trap = next_trap;
                        steps.push_back(step);
                        BuildProcessNext(h, bottom, top, next_trap, &curr_node, nodes);
                    } else {
                        return; // disconnected — stop
                    }
                }
            }
        }
    } // anonymous namespace

    const char* SearchResultName(SearchResult r)
    {
        switch (r) {
        case SearchResult::OK:                    return "OK";
        case SearchResult::InvalidData:           return "InvalidData";
        case SearchResult::SrcTrapezoidNotFound:  return "SrcTrapNotFound";
        case SearchResult::DstTrapezoidNotFound:  return "DstTrapNotFound";
        case SearchResult::Unreachable:           return "Unreachable";
        }
        return "?";
    }

    int TrapezoidPathfinder::FindContainingPlane(const GW::GamePos& pos) const
    {
        if (!m_data) return -1;
        for (size_t i = 0; i < m_data->planes.size(); ++i) {
            const auto& plane = m_data->planes[i];
            if (!plane.root_node) continue;
            const GW::PathingTrapezoid* t = WalkBspToTrap(plane, pos);
            if (t && IsPointInTrap(pos, t)) return static_cast<int>(i);
        }
        return -1;
    }

    SearchResult TrapezoidPathfinder::Search(const GW::GamePos& src, const GW::GamePos& dst, std::vector<Waypoint>& out_waypoints)
    {
        out_waypoints.clear();
        if (!m_data || total_traps == 0) return SearchResult::InvalidData;

        // Bumped from OpenTyria's in-game constant of 10000 — that capped large
        // explorables (e.g. Maguuma) where portals can be 20-30k units apart and
        // forced a Euclidean fallback for valid long walks.
        const float MAX_COST = 100000.f;

        // Use the closest-snap variant to handle portal positions that sit on
        // the edge of the walkable mesh — same fallback GWTB's visgraph AStar
        // uses (see FindClosestTrapezoid in Pathing.cpp).
        const GW::PathingTrapezoid* src_trap = FindClosestTrapezoid(m_data, src);
        if (!src_trap) return SearchResult::SrcTrapezoidNotFound;
        const GW::PathingTrapezoid* dst_trap = FindClosestTrapezoid(m_data, dst);
        if (!dst_trap) return SearchResult::DstTrapezoidNotFound;

        if (src_trap == dst_trap) {
            Waypoint wp;
            wp.pos = dst;
            wp.trap_id = IndexOf(src_trap);
            out_waypoints.push_back(wp);
            return SearchResult::OK;
        }

        prioq.clear();
        nodes.assign(total_traps, PathFindNode{});

        PathFindPoint closest_point{};
        float closest_point_dist = INFINITY;

        // Guard before AddNode indexes nodes[]: FindClosestTrapezoid normally returns an indexed trap, but a
        // bad/partial map could miss. Explicit here since AddNode now writes through nodes.data() (no debug check).
        const uint32_t src_idx = IndexOf(src_trap);
        if (src_idx == UINT32_MAX) return SearchResult::SrcTrapezoidNotFound;
        AddNode(nodes, prioq, src_idx, nullptr, src_trap, src, 0.f, INFINITY);

        PathHeapNode top;
        // Raw pointer — skip MSVC debug bounds checks; `nodes` is sized once above and never resized here.
        PathFindNode* const N = nodes.data();
        while (HeapPop(prioq, top)) {
            PathFindNode* curr_node = &N[top.trap_id];
            curr_node->closed = true;
            const GW::PathingTrapezoid* curr_trap = curr_node->trap;

            if (curr_trap == dst_trap) {
                ReversePath(curr_node);
                PathFindPoint sp{src, src_trap}, dp{dst, dst_trap};
                CreateWaypoints(*this, m_data, nodes, steps, out_waypoints, sp, dp);
                return SearchResult::OK;
            }

            float d = DistToTrap(dst, curr_trap);
            if (d < closest_point_dist) {
                closest_point_dist = d;
                closest_point.trap = curr_trap;
                closest_point.pos.zplane = curr_node->pos.zplane;
            }

            Vec2f dst_pos = To2(dst);
            auto try_visit_above = [&](const GW::PathingTrapezoid* nb) {
                if (!nb) return;
                uint32_t nidx = IndexOf(nb);
                if (nidx == UINT32_MAX || !N[nidx].closed)
                    VisitAbove(*this, nodes, prioq, curr_node, dst_pos, nb, MAX_COST);
            };
            auto try_visit_below = [&](const GW::PathingTrapezoid* nb) {
                if (!nb) return;
                uint32_t nidx = IndexOf(nb);
                if (nidx == UINT32_MAX || !N[nidx].closed)
                    VisitBelow(*this, nodes, prioq, curr_node, dst_pos, nb, MAX_COST);
            };

            try_visit_above(curr_trap->top_left);
            try_visit_above(curr_trap->top_right);
            try_visit_below(curr_trap->bottom_left);
            try_visit_below(curr_trap->bottom_right);

            if (curr_trap->portal_left != 0xFFFF)
                VisitPortalSide(*this, m_data, nodes, prioq, curr_node, dst_pos, curr_trap->portal_left, MAX_COST, true);
            if (curr_trap->portal_right != 0xFFFF)
                VisitPortalSide(*this, m_data, nodes, prioq, curr_node, dst_pos, curr_trap->portal_right, MAX_COST, false);
        }

        // Couldn't reach dst — return partial path to closest reachable trap
        // (matches OpenTyria's fallback semantics).
        if (closest_point.trap && IsPointInTrap(dst, closest_point.trap)) {
            closest_point.pos = dst;
            uint32_t cidx = IndexOf(closest_point.trap);
            if (cidx != UINT32_MAX) {
                ReversePath(&nodes[cidx]);
                PathFindPoint sp{src, src_trap};
                CreateWaypoints(*this, m_data, nodes, steps, out_waypoints, sp, closest_point);
            }
        }
        return SearchResult::Unreachable;
    }

    float TrapezoidPathfinder::SearchDistance(const GW::GamePos& src, const GW::GamePos& dst, SearchResult* out_reason)
    {
        std::vector<Waypoint> wps;
        SearchResult r = Search(src, dst, wps);
        if (out_reason) *out_reason = r;
        if (r != SearchResult::OK) return INFINITY;
        // Sum segment lengths from src through waypoints.
        float total = 0.f;
        Vec2f prev = To2(src);
        for (const auto& wp : wps) {
            Vec2f cur = To2(wp.pos);
            total += V2Dist(prev, cur);
            prev = cur;
        }
        return total;
    }

} // namespace OpenTyria
