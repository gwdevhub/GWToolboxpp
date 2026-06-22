#include "stdafx.h"

#include <array>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <map>
#include <unordered_map>
#include <unordered_set>

#include <GWCA/GameEntities/Pathing.h>

#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourCommon.h>
#include <DetourAlloc.h>

#include <Logger.h>

#include "NavMesh.h"

static_assert(sizeof(dtPolyRef) == sizeof(unsigned int), "dtPolyRef must be 32-bit");

namespace {
    using namespace Pathing;

    constexpr float kPlaneSeparation = 1000.0f;
    constexpr float kWalkableClimb   = 200.0f;
    constexpr float kWalkableHeight  = 20.0f;
    constexpr float kWalkableRadius  = 0.0f; // trapezoids are already the walkable surface, no erosion

    constexpr unsigned short kWalkableFlag = 0x1;
    constexpr unsigned char  kPolyArea     = 1; // traversal is gated by flags, not area

    constexpr float kOffMeshRadPortal   = 96.0f;
    constexpr float kOffMeshRadTeleport = 256.0f;

    constexpr unsigned short kMeshNullIdx = 0xffff;    // RC_MESH_NULL_IDX: border / vertex padding
    constexpr unsigned int   kMaxGroundPolys = 0x8000; // internal neighbour indices must stay below this
    constexpr float          kEps = 0.5f;

    inline bool IsDegenerate(const GW::PathingTrapezoid& t) { return std::fabs(t.YT - t.YB) < 1.0f; }

    inline GW::Vec2f Centroid(const GW::PathingTrapezoid& t)
    {
        return {(t.XTL + t.XTR + t.XBL + t.XBR) * 0.25f, (t.YT + t.YB) * 0.5f};
    }

    inline GW::Vec2f ClampToTrap(const GW::PathingTrapezoid& t, const GW::Vec2f& p)
    {
        const float lo = std::min(t.YB, t.YT), hi = std::max(t.YB, t.YT);
        const float y = std::min(std::max(p.y, lo), hi);
        const float denom = t.YT - t.YB;
        const float u = std::fabs(denom) > 1e-4f ? (y - t.YB) / denom : 0.f; // 0 = bottom edge, 1 = top edge
        const float lx = t.XBL + (t.XTL - t.XBL) * u;
        const float rx = t.XBR + (t.XTR - t.XBR) * u;
        const float x = std::min(std::max(p.x, std::min(lx, rx)), std::max(lx, rx));
        return {x, y};
    }
}

namespace Pathing {

    NavMesh::~NavMesh() { DestroyMesh(); }

    void NavMesh::DestroyMesh()
    {
        if (m_navmesh) { dtFreeNavMesh(m_navmesh); m_navmesh = nullptr; }
        m_poly_plane.clear();
#ifdef _DEBUG
        m_trap_to_poly.clear();
#endif
        m_ground_poly_count = 0;
        m_plane_count = 0;
        m_poly_base = 0;
    }

    int NavMesh::PlaneIndex(uint32_t zplane) const
    {
        if (zplane >= (uint32_t)m_plane_count) return 0;
        return (int)zplane;
    }

    float NavMesh::PlaneY(int plane) const { return (float)plane * kPlaneSeparation; }

