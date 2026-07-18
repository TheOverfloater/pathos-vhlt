/*
===============================================
Pathos Engine - Created by Andrew Stephen "Overfloater" Lucas

Copyright 2016
All Rights Reserved.
===============================================
*/

#include "lightgrid.h"
#include "bspfile.h"
#include "mathtypes.h"
#include "mathlib.h"
#include "qrad.h"
#include "vertexlight.h"
#include "cbitset.h"

#include <vector>

//
// This code was created using the ericw tools code, so credit goes to Eric Wasylishen (AKA ericw)
// 

// Vector containing all of our light grid sample data
std::vector<lightgrid_sample_t>	g_lightGridSamplesVector;
// Vector containing all leaves
std::vector<octree_leaf_t>		g_octreeLeavesVector;
// Vector containing all nodes
std::vector<octree_node_t>		g_octreeNodesVector;

// Grid size in terms of sample counts
int								g_gridSize[3];
// Grid mins coordinates
vec3_t							g_gridMinsCoords;
// Direct lights
extern directlight_t*			directlights[MAX_MAP_LEAFS];
// Number of direct lights		
extern int						numdlights;
// Number of light_env lights
extern int						numenvlights;

// Max depth of the octree
static constexpr int			MAX_OCTREE_DEPTH = 5;
// If any axis is fewer than this many grid points, make them a leaf
static constexpr int			MIN_NODE_DIMENSION = 4;
// Number of samples per thread
static constexpr int			SAMPLES_PER_THREAD = 256;

// Thread info structure
std::vector<lightgrid_threadinfo_t> g_sampleThreadInfos;

void DumpSMD_LightGrid( void )
{
	char filepath[MAX_PATH];
	strcpy(filepath, "D:/test.smd");

	FILE* pf = fopen(filepath, "w");
	if(!pf)
		return;

	fprintf(pf, "version 1\n");
	fprintf(pf, "nodes\n");
	fprintf(pf, "  0 \"bone01\"  -1\n");
	fprintf(pf, "end\n");
	fprintf(pf, "skeleton\n");
	fprintf(pf, "time 0\n");
	fprintf(pf, "  0 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000\n");
	fprintf(pf, "end\n");

	fprintf(pf, "triangles\n");

	for(int i = 0; i < g_lightGridSamplesVector.size(); i++)
	{
		lightgrid_sample_t& sample = g_lightGridSamplesVector[i];
		if(sample.occluded)
			continue;

		fprintf(pf, "null.tga\n");

		for(Uint32 j = 0; j < 3; j++)
		{
			vec3_t v1;
			VectorCopy(sample.origin, v1);
			v1[j] += 1;

			fprintf(pf, "  0   %.4f  %.4f  %.4f  %.4f  %.4f  %.4f  %.4f  %.4f\n",
			v1[0], v1[1], v1[2],
			0, 0, 0,
			0, 0);
		}
	}

	fprintf(pf, "end\n");
	fclose(pf);
}

// =====================================================================================
//  BuildLightGrid
//  
//  Create light grid
// =====================================================================================
void BuildLightGrid( void )
{
	// Calculate the size of the grid
	vec3_t gridmaxs;
	CalcLightGridBounds(g_gridMinsCoords, gridmaxs);

	// Determine size of the entire sample grid
	vec3_t worldSize;
	VectorSubtract(gridmaxs, g_gridMinsCoords, worldSize);

	for(int i = 0; i < 3; i++)
	{
		float div = worldSize[i] / g_lightgriddistance;
		g_gridSize[i] = ceil(div);
	}

	int gridSampleCount = g_gridSize[0] * g_gridSize[1] * g_gridSize[2];
	g_lightGridSamplesVector.resize(gridSampleCount);

	int infoCount = ceil((float)gridSampleCount / (float)SAMPLES_PER_THREAD);
	g_sampleThreadInfos.resize(infoCount);

	for(int i = 0; i < infoCount; i++)
	{
		int start = i * SAMPLES_PER_THREAD;
		int end = (i + 1) * SAMPLES_PER_THREAD;
		if(end > gridSampleCount)
			end = gridSampleCount;

		g_sampleThreadInfos[i].firstsample = start;
		g_sampleThreadInfos[i].numsamples = end - start;
	}

	// Calculate lighting for each sample, and determine if it's actually usable
	NamedRunThreadsOnIndividual(infoCount, g_estimate, ProcessGridSamplePoints);

	// Create the octree lump
	MakeOctreeLump();
}

// =====================================================================================
//  CalcLightGridBounds
//  
//  Determine light grid mins/maxs based on visible surfaces
// =====================================================================================
void CalcLightGridBounds( vec3_t& outmins, vec3_t& outmaxs )
{
	// Reset these
	for(int i = 0; i < 3; i++)
	{
		outmins[i] = MAX_FLOAT_VALUE;
		outmaxs[i] = -MAX_FLOAT_VALUE;
	}

	// We get the mins/maxs of the light grid from the visible faces
	// of the worldmodel
	const dmodel_t& worldmodel = g_dmodels[0];
	int numfaces = worldmodel.firstface + worldmodel.numfaces;
	for(int i = worldmodel.firstface; i < numfaces; i++)
	{
		const dface_t& face = g_dfaces[i];

		for(int j = 0; j < face.numedges; j++)
		{
			// Retreive the vertex index
			int firstedge = face.firstedge + j;
			int surfedgeindex = g_dsurfedges[firstedge];
			int vertexindex;
			if(surfedgeindex < 0)
				vertexindex = g_dedges[-surfedgeindex].v[1];
			else
				vertexindex = g_dedges[surfedgeindex].v[0];

			// Get vertex coordinate
			vec3_t origin;
			VectorCopy(g_dvertexes[vertexindex].point, origin);

			for(int k = 0; k < 3; k++)
			{
				if(outmins[k] > origin[k])
					outmins[k] = origin[k];

				if(outmaxs[k] < origin[k])
					outmaxs[k] = origin[k];
			}
		}
	}
}

