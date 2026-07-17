/*
===============================================
Pathos Engine - Created by Andrew Stephen "Overfloater" Lucas

Copyright 2016
All Rights Reserved.
===============================================
*/

#ifndef LIGHTGRID_H
#define LIGHTGRID_H

#include "cmdlib.h"
#include "mathlib.h"
#include "bspfile.h"

// Flag for if the entire bounds is occluded for a node
static constexpr int FL_OCTREE_NODE_OCCLUDED	= (1<<31);
// Flag for if an octree node is a leaf
static constexpr int FL_OCTREE_NODE_LEAF		= (1<<30);
// If neither flags are set, this is a node
static constexpr int FL_OCTREE_FLAGS			= (FL_OCTREE_NODE_OCCLUDED|FL_OCTREE_NODE_LEAF);

struct lightgrid_threadinfo_t
{
	lightgrid_threadinfo_t():
		firstsample(-1),
		numsamples(0)
	{}

	int firstsample;
	int numsamples;
};

struct lightgrid_sample_t
{
	lightgrid_sample_t():
		dataoffset(-1),
		occluded(false)
	{
		memset(styles, 255, sizeof(styles));
		styles[0] = 0;

		memset(origin, 0, sizeof(origin));
		memset(light_diffuse, 0, sizeof(light_diffuse));
		memset(light_ambient, 0, sizeof(light_ambient));
		memset(light_direction, 0, sizeof(light_direction));
	}

	bool operator==( const lightgrid_sample_t& other ) const
	{
		if(memcmp(styles, other.styles, sizeof(styles))
			|| memcmp(light_diffuse, other.light_diffuse, sizeof(light_diffuse))
			|| memcmp(light_ambient, other.light_ambient, sizeof(light_ambient))
			|| memcmp(light_direction, other.light_direction, sizeof(light_direction))
			|| occluded != other.occluded)
			return false;
		else
			return true;
	}
	
	vec3_t origin;
	byte styles[MAXLIGHTMAPS];
	vec3_t light_diffuse[MAXLIGHTMAPS];
	vec3_t light_ambient[MAXLIGHTMAPS];
	vec3_t light_direction[MAXLIGHTMAPS];
	int dataoffset;
	bool occluded;
};

struct octree_node_t
{
	octree_node_t()
	{
		memset(divisionpoint, 0, sizeof(divisionpoint));
		memset(children, 0, sizeof(children));
	}

	octree_node_t(const int* pdivisionpoint, const int* pchildren)
	{
		for(int i = 0; i < 3; i++)
			divisionpoint[i] = pdivisionpoint[i];

		for(int i = 0; i < 8; i++)
			children[i] = pchildren[i];
	}

	int divisionpoint[3];
	int children[8];
};

struct octree_leaf_t
{
	octree_leaf_t()
	{
		memset(mins, 0, sizeof(mins));
		memset(size, 0, sizeof(size));
	}

	octree_leaf_t(const int* pmins, const int* psize)
	{
		for(int i = 0; i < 3; i++)
			mins[i] = pmins[i];

		for(int i = 0; i < 3; i++)
			size[i] = psize[i];
	}

	int mins[3];
	int size[3];
};

extern void BuildLightGrid( void );
extern void CalcLightGridBounds( vec3_t& outmins, vec3_t& outmaxs );
extern void ProcessGridSamplePoints( int sampleIndex );
extern bool IsPointInSolidRecursive( const dmodel_t& model, int nodeindex, const vec3_t& point );
extern bool FixLightOnFace( const dmodel_t& model, const vec3_t& point, float distance, vec3_t outpos );
extern void GatherGridPointLight( const vec3_t pos, const byte* const pvs, lightgrid_sample_t& gridsample );
extern void MakeOctreeLump( void );
extern void CountOccludedAndUnoccludedSamples( const int* pmins, const int* psize, int& numoccluded, int& numunoccluded );
extern void GetDivisionPoint( const int* pmins, const int* psize, int* pout );

extern int LightGrid_GetChildIndex( const int* pdivisionpoint, const int* ptestpoint );
extern void LightGrid_GetOctant( int i, const int* pmins, const int* psize, const int* pdivsionpoint, int* poutchildmins, int* poutchildsize );
extern int LightGrid_GetGridIndex( int x, int y, int z );
extern void LightGrid_GetWorldCoordsFromGridIndex( int* pindex, const vec3_t& gridmins, vec3_t& outcoords );
extern int LightGrid_BuildOctree( int* pmins, const int* psize, int depth, int& occludedcellcount );
#endif // LIGHTGRID_H