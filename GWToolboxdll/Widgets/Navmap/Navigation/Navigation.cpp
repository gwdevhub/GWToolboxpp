#include "stdafx.h"
#include "Navigation.h"

#include <Recast.h>
#include <DetourCommon.h>

//#include "RecastDebugDraw.h"
//#include "Sample_TempObstacles.h"

//#include "DetourNavMesh.h"
//#include "DetourNavMeshQuery.h"

Navmesh::Navmesh(rcAssertFailFunc * recast_assert_func, dtAssertFailFunc * detour_assert_func)
{
    rcAssertFailSetCustom(recast_assert_func);
    dtAssertFailSetCustom(detour_assert_func);

    sample = new Sample_TempObstacles();
    geom = new InputGeom();
}

Navmesh::~Navmesh()
{
    if(geom){delete geom; geom = nullptr;}
    if(sample){delete sample; sample = nullptr;}
}

bool Navmesh::Raycast() //WIP
{
    /*
    float hitTime;
							bool hit = geom->raycastMesh(rayStart, rayEnd, hitTime);

			if (hit)
			{
				if (SDL_GetModState() & KMOD_CTRL)
				{
					// Marker
					markerPositionSet = true;
					markerPosition[0] = rayStart[0] + (rayEnd[0] - rayStart[0]) * hitTime;
					markerPosition[1] = rayStart[1] + (rayEnd[1] - rayStart[1]) * hitTime;
					markerPosition[2] = rayStart[2] + (rayEnd[2] - rayStart[2]) * hitTime;
				}
				else
				{
					float pos[3];
					pos[0] = rayStart[0] + (rayEnd[0] - rayStart[0]) * hitTime;
					pos[1] = rayStart[1] + (rayEnd[1] - rayStart[1]) * hitTime;
					pos[2] = rayStart[2] + (rayEnd[2] - rayStart[2]) * hitTime;
					sample->handleClick(rayStart, pos, processHitTestShift);
    */
     
    return false;
}

bool Navmesh::AddTempObstacle() //WIP
{
    //These need to be rewritten because radius should be dynamic
    //sample->addTempObstacle
    //sample->removeTempObstacle
    //sample->clearAllTempObstacles

    return false;
}

bool Navmesh::AddOffMeshConnection() //WIP
{
    //const unsigned char area = SAMPLE_POLYAREA_JUMP;
			//const unsigned short flags = SAMPLE_POLYFLAGS_JUMP;
			//geom->addOffMeshConnection(m_hitPos, p, m_sample->getAgentRadius(), m_bidir ? 1 : 0, area, flags);
			//m_hitPosSet = false;

			//if (geom)
		//geom->drawOffMeshConnections(&dd, true);

    return false;
}