    bool NavMesh::Build(const PathingMapData& data, const MapSpecific::Teleports& teleports)
    {
        DestroyMesh();
        if (!data.IsValid()) return false;
        m_plane_count = (int)data.planes.size();
        if (m_plane_count <= 0 || m_plane_count > PATHING_MAX_PLANE_COUNT) return false;

        size_t total_traps = 0;
        for (const auto& plane : data.planes) total_traps += plane.trapezoid_count;

        std::unordered_map<const GW::PathingTrapezoid*, uint32_t> trap_idx;
        trap_idx.reserve(total_traps);
        struct TrapRef { const GW::PathingTrapezoid* t; int plane; };
        std::vector<TrapRef> trap_list;
        trap_list.reserve(total_traps);
        m_poly_plane.reserve(total_traps);

        float minx = FLT_MAX, miny = FLT_MAX, maxx = -FLT_MAX, maxy = -FLT_MAX;
        for (int p = 0; p < m_plane_count; ++p) {
            const auto& plane = data.planes[p];
            for (uint32_t i = 0; i < plane.trapezoid_count; ++i) {
                const auto* t = &plane.trapezoids[i];
                if (IsDegenerate(*t)) continue;
                minx = std::min({minx, t->XTL, t->XTR, t->XBL, t->XBR});
                maxx = std::max({maxx, t->XTL, t->XTR, t->XBL, t->XBR});
                miny = std::min({miny, t->YT, t->YB});
                maxy = std::max({maxy, t->YT, t->YB});
                const uint32_t idx = (uint32_t)trap_list.size();
                trap_idx.emplace(t, idx);
                trap_list.push_back({t, p});
                m_poly_plane.push_back((uint16_t)p);
            }
        }
        m_ground_poly_count = (uint32_t)trap_list.size();
        if (m_ground_poly_count == 0) return false;
        if (m_ground_poly_count >= kMaxGroundPolys) {
            Log::Error("[navmesh] map %u has %u trapezoids, exceeds neighbour-index limit", data.map_file_id, m_ground_poly_count);
            DestroyMesh();
            return false;
        }

        minx -= 16.f; miny -= 16.f; maxx += 16.f; maxy += 16.f;
        m_cs = std::max(1.0f, std::max(maxx - minx, maxy - miny) / 60000.0f); // keep quantised verts in uint16 range
        const float yrange = (float)(m_plane_count + 1) * kPlaneSeparation;
        m_ch = std::max(1.0f, yrange / 60000.0f);
        const float bmin[3] = {minx, -kPlaneSeparation, miny};
        const float bmax[3] = {maxx, yrange, maxy};

        auto resolveReal = [](const GW::PathingTrapezoid* from, const GW::PathingTrapezoid* start, bool going_down, float ex0, float ex1) -> const GW::PathingTrapezoid* {
            if (!start) return nullptr;
            if (!IsDegenerate(*start)) return start;
            const float fwdEdgeY = going_down ? from->YB : from->YT;
            for (int pass = 0; pass < 2; ++pass) {
                const GW::PathingTrapezoid* best = nullptr;
                float best_ov = -FLT_MAX;
                std::vector<const GW::PathingTrapezoid*> stack{start};
                std::unordered_set<const GW::PathingTrapezoid*> seen{from};
                while (!stack.empty() && seen.size() < 256) {
                    const GW::PathingTrapezoid* n = stack.back();
                    stack.pop_back();
                    if (!n || seen.contains(n)) continue;
                    seen.insert(n);
                    if (!IsDegenerate(*n)) {
                        const float nlo = std::min(n->YB, n->YT), nhi = std::max(n->YB, n->YT);
                        const bool forward = going_down ? (nhi <= fwdEdgeY + kEps) : (nlo >= fwdEdgeY - kEps);
                        if (pass == 1 && !forward) continue;
                        const float a = going_down ? n->XTL : n->XBL;
                        const float b = going_down ? n->XTR : n->XBR;
                        const float ov = std::min(std::max(a, b), ex1) - std::max(std::min(a, b), ex0);
                        if (pass == 1 && ov <= 0.f) continue; // an all-directions fallback hop must genuinely overlap in X, never a far trap across a gap
                        if (ov > best_ov) { best_ov = ov; best = n; }
                        continue;
                    }
                    if (pass == 0) {
                        stack.push_back(going_down ? n->bottom_left : n->top_left);
                        stack.push_back(going_down ? n->bottom_right : n->top_right);
                    }
                    else {
                        stack.push_back(n->top_left);    stack.push_back(n->top_right);
                        stack.push_back(n->bottom_left); stack.push_back(n->bottom_right);
                    }
                }
                if (best) return best;
            }
            return nullptr;
        };
        auto polyOf = [&](const GW::PathingTrapezoid* t) -> int {
            if (!t) return -1;
            const auto it = trap_idx.find(t);
            return it == trap_idx.end() ? -1 : (int)it->second;
        };

        auto edgeXAt = [](const GW::PathingTrapezoid* tt, float y, bool right) -> float {
            const float d = tt->YT - tt->YB;
            const float u = std::fabs(d) > 1e-4f ? (y - tt->YB) / d : 0.f;
            return right ? (tt->XBR + (tt->XTR - tt->XBR) * u) : (tt->XBL + (tt->XTL - tt->XBL) * u);
        };
        std::unordered_set<const GW::Portal*> resolved_portals;
        auto resolvePortalEdge = [&](const GW::PathingTrapezoid* tt, const auto& pl, bool leftSide) -> int {
            const uint16_t pix = leftSide ? tt->portal_left : tt->portal_right;
            if (pix >= pl.portal_count) return -1;
            const GW::Portal* mine = &pl.portals[pix];
            const GW::Portal* pr = mine->pair;
            if (!pr || (pr->flags & 0x4)) return -1;
            const float aYlo = std::min(tt->YB, tt->YT), aYhi = std::max(tt->YB, tt->YT);
            const GW::PathingTrapezoid* best = nullptr; float bestov = kEps;
            struct Item { GW::PathingTrapezoid* t; uint16_t plane; };
            std::vector<Item> stack;
            for (uint32_t i = 0; i < pr->count; ++i) stack.push_back({pr->trapezoids[i], (uint16_t)pr->portal_plane});
            std::unordered_set<const GW::PathingTrapezoid*> seen;
            while (!stack.empty() && seen.size() < 256) {
                const Item cur = stack.back(); stack.pop_back();
                GW::PathingTrapezoid* b = cur.t;
                if (!b || seen.count(b)) continue;
                seen.insert(b);
                if (IsDegenerate(*b)) {
                    if (cur.plane < (uint16_t)m_plane_count) {
                        const auto& cpl = data.planes[cur.plane];
                        for (uint16_t cpix : {b->portal_left, b->portal_right}) {
                            if (cpix >= cpl.portal_count) continue;
                            const GW::Portal* cpr = cpl.portals[cpix].pair;
                            if (!cpr || (cpr->flags & 0x4)) continue;
                            for (uint32_t i = 0; i < cpr->count; ++i) stack.push_back({cpr->trapezoids[i], (uint16_t)cpr->portal_plane});
                        }
                    }
                    continue;
                }
                if (polyOf(b) < 0) continue;
                const float bYlo = std::min(b->YB, b->YT), bYhi = std::max(b->YB, b->YT);
                const float ov = std::min(aYhi, bYhi) - std::max(aYlo, bYlo);
                if (ov <= bestov) continue;
                const float ym = 0.5f * (std::max(aYlo, bYlo) + std::min(aYhi, bYhi));
                if (std::fabs(edgeXAt(tt, ym, !leftSide) - edgeXAt(b, ym, leftSide)) > 64.f) continue; // edges roughly align in X
                bestov = ov; best = b;
            }
            if (!best) return -1;
            resolved_portals.insert(mine);
            resolved_portals.insert(mine->pair);
            return polyOf(best);
        };

        auto edgeKey = [](float xb, float yb, float xt, float yt, int plane) -> std::array<long, 5> {
            return {lroundf(xb), lroundf(yb), lroundf(xt), lroundf(yt), (long)plane};
        };
        std::map<std::array<long, 5>, uint32_t> leftEdgeOwner, rightEdgeOwner;
        for (uint32_t i = 0; i < m_ground_poly_count; ++i) {
            const auto* tt = trap_list[i].t;
            const int p = trap_list[i].plane;
            leftEdgeOwner[edgeKey(tt->XBL, tt->YB, tt->XTL, tt->YT, p)] = i;
            rightEdgeOwner[edgeKey(tt->XBR, tt->YB, tt->XTR, tt->YT, p)] = i;
        }
        auto sameplaneRight = [&](const GW::PathingTrapezoid* tt, int p) -> int {
            const auto it = leftEdgeOwner.find(edgeKey(tt->XBR, tt->YB, tt->XTR, tt->YT, p));
            return it == leftEdgeOwner.end() ? -1 : (int)it->second;
        };
        auto sameplaneLeft = [&](const GW::PathingTrapezoid* tt, int p) -> int {
            const auto it = rightEdgeOwner.find(edgeKey(tt->XBL, tt->YB, tt->XTL, tt->YT, p));
            return it == rightEdgeOwner.end() ? -1 : (int)it->second;
        };

        const int nvp = DT_VERTS_PER_POLYGON;
        std::vector<unsigned short> verts;
        verts.reserve(total_traps * 4 * 3);
        std::vector<unsigned short> polys(m_ground_poly_count * 2 * nvp, kMeshNullIdx);
        std::vector<unsigned char> pareas(m_ground_poly_count, kPolyArea);
        std::vector<unsigned short> pflags(m_ground_poly_count, kWalkableFlag);
        std::unordered_map<uint64_t, unsigned short> vmap;
        vmap.reserve(total_traps * 4);

        auto quant = [&](float x, float y, float z, unsigned short out[3]) -> bool {
            long ix = lroundf((x - bmin[0]) / m_cs);
            long iy = lroundf((y - bmin[1]) / m_ch);
            long iz = lroundf((z - bmin[2]) / m_cs);
            if (ix < 0) ix = 0; if (iy < 0) iy = 0; if (iz < 0) iz = 0;
            if (ix > 65535 || iy > 65535 || iz > 65535) return false;
            out[0] = (unsigned short)ix; out[1] = (unsigned short)iy; out[2] = (unsigned short)iz;
            return true;
        };
        auto addVert = [&](float gx, float gy, float planeY, unsigned short& outIdx) -> bool {
            unsigned short q[3];
            if (!quant(gx, planeY, gy, q)) return false; // Detour coords: x=gw.x, y=planeY, z=gw.y
            const uint64_t key = ((uint64_t)q[0] << 32) | ((uint64_t)q[1] << 16) | (uint64_t)q[2];
            const auto it = vmap.find(key);
            if (it != vmap.end()) { outIdx = it->second; return true; }
            const size_t vc = verts.size() / 3;
            if (vc >= 0xfffe) return false; // dtCreateNavMeshData requires vertCount < 0xffff
            outIdx = (unsigned short)vc;
            verts.push_back(q[0]); verts.push_back(q[1]); verts.push_back(q[2]);
            vmap.emplace(key, outIdx);
            return true;
        };

        bool overflow = false;
        for (uint32_t pi = 0; pi < m_ground_poly_count && !overflow; ++pi) {
            const GW::PathingTrapezoid* t = trap_list[pi].t;
            const float planeY = PlaneY(trap_list[pi].plane);

            struct V { float x, y; };
            V ring[6]; int nei[6]; int n = 0;
            auto push = [&](float x, float y, int neighbour) { ring[n] = {x, y}; nei[n] = neighbour; ++n; };

            const GW::PathingTrapezoid* nblT = resolveReal(t, t->bottom_left, true, t->XBL, t->XBR);
            const GW::PathingTrapezoid* nbrT = resolveReal(t, t->bottom_right, true, t->XBL, t->XBR);
            const GW::PathingTrapezoid* ntlT = resolveReal(t, t->top_left, false, t->XTL, t->XTR);
            const GW::PathingTrapezoid* ntrT = resolveReal(t, t->top_right, false, t->XTL, t->XTR);
            const int nbl = polyOf(nblT);
            const int nbr = polyOf(nbrT);
            const int ntl = polyOf(ntlT);
            const int ntr = polyOf(ntrT);
            const auto& tplane = data.planes[trap_list[pi].plane];
            int nright = resolvePortalEdge(t, tplane, false); // cross-plane neighbour across the right edge
            if (nright < 0) nright = sameplaneRight(t, trap_list[pi].plane);
            int nleft = resolvePortalEdge(t, tplane, true);   // and across the left edge
            if (nleft < 0) nleft = sameplaneLeft(t, trap_list[pi].plane);

            const bool bottom_split = nbl >= 0 && nbr >= 0 && nbl != nbr && (t->XBR - t->XBL) > 2.f * kEps;
            if (bottom_split) {
                float sx = (t->bottom_left && t->bottom_right) ? 0.5f * (t->bottom_left->XTR + t->bottom_right->XTL) : 0.5f * (t->XBL + t->XBR);
                sx = std::min(std::max(sx, t->XBL + kEps), t->XBR - kEps);
                push(t->XBL, t->YB, nbl);
                push(sx, t->YB, nbr);
            }
            else if (nbl >= 0) {
                const float hi = std::min(t->XBR, nblT->XTR);
                push(t->XBL, t->YB, nbl);
                if (hi < t->XBR - 2.f * kEps) push(std::max(hi, t->XBL + kEps), t->YB, -1); // uncovered right = wall
            }
            else if (nbr >= 0) {
                const float lo = std::max(t->XBL, nbrT->XTL);
                if (lo > t->XBL + 2.f * kEps) { push(t->XBL, t->YB, -1); push(std::min(lo, t->XBR - kEps), t->YB, nbr); } // uncovered left = wall
                else push(t->XBL, t->YB, nbr);
            }
            else {
                push(t->XBL, t->YB, -1);
            }
            push(t->XBR, t->YB, nright);

            const bool top_split = ntl >= 0 && ntr >= 0 && ntl != ntr && (t->XTR - t->XTL) > 2.f * kEps;
            if (top_split) {
                float sx = (t->top_left && t->top_right) ? 0.5f * (t->top_left->XBR + t->top_right->XBL) : 0.5f * (t->XTL + t->XTR);
                sx = std::min(std::max(sx, t->XTL + kEps), t->XTR - kEps);
                push(t->XTR, t->YT, ntr);
                push(sx, t->YT, ntl);
            }
            else if (ntr >= 0) {
                const float lo = std::max(t->XTL, ntrT->XBL);
                push(t->XTR, t->YT, ntr);
                if (lo > t->XTL + 2.f * kEps) push(std::min(lo, t->XTR - kEps), t->YT, -1); // uncovered left = wall
            }
            else if (ntl >= 0) {
                const float hi = std::min(t->XTR, ntlT->XBR);
                if (hi < t->XTR - 2.f * kEps) { push(t->XTR, t->YT, -1); push(std::max(hi, t->XTL + kEps), t->YT, ntl); } // uncovered right = wall
                else push(t->XTR, t->YT, ntl);
            }
            else {
                push(t->XTR, t->YT, -1);
            }
            push(t->XTL, t->YT, nleft);

            float s2 = 0.f;
            for (int i = 0; i < n; ++i) s2 += ring[i].x * ring[(i + 1) % n].y - ring[(i + 1) % n].x * ring[i].y;
            if (s2 > 0.f) {
                V rv[6]; int rn[6];
                for (int k = 0; k < n; ++k) {
                    rv[k] = ring[n - 1 - k];
                    rn[k] = nei[((n - 2 - k) % n + n) % n];
                }
                std::memcpy(ring, rv, sizeof(V) * n);
                std::memcpy(nei, rn, sizeof(int) * n);
            }

            unsigned short* P = &polys[pi * 2 * nvp];
            for (int i = 0; i < n; ++i) {
                unsigned short vi;
                if (!addVert(ring[i].x, ring[i].y, planeY, vi)) { overflow = true; break; }
                P[i] = vi;
                P[nvp + i] = (nei[i] < 0) ? kMeshNullIdx : (unsigned short)nei[i];
            }
        }
        if (overflow) {
            Log::Error("[navmesh] vertex overflow building map %u", data.map_file_id);
            DestroyMesh();
            return false;
        }
#ifdef _DEBUG
        m_trap_to_poly = trap_idx; // ghost-wall audit: trap -> poly index (1:1 in the hand-built mesh)
#endif

        std::vector<float> omVerts, omRad;
        std::vector<unsigned short> omFlags;
        std::vector<unsigned char> omAreas, omDir;
        std::vector<unsigned int> omUser;
        std::vector<std::pair<uint16_t, uint16_t>> omPlanesByUser;

        auto addOffMesh = [&](GW::Vec2f a, int planeA, GW::Vec2f b, int planeB, float rad, bool bidir) {
            omVerts.push_back(a.x); omVerts.push_back(PlaneY(planeA)); omVerts.push_back(a.y);
            omVerts.push_back(b.x); omVerts.push_back(PlaneY(planeB)); omVerts.push_back(b.y);
            omRad.push_back(rad);
            omFlags.push_back(kWalkableFlag);
            omAreas.push_back(kPolyArea);
            omDir.push_back(bidir ? (unsigned char)DT_OFFMESH_CON_BIDIR : 0);
            omUser.push_back((unsigned int)omPlanesByUser.size());
            omPlanesByUser.push_back({(uint16_t)planeA, (uint16_t)planeB});
        };
        auto firstMeshed = [&](GW::PathingTrapezoid** traps, uint32_t count, const GW::PathingTrapezoid*& outT, int& outPlane) -> bool {
            if (!traps) return false;
            for (uint32_t i = 0; i < count; ++i) {
                const int idx = polyOf(traps[i]);
                if (idx < 0) continue;
                outT = traps[i];
                outPlane = m_poly_plane[idx];
                return true;
            }
            return false;
        };

        std::unordered_set<const GW::Portal*> seen;
        for (const auto& plane : data.planes) {
            for (uint32_t i = 0; i < plane.portal_count; ++i) {
                const GW::Portal* portal = &plane.portals[i];
                if ((portal->flags & 0x4) || !portal->pair || seen.count(portal)) continue;
                if (resolved_portals.count(portal) || resolved_portals.count(portal->pair)) continue; // already a real shared-edge link
                seen.insert(portal);
                seen.insert(portal->pair);
                const GW::PathingTrapezoid* ta = nullptr; const GW::PathingTrapezoid* tb = nullptr; int pa = 0, pb = 0;
                if (!firstMeshed(portal->trapezoids, portal->count, ta, pa)) continue;
                if (!firstMeshed(portal->pair->trapezoids, portal->pair->count, tb, pb)) continue;
                const GW::Vec2f mid = 0.5f * (Centroid(*ta) + Centroid(*tb));
                const GW::Vec2f pca = ClampToTrap(*ta, ClampToTrap(*tb, mid));
                const GW::Vec2f pcb = ClampToTrap(*tb, pca);
                addOffMesh(pca, pa, pcb, pb, kOffMeshRadPortal, true);
            }
        }
        for (const auto& tp : teleports) {
            const bool bidir = tp.m_directionality == MapSpecific::Teleport::direction::both_ways;
            addOffMesh({tp.m_enter.x, tp.m_enter.y}, PlaneIndex(tp.m_enter.zplane),
                       {tp.m_exit.x, tp.m_exit.y}, PlaneIndex(tp.m_exit.zplane), kOffMeshRadTeleport, bidir);
        }

        dtNavMeshCreateParams params;
        std::memset(&params, 0, sizeof(params));
        params.verts = verts.data();
        params.vertCount = (int)(verts.size() / 3);
        params.polys = polys.data();
        params.polyAreas = pareas.data();
        params.polyFlags = pflags.data();
        params.polyCount = (int)m_ground_poly_count;
        params.nvp = nvp;
        params.offMeshConVerts = omVerts.empty() ? nullptr : omVerts.data();
        params.offMeshConRad = omRad.empty() ? nullptr : omRad.data();
        params.offMeshConFlags = omFlags.empty() ? nullptr : omFlags.data();
        params.offMeshConAreas = omAreas.empty() ? nullptr : omAreas.data();
        params.offMeshConDir = omDir.empty() ? nullptr : omDir.data();
        params.offMeshConUserID = omUser.empty() ? nullptr : omUser.data();
        params.offMeshConCount = (int)omRad.size();
        params.walkableHeight = kWalkableHeight;
        params.walkableRadius = kWalkableRadius;
        params.walkableClimb = kWalkableClimb;
        std::memcpy(params.bmin, bmin, sizeof(bmin));
        std::memcpy(params.bmax, bmax, sizeof(bmax));
        params.cs = m_cs;
        params.ch = m_ch;
        params.buildBvTree = true;

        unsigned char* navData = nullptr;
        int navSize = 0;
        if (!dtCreateNavMeshData(&params, &navData, &navSize)) {
            Log::Error("[navmesh] dtCreateNavMeshData failed (map %u, %u polys)", data.map_file_id, m_ground_poly_count);
            DestroyMesh();
            return false;
        }
        m_navmesh = dtAllocNavMesh();
        if (!m_navmesh) { dtFree(navData); DestroyMesh(); return false; }
        if (dtStatusFailed(m_navmesh->init(navData, navSize, DT_TILE_FREE_DATA))) {
            dtFree(navData);
            DestroyMesh();
            return false;
        }

        const dtMeshTile* tile = static_cast<const dtNavMesh*>(m_navmesh)->getTile(0);
        if (!tile || !tile->header) { DestroyMesh(); return false; }
        m_poly_base = m_navmesh->getPolyRefBase(tile);
        return true;
    }

