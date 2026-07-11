/*
===============================================
Pathos Engine - Created by Andrew Stephen "Overfloater" Lucas

Copyright 2016
All Rights Reserved.
===============================================
*/

#ifndef VBMBVH_H
#define VBMBVH_H

#include <Windows.h>
#include <vector>
#include <string>

#include "vbmbvh.h"
#include "mathtypes.h"
#include "vbmformat.h"
#include "studio.h"
#include "vbmcache.h"

// Triangle index alloc size
static constexpr int TRIANGLE_INDEX_ALLOC_SIZE = 128;

struct pmffile_t;
struct model_texture_t;
class CVBMCache;

//===========================
// CVBMBVH
// 
//===========================
class CVBMBVH
{
public:
	enum planetype_t
	{
		PLANE_UNDEFINED = -1,
		PLANE_X = 0,
		PLANE_Y,
		PLANE_Z,
		PLANE_AX,
		PLANE_AY,
		PLANE_AZ
	};

	struct bvhthreadinfo_t
	{
		bvhthreadinfo_t():
			basedistance(0),
			distance(0),
			tracefraction(0),
			planedistance(0),
			trianglesvector(TRIANGLE_INDEX_ALLOC_SIZE),
			numtriangles(0)
		{
			memset(normdirection, 0, sizeof(normdirection));
			memset(endposition, 0, sizeof(endposition));
			memset(planenormal, 0, sizeof(planenormal));
		}
		// Base distance
		float basedistance;
		// Final distance
		float distance;
		// Normalized ray direction
		vec3_t normdirection;
		// Fraction of trace returned
		float tracefraction;
		// End position where trace hit
		vec3_t endposition;
		// Normal of surface we hit
		vec3_t planenormal;
		// Plane distance of the normal we hit
		float planedistance;

		// Triangle index array
		std::vector<int> trianglesvector;
		// Triangle count
		int numtriangles;
	};

	struct bvhvertex_t
	{
		bvhvertex_t()
		{
			memset(origin, 0, sizeof(origin));
			memset(texcoords, 0, sizeof(texcoords));
		}

		vec3_t origin;
		float texcoords[2];
	};

	struct bvhtriangle_t
	{
		bvhtriangle_t():
			skinref(-1),
			distance(0),
			signbits(0),
			planetype(0)
		{
			memset(centroid, 0, sizeof(centroid));
			memset(normal, 0, sizeof(normal));
			memset(vertindexes, 0, sizeof(vertindexes));
			memset(edge00, 0, sizeof(edge00));
			memset(edge02, 0, sizeof(edge02));
		}

		vec3_t centroid;
		vec3_t normal;
		int vertindexes[3];
		int skinref;
		float distance;
		int signbits;
		int planetype;

		vec3_t edge00;
		vec3_t edge02;
	};

	struct bvhnode_t
	{
		bvhnode_t():
			index(-1),
			isleaf(false)
		{
			memset(mins, 0, sizeof(mins));
			memset(maxs, 0, sizeof(maxs));
			memset(childnodes, 0, sizeof(childnodes));
		}

		int index;
		vec3_t mins;
		vec3_t maxs;
		int childnodes[2];
		bool isleaf;

		std::vector<int> triindexes;
	};

	struct bvhsubmodel_t
	{
		bvhsubmodel_t(int numtriangles):
			triangles(numtriangles)
		{
			memset(mins, 0, sizeof(mins));
			memset(maxs, 0, sizeof(maxs));
		}

		~bvhsubmodel_t()
		{
			if(!pbvhnodes.empty())
			{
				for(int i = 0; i < pbvhnodes.size(); i++)
					delete pbvhnodes[i];

				pbvhnodes.clear();
			}
		}

		void setVertexCount( int vertexcount )
		{
			vertexes.resize(vertexcount);
		}

		std::string name;

		vec3_t mins;
		vec3_t maxs;

		std::vector<bvhtriangle_t> triangles;
		std::vector<bvhvertex_t> vertexes;
		std::vector<bvhnode_t*> pbvhnodes;
	};