void rcMeshLoaderObj::FinalizeMesh(const std::string& filename)
{
    // Calculate normals.
	m_normals = new float[m_triCount*3];
	for (int i = 0; i < m_triCount*3; i += 3)
	{
		const float* v0 = &m_verts[m_tris[i]*3];
		const float* v1 = &m_verts[m_tris[i+1]*3];
		const float* v2 = &m_verts[m_tris[i+2]*3];
		float e0[3], e1[3];
		for (int j = 0; j < 3; ++j)
		{
			e0[j] = v1[j] - v0[j];
			e1[j] = v2[j] - v0[j];
		}
		float* n = &m_normals[i];
		n[0] = e0[1]*e1[2] - e0[2]*e1[1];
		n[1] = e0[2]*e1[0] - e0[0]*e1[2];
		n[2] = e0[0]*e1[1] - e0[1]*e1[0];
		float d = sqrtf(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
		if (d > 0)
		{
			d = 1.0f/d;
			n[0] *= d;
			n[1] *= d;
			n[2] *= d;
		}
	}
	
	m_filename = filename;
}

bool FillMesh(rcMeshLoaderObj * mesh, const std::string& filename, float * verts, int * tris, size_t vcount, size_t icount)
{
    int cap = 0;
    for(size_t i = 0; i < vcount; i += 3)mesh->addVertex(verts[i + 0], verts[i + 1], verts[i + 2], cap);
    cap = 0;
    for(size_t i = 0; i < icount; i += 3)mesh->addTriangle(tris[i + 0], tris[i + 1], tris[i + 2], cap);

    mesh->FinalizeMesh(filename);

    return true;
}

bool InputGeom::loadMesh2(rcContext* ctx, const std::string& filepath, float * verts, int * tris, size_t vcount, size_t icount)
{
	if (m_mesh)
	{
		delete m_chunkyMesh;
		m_chunkyMesh = 0;
		delete m_mesh;
		m_mesh = 0;
	}
	m_offMeshConCount = 0;
	m_volumeCount = 0;
	
	m_mesh = new rcMeshLoaderObj;
	if (!m_mesh)
	{
		ctx->log(RC_LOG_ERROR, "loadMesh: Out of memory 'm_mesh'.");
		return false;
	}
	if (!FillMesh(m_mesh, filepath, verts, tris, vcount, icount))
	{
		ctx->log(RC_LOG_ERROR, "buildTiledNavigation: Could not load '%s'", filepath.c_str());
		return false;
	}

	rcCalcBounds(m_mesh->getVerts(), m_mesh->getVertCount(), m_meshBMin, m_meshBMax);

	m_chunkyMesh = new rcChunkyTriMesh;
	if (!m_chunkyMesh)
	{
		ctx->log(RC_LOG_ERROR, "buildTiledNavigation: Out of memory 'm_chunkyMesh'.");
		return false;
	}
	if (!rcCreateChunkyTriMesh(m_mesh->getVerts(), m_mesh->getTris(), m_mesh->getTriCount(), 256, m_chunkyMesh))
	{
		ctx->log(RC_LOG_ERROR, "buildTiledNavigation: Failed to build chunky mesh.");
		return false;
	}		

	return true;
}

bool Navmesh::Build(float * verts, int * tris, size_t vcount, size_t icount, NavmeshBuildSettings& nbs)
{
    if(!sample || !geom || !verts || !tris || vcount <= 0 || icount <= 0)return false;

    sample->m_filterLowHangingObstacles = nbs.filterLowHangingObstacles;
	sample->m_filterLedgeSpans = nbs.filterLedgeSpans;
	sample->m_filterWalkableLowHeightSpans = nbs.filterWalkableLowHeightSpans;

    sample->m_keepInterResults = nbs.debug_keep_interim;
    sample->m_drawMode = (Sample_TempObstacles::DrawMode)nbs.debug_draw_mode;
    sample->m_tileSize = nbs.tile_size;

    BuildContext ctx;
    sample->setContext(&ctx);

    geom->m_buildSettings = nbs.settings;
    geom->m_hasBuildSettings = true;
	if(!geom->loadMesh2(&ctx, "GW", verts, tris, vcount, icount))return false;
    rcVcopy(nbs.settings.navMeshBMin, geom->getMeshBoundsMin());
	rcVcopy(nbs.settings.navMeshBMax, geom->getMeshBoundsMax());
    geom->m_buildSettings = nbs.settings;
    ComputeMaxTileSize();
    sample->handleMeshChanged(geom);
	//sample->collectSettings(nbs.settings);

	ctx.resetLog();
	if(!sample->handleBuild())
	{
		ctx.dumpLog("Navigation build log %s:", "GW");
        return false;
	}

    return true;
}

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>

void Navmesh::Update(const float dt)
{
    GW::AgentLiving * me = GW::Agents::GetPlayerAsAgentLiving();
    if(!me)return;

    sample->handleUpdate(dt);
}

void Navmesh::Render()
{
    sample->handleRender();
}

void Navmesh::ViewLog() //WIP
{
    //for (int i = 0; i < ctx.getLogCount(); ++i)
    //imguiLabel(ctx.getLogText(i));
    //ctx.dumpLog("Geom load log %s:", meshName.c_str());
}

bool Navmesh::ComputePath() //WIP
{
    //sample->setTool first
    return false;
}

extern int EXPECTED_LAYERS_PER_TILE;

void Navmesh::ComputeMaxTileSize()
{
	const float* bmin = geom->getNavMeshBoundsMin();
	const float* bmax = geom->getNavMeshBoundsMax();
	
	int gw = 0, gh = 0;
	rcCalcGridSize(bmin, bmax, sample->m_cellSize, &gw, &gh);
	const int ts = (int)sample->m_tileSize;
	const int tw = (gw + ts-1) / ts;
	const int th = (gh + ts-1) / ts;

	// Max tiles and max polys affect how the tile IDs are caculated.
	// There are 22 bits available for identifying a tile and a polygon.
	int tileBits = rcMin((int)dtIlog2(dtNextPow2(tw*th*EXPECTED_LAYERS_PER_TILE)), 14);
	if (tileBits > 14) tileBits = 14;
	int polyBits = 22 - tileBits;
	sample->m_maxTiles = 1 << tileBits;
	sample->m_maxPolysPerTile = 1 << polyBits;
}