// =====================================================================================
//  ProcessGridSamplePoints
//  
//  Determine light grid mins/maxs based on visible surfaces
// =====================================================================================
void ProcessGridSamplePoints( int threadInfoIndex )
{
	lightgrid_threadinfo_t& info = g_sampleThreadInfos[threadInfoIndex];

	for(int i = 0; i < info.numsamples; i++)
	{
		int sampleIndex = info.firstsample + i;

		// Calculate our actual coordinates
		vec3_t localCoords;
		localCoords[2] = (sampleIndex / (g_gridSize[0] * g_gridSize[1]));
		localCoords[1] = (sampleIndex / g_gridSize[0]) % g_gridSize[1];
		localCoords[0] = sampleIndex % g_gridSize[0];

		vec3_t worldPosition;
		VectorMA(g_gridMinsCoords, g_lightgriddistance, localCoords, worldPosition);

		lightgrid_sample_t& gridSample = g_lightGridSamplesVector[sampleIndex];
		VectorCopy(worldPosition, gridSample.origin);

		// Determine if this position is valid in the world
		const dmodel_t& worldmodel = g_dmodels[0];
		bool occluded = IsPointInSolidRecursive(worldmodel, worldmodel.headnode[0], worldPosition);
		if(occluded)
		{
			vec3_t resultpos;
			if(FixLightOnFace(worldmodel, worldPosition, 2.0f, resultpos))
			{
				// We were able to nudge this out of a solids
				VectorCopy(resultpos, worldPosition);
			}
			else
			{
				// We couldn't nudge the sample, so don't bother
				gridSample.occluded = true;
			}
		}

		// If not occluded, calculate lighting hitting grid
		if(!gridSample.occluded)
		{
			dleaf_t* leaf = PointInLeaf(worldPosition);
			byte pvs[(MAX_MAP_LEAFS + 7) / 8];
			if (leaf->visofs != -1)
				DecompressVis(&g_dvisdata[leaf->visofs], pvs, sizeof(pvs));
			else
				memset(pvs, 0xFF, sizeof(pvs));

			GatherGridPointLight(worldPosition, pvs, gridSample);
		}
	}
}

// =====================================================================================
//  R_IsPointInSolid
//  
//  Determine if the coordinate is in a solid or not
// =====================================================================================
bool IsPointInSolidRecursive( const dmodel_t& model, int nodeindex, const vec3_t& point )
{
	if(nodeindex < 0)
	{
		int leafindex = (-1 - nodeindex);
		const dleaf_t* pleaf = &g_dleafs[leafindex];

		return (pleaf->contents == CONTENTS_SKY || pleaf->contents == CONTENTS_SOLID) ? true : false;
	}
	else
	{
		dnode_t& node = g_dnodes[nodeindex];
		dplane_t& plane = g_dplanes[node.planenum];

		float dot;
		switch(plane.type)
		{
		case plane_x:
			dot = point[0] - plane.dist; break;
		case plane_y:
			dot = point[1] - plane.dist; break;
		case plane_z:
			dot = point[2] - plane.dist; break;
		default:
			dot = DotProduct(point, plane.normal) - plane.dist;
			break;
		}

		if(dot > 0.1)
			return IsPointInSolidRecursive(model, node.children[0], point);
		else if(dot < -0.1)
			return IsPointInSolidRecursive(model, node.children[1], point);
		else
			return (IsPointInSolidRecursive(model, node.children[0], point) || IsPointInSolidRecursive(model, node.children[1], point));
	}
}

// =====================================================================================
//  ProcessGridSamplePoints
//  
//  Determine light grid mins/maxs based on visible surfaces
// =====================================================================================
bool FixLightOnFace( const dmodel_t& model, const vec3_t& point, float distance, vec3_t& outpos )
{
	if(!IsPointInSolidRecursive(model, model.headnode[0], point))
	{
		VectorCopy(point, outpos);
		return true;
	}

	for(int i = 0; i < 6; i++)
	{
		vec3_t testpoint;
		VectorCopy(point, testpoint);

		int axis = i / 2;
		bool positive = i % 2;
		testpoint[axis] += (positive ? distance : -distance);

		if(!IsPointInSolidRecursive(model, model.headnode[0], testpoint))
		{
			VectorCopy(testpoint, outpos);
			return true;
		}
	}

	VectorClear(outpos);
	return false;
}

// =====================================================================================
//  LightGrid_GetChildIndex
// =====================================================================================
int LightGrid_GetChildIndex( const int* pdivisionpoint, const int* ptestpoint )
{
	int sign[3];
	for(int i = 0; i < 3; i++)
		sign[i] = (ptestpoint[i] >= pdivisionpoint[i]) ? 1 : 0;

	return (4 * sign[0]) + (2 * sign[1]) + sign[2];
}