    void NavMesh::DebugExtractEdges(std::vector<DebugEdge>& out) const
    {
        out.clear();
        if (!m_navmesh) return;
        const dtMeshTile* tile = static_cast<const dtNavMesh*>(m_navmesh)->getTile(0);
        if (!tile || !tile->header) return;
        out.reserve(tile->header->polyCount * 3);
        for (int i = 0; i < tile->header->polyCount; ++i) {
            const dtPoly& p = tile->polys[i];
            if (p.getType() == DT_POLYTYPE_OFFMESH_CONNECTION) continue;
            const int nv = (int)p.vertCount;
            for (int j = 0; j < nv; ++j) {
                const unsigned short ia = p.verts[j], ib = p.verts[(j + 1) % nv];
                const bool wall = (p.neis[j] == 0);
                if (!wall && ia > ib) continue; // shared internal edge: emit once
                const float* va = &tile->verts[ia * 3];
                const float* vb = &tile->verts[ib * 3];
                const int pa = (int)lroundf(va[1] / kPlaneSeparation);
                const int pb = (int)lroundf(vb[1] / kPlaneSeparation);
                out.push_back({GW::GamePos(va[0], va[2], (uint32_t)pa), GW::GamePos(vb[0], vb[2], (uint32_t)pb), wall});
            }
        }
    }

#ifdef _DEBUG
    bool NavMesh::AreTrapsConnected(const GW::PathingTrapezoid* a, const GW::PathingTrapezoid* b) const
    {
        if (a == b) return true;
        if (!m_navmesh) return true;
        const auto ia = m_trap_to_poly.find(a);
        const auto ib = m_trap_to_poly.find(b);
        if (ia == m_trap_to_poly.end() || ib == m_trap_to_poly.end()) return true; // unmeshed (degenerate connector): skip
        const uint32_t pa = ia->second, pb = ib->second;
        if (pa == pb) return true;
        const dtMeshTile* tile = static_cast<const dtNavMesh*>(m_navmesh)->getTile(0);
        if (!tile || !tile->header) return true;
        const unsigned int total = (unsigned int)tile->header->polyCount;
        auto polyIdx = [&](dtPolyRef ref) -> unsigned int { return (unsigned int)(ref - m_poly_base); };
        // Walk pa's links: direct adjacency, or 2-hop through an off-mesh connection poly.
        for (unsigned int li = tile->polys[pa].firstLink; li != DT_NULL_LINK; li = tile->links[li].next) {
            const unsigned int n = polyIdx(tile->links[li].ref);
            if (n >= total) continue;
            if (n == pb) return true;
            if (tile->polys[n].getType() == DT_POLYTYPE_OFFMESH_CONNECTION) {
                for (unsigned int lj = tile->polys[n].firstLink; lj != DT_NULL_LINK; lj = tile->links[lj].next) {
                    if (polyIdx(tile->links[lj].ref) == pb) return true;
                }
            }
        }
        return false;
    }
#endif

} // namespace Pathing
