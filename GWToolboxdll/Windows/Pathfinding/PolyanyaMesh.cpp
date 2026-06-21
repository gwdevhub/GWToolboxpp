#include "stdafx.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include <GWCA/GameEntities/Pathing.h>

#include <Logger.h>

#include "PolyanyaMesh.h"

// =============================================================================
// PolyanyaMesh
//
// Polyanya migration, phase A: a CONFORMING convex navmesh built from GW's
// trapezoid decomposition. Each non-degenerate trapezoid becomes one convex
// polygon. Same-plane walkable connections (captured by the visgraph as
// PortalAdjacency) are turned into shared ring edges so that adjacent polygons
// reference each other through exactly-shared vertices (the conforming
// property Polyanya requires). Cross-plane connections and teleports become
// off-mesh InterPlaneLinks.
//
// We consume the captured adjacency directly — we never re-derive neighbours
// from raw GW trapezoid pointers here. The visgraph already did that work and
// recorded the exact shared interval [a,b] plus per-endpoint viability.
//
// Winding convention: CCW in GW's (x, y) plane, i.e. positive shoelace area
// (2A = Σ x_i·y_{i+1} − x_{i+1}·y_i). Polyanya treats polygons as CCW so that
// the interior lies to the left of each directed edge. When a built ring comes
// out CW we reverse vertex_ids and edge_neighbour in lockstep so neighbour i
// stays attached to edge i.
// =============================================================================

namespace {
    using namespace Pathing;

    // Mirrors NavMesh.cpp: a trapezoid whose top and bottom edges nearly coincide
    // has no area and is skipped (these are GW's connective "zero-height" traps).
    inline bool IsDegenerate(const GW::PathingTrapezoid& t) { return std::fabs(t.YT - t.YB) < 1.0f; }

    // Minimum length for a ring sub-segment to be emitted. Shorter slivers are
    // dropped so coincident cut endpoints don't spawn zero-area edges.
    constexpr float kMinSeg = 0.5f;

    // Dedup epsilon: vertices within this distance (per axis, after rounding)
    // collapse to one canonical index. Small enough that distinct trapezoid
    // corners never merge, large enough that the identical shared-interval
    // endpoints recorded from two traps round to the same key.
    constexpr float kVertEps = 0.01f;

    // One sub-interval recorded on a trapezoid edge: the connection covers
    // [a,b] along that edge and leads to neighbour poly `neighbour`.
    struct CutInterval {
        GW::Vec2f a, b;
        int32_t neighbour;
        bool a_viable, b_viable;
    };

    // The 4 corners of a trapezoid in the fixed ring order the builder walks:
    //   0 = TL (XTL,YT)  1 = TR (XTR,YT)  2 = BR (XBR,YB)  3 = BL (XBL,YB)
    // Edges, in order, are TL->TR (top), TR->BR (right), BR->BL (bottom),
    // BL->TL (left); this matches the Edge enum index used by the visgraph.
    inline GW::Vec2f Corner(const GW::PathingTrapezoid& t, int i)
    {
        switch (i) {
            case 0: return {t.XTL, t.YT};
            case 1: return {t.XTR, t.YT};
            case 2: return {t.XBR, t.YB};
            default: return {t.XBL, t.YB};
        }
    }

    // Which of trapezoid t's 4 edges geometrically contains segment [a,b] (both endpoints on the
    // edge line, within its extent)? Returns 0..3, or -1 if the interval lies on no edge of t (a
    // non-coincident "portal" connection rather than a true shared edge). Edge-label-independent.
    inline int WhichEdge(const GW::PathingTrapezoid& t, const GW::Vec2f& a, const GW::Vec2f& b)
    {
        // True if p projects within the edge's extent; reports its perpendicular distance.
        auto onSeg = [](const GW::Vec2f& c0, const GW::Vec2f& c1, const GW::Vec2f& p, float& perp) -> bool {
            const float ex = c1.x - c0.x, ey = c1.y - c0.y;
            const float len2 = ex * ex + ey * ey;
            if (len2 < 1e-3f) return false;
            const float tt = ((p.x - c0.x) * ex + (p.y - c0.y) * ey) / len2;
            perp = std::fabs((p.x - c0.x) * ey - (p.y - c0.y) * ex) / std::sqrt(len2);
            return tt > -0.02f && tt < 1.02f;
        };
        // Pick the edge with the SMALLEST perpendicular distance (not the first within tolerance),
        // so an interval near a shared corner isn't misassigned to the adjacent edge.
        int best = -1;
        float best_perp = 2.0f;
        for (int e = 0; e < 4; ++e) {
            const GW::Vec2f c0 = Corner(t, e), c1 = Corner(t, (e + 1) & 3);
            float pa_ = 0, pb_ = 0;
            if (onSeg(c0, c1, a, pa_) && onSeg(c0, c1, b, pb_)) {
                const float m = std::max(pa_, pb_);
                if (m < best_perp) { best_perp = m; best = e; }
            }
        }
        return best;
    }