// =====================================================================================
//  LightGrid_GetChildIndex
// =====================================================================================
void LightGrid_GetOctant( int i, const int* pmins, const int* psize, const int* pdivsionpoint, int* poutchildmins, int* poutchildsize )
{
	for(int j = 0; j < 3; j++)
	{
		int bit;
		switch(j)
		{
		case 0: 
			bit = 4; 
			break;
		case 1: 
			bit = 2; 
			break;
		case 2: 
		default:
			bit = 1; 
			break;
		}

		if(i & bit)
		{
			poutchildmins[j] = pdivsionpoint[j];
			poutchildsize[j] = pmins[j] + psize[j] - pdivsionpoint[j];
		}
		else
		{
			poutchildmins[j] = pmins[j];
			poutchildsize[j] = pdivsionpoint[j] - pmins[j];
		}
	}
}

// =====================================================================================
//  LightGrid_GetGridIndex
// =====================================================================================
int LightGrid_GetGridIndex( int x, int y, int z ) 
{ 
	return (g_gridSize[0] * g_gridSize[1] * z) + (g_gridSize[0] * y) + x; 
}

// =====================================================================================
//  LightGrid_GetWorldCoordsFromGridIndex
// =====================================================================================
void LightGrid_GetWorldCoordsFromGridIndex( int* pindex, const vec3_t& gridmins, vec3_t& outcoords ) 
{ 
	for(int i = 0; i < 3; i++)
		outcoords[i] = gridmins[i] + (pindex[i] * g_lightgriddistance);
}

// =====================================================================================
//  BuildVertexLights
// =====================================================================================
void GatherGridPointLight( const vec3_t pos, const byte* const pvs, lightgrid_sample_t& gridsample )
{
	int i;
	byte add_styles[ALLSTYLES];
	vec3_t adds[ALLSTYLES];
	vec3_t adds_ambient[ALLSTYLES];
	vec3_t adds_diffuse[ALLSTYLES];
	vec3_t directions[ALLSTYLES];

	memset(adds, 0, sizeof(adds));
	memset(adds_ambient, 0, sizeof(adds_ambient));
	memset(adds_diffuse, 0, sizeof(adds_diffuse));
	memset(directions, 0, sizeof(directions));
	memset(add_styles, 0, sizeof(add_styles));
	add_styles[0] = 0;

	CBitSet directLightTraceSetBitset(numdlights);
	CBitSet directLightTraceBitset(numdlights);

	std::vector<CBitSet> envLightsTraceSetBitset(numenvlights);
	std::vector<CBitSet> envLightsBitset(numenvlights);

	std::vector<CBitSet> skyLightsTraceSetBitset(numenvlights);
	std::vector<CBitSet> skyLightsBitset(numenvlights);

	// First do direct lighting and collect strongest light directions
	// from all light sources
	for (int step = 0; step < 2; step++)
	{
		for (i = 0; i < 1 + g_dmodels[0].visleafs; i++)
		{
			directlight_t* l = directlights[i];
			if (!l)
				continue;

			if (i == 0 ? g_sky_lighting_fix : pvs[(i - 1) >> 3] & (1 << ((i - 1) & 7)))
			{
				for (; l; l = l->next)
				{
					AddLight(l, directLightTraceBitset, directLightTraceSetBitset, envLightsBitset, envLightsTraceSetBitset, skyLightsBitset, skyLightsTraceSetBitset, directions, pos, pvs, nullptr, 1.0, add_styles, step, 0, -1, adds, adds_ambient, adds_diffuse, NULL, NULL, false, true, false);
				}
			}
		}
	}

	// Normalize gathered light direction
	for (i = 0; i < ALLSTYLES; i++)
		VectorNormalize(directions[i]);

	// Now calculate the ambient and direct lighting based on the dominant light vector
	for (int step = 0; step < 2; step++)
	{
		for (i = 0; i < 1 + g_dmodels[0].visleafs; i++)
		{
			directlight_t* l = directlights[i];
			if (l && (i == 0 ? g_sky_lighting_fix : pvs[(i - 1) >> 3] & (1 << ((i - 1) & 7))))
			{
				for (; l; l = l->next)
					AddLight(l, directLightTraceBitset, directLightTraceSetBitset, envLightsBitset, envLightsTraceSetBitset, skyLightsBitset, skyLightsTraceSetBitset, directions, pos, pvs, nullptr, 1.0, add_styles, step, 0, -1, adds, adds_ambient, adds_diffuse, NULL, NULL, true, true, false);
			}
		}
	}

	// Bounced lighting
	for (i = 0; i < (int)g_num_patches; i++)
	{
		patch_t* p = &g_patches[i];
		if (p->leafnum != 0 && !(pvs[(p->leafnum - 1) >> 3] & (1 << ((p->leafnum - 1) & 7))))
			continue;

		vec3_t v_delta;
		VectorSubtract(p->origin, pos, v_delta);
		float d2 = DotProduct(v_delta, v_delta);
		float d = sqrt(d2);
		if (d < 1.0f)
			d = 1.0f;

		// Do not use normal here, as sample points don't have any
		float dot_rec =  1 / d; 
		if(dot_rec < 0)
			continue;

		vec3_t planeNormal;
		VectorCopy(getPlaneFromFaceNumber(p->faceNumber)->normal, planeNormal);
		float dot_em = -DotProduct(v_delta, planeNormal) / d;
		if(dot_em < 0)
			continue;

		float scale = (dot_rec * dot_em * p->area) / (Q_PI * d2 + p->area);
		if(scale <= 0)
			continue;

		if (TestLine(pos, p->origin) != CONTENTS_EMPTY)
			continue;

		vec3_t transparency;
		int opaquestyle;
#if 0
		if (TestSegmentAgainstOpaqueList(pos, p->origin, transparency, opaquestyle, true))
			continue;
#else
		VectorFill(transparency, 1);
		opaquestyle = -1;
#endif
		for (int j = 0; j < MAXLIGHTMAPS && p->totalstyle[j] != 255; j++)
		{
			// Account for opaque
			int bouncestyle = p->totalstyle[j];
			if (opaquestyle != -1)
			{
				if (bouncestyle == 0 || bouncestyle == opaquestyle)
				{
					// Use opaque style as bounce style
					bouncestyle = opaquestyle;
				}
				else
				{
					// dynamic light of other styles hits this toggleable opaque entity, then it completely vanishes.
					continue; 
				}
			}

			// Do NOT allow the bounce lighting to add new styles, this can mess up things with ALD consistency, so
			// get the original sample on this style and see if there's usable values here
			vec_t dsamplebrightness = GetBrightestSample(adds[bouncestyle], adds_ambient[bouncestyle], adds_diffuse[bouncestyle]);
			if(bouncestyle == 0 || dsamplebrightness > g_corings[bouncestyle] * 0.1)
			{
				vec3_t patch_add;
				VectorMA(adds_ambient[bouncestyle], scale, p->totallight[j], patch_add);
				VectorMultiply(patch_add, transparency, adds_ambient[bouncestyle]);
			}
		}
	}

	// Clear this out before finalizing, as we don't use normal lighting here
	memset(adds, 0, sizeof(adds));

	// Now collect usable styles
	vec_t maxlights[ALLSTYLES];
	for (int i = 0; i < ALLSTYLES; i++)
	{
		vec_t b = GetBrightestSample(adds[i], adds_ambient[i], adds_diffuse[i]);
		maxlights[i] = qmax(maxlights[i], b);

		if (maxlights[i] <= g_corings[i] * 0.1) // light is too dim, discard this style to reduce RAM usage
			maxlights[i] = 0;
	}

	// Set finals
	for(int i = 0; i < MAXLIGHTMAPS; i++)
	{
		int bestindex = -1;
		if (i == 0)
		{
			bestindex = 0;
		}
		else
		{
			vec_t bestmaxlight = 0;
			for (int j = 1; j < ALLSTYLES && add_styles[j] != 255; j++)
			{
				if (maxlights[j] > bestmaxlight + NORMAL_EPSILON)
				{
					bestmaxlight = maxlights[j];
					bestindex = j;
				}
			}
		}

		if (bestindex != -1)
		{
			maxlights[bestindex] = 0;
			gridsample.styles[i] = bestindex;

			VectorCopy(adds_ambient[bestindex], gridsample.light_ambient[i]);
			VectorCopy(adds_diffuse[bestindex], gridsample.light_diffuse[i]);
			VectorCopy(directions[bestindex], gridsample.light_direction[i]);
		}
		else
		{
			gridsample.styles[i] = 255;
		}
	}
}

