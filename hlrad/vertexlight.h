/*
===============================================
Pathos Engine - Created by Andrew Stephen "Overfloater" Lucas

Copyright 2016
All Rights Reserved.
===============================================
*/

#ifndef VERTEXLIGHT_H
#define VERTEXLIGHT_H

#include "qrad.h"
#include "vbmcache.h"
#include "mathlib.h"
#include "mathtypes.h"
#include "cbitset.h"

// Number of vertices per thread
static const int MAX_THREAD_VERTEXES = 1024;

// Data structure for thread information for
// processing vertices
struct vbaked_threadinfo_t
{
	vbaked_threadinfo_t():
		plightinfo(nullptr),
		pcache(nullptr),
		firstvertexindex(0),
		numvertexes(0),
		pnumstyles(nullptr),
		pent_styles(nullptr),
		psamples_amb(nullptr),
		psamples_diff(nullptr),
		psamples_vecs(nullptr)
	{
	}

	void setvertexcount( int _numvertexes )
	{
		numvertexes = _numvertexes;

		psamples_amb = new vec3_t*[numvertexes];
		for(int i = 0; i < numvertexes; i++)
		{
			psamples_amb[i] = new vec3_t[1];
			memset(psamples_amb[i], 0, sizeof(vec3_t));
		}

		psamples_diff = new vec3_t*[numvertexes];
		for(int i = 0; i < numvertexes; i++)
		{
			psamples_diff[i] = new vec3_t[1];
			memset(psamples_diff[i], 0, sizeof(vec3_t));
		}

		psamples_vecs = new vec3_t*[numvertexes];
		for(int i = 0; i < numvertexes; i++)
		{
			psamples_vecs[i] = new vec3_t[1];
			memset(psamples_vecs[i], 0, sizeof(vec3_t));
		}

		pent_styles = new byte*[numvertexes];
		for(int i = 0; i < numvertexes; i++)
		{
			pent_styles[i] = new byte[1];
			pent_styles[i][0] = 0;
		}

		pnumstyles = new int[numvertexes];
		for(int i = 0; i < numvertexes; i++)
			pnumstyles[i] = 1;
	}

	~vbaked_threadinfo_t()
	{
		if(pent_styles)
		{
			for(int i = 0; i < numvertexes; i++)
				delete[] pent_styles[i];

			delete[] pent_styles;
		}

		if(psamples_amb)
		{
			for(int i = 0; i < numvertexes; i++)
				delete[] psamples_amb[i];

			delete[] psamples_amb;
		}

		if(psamples_diff)
		{
			for(int i = 0; i < numvertexes; i++)
				delete[] psamples_diff[i];

			delete[] psamples_diff;
		}

		if(psamples_vecs)
		{
			for(int i = 0; i < numvertexes; i++)
				delete[] psamples_vecs[i];

			delete[] psamples_vecs;
		}

		if(pnumstyles)
			delete[] pnumstyles;
	}

	entity_lightinginfo_t* plightinfo;
	CVBMCache::vertexcache_t* pcache;
	int firstvertexindex;
	int numvertexes;
	int* pnumstyles;

	byte** pent_styles;
	vec3_t **psamples_amb;
	vec3_t **psamples_diff;
	vec3_t **psamples_vecs;
};

extern vec_t GetBrightestSample( vec3_t& add, vec3_t& add_ambient, vec3_t& add_diffuse );
extern void GatherVertexLight(const vec3_t pos, const byte* const pvs, const vec3_t normal, byte*& styles, vec3_t*& sample_ambient, vec3_t*& sample_diffuse, vec3_t*& sample_lightvectors, int& numstyles);
extern void AddLight(directlight_t* l, CBitSet& directLightTraceBitset, CBitSet& directLightTraceSetBitset, std::vector<CBitSet>& envLightsBitset, std::vector<CBitSet>& envLightsSetBitset, std::vector<CBitSet>& envSkyLightBitset, std::vector<CBitSet>& envSkyLightSetBitset, vec3_t* pdirections, const vec3_t pos, const byte* const pvs, const vec_t* pnormal, float normalfactor, byte* styles, int step, int miptex, int texlightgap_surfacenum, vec3_t* padds, vec3_t* padds_ambient, vec3_t* padds_diffuse, vec3_t* padds_lightvectors, vec3_t* ptexlightgap_textoworld, bool bumpinfopass, bool createbumpmapdata, bool softsky);
extern void BuildVertexLights( void );
extern void LoadEntityVBMModels( void );

#endif //VERTEXLIGHT_H