	struct bvhbodypart_t
	{
		bvhbodypart_t():
			base(-1)
		{}

		~bvhbodypart_t()
		{
			if(!submodels.empty())
			{
				for(int i = 0; i < submodels.size(); i++)
					delete submodels[i];

				submodels.clear();
			}
		}

		std::string name;
		int base;

		std::vector<bvhsubmodel_t*> submodels;
	};

public:
	// Constructor
	CVBMBVH( const vbmheader_t* pvbmheader, const studiohdr_t* pstudioheader, int sequence, float scale, int cacheindex );
	// Destructor
	~CVBMBVH( void );

public:
	// Set up VBH object
	bool SetupBVH( CVBMCache& vbmCache );

	// Get sequence used
	int GetSequence( void ) { return m_sequenceIndex; }
	// Get scale used
	float GetScale( void ) { return m_modelScale; }
	// Return the cache index of this BVH
	int GetCacheIndex( void ) const { return m_cacheIndex; }
	// Returns local min bounds of BVH
	const vec3_t& GetMins( void ) { return m_mins; }
	// Returns local max bounds of BVH
	const vec3_t& GetMaxs( void ) { return m_maxs; }

	// Perform line trace
	bool TraceLine( const vec3_t& start, const vec3_t& end, int body, int skin, float* poutfraction = nullptr, vec3_t* poutnormal = nullptr, vec3_t* poutposition = nullptr, float* poutplanedistance = nullptr ) const;

	// Return the vbm header used
	const vbmheader_t* GetVBMHeader( void ) const { return m_pVBMheader; }
	// Return the studio header used
	const studiohdr_t* GetStudioeader( void ) const { return m_pStudioHeader; }

private:
	// Update bounds of a BVH node
	void UpdateBVHNodeBounds( bvhsubmodel_t* pdestsubmodel, bvhnode_t* pnode );
	// Subdivides a BVH node
	void SubdivideBVHNode( bvhsubmodel_t* pdestsubmodel, bvhnode_t* pnode );
	// Set up a bodypart for processing
	const bvhsubmodel_t* SetupModel( int bodypart, int bodyvalue ) const;

	// Recurse down the tree with a point trace
	void RecurseTreePointTrace( bvhthreadinfo_t& threadinfo, const bvhsubmodel_t* psubmodel, const vec3_t& start, const vec3_t& end, const bvhnode_t* pbvhnode ) const;
	// Add triangle to the list
	void AddBVHNodeTriangles( bvhthreadinfo_t& threadinfo, const bvhnode_t* pbvhnode ) const;
	// Perform a line test against a triangle
	bool TestLineTriangleIntersect( bvhthreadinfo_t& threadinfo, const vec3_t& start, const vec3_t& end, const std::vector<bvhvertex_t>& vertexes, const bvhtriangle_t* ptriangle, vec3_t& impactPosition, vec3_t& impactNormal, float& planeDistance, float& fraction ) const;
	// Check alpha value of a texture for collision
	bool TestLineTriangleIntersectWithAlpha( bvhthreadinfo_t& threadinfo, const vec3_t& start, const std::vector<bvhvertex_t>& vertexes, const bvhtriangle_t* ptriangle, const model_texture_t* ptexture, vec3_t& impactPosition, vec3_t& impactNormal, float& planeDistance, float& fraction ) const;

private:
	// VBM file this BVH is using
	const vbmheader_t* m_pVBMheader;
	// Studiomodel file this is using
	const studiohdr_t* m_pStudioHeader;
	
	// Array of material files
	std::vector<const pmffile_t*> m_pMaterialFilesVector;
	// Array of bodyparts
	std::vector<bvhbodypart_t*> m_pBodyPartsVector;

	// Sequence index used
	int m_sequenceIndex;
	// Scale used
	float m_modelScale;
	// Cache index of this BVH
	int m_cacheIndex;

	// Mins of entire model
	vec3_t m_mins;
	// Maxs of entire mdoel
	vec3_t m_maxs;
	
};
#endif // VBMBVH_H