// =====================================================================================
//  GetDivisionPoint
// =====================================================================================
void GetDivisionPoint( const int* pmins, const int* psize, int* pout )
{
	for(int i = 0; i < 3; i++)
		pout[i] = pmins[i] + psize[i] / 2;
}

// =====================================================================================
//  CountOccludedAndUnoccludedSamples
// =====================================================================================
void CountOccludedAndUnoccludedSamples( const int* pmins, const int* psize, int& numoccluded, int& numunoccluded )
{
	numoccluded = 0;
	numunoccluded = 0;

	int maxx = pmins[0] + psize[0];
	int maxy = pmins[1] + psize[1];
	int maxz = pmins[2] + psize[2];

	for(int z = pmins[2]; z < maxz; z++)
	{
		for(int y = pmins[1]; y < maxy; y++)
		{
			for(int x = pmins[0]; x < maxx; x++)
			{
				int sampleindex = LightGrid_GetGridIndex(x, y, z);
				if(g_lightGridSamplesVector[sampleindex].occluded)
					numoccluded++;
				else
					numunoccluded++;
			}
		}
	}
}

// =====================================================================================
//  MakeOctreeLump
// =====================================================================================
int LightGrid_BuildOctree( int* pmins, const int* psize, int depth, int& occludedcellcount )
{
	assert(psize[0] > 0);
	assert(psize[1] > 0);
	assert(psize[2] > 0);

	// Count occlued/unoccluded sample points
	int numoccluded, numunoccluded;
	CountOccludedAndUnoccludedSamples(pmins, psize, numoccluded, numunoccluded);
	if(!numunoccluded)
	{
		occludedcellcount += (psize[0] * psize[1] * psize[2]);
		return FL_OCTREE_NODE_OCCLUDED;
	}
	else
	{
		occludedcellcount += numoccluded;
	}

	// Create a leaf if: We're less than the min node dim on any of the
	// axes, or we've reached the max depth, or if we have less than
	// eight non-occluded sample points
	if(psize[0] < MIN_NODE_DIMENSION || psize[1] < MIN_NODE_DIMENSION 
		|| psize[2] < MIN_NODE_DIMENSION || depth == MAX_OCTREE_DEPTH
		|| numunoccluded < 8)
	{
		// Create new leaf
		octree_leaf_t leaf(pmins, psize);
		int leafnum = g_octreeLeavesVector.size();
		g_octreeLeavesVector.push_back(leaf);

		return (FL_OCTREE_NODE_LEAF | leafnum);
	}
	else
	{
		// Get divison point for node
		int divisionpoint[3];
		GetDivisionPoint(pmins, psize, divisionpoint);

		// Create 8 child nodes
		int children[8] = { 0 };
		for(int i = 0; i < 8; i++)
		{
			int childmins[3];
			int childsize[3];
			LightGrid_GetOctant(i, pmins, psize, divisionpoint, childmins, childsize);
			children[i] = LightGrid_BuildOctree(childmins, childsize, depth+1, occludedcellcount);
		}

		int nodenum = g_octreeNodesVector.size();
		octree_node_t node(divisionpoint, children);
		g_octreeNodesVector.push_back(node);
		return nodenum;
	}
}

