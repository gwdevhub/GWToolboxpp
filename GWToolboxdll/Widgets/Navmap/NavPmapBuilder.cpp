#include "stdafx.h"
#include "NavPmapBuilder.h"
#include <Modules/Resources.h>
#include <Widgets/Minimap/D3DVertex.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/MapContext.h>
#include <GWCA/Managers/MapMgr.h>

#include <algorithm>

struct Point {
    float x, y;
    Point(float _x, float _y) : x(_x), y(_y) {}
};

struct Triangle {
    Point A, B, C;
    Triangle(const Point& _A, const Point& _B, const Point& _C) : A(_A), B(_B), C(_C) {}
};

std::vector<Triangle> splitTriangle(const Triangle& triangle, int n) {
    std::vector<Triangle> triangles;
    triangles.push_back(triangle);

    for (int i = 0; i < n; ++i) {
        std::vector<Triangle> newTriangles;

        for (const auto& oldTriangle : triangles) {
            Point AB((oldTriangle.A.x + oldTriangle.B.x) / 2, (oldTriangle.A.y + oldTriangle.B.y) / 2);
            Point BC((oldTriangle.B.x + oldTriangle.C.x) / 2, (oldTriangle.B.y + oldTriangle.C.y) / 2);
            Point AC((oldTriangle.A.x + oldTriangle.C.x) / 2, (oldTriangle.A.y + oldTriangle.C.y) / 2);

            newTriangles.push_back(Triangle(oldTriangle.A, AB, AC));
            newTriangles.push_back(Triangle(AB, oldTriangle.B, BC));
            newTriangles.push_back(Triangle(AC, BC, oldTriangle.C));
            newTriangles.push_back(Triangle(AB, BC, AC));
        }

        triangles = newTriangles;
    }

    return triangles;
}

void GWAssertLogRecast(const char * expression, const char * file, int line)
{
    Log::Error("Navigation [Recast]: %s %s %u", expression, file, line);
}

void GWAssertLogDetour(const char * expression, const char * file, int line)
{
    Log::Error("Navigation [Detour]: %s %s %u", expression, file, line);
}

volatile bool navmesh_building = false;

bool NavPMapBuilder::Build(GW::PathingMapArray * path_map)
{
    if(!path_map || navmesh_building)return false;

    if(nm){delete nm;}
    nm = new Navmesh(GWAssertLogRecast, GWAssertLogDetour);
    auto * gctx = GW::GetGameContext();

    navmesh_building = true;
    Resources::EnqueueWorkerTask([this, path_map, gctx]
    {
        size_t trapez_count_ = 0;
        for(const GW::PathingMap& map : *path_map) trapez_count_ += map.h0014;
        size_t total_tri_count = trapez_count_ * 2 * nbs.triangle_subdivision_count;
        size_t total_vert_count_ = total_tri_count * 3;

        if(gctx->map->sub1->pathing_map_block.size() != path_map->size())
        {
            navmesh_building = false;
            return;
        }

        auto& blockmap = gctx->map->sub1->pathing_map_block;

        _vertices.clear();
        _indices.clear();
        _vertices.reserve(total_vert_count_ * 3);
        _indices.reserve(total_tri_count * 3);

        size_t layer = 0;
        for(const GW::PathingMap& pmap : *path_map)
        {
            if(!blockmap[layer])
            {
                for(size_t j = 0; j < pmap.trapezoid_count; ++j)
                {
                    GW::PathingTrapezoid& trap = pmap.trapezoids[j];

                    D3DVertex q_verts[6];

                    q_verts[0].x = trap.XTL;
                    q_verts[0].z = trap.YT;
                    q_verts[1].x = trap.XTR;
                    q_verts[1].z = trap.YT;
                    q_verts[2].x = trap.XBL;
                    q_verts[2].z = trap.YB;

                    q_verts[3].x = trap.XBL;
                    q_verts[3].z = trap.YB;
                    q_verts[4].x = trap.XTR;
                    q_verts[4].z = trap.YT;
                    q_verts[5].x = trap.XBR;
                    q_verts[5].z = trap.YB;

                    for(auto& it : q_verts)it.y = 0.0f;

                    Triangle originalTriangle1(Point(q_verts[0].x, q_verts[0].z), Point(q_verts[1].x, q_verts[1].z), Point(q_verts[2].x, q_verts[2].z));
                    std::vector<Triangle> result1 = splitTriangle(originalTriangle1, nbs.triangle_subdivision_count);
                    Triangle originalTriangle2(Point(q_verts[3].x, q_verts[3].z), Point(q_verts[4].x, q_verts[4].z), Point(q_verts[5].x, q_verts[5].z));
                    std::vector<Triangle> result2 = splitTriangle(originalTriangle2, nbs.triangle_subdivision_count);

                    for(auto& it : result1)
                    {
                        _vertices.push_back(it.A.x);
                        _vertices.push_back((float)layer);
                        _vertices.push_back(it.A.y);

                        _vertices.push_back(it.B.x);
                        _vertices.push_back((float)layer);
                        _vertices.push_back(it.B.y);

                        _vertices.push_back(it.C.x);
                        _vertices.push_back((float)layer);
                        _vertices.push_back(it.C.y);
                    }
                    for(auto& it : result2)
                    {
                        _vertices.push_back(it.A.x);
                        _vertices.push_back((float)layer);
                        _vertices.push_back(it.A.y);

                        _vertices.push_back(it.B.x);
                        _vertices.push_back((float)layer);
                        _vertices.push_back(it.B.y);

                        _vertices.push_back(it.C.x);
                        _vertices.push_back((float)layer);
                        _vertices.push_back(it.C.y);
                    }
                }
            }
            ++layer;
        }

        size_t offset = 0;
        for(size_t j = 0; j < _vertices.size() / 3 / 3; ++j)
        {
            _indices.push_back(offset + 0);
            _indices.push_back(offset + 1);
            _indices.push_back(offset + 2);

            offset += 3;
        }

        Resources::EnqueueMainTask([this]
        {
            auto fetch_height = [](float x, float z, uint32_t l, float r) -> float
            {
                float ret = 0.0f;
                GW::GamePos gp{x, z, l};
                float query1 = GW::Map::QueryAltitude(gp, r, ret); if(query1 <= 0.0f)ret = 0.0f;
                return ret;
            };

            for(size_t j = 0; j < _vertices.size(); j += 3)
            {
                _vertices[j + 1] = -fetch_height(_vertices[j + 0], _vertices[j + 2], (uint32_t)_vertices[j + 1], nbs.altitude_radius) / nbs.scale;
                _vertices[j + 0] /= nbs.scale;
                _vertices[j + 2] /= nbs.scale;
            }

            Resources::EnqueueWorkerTask([this]
            {
                if(!nm->Build(_vertices.data(), _indices.data(), _vertices.size(), _indices.size(), nbs))
                {
                    Resources::EnqueueMainTask([this]
                    {
                        Log::Error("Navmesh build process failed");
                        navmesh_building = false;
                    });
                }
                else
                {
                    Resources::EnqueueMainTask([this]
                    {
                        if(show_success_message)
                        {
                            if(!nm->sample)Log::Info("Navmesh build process succeeded");
                            else Log::Info("Navmesh built in %.1f ms using %.1f kb of memory", nm->sample->m_cacheBuildTimeMs, (float)nm->sample->m_cacheBuildMemUsage / 1024.0f);
                        }
                        
                        navmesh_building = false;
                    });
                }
            });
        });
    });

    return true;
}
