#pragma once

//#define RC_DISABLE_ASSERTS

#include "RecastDemo/Sample_TempObstacles.h"
#include "RecastDemo/InputGeom.h"

#include <RecastAssert.h>
#include <DetourAssert.h>

struct NavmeshBuildSettings
{
    NavmeshBuildSettings()
    {
        // Cell size in world units
	    settings.cellSize = 0.3f;
	    // Cell height in world units
	    settings.cellHeight = 0.2f;
	    // Agent height in world units
	    settings.agentHeight = 2.0f;
	    // Agent radius in world units
	    settings.agentRadius = 0.6f;
	    // Agent max climb in world units
	    settings.agentMaxClimb = 0.9f;
	    // Agent max slope in degrees
	    settings.agentMaxSlope = 45.0f;
	    // Region minimum size in voxels.
	    // regionMinSize = sqrt(regionMinArea)
	    settings.regionMinSize = 8;
	    // Region merge size in voxels.
	    // regionMergeSize = sqrt(regionMergeArea)
	    settings.regionMergeSize = 20;
	    // Edge max length in world units
	    settings.edgeMaxLen = 12.0f;
	    // Edge max error in voxels
	    settings.edgeMaxError = 1.3f;
	    settings.vertsPerPoly = 6.0f;
	    // Detail sample distance in voxels
	    settings.detailSampleDist = 6.0f;
	    // Detail sample max error in voxel heights.
	    settings.detailSampleMaxError = 1.0f;
	    // Partition type, see SamplePartitionType
	    settings.partitionType = (int)SAMPLE_PARTITION_WATERSHED;
	    // Bounds of the area to mesh
	    //float navMeshBMin[3];
	    //float navMeshBMax[3];
	    // Size of the tiles in voxels
	    settings.tileSize = 48.0f;
    }

    bool debug_keep_interim = false;
    int debug_draw_mode = 1;
    float scale = 100.0f;
    float altitude_radius = 1.0f;
    int triangle_subdivision_count = 3;
    bool filterLowHangingObstacles = true;
	bool filterLedgeSpans = true;
	bool filterWalkableLowHeightSpans = true;
    float tile_size = 48.0f;
    BuildSettings settings;
};

class Navmesh
{
public:
    Navmesh(rcAssertFailFunc * recast_assert_func, dtAssertFailFunc * detour_assert_func);
    ~Navmesh();

    bool Build(float * verts, int * tris, size_t vcount, size_t icount, NavmeshBuildSettings& nbs);
    void Update(const float dt);
    void Render();
    void ViewLog();

    bool Raycast();
    bool AddTempObstacle();
    bool AddOffMeshConnection();
    //AddConvexVolume() //ConvexVolumeTool.h
	//AddAgent() //CrowdTool.h

    bool ComputePath();

//private:
    Sample_TempObstacles * sample = nullptr;
    InputGeom * geom = nullptr;

private:
    void ComputeMaxTileSize();
};