// =====================================================================================
//  MakeOctreeLump
// =====================================================================================
void LightGrid_OctreeLookup_Recursive( int nodeindex, const int* ptestpoint, bool& occluded, lightgrid_sample_t& outsample )
{
	if(nodeindex & FL_OCTREE_NODE_OCCLUDED)
	{
		occluded = true;
		outsample = lightgrid_sample_t();
	}
	else if(nodeindex & FL_OCTREE_NODE_LEAF)
	{
		int index = LightGrid_GetGridIndex(ptestpoint[0], ptestpoint[1], ptestpoint[2]);
		const lightgrid_sample_t& sample = g_lightGridSamplesVector[index];
		outsample = sample;
		occluded = sample.occluded;
	}
	else
	{
		const octree_node_t& node = g_octreeNodesVector[nodeindex];
		int childindex = LightGrid_GetChildIndex(node.divisionpoint, ptestpoint);
		LightGrid_OctreeLookup_Recursive(node.children[childindex], ptestpoint, occluded, outsample);
	}
}

// =====================================================================================
//  MakeOctreeLump
// =====================================================================================
void MakeOctreeLump( void )
{
#ifdef DEBUG
	int points1[3];
	int points2[3];
	int points3[3];

	// Perform sanity checks on funtions to ensure they work properly
	points1[0] = 1; points1[1] = 1; points1[2] = 1;
	points2[0] = 2; points2[1] = 2; points2[2] = 2;
    assert(LightGrid_GetChildIndex(points1, points2) == 7);

	points2[0] = 1; points2[1] = 1; points2[2] = 0;
    assert(LightGrid_GetChildIndex(points1, points2) == 6);

	points2[0] = 1; points2[1] = 0; points2[2] = 1;
	assert(LightGrid_GetChildIndex(points1, points2) == 5);

	points2[0] = 1; points2[1] = 0; points2[2] = 0;
	assert(LightGrid_GetChildIndex(points1, points2) == 4);

	points2[0] = 0; points2[1] = 1; points2[2] = 1;
	assert(LightGrid_GetChildIndex(points1, points2) == 3);

	points2[0] = 0; points2[1] = 1; points2[2] = 0;
	assert(LightGrid_GetChildIndex(points1, points2) == 2);

	points2[0] = 0; points2[1] = 0; points2[2] = 1;
	assert(LightGrid_GetChildIndex(points1, points2) == 1);

	points2[0] = 0; points2[1] = 0; points2[2] = 0;
	assert(LightGrid_GetChildIndex(points1, points2) == 0);

	// Perform sanity check on GetOctant
	points1[0] = 0; points1[1] = 0; points1[2] = 0;
	points2[0] = 2; points2[1] = 2; points2[2] = 2;
	points3[0] = 1; points3[1] = 1; points3[2] = 1;

	int childmins[3];
	int childsize[3];
	LightGrid_GetOctant(0, points1, points2, points3, childmins, childsize);
	assert(childmins[0] == 0 && childmins[1] == 0 && childmins[2] == 0 && childsize[0] == 1 && childsize[1] == 1 && childsize[2] == 1);

	LightGrid_GetOctant(7, points1, points2, points3, childmins, childsize);
	assert(childmins[0] == 1 && childmins[1] == 1 && childmins[2] == 1 && childsize[0] == 1 && childsize[1] == 1 && childsize[2] == 1);
#endif

	// Build the root node
	int occludednodecount = 0;
	int rootnodecoords[3] = {0, 0, 0};
	int rootnodeindex = LightGrid_BuildOctree(rootnodecoords, g_gridSize, 0, occludednodecount);

	// Collect stats
	int storedcellcount = 0;
	for(int i = 0; i < g_octreeLeavesVector.size(); i++)
	{
		octree_leaf_t& leaf = g_octreeLeavesVector[i];
		int count = (leaf.size[0] * leaf.size[1] * leaf.size[2]);
		storedcellcount += count;
	}

#ifdef DEBUG
	DumpSMD_LightGrid();

	// Perform self-check
	for(int z = 0; z < g_gridSize[2]; z++)
	{
		for(int y = 0; y < g_gridSize[1]; y++)
		{
			for(int x = 0; x < g_gridSize[0]; x++)
			{
				bool occluded = false;
				int coords[3] = {x, y, z};
				lightgrid_sample_t sample;
				LightGrid_OctreeLookup_Recursive(rootnodeindex, coords, occluded, sample);

				int sampleindex = LightGrid_GetGridIndex(x, y, z);
				lightgrid_sample_t& gridsample = g_lightGridSamplesVector[sampleindex];

				if(occluded)
				{
					assert(gridsample.occluded);
				}
				else
				{
					assert(!gridsample.occluded);
					assert(gridsample == sample);
				}
			}
		}
	}
#endif

	// Calculate size of raw sample data
	int rawsampledatasize = 0;
	for(int i = 0; i < g_octreeLeavesVector.size(); i++)
	{
		octree_leaf_t& leaf = g_octreeLeavesVector[i];

		int maxs[3];
		for(int j = 0; j < 3; j++)
			maxs[j] = leaf.mins[j] + leaf.size[j];

		for(int z = leaf.mins[2]; z < maxs[2]; z++)
		{
			for(int y = leaf.mins[1]; y < maxs[1]; y++)
			{
				for(int x = leaf.mins[0]; x < maxs[0]; x++)
				{
					int sampleindex = LightGrid_GetGridIndex(x, y, z);
					lightgrid_sample_t& sample = g_lightGridSamplesVector[sampleindex];

					int j = 0;
					for(; j < MAXLIGHTMAPS; j++)
					{
						if(sample.styles[j] == 255)
							break;
					}

					sample.dataoffset = rawsampledatasize;
					rawsampledatasize += 3 * j;
				}
			}
		}
	}
	
	float sizekbytes = (static_cast<float>(rawsampledatasize * 3) / 1024.0f);

	int totalcount = (occludednodecount + storedcellcount);
	Log("Light grid octree statistics:\n");
	Log(" - %d grid nodes stored(%d%%)\n", storedcellcount, static_cast<int>((static_cast<float>(storedcellcount) / static_cast<float>(totalcount)) * 100));
	Log(" - %d occluded nodes(%d%%)\n", occludednodecount, static_cast<int>((static_cast<float>(occludednodecount) / static_cast<float>(totalcount)) * 100));
	Log(" - %d nodes total\n", totalcount);
	Log("Light grid raw sample data size: %.4f kbytes\n", sizekbytes);

	int compressiontypes[NB_LIGHTGRID_DATA_LAYERS] = { 0 };
	int compressionlevels[NB_LIGHTGRID_DATA_LAYERS] = { 0 };
	int lightdatasizes[NB_LIGHTGRID_DATA_LAYERS] = { 0 };
	byte* psampledata[NB_LIGHTGRID_DATA_LAYERS] = { nullptr };
	for(int i = 0; i < NB_LIGHTGRID_DATA_LAYERS; i++)
	{
		psampledata[i] = new byte[rawsampledatasize];
		memset(psampledata[i], 0, sizeof(byte)*rawsampledatasize);
	}

	// Create buffers for light data
	int rawsampledataoffset = 0;
	int sampleindexoffset = 0;
	std::vector<int> sampleindexes(storedcellcount);

	for(int i = 0; i < g_octreeLeavesVector.size(); i++)
	{
		octree_leaf_t& leaf = g_octreeLeavesVector[i];

		int maxs[3];
		for(int j = 0; j < 3; j++)
			maxs[j] = leaf.mins[j] + leaf.size[j];

		// Save samples
		leaf.firstsample = sampleindexoffset;
		for(int z = leaf.mins[2]; z < maxs[2]; z++)
		{
			for(int y = leaf.mins[1]; y < maxs[1]; y++)
			{
				for(int x = leaf.mins[0]; x < maxs[0]; x++)
				{
					// Get source sample
					int sampleindex = LightGrid_GetGridIndex(x, y, z);
					lightgrid_sample_t& sample = g_lightGridSamplesVector[sampleindex];

					// Get destination sample
					sampleindexes[sampleindexoffset] = sampleindex;
					sampleindexoffset++;

					if(sample.occluded)
					{
						// Don't bother with this sample if it's occluded
						// Note: Still needs to be added to the output as
						// we need fast indexing via the tile coords
						sample.rawsampleoffset = -1;
						continue;
					}

					sample.rawsampleoffset = rawsampledataoffset;

					int j = 0;
					int stylecount = 0;
					for(; j < MAXLIGHTMAPS; j++)
					{
						if(sample.styles[j] == 255)
							continue;

						// Set final ambient
						for(int k = 0; k < 3; k++)
						{
							float value = sample.light_ambient[j][k];
							if (value < g_minlight)
								value = g_minlight;

							if (g_colour_qgamma[k] != 1.0)
								value = (float)pow(value / 256.0f, g_colour_qgamma[k]) * 256.0f;

							int ivalue = value;
							if(ivalue < 0)
								ivalue = 0;
							else if(ivalue > 255)
								ivalue = 255;

							byte* pdestdata = (psampledata[LIGHTGRID_LAYER_AMBIENT] + sample.rawsampleoffset + j * 3) + k;
							(*pdestdata) = ivalue;
						}

						// Set final diffuse
						for(int k = 0; k < 3; k++)
						{
							float value = sample.light_diffuse[j][k];
							value *= g_colour_lightscale[k];
							if (value < g_minlight)
								value = g_minlight;

							if (g_colour_qgamma[k] != 1.0)
								value = (float)pow(value / 256.0f, g_colour_qgamma[k]) * 256.0f;

							int ivalue = value;
							if(ivalue < 0)
								ivalue = 0;
							else if(ivalue > 255)
								ivalue = 255;

							byte* pdestdata = (psampledata[LIGHTGRID_LAYER_DIFFUSE] + sample.rawsampleoffset + j * 3) + k;
							(*pdestdata) = ivalue;
						}

						// Set final lightvec
						for(int k = 0; k < 3; k++)
						{
							float value = sample.light_direction[j][k];
							int ivalue = (value + 1.0f) * 127.5f;
							if(ivalue < 0)
								ivalue = 0;
							else if(ivalue > 255)
								ivalue = 255;

							byte* pdestdata = (psampledata[LIGHTGRID_LAYER_VECTORS] + sample.rawsampleoffset + j * 3) + k;
							(*pdestdata) = ivalue;
						}

						stylecount++;
					}

					// Increment offset into sampling data
					rawsampledataoffset += (stylecount * 3);
				}
			}
		}

		// Set final count
		leaf.numsamples = sampleindexoffset - leaf.firstsample;
	}

	// Compress lightmap data
	FinalizeLightData(rawsampledatasize, psampledata[LIGHTGRID_LAYER_AMBIENT], lightdatasizes[LIGHTGRID_LAYER_AMBIENT], compressionlevels[LIGHTGRID_LAYER_AMBIENT], compressiontypes[LIGHTGRID_LAYER_AMBIENT], nullptr);
	FinalizeLightData(rawsampledatasize, psampledata[LIGHTGRID_LAYER_DIFFUSE], lightdatasizes[LIGHTGRID_LAYER_DIFFUSE], compressionlevels[LIGHTGRID_LAYER_DIFFUSE], compressiontypes[LIGHTGRID_LAYER_DIFFUSE], nullptr);
	FinalizeLightData(rawsampledatasize, psampledata[LIGHTGRID_LAYER_VECTORS], lightdatasizes[LIGHTGRID_LAYER_VECTORS], compressionlevels[LIGHTGRID_LAYER_VECTORS], compressiontypes[LIGHTGRID_LAYER_VECTORS], nullptr);

	Log("Done compressing sample data for light grid\n");

	float sizekbytesold = (static_cast<float>(rawsampledatasize) / 1024.0f);
	float sizekbytesnew = (static_cast<float>(lightdatasizes[LIGHTGRID_LAYER_AMBIENT]) / 1024.0f);
	Log("Reduced ambient data from %.2f kbytes to %.2f kbytes(%.2f%%)\n", sizekbytesold, sizekbytesnew, static_cast<float>((sizekbytesnew/sizekbytesold)*100));

	sizekbytesnew = (static_cast<float>(lightdatasizes[LIGHTGRID_LAYER_DIFFUSE]) / 1024.0f);
	Log("Reduced diffuse data from %.2f kbytes to %.2f kbytes(%.2f%%)\n", sizekbytesold, sizekbytesnew, static_cast<float>((sizekbytesnew/sizekbytesold)*100));

	sizekbytesnew = (static_cast<float>(lightdatasizes[LIGHTGRID_LAYER_VECTORS]) / 1024.0f);
	Log("Reduced vectors data from %.2f kbytes to %.2f kbytes(%.2f%%)\n", sizekbytesold, sizekbytesnew, static_cast<float>((sizekbytesnew/sizekbytesold)*100));

	// Count in header, nodes, leaves, samples and sample indexes
	int lumpdatasize = sizeof(dlightgridlumpheader_t);
	lumpdatasize += g_octreeLeavesVector.size() * sizeof(dlightgridleaf_t);
	lumpdatasize += g_octreeNodesVector.size() * sizeof(dlightgridnode_t);
	lumpdatasize += storedcellcount * sizeof(dlightgridsample_t);
	lumpdatasize += lightdatasizes[LIGHTGRID_LAYER_AMBIENT] + lightdatasizes[LIGHTGRID_LAYER_DIFFUSE] + lightdatasizes[LIGHTGRID_LAYER_VECTORS];

	if(g_dlightgriddata)
		delete[] g_dlightgriddata;

	g_dlightgriddata = new byte[lumpdatasize];
	memset(g_dlightgriddata, 0, sizeof(byte)*lumpdatasize);
	g_dlightgriddatasize = lumpdatasize;

	int dataoffset = sizeof(dlightgridlumpheader_t);
	dlightgridlumpheader_t* pheader = reinterpret_cast<dlightgridlumpheader_t*>(g_dlightgriddata);

	for(int i = 0; i < 3; i++)
		pheader->grid_distance[i] = g_lightgriddistance;

	for(int i = 0; i < 3; i++)
		pheader->grid_size[i] = g_gridSize[i];

	VectorCopy(g_gridMinsCoords, pheader->grid_mins);

	pheader->rawsampledatasize = rawsampledatasize;
	pheader->rootnodeindex = rootnodeindex;
	pheader->totalsize = lumpdatasize;
	pheader->nodesoffset = dataoffset;
	pheader->numnodes = g_octreeNodesVector.size();
	dataoffset += pheader->numnodes * sizeof(dlightgridnode_t);

	for(int i = 0; i < pheader->numnodes; i++)
	{
		dlightgridnode_t* pdestnode = reinterpret_cast<dlightgridnode_t*>(reinterpret_cast<byte*>(pheader) + pheader->nodesoffset) + i;
		octree_node_t& srcnode = g_octreeNodesVector[i];

		for(int j = 0; j < 8; j++)
			pdestnode->children[j] = srcnode.children[j];

		for(int j = 0; j < 3; j++)
			pdestnode->divisionpoint[j] = srcnode.divisionpoint[j];
	}

	// Create sample block
	pheader->sampleoffset = dataoffset;
	pheader->numsamples = storedcellcount;
	dataoffset += pheader->numsamples * sizeof(dlightgridsample_t);

	// Get ptr to samples
	dlightgridsample_t* pdestsamples = reinterpret_cast<dlightgridsample_t*>(reinterpret_cast<byte*>(pheader) + pheader->sampleoffset);

	// Save leaves and samples
	pheader->leafsoffset = dataoffset;
	pheader->numleafs = g_octreeLeavesVector.size();
	dataoffset += pheader->numleafs * sizeof(dlightgridleaf_t);

	sampleindexoffset = 0;
	for(int i = 0; i < pheader->numleafs; i++)
	{
		dlightgridleaf_t* pdestleaf = reinterpret_cast<dlightgridleaf_t*>(reinterpret_cast<byte*>(pheader) + pheader->leafsoffset) + i;
		octree_leaf_t& srcleaf = g_octreeLeavesVector[i];

		for(int j = 0; j < 3; j++)
			pdestleaf->mins[j] = srcleaf.mins[j];

		for(int j = 0; j < 3; j++)
			pdestleaf->size[j] = srcleaf.size[j];

		int maxs[3];
		for(int j = 0; j < 3; j++)
			maxs[j] = srcleaf.mins[j] + srcleaf.size[j];

		pdestleaf->firstsample = srcleaf.firstsample;
		pdestleaf->numsamples = srcleaf.numsamples;

		// Save samples
		pdestleaf->firstsample = sampleindexoffset;
		for(int j = 0; j < srcleaf.numsamples; j++)
		{
			int srcsampleindex = sampleindexes[sampleindexoffset];
			lightgrid_sample_t& srcsample = g_lightGridSamplesVector[srcsampleindex];
			dlightgridsample_t* pdestsample = &pdestsamples[sampleindexoffset];
			sampleindexoffset++;

			if(srcsample.occluded)
			{
				// Occluded sample, don't bother with it
				pdestsample->rawsampleoffset = -1;
			}
			else
			{
				pdestsample->rawsampleoffset = srcsample.rawsampleoffset;

				for(int k = 0; k < MAXLIGHTMAPS; k++)
					pdestsample->styles[k] = srcsample.styles[k];
			}
		}
	}

	// Save compressed ambient data
	pheader->ambientdataoffset = dataoffset;
	pheader->ambientcompressedsize = lightdatasizes[LIGHTGRID_LAYER_AMBIENT];
	pheader->ambientcompressionlevel = compressionlevels[LIGHTGRID_LAYER_AMBIENT];
	pheader->ambientcompressiontype = compressiontypes[LIGHTGRID_LAYER_AMBIENT];
	dataoffset += pheader->ambientcompressedsize;

	byte* pdestdata = reinterpret_cast<byte*>(pheader) + pheader->ambientdataoffset;
	memcpy(pdestdata, psampledata[LIGHTGRID_LAYER_AMBIENT], sizeof(byte)*pheader->ambientcompressedsize);

	// Save compressed diffuse data
	pheader->diffusedataoffset = dataoffset;
	pheader->diffusecompressedsize = lightdatasizes[LIGHTGRID_LAYER_DIFFUSE];
	pheader->diffusecompressionlevel = compressionlevels[LIGHTGRID_LAYER_DIFFUSE];
	pheader->diffusecompressiontype = compressiontypes[LIGHTGRID_LAYER_DIFFUSE];
	dataoffset += pheader->diffusecompressedsize;

	pdestdata = reinterpret_cast<byte*>(pheader) + pheader->diffusedataoffset;
	memcpy(pdestdata, psampledata[LIGHTGRID_LAYER_DIFFUSE], sizeof(byte)*pheader->diffusecompressedsize);

	// Save compressed diffuse data
	pheader->vectorsdataoffset = dataoffset;
	pheader->vectorscompressedsize = lightdatasizes[LIGHTGRID_LAYER_VECTORS];
	pheader->vectorscompressionlevel = compressionlevels[LIGHTGRID_LAYER_VECTORS];
	pheader->vectorscompressiontype = compressiontypes[LIGHTGRID_LAYER_VECTORS];
	dataoffset += pheader->vectorscompressedsize;

	pdestdata = reinterpret_cast<byte*>(pheader) + pheader->vectorsdataoffset;
	memcpy(pdestdata, psampledata[LIGHTGRID_LAYER_VECTORS], sizeof(byte)*pheader->vectorscompressedsize);

	// Sanity check
	assert(sampleindexoffset == storedcellcount);
	assert(rawsampledataoffset == rawsampledatasize);
	assert(dataoffset == lumpdatasize);

	for(int i = 0; i < NB_LIGHTGRID_DATA_LAYERS; i++)
		delete[] psampledata[i];

	// Clear all data used
	g_lightGridSamplesVector.clear();
	g_octreeLeavesVector.clear();
	g_octreeNodesVector.clear();
}