    // BSP descent on one plane (mirrors Pathing.cpp FindTrapezoid). Returns the
    // containing trapezoid or nullptr.
    const GW::PathingTrapezoid* WalkBsp(const CopiedPathingMap& pm, const GW::GamePos& p)
    {
        const GW::Node* n = pm.root_node;
        int cnt = 50000;
        while (n && cnt--) {
            switch (n->type) {
                case 0: {
                    const auto* xn = static_cast<const GW::XNode*>(n);
                    const float d = (p.y - xn->pos.y) * xn->dir.x - (p.x - xn->pos.x) * xn->dir.y;
                    n = (d >= 0.0f) ? xn->right : xn->left;
                    break;
                }
                case 1: {
                    const auto* yn = static_cast<const GW::YNode*>(n);
                    if (p.y > yn->pos.y) n = yn->above;
                    else if (p.y < yn->pos.y) n = yn->below;
                    else n = (p.x >= yn->pos.x) ? yn->above : yn->below;
                    break;
                }
                case 2:
                    return static_cast<const GW::SinkNode*>(n)->trapezoid;
                default:
                    return nullptr;
            }
        }
        return nullptr;
    }
} // namespace

namespace Pathing {

    int32_t PolyanyaMesh::PolyOfTrapezoid(const GW::PathingTrapezoid* t) const
    {
        const auto it = m_trap_to_poly.find(t);
        return it == m_trap_to_poly.end() ? -1 : (int32_t)it->second;
    }

    bool PolyanyaMesh::Build(const PathingMapData& data, const MapSpecific::Teleports& teleports,
                             const std::vector<PortalAdjacency>& adjacency)
    {
        m_ready = false;
        m_verts.clear();
        m_polys.clear();
        m_links.clear();
        m_plane_polys.clear();
        m_trap_to_poly.clear();

        const size_t plane_count = data.planes.size();
        m_plane_polys.resize(plane_count);

        // --- 1. Polys: one per non-degenerate trapezoid -----------------------
        for (uint16_t pi = 0; pi < (uint16_t)plane_count; ++pi) {
            const auto& plane = data.planes[pi];
            for (uint32_t ti = 0; ti < plane.trapezoid_count; ++ti) {
                const GW::PathingTrapezoid* t = &plane.trapezoids[ti];
                if (IsDegenerate(*t)) continue;
                const uint32_t poly_index = (uint32_t)m_polys.size();
                m_polys.push_back(ConvexPoly{pi, {}, {}});
                m_trap_to_poly[t] = poly_index;
                m_plane_polys[pi].push_back(poly_index);
            }
        }

        // --- 2. Per-(trap, edge) cut lists from same-plane adjacency ----------
        // cuts[poly_index][edge 0..3] = sub-intervals covering that edge.
        std::vector<std::array<std::vector<CutInterval>, 4>> cuts(m_polys.size());

        [[maybe_unused]] int diag_ea_fb = 0, diag_eb_fb = 0;
        for (const auto& pa : adjacency) {
            if (pa.layer_a != pa.layer_b) continue; // cross-plane handled as a link
            const int32_t pia = PolyOfTrapezoid(pa.trap_a);
            const int32_t pib = PolyOfTrapezoid(pa.trap_b);
            if (pia < 0 || pib < 0) continue; // a degenerate (unmeshed) endpoint
            // GW's portal edge LABEL is unreliable (it can call a geometric bottom-edge seam a "left"
            // portal), so place BOTH cuts on the edge that geometrically contains [a,b]; fall back to the
            // label only if geometry can't decide (shouldn't happen — every interval lies on both traps).
            int ea = WhichEdge(*pa.trap_a, pa.a, pa.b);
            int eb = WhichEdge(*pa.trap_b, pa.a, pa.b);
            if (ea < 0) { ea = (int)(pa.edge_a & 3u); ++diag_ea_fb; }
            if (eb < 0) { eb = (int)((pa.edge_a + 2u) & 3u); ++diag_eb_fb; }
            cuts[pia][(uint8_t)ea].push_back({pa.a, pa.b, pib, pa.a_viable, pa.b_viable});
            cuts[pib][(uint8_t)eb].push_back({pa.a, pa.b, pia, pa.a_viable, pa.b_viable});
        }

        // --- vertex dedup (no quantization: key rounds, store keeps exact) -----
        // Key by (plane, x, y) rounded to kVertEps. Shared interval endpoints have
        // identical exact coords on both traps, so they round to one key -> one id.
        std::unordered_map<uint64_t, uint32_t> vmap;
        vmap.reserve(m_polys.size() * 4);
        auto vkey = [](uint16_t plane, float x, float y) -> uint64_t {
            const int64_t ix = (int64_t)std::llround((double)x / kVertEps);
            const int64_t iy = (int64_t)std::llround((double)y / kVertEps);
            // mix the three components into 64 bits; ranges are well within budget
            uint64_t k = (uint64_t)plane;
            k = k * 0x9E3779B97F4A7C15ull + (uint64_t)(ix & 0xFFFFFFFFll);
            k = k * 0x9E3779B97F4A7C15ull + (uint64_t)(iy & 0xFFFFFFFFll);
            return k;
        };
        auto addVert = [&](uint16_t plane, float x, float y, bool viable) -> uint32_t {
            const uint64_t key = vkey(plane, x, y);
            const auto it = vmap.find(key);
            if (it != vmap.end()) {
                if (viable) m_verts[it->second].is_viable = true; // OR-in viability
                return it->second;
            }
            const uint32_t id = (uint32_t)m_verts.size();
            m_verts.push_back(PolyVertex{x, y, plane, viable});
            vmap.emplace(key, id);
            return id;
        };

        // --- 3. Conforming ring per poly --------------------------------------
        // Walk edges in the fixed corner order; for each edge collect the cut
        // endpoints plus the two corners, sort along the edge, and emit one ring
        // vertex per unique point. Each sub-segment's neighbour is the cut whose
        // [min,max] covers the sub-segment midpoint (-1 = wall otherwise).
        size_t poly_index = 0;
        for (uint16_t pi = 0; pi < (uint16_t)plane_count; ++pi) {
            const auto& plane = data.planes[pi];
            for (uint32_t ti = 0; ti < plane.trapezoid_count; ++ti) {
                const GW::PathingTrapezoid* t = &plane.trapezoids[ti];
                if (IsDegenerate(*t)) continue;

                ConvexPoly& poly = m_polys[poly_index];
                auto& edge_cuts = cuts[poly_index];

                // Endpoint of a candidate split point along the current edge.
                struct EP { float key; GW::Vec2f p; bool viable; };

                for (int e = 0; e < 4; ++e) {
                    const GW::Vec2f c0 = Corner(*t, e);
                    const GW::Vec2f c1 = Corner(*t, (e + 1) & 3);
                    // top(0)/bottom(2) edges are horizontal -> sort by X;
                    // right(1)/left(3) edges are slanted     -> sort by Y.
                    const bool by_x = (e == 0 || e == 2);
                    auto sort_key = [&](const GW::Vec2f& p) { return by_x ? p.x : p.y; };

                    std::vector<EP> pts;
                    pts.push_back({sort_key(c0), c0, false});
                    pts.push_back({sort_key(c1), c1, false});
                    for (const auto& ci : edge_cuts[e]) {
                        pts.push_back({sort_key(ci.a), ci.a, ci.a_viable});
                        pts.push_back({sort_key(ci.b), ci.b, ci.b_viable});
                    }

                    // Sort along the edge direction c0 -> c1.
                    const bool ascending = sort_key(c1) >= sort_key(c0);
                    std::sort(pts.begin(), pts.end(), [&](const EP& l, const EP& r) {
                        return ascending ? (l.key < r.key) : (l.key > r.key);
                    });

                    // Walk consecutive unique points, emitting ring vertices and
                    // the neighbour for each sub-segment.
                    GW::Vec2f prev = pts.front().p;
                    poly.vertex_ids.push_back(addVert(pi, prev.x, prev.y, pts.front().viable));

                    for (size_t k = 1; k < pts.size(); ++k) {
                        const GW::Vec2f cur = pts[k].p;
                        const float dx = cur.x - prev.x, dy = cur.y - prev.y;
                        if (std::sqrt(dx * dx + dy * dy) < kMinSeg) {
                            // coincident with prev: fold its viability into prev vertex
                            if (pts[k].viable)
                                m_verts[poly.vertex_ids.back()].is_viable = true;
                            continue;
                        }
                        const GW::Vec2f mid{0.5f * (prev.x + cur.x), 0.5f * (prev.y + cur.y)};
                        const float m = sort_key(mid);
                        int32_t nb = -1;
                        for (const auto& ci : edge_cuts[e]) {
                            const float lo = std::min(sort_key(ci.a), sort_key(ci.b));
                            const float hi = std::max(sort_key(ci.a), sort_key(ci.b));
                            if (m >= lo && m <= hi) { nb = ci.neighbour; break; }
                        }
                        poly.edge_neighbour.push_back(nb);
                        poly.vertex_ids.push_back(addVert(pi, cur.x, cur.y, pts[k].viable));
                        prev = cur;
                    }
                    // The last emitted vertex is the edge's terminal corner; it is
                    // shared with the next edge's first corner and dedupes there, so
                    // drop the duplicate trailing vertex_id (keep edge_neighbour parity).
                    if (!poly.vertex_ids.empty())
                        poly.vertex_ids.pop_back();
                }

                ++poly_index;
            }
        }

        // --- 5. Winding: ensure CCW (positive shoelace); reverse in lockstep ---
        for (auto& poly : m_polys) {
            const size_t n = poly.vertex_ids.size();
            if (n < 3) continue;
            double a2 = 0.0;
            for (size_t i = 0; i < n; ++i) {
                const PolyVertex& v0 = m_verts[poly.vertex_ids[i]];
                const PolyVertex& v1 = m_verts[poly.vertex_ids[(i + 1) % n]];
                a2 += (double)v0.x * v1.y - (double)v1.x * v0.y;
            }
            if (a2 < 0.0) {
                // CW -> reverse to CCW. Mirror NavMesh.cpp's lockstep reverse so
                // each neighbour stays attached to its edge: reversing vertices
                // [0..n-1] maps edge i to edge (n-2-i).
                std::vector<uint32_t> rv(n);
                std::vector<int32_t> rn(n);
                for (size_t k = 0; k < n; ++k) {
                    rv[k] = poly.vertex_ids[n - 1 - k];
                    rn[k] = poly.edge_neighbour[((n - 2 - k) % n + n) % n];
                }
                poly.vertex_ids = std::move(rv);
                poly.edge_neighbour = std::move(rn);
            }
        }

        // --- 6a. Inter-plane links from cross-plane adjacency -----------------
        for (const auto& pa : adjacency) {
            if (pa.layer_a == pa.layer_b) continue;
            const int32_t pia = PolyOfTrapezoid(pa.trap_a);
            const int32_t pib = PolyOfTrapezoid(pa.trap_b);
            if (pia < 0 || pib < 0) continue;
            m_links.push_back(InterPlaneLink{
                (uint32_t)pia, (uint32_t)pib, pa.a, pa.b,
                pa.layer_a, pa.layer_b, 0.0f, false, false});
        }

        // --- 6b. Inter-plane links from teleports -----------------------------
        for (const auto& tp : teleports) {
            if (data.planes.empty()) break;
            const GW::PathingTrapezoid* te = WalkBsp(
                data.planes.size() > tp.m_enter.zplane ? data.planes[tp.m_enter.zplane] : data.planes[0],
                tp.m_enter);
            const GW::PathingTrapezoid* tx = WalkBsp(
                data.planes.size() > tp.m_exit.zplane ? data.planes[tp.m_exit.zplane] : data.planes[0],
                tp.m_exit);
            const int32_t pe = te ? PolyOfTrapezoid(te) : -1;
            const int32_t px = tx ? PolyOfTrapezoid(tx) : -1;
            if (pe < 0 || px < 0) continue;
            const float cost = GW::GetDistance(tp.m_enter, tp.m_exit) * 0.01f;
            m_links.push_back(InterPlaneLink{
                (uint32_t)pe, (uint32_t)px, {tp.m_enter.x, tp.m_enter.y}, {tp.m_exit.x, tp.m_exit.y},
                (uint16_t)tp.m_enter.zplane, (uint16_t)tp.m_exit.zplane, cost,
                tp.m_directionality != MapSpecific::Teleport::direction::both_ways, true});
        }

        m_ready = !m_polys.empty();

#ifdef _DEBUG
        // --- conformance asserts ------------------------------------------
        int convex_violations = 0;
        for (size_t p = 0; p < m_polys.size(); ++p) {
            const auto& poly = m_polys[p];
            const size_t n = poly.vertex_ids.size();
            if (n < 3) continue;
            int sign = 0;
            for (size_t i = 0; i < n; ++i) {
                const PolyVertex& a = m_verts[poly.vertex_ids[i]];
                const PolyVertex& b = m_verts[poly.vertex_ids[(i + 1) % n]];
                const PolyVertex& c = m_verts[poly.vertex_ids[(i + 2) % n]];
                const float cross = (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
                if (cross > 1e-3f) { if (sign < 0) { ++convex_violations; break; } sign = 1; }
                else if (cross < -1e-3f) { if (sign > 0) { ++convex_violations; break; } sign = -1; }
                // |cross| ~ 0 is collinear and allowed
            }
        }

        // Undirected non-wall edge -> list of (poly, neighbour). A conforming mesh
        // has each such edge used by exactly two polys whose neighbours point back
        // at each other.
        struct EdgeUse { uint32_t poly; int32_t neighbour; };
        std::unordered_map<uint64_t, std::vector<EdgeUse>> edge_use;
        edge_use.reserve(m_verts.size() * 2);
        for (size_t p = 0; p < m_polys.size(); ++p) {
            const auto& poly = m_polys[p];
            const size_t n = poly.vertex_ids.size();
            for (size_t i = 0; i < n; ++i) {
                if (poly.edge_neighbour[i] < 0) continue;
                const uint32_t va = poly.vertex_ids[i];
                const uint32_t vb = poly.vertex_ids[(i + 1) % n];
                const uint32_t lo = std::min(va, vb), hi = std::max(va, vb);
                const uint64_t ek = ((uint64_t)lo << 32) | hi;
                edge_use[ek].push_back({(uint32_t)p, poly.edge_neighbour[i]});
            }
        }
        int edge_violations = 0;
        // Diagnostic categorisation of the violations to find the root cause.
        int v_1use = 0, v_3plus = 0, v_mismatch = 0, v_horiz = 0, v_slant = 0;
        int v_subdiv = 0, v_nearmiss = 0, samples = 0;
        for (const auto& kv : edge_use) {
            const auto& uses = kv.second;
            bool bad = false;
            if (uses.size() != 2) bad = true;
            else if (uses[0].neighbour != (int32_t)uses[1].poly || uses[1].neighbour != (int32_t)uses[0].poly)
                bad = true;
            if (!bad) continue;
            ++edge_violations;

            const uint32_t va_id = (uint32_t)(kv.first >> 32), vb_id = (uint32_t)(kv.first & 0xffffffffu);
            const PolyVertex& va = m_verts[va_id];
            const PolyVertex& vb = m_verts[vb_id];
            if (uses.size() == 1) ++v_1use; else if (uses.size() >= 3) ++v_3plus; else ++v_mismatch;
            const bool horiz = std::fabs(va.y - vb.y) < 0.5f;
            if (horiz) ++v_horiz; else ++v_slant;

            // For 1-use violations, inspect the claimed neighbour: does it have a vertex strictly
            // ON segment (va,vb) interior (subdivision mismatch), or a near-miss vertex close to an
            // endpoint but not deduped (coordinate-precision failure)?
            if (uses.size() == 1) {
                const int32_t Nb = uses[0].neighbour;
                bool subdiv = false, nearmiss = false;
                if (Nb >= 0 && Nb < (int32_t)m_polys.size()) {
                    const float ex = vb.x - va.x, ey = vb.y - va.y;
                    const float len2 = ex * ex + ey * ey;
                    for (const uint32_t wid : m_polys[Nb].vertex_ids) {
                        if (wid == va_id || wid == vb_id) continue;
                        const PolyVertex& w = m_verts[wid];
                        const float dax = w.x - va.x, day = w.y - va.y;
                        const float dbx = w.x - vb.x, dby = w.y - vb.y;
                        if (std::sqrt(dax * dax + day * day) < 1.0f || std::sqrt(dbx * dbx + dby * dby) < 1.0f) nearmiss = true;
                        if (len2 > 1e-3f) {
                            const float t = (dax * ex + day * ey) / len2;
                            const float perp = std::fabs(dax * ey - day * ex) / std::sqrt(len2);
                            if (t > 0.02f && t < 0.98f && perp < 1.0f) subdiv = true;
                        }
                    }
                }
                if (subdiv) ++v_subdiv;
                if (nearmiss) ++v_nearmiss;
                if (samples < 8) {
                    ++samples;
                    Log::Log("[polyanya-diag] viol (%.1f,%.1f)-(%.1f,%.1f) pl%u %s poly%u->nb%d subdiv=%d nearmiss=%d",
                        va.x, va.y, vb.x, vb.y, va.plane, horiz ? "horiz" : "slant",
                        uses[0].poly, Nb, (int)subdiv, (int)nearmiss);
                }
                // Definitive dump: the full vertex rings of the failing pair, so we can see why the
                // seam isn't shared (subdivision, different segment, or wrong neighbour index).
                if (samples <= 3 && Nb >= 0 && Nb < (int32_t)m_polys.size()) {
                    auto ring = [&](uint32_t pidx) {
                        std::string s;
                        char buf[48];
                        const auto& pl = m_polys[pidx];
                        for (size_t i = 0; i < pl.vertex_ids.size(); ++i) {
                            const PolyVertex& w = m_verts[pl.vertex_ids[i]];
                            std::snprintf(buf, sizeof(buf), "(%.0f,%.0f)>%d ", w.x, w.y, pl.edge_neighbour[i]);
                            s += buf;
                        }
                        return s;
                    };
                    Log::Log("[polyanya-dump] poly%u: %s", uses[0].poly, ring(uses[0].poly).c_str());
                    Log::Log("[polyanya-dump] nb%d:   %s", Nb, ring((uint32_t)Nb).c_str());
                }
            }
        }
        if (edge_violations)
            Log::Log("[polyanya-diag] %d violations: 1use=%d 3plus=%d mismatch=%d | horiz=%d slant=%d | of-1use: subdiv=%d nearmiss=%d",
                edge_violations, v_1use, v_3plus, v_mismatch, v_horiz, v_slant, v_subdiv, v_nearmiss);
        Log::Log("[polyanya-diag] edge label-fallbacks: trap_a=%d trap_b=%d", diag_ea_fb, diag_eb_fb);

        size_t cross_links = 0, teleport_links = 0;
        for (const auto& l : m_links) { if (l.is_teleport) ++teleport_links; else ++cross_links; }

        Log::Log("[polyanya] mesh built: %zu polys, %zu verts, %zu links (%zu cross-plane, %zu teleport); "
                 "convex_violations=%d edge_violations=%d",
                 m_polys.size(), m_verts.size(), m_links.size(), cross_links, teleport_links,
                 convex_violations, edge_violations);
        if (convex_violations || edge_violations)
            Log::Log("[polyanya] WARNING: %d non-convex polys, %d non-conforming shared edges",
                     convex_violations, edge_violations);
#endif

        return m_ready;
    }

    bool PolyanyaMesh::AreConnected(const GW::PathingTrapezoid* a, const GW::PathingTrapezoid* b) const
    {
        const int32_t pa = PolyOfTrapezoid(a);
        const int32_t pb = PolyOfTrapezoid(b);
        if (pa < 0 || pb < 0) return true; // unmeshed (degenerate connector): skip, mirrors the Detour audit
        if (pa == pb) return true;
        for (const int32_t nb : m_polys[pa].edge_neighbour)
            if (nb == pb) return true; // shared ring edge (same plane)
        for (const auto& l : m_links)  // cross-plane portal or teleport
            if ((l.poly_a == (uint32_t)pa && l.poly_b == (uint32_t)pb) ||
                (l.poly_a == (uint32_t)pb && l.poly_b == (uint32_t)pa))
                return true;
        return false;
    }

    void PolyanyaMesh::DebugExtractEdges(std::vector<DebugEdge>& out) const
    {
        out.clear();
        for (size_t p = 0; p < m_polys.size(); ++p) {
            const auto& poly = m_polys[p];
            const size_t n = poly.vertex_ids.size();
            for (size_t i = 0; i < n; ++i) {
                const int32_t nb = poly.edge_neighbour[i];
                const bool wall = nb < 0;
                // Emit each internal (non-wall) edge once: only from the lower-indexed poly.
                if (!wall && nb <= (int32_t)p) continue;
                const PolyVertex& a = m_verts[poly.vertex_ids[i]];
                const PolyVertex& b = m_verts[poly.vertex_ids[(i + 1) % n]];
                out.push_back(DebugEdge{
                    GW::GamePos(a.x, a.y, (uint32_t)a.plane),
                    GW::GamePos(b.x, b.y, (uint32_t)b.plane),
                    wall});
            }
        }
    }

} // namespace Pathing
