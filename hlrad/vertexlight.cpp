/*
===============================================
Pathos Engine - Created by Andrew Stephen "Overfloater" Lucas

Copyright 2016
All Rights Reserved.
===============================================
*/

#include "qrad.h"
#include <assert.h>
#include "datatypes.h"
#include "aldformat.h"
#include "miniz.h"
#include <string>
#include "vbmcache.h"
#include "studio_util.h"
#include "md5.h"
#include "vertexlight.h"

// Mod directory
extern char				g_modDir[_MAX_PATH];
// Direct lights
extern directlight_t*	directlights[MAX_MAP_LEAFS];

// Thread infos for baked vertex lighting
vbaked_threadinfo_t*	g_pBakedVertexThreadInfos = nullptr;
// Number of baked vertex thread infos
int						g_numBakedVertexThreadInfos = 0;

// Env model flags
static const int ENVMODEL_SF_NO_SHADOWS				= (1<<4);

// Renderfx values for special env_model render types
static const int ENVMODEL_RENDERFX_SCALED			= 117;
static const int ENVMODEL_RENDERFX_SCALED_PORTAL	= 118;
static const int ENVMODEL_RENDERFX_SCALED_SKY		= 72;

// =====================================================================================
//  LoadEntityVBMModels
// =====================================================================================
void LoadEntityVBMModels( void )
{
	if(!strlen(g_modDir))
	{
		Warning("No mod folder specified, VBM models will not have shadows. Use '-moddir' to specify the mod directory.\n");
		return;
	}

	// Set the mod directory
	gVBMCache.SetModDirectoryPath(g_modDir);

	// Go throuch each entity and load VBMs, set up BVHs
    for (int i = 0; i < g_numentities; i++)
	{
        entity_t* pentity = &g_entities[i];
        const char* pstrClassname = ValueForKey(pentity, "classname");
		if(!pstrClassname)
			continue;

		// Ensure it's an env_model
		if(strcmp(pstrClassname, "env_model") != 0)
			continue;

		// Fetch targetname. If it's there, we can't use this entity
		const char* pstr = ValueForKey(pentity, "targetname");
		if(pstr && strlen(pstr) > 0)
			continue;

		// Get model
		pstr = ValueForKey(pentity, "model");
		if(!pstr || strlen(pstr) <= 0)
			continue;

		const CVBMCache::vbmcache_t* pcache = gVBMCache.LoadModel(pstr);
		if(!pcache)
		{
			const char* pstrorigin = ValueForKey(pentity, "origin");
			Warning("Couldn't load model '%s' for entity at position '%s' with classname '%s'.\n", pstr, pstrorigin, pstrClassname);
			pentity->extradataindex = -1;
			continue;
		}

		// Now build the vertex cache, we'll need it for lighting
		// Get values we'll need
		int sequence = IntForKey(pentity, "sequence");
		if(sequence && sequence > pcache->pstudiohdr->numseq)
			sequence = 0;

		vec3_t origin;
		GetVectorForKey(pentity, "origin", origin);

		vec3_t angles;
		GetVectorForKey(pentity, "angles", angles);

		float scale = 1.0;
		int renderfx = IntForKey(pentity, "renderfx");
		if(renderfx == ENVMODEL_RENDERFX_SCALED
			|| renderfx == ENVMODEL_RENDERFX_SCALED_PORTAL
			|| renderfx == ENVMODEL_RENDERFX_SCALED_SKY)
			scale = FloatForKey(pentity, "scale");

		// Call on the class to build the vertex cache
		const CVBMCache::vertexcache_t* pvertexcache = gVBMCache.BuildVertexCache(pcache, origin, angles, sequence, scale);
		if(!pvertexcache)
			continue;

		int body = IntForKey(pentity, "body");
		int skin = IntForKey(pentity, "skin");

		// Create entry for this entity
		pentity->extradataindex = g_numEntityLightingInfos;
		g_numEntityLightingInfos++;

		entity_lightinginfo_t& lightingInfo = g_entityLightingInfos[pentity->extradataindex];
		lightingInfo.entindex = i;
		lightingInfo.vcacheindex = pvertexcache->index;
		lightingInfo.modelindex = pcache->cacheindex;
		lightingInfo.body = body;
		lightingInfo.skin = skin;

		VectorCopy(origin, lightingInfo.origin);
		VectorCopy(angles, lightingInfo.angles);

		for(int j = 0; j < 3; j++)
		{
			lightingInfo.mins[j] = MAX_FLOAT_VALUE;
			lightingInfo.maxs[j] = -MAX_FLOAT_VALUE;
		}

		// Calculate vertex cache mins/maxs, cache vertexes
		// will be in real space coords
		for(int j = 0; j < pvertexcache->numvertexes; j++)
		{
			CVBMCache::cachevertex_t* pvertex = &pvertexcache->pvertexes[j];
			for(int k = 0; k < 3; k++)
			{
				if(lightingInfo.mins[k] > pvertex->origin[k])
					lightingInfo.mins[k] = pvertex->origin[k];

				if(lightingInfo.maxs[k] < pvertex->origin[k])
					lightingInfo.maxs[k] = pvertex->origin[k];
			}
		}

		// Set up world to local matrix
		vec3_t _angles;
		VectorCopy(angles, _angles);
		_angles[PITCH] = -angles[PITCH];

		AngleMatrix(_angles, lightingInfo.worldlocalmatrix);
		for(int j = 0; j < 3; j++)
			lightingInfo.worldlocalmatrix[j][3] = origin[j];

		// Check spawnflags for no shadowing flag, and if not present, create BVH
		int spawnflags = IntForKey(pentity, "spawnflags");
		if(!(spawnflags & ENVMODEL_SF_NO_SHADOWS))
		{
			CVBMBVH* pBVH = gVBMCache.BuildBVHForEntity(pcache, sequence, scale);
			if(!pBVH)
			{
				Warning("Couldn't create BVH for entity with model '%s' at position '%.4f %.4f %.4f' with classname '%s'.\n", pcache->name.c_str(), origin[0], origin[1], origin[2], pstrClassname);
				lightingInfo.bvhindex = -1;
			}
			else
			{
				// Transform BVH mins/maxs to entity space
				TransformMinsMaxs(lightingInfo.worldlocalmatrix, pBVH->GetMins(), pBVH->GetMaxs(), lightingInfo.bvhmins, lightingInfo.bvhmaxs);
				// Set the index of the BVH
				lightingInfo.bvhindex = pBVH->GetCacheIndex();
			}
		}
	}
}

// =====================================================================================
//  CalcVertexLighting
// =====================================================================================
void CalcVertexLighting( int threadInfoIndex )
{
	vbaked_threadinfo_t& threadInfo = g_pBakedVertexThreadInfos[threadInfoIndex];
	CVBMCache::vertexcache_t* pvertexcache = threadInfo.pcache;

	// Calculate lighting for the vertexes
	for(int i = 0; i < threadInfo.numvertexes; i++)
	{
		int vertexindex = threadInfo.firstvertexindex + i;
		CVBMCache::cachevertex_t* pvertex = &pvertexcache->pvertexes[vertexindex];

		dleaf_t* leaf = PointInLeaf(pvertex->origin);
		byte pvs[(MAX_MAP_LEAFS + 7) / 8];
		if (leaf->visofs != -1)
			DecompressVis(&g_dvisdata[leaf->visofs], pvs, sizeof(pvs));
		else
			memset(pvs, 0xFF, sizeof(pvs));

		GatherVertexLight(pvertex->origin, pvs, pvertex->normal, threadInfo.pent_styles[i], threadInfo.psamples_amb[i], threadInfo.psamples_diff[i], threadInfo.psamples_vecs[i], threadInfo.pnumstyles[i]);
	}
}

// =====================================================================================
//  FinalizeVBMModelLighting
// =====================================================================================
void FinalizeVBMModelLighting( CVBMCache::vertexcache_t* pvertexcache, entity_lightinginfo_t& lightingInfo, byte* ent_styles )
{
	byte			v_styles[ALLSTYLES];
	sample_t		*v_samples[ALLSTYLES];

	memset(v_styles, 255, sizeof(v_styles));
	v_styles[0] = 0;

	// Allocate samples
	for(int i = 0; i < ALLSTYLES; i++)
	{
		v_samples[i] = new sample_t[pvertexcache->numvertexes];
		memset(v_samples[i], 0, sizeof(sample_t)*pvertexcache->numvertexes);
	}

	for(int i = 0; i < lightingInfo.numthreadinfos; i++)
	{
		int infoindex = lightingInfo.firsthreadinfoindex + i;
		vbaked_threadinfo_t& threadInfo = g_pBakedVertexThreadInfos[infoindex];

		for(int j = 0; j < threadInfo.numvertexes; j++)
		{
			int vertexindex = threadInfo.firstvertexindex + j;
			CVBMCache::cachevertex_t& vertex = pvertexcache->pvertexes[vertexindex];

			for (int style = 0; style < threadInfo.pnumstyles[j]; ++style)
			{
				vec3_t null;
				VectorClear(null);

				vec_t samplebrightness = GetBrightestSample(null, threadInfo.psamples_amb[j][style], threadInfo.psamples_diff[j][style]);
				if (samplebrightness > g_corings[style] * 0.1)
				{
					int style_index;
					int seekstyle = threadInfo.pent_styles[j][style];
					for (style_index = 0; style_index < ALLSTYLES; style_index++)
					{
						if (v_styles[style_index] == seekstyle || v_styles[style_index] == 255)
						{
							break;
						}
					}

					if (style_index == ALLSTYLES) // shouldn't happen
					{
						if (++stylewarningcount >= stylewarningnext)
						{
							stylewarningnext = stylewarningcount * 2;
							Warning("Too many direct light styles on an entity(%f,%f,%f)", vertex.origin[0], vertex.origin[1], vertex.origin[2]);
							Warning(" total %d warnings for too many styles", stylewarningcount);
						}
						return;
					}

					if (v_styles[style_index] == 255)
						v_styles[style_index] = seekstyle;

					VectorCopy(threadInfo.psamples_amb[j][style], v_samples[style_index][vertexindex].light_ambient);
					VectorCopy(threadInfo.psamples_diff[j][style], v_samples[style_index][vertexindex].light_diffuse);
					VectorCopy(threadInfo.psamples_vecs[j][style], v_samples[style_index][vertexindex].light_vector);
				}
			}
		}
	}

	// Determine lightstyles
	vec_t maxlights[ALLSTYLES];
	for (int j = 0; j < ALLSTYLES && v_styles[j] != 255; j++)
	{
		maxlights[j] = 0;
		for (int i = 0; i < pvertexcache->numvertexes; i++)
		{
			vec_t b = GetBrightestSample(v_samples[j][i].light, v_samples[j][i].light_ambient, v_samples[j][i].light_diffuse);
			maxlights[j] = qmax(maxlights[j], b);
		}

		if (maxlights[j] <= g_corings[v_styles[j]] * 0.1) // light is too dim, discard this style to reduce RAM usage
			maxlights[j] = 0;
	}

	for (int k = 0; k < MAXLIGHTMAPS; k++)
	{
		int bestindex = -1;
		if (k == 0)
		{
			bestindex = 0;
		}
		else
		{
			vec_t bestmaxlight = 0;
			for (int j = 1; j < ALLSTYLES && v_styles[j] != 255; j++)
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
			ent_styles[k] = v_styles[bestindex];

			for(int i = 0; i < pvertexcache->numvertexes; i++)
			{
				VectorCopy(v_samples[bestindex][i].light_ambient, lightingInfo.plightdata[k][VERTEX_LIGHTING_AMBIENT][i]);
				VectorCopy(v_samples[bestindex][i].light_diffuse, lightingInfo.plightdata[k][VERTEX_LIGHTING_DIFFUSE][i]);
				VectorCopy(v_samples[bestindex][i].light_vector, lightingInfo.plightdata[k][VERTEX_LIGHTING_VECTORS][i]);
			}
		}
		else
		{
			ent_styles[k] = 255;
		}
	}

	// Release data we allocated
	for(int i = 0; i < ALLSTYLES; i++)
		delete[] v_samples[i];
}

// =====================================================================================
//  BuildVertexLights
// =====================================================================================
void BuildVertexLights( void )
{
	// Calculate required data and it's total amount
	int numVertexes = 0;
	g_numBakedVertexThreadInfos = 0;
	for (int i = 0; i < g_numEntityLightingInfos; i++)
	{
		entity_lightinginfo_t& lightingInfo = g_entityLightingInfos[i];
		if(lightingInfo.vcacheindex == -1)
			continue;

		CVBMCache::vertexcache_t* pvertexcache = gVBMCache.GetVertexCacheByIndex(lightingInfo.vcacheindex);
		if(!pvertexcache)
			continue;

		int numthreadinfos = ceil(static_cast<Float>(pvertexcache->numvertexes) / static_cast<Float>(MAX_THREAD_VERTEXES));
		g_numBakedVertexThreadInfos += numthreadinfos;
		numVertexes += pvertexcache->numvertexes;
	}

	if(!g_numBakedVertexThreadInfos)
		return;

	// Allocate info array
	g_pBakedVertexThreadInfos = new vbaked_threadinfo_t[g_numBakedVertexThreadInfos];

	int infoIndex = 0;
	for (int i = 0; i < g_numEntityLightingInfos; i++)
	{
		entity_lightinginfo_t& lightingInfo = g_entityLightingInfos[i];
		if(lightingInfo.vcacheindex == -1)
			continue;

		CVBMCache::vertexcache_t* pvertexcache = gVBMCache.GetVertexCacheByIndex(lightingInfo.vcacheindex);
		if(!pvertexcache)
			continue;

		lightingInfo.firsthreadinfoindex = infoIndex;

		int offset = 0;
		lightingInfo.numthreadinfos = ceil(static_cast<Float>(pvertexcache->numvertexes) / static_cast<Float>(MAX_THREAD_VERTEXES));
		for(int j = 0; j < lightingInfo.numthreadinfos; j++)
		{
			vbaked_threadinfo_t& threadInfo = g_pBakedVertexThreadInfos[infoIndex];
			CVBMCache::cachevertex_t* pvertex = &pvertexcache->pvertexes[j];
			threadInfo.firstvertexindex = offset;

			offset += MAX_THREAD_VERTEXES;
			if(offset > pvertexcache->numvertexes)
				offset = pvertexcache->numvertexes;

			int vertexcount = offset - threadInfo.firstvertexindex;
			threadInfo.setvertexcount(vertexcount);

			threadInfo.plightinfo = &lightingInfo;
			threadInfo.pcache = pvertexcache;
			infoIndex++;		
		}

	}

	// generate a position map for each face
	NamedRunThreadsOnIndividual(g_numBakedVertexThreadInfos, g_estimate, CalcVertexLighting);

	// Size of vertex lighting buffer
	int vertexbuffersize = 0;
	// Calculate required data and it's total amount. We need to do this separately because
	// we need the final number of styles after we're done calculating lighting and styles
	for (int i = 0; i < g_numEntityLightingInfos; i++)
	{
		entity_lightinginfo_t& lightingInfo = g_entityLightingInfos[i];
		if(lightingInfo.vcacheindex == -1)
			continue;

		CVBMCache::vertexcache_t* pvertexcache = gVBMCache.GetVertexCacheByIndex(lightingInfo.vcacheindex);
		if(!pvertexcache)
			continue;

		const CVBMCache::vbmcache_t* pvbmcache = gVBMCache.GetVBMCacheByIndex(lightingInfo.modelindex);
		if(!pvbmcache)
			continue;

		for(int j = 0; j < MAXLIGHTMAPS; j++)
		{
			for(int k = 0; k < NB_BAKED_VERTEXLIGHT_LAYERS; k++)
			{
				lightingInfo.plightdata[j][k] = new vec3_t[pvertexcache->numvertexes];
				memset(lightingInfo.plightdata[j][k], 0, sizeof(vec3_t)*pvertexcache->numvertexes);
			}

			lightingInfo.styles[j] = 255;
		}

		// Always mark as 0
		lightingInfo.styles[0] = 0;

		FinalizeVBMModelLighting(pvertexcache, lightingInfo, lightingInfo.styles);

		int stylecount = 1;
		for(int j = 1; j < MAXLIGHTMAPS; j++)
		{
			if(lightingInfo.styles[j] != 255)
				stylecount++;
		}

		lightingInfo.bufferoffset = vertexbuffersize;
		vertexbuffersize += sizeof(byte) * 3 * pvertexcache->numvertexes * stylecount;

		// Set keyvalues in entity
		entity_t* pentity = &g_entities[lightingInfo.entindex];

		// Set offset into lighting data
		char szString[256];
		sprintf(szString, "%d", lightingInfo.bufferoffset);
		SetKeyValue(pentity, "vlight_offset", szString);

		// Set vertex hash value
		const vbmvertex_t* pvertexdata = pvbmcache->pvbmheader->getVertexes();
		int vertexdatasize = sizeof(vbmvertex_t)*pvbmcache->pvbmheader->numverts;
		CMD5 vertexHash(reinterpret_cast<const byte*>(pvertexdata), vertexdatasize);
		std::string finalHash = vertexHash.HexDigest();
		SetKeyValue(pentity, "vlight_hash", finalHash.c_str());

		// Set vertex count
		sprintf(szString, "%d", pvertexcache->numvertexes);
		SetKeyValue(pentity, "vlight_vertexcount", szString);

		// Set styles
		sprintf(szString, "%d;%d;%d;%d", lightingInfo.styles[0], lightingInfo.styles[1], lightingInfo.styles[2], lightingInfo.styles[3]);
		SetKeyValue(pentity, "vlight_styles", szString);
	}

	if(g_pBakedVertexThreadInfos)
		delete[] g_pBakedVertexThreadInfos;

	if(!vertexbuffersize)
		return;

	// Create vertex lighting buffers
	g_dvertexlightdatasize = vertexbuffersize * sizeof(byte) * 3;

	// Set up ambient
	g_dvertexlightdata_ambient = new byte[g_dvertexlightdatasize];
	g_dvertexlightdata_ambient_compression = 0;
	g_dvertexlightdata_ambient_compression_level = 0;
	g_dvertexlightdata_ambient_checksum = 0;
	g_dvertexlightdatasize_ambient_actual = 0;

	// Set up diffuse
	g_dvertexlightdata_diffuse = new byte[g_dvertexlightdatasize];
	g_dvertexlightdata_diffuse_compression = 0;
	g_dvertexlightdata_diffuse_compression_level = 0;
	g_dvertexlightdata_diffuse_checksum = 0;
	g_dvertexlightdatasize_diffuse_actual = 0;

	// Set up vectors
	g_dvertexlightdata_vectors = new byte[g_dvertexlightdatasize];
	g_dvertexlightdata_vectors_compression = 0;
	g_dvertexlightdata_vectors_compression_level = 0;
	g_dvertexlightdata_vectors_checksum = 0;
	g_dvertexlightdatasize_vectors_actual = 0;

	for (int i = 0; i < g_numEntityLightingInfos; i++)
	{
		entity_lightinginfo_t& lightingInfo = g_entityLightingInfos[i];
		if(lightingInfo.bufferoffset == -1)
			continue;

		CVBMCache::vertexcache_t* pvertexcache = gVBMCache.GetVertexCacheByIndex(lightingInfo.vcacheindex);
		if(!pvertexcache)
			continue;

		// Finalize data
		int styleoffset = 0;
		for(int j = 0; j < MAXLIGHTMAPS; j++)
		{
			if(lightingInfo.styles[j] == 255)
				continue;

			int current_offset = lightingInfo.bufferoffset + (styleoffset * pvertexcache->numvertexes * 3);

			for (int k = 0; k < pvertexcache->numvertexes; k++)
			{
				for (int l = 0; l < 3; l++)
				{
					// Multiply ambient color by lightscale
					float amb = lightingInfo.plightdata[j][VERTEX_LIGHTING_AMBIENT][k][l];
					//amb *= g_colour_lightscale[l];
					if (amb < g_minlight)
						amb = g_minlight;

					// Apply color gamma to ambient layer
					if (g_colour_qgamma[l] != 1.0)
						amb = (float)pow(amb / 256.0f, g_colour_qgamma[l]) * 256.0f;

					int iamb = amb;
					if(iamb < 0)
						iamb = 0;
					else if(iamb > 255)
						iamb = 255;

					// Multibly diffuse light by color lightscale
					float diff = lightingInfo.plightdata[j][VERTEX_LIGHTING_DIFFUSE][k][l];
					diff *= g_colour_lightscale[l];
					if (diff < g_minlight)
						diff = g_minlight;

					// Apply color gamma to diffuse layer
					if (g_colour_qgamma[l] != 1.0)
						diff = (float)pow(diff / 256.0f, g_colour_qgamma[l]) * 256.0f;

					int idiff = diff;
					if(idiff < 0)
						idiff = 0;
					else if(idiff > 255)
						idiff = 255;

					float vec = lightingInfo.plightdata[j][VERTEX_LIGHTING_VECTORS][k][l];
					int ivec = (vec + 1.0f) * 127.5f;
					if(ivec < 0)
						ivec = 0;
					else if(ivec > 255)
						ivec = 255;

					g_dvertexlightdata_ambient[current_offset + (k * 3) + l] = iamb;
					g_dvertexlightdata_diffuse[current_offset + (k * 3) + l] = idiff;
					g_dvertexlightdata_vectors[current_offset + (k * 3) + l] = ivec;
				}
			}

			styleoffset++;
		}
	}
}

// =====================================================================================
//  BuildVertexLights
// =====================================================================================
void GatherVertexLight(const vec3_t pos, const byte* const pvs, const vec3_t normal, byte*& styles, vec3_t*& sample_ambient, vec3_t*& sample_diffuse, vec3_t*& sample_lightvectors, int& numstyles)
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
					AddLight(l, directions, pos, pvs, normal, 1.0, add_styles, step, 0, -1, adds, adds_ambient, adds_diffuse, NULL, NULL, false, true, false);
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
					AddLight(l, directions, pos, pvs, normal, 1.0, add_styles, step, 0, -1, adds, adds_ambient, adds_diffuse, NULL, NULL, true, true, false);
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

		// Do not use normal here, as it's better looking to distribute lighting
		// across all vertices regardless of facing
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
		if (TestSegmentAgainstOpaqueList(pos, p->origin, transparency, opaquestyle, true))
			continue;

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
	for (int style = 0; style < ALLSTYLES; ++style)
	{
		vec_t samplebrightness = GetBrightestSample(adds[style], adds_ambient[style], adds_diffuse[style]);
		if (samplebrightness > g_corings[style] * 0.1)
		{
			int style_index;
			for (style_index = 0; style_index < numstyles; style_index++)
			{
				if (styles[style_index] == style)
					break;
			}

			if (style_index == ALLSTYLES) // shouldn't happen
			{
				if (++stylewarningcount >= stylewarningnext)
				{
					stylewarningnext = stylewarningcount * 2;
					Warning("Too many direct light styles on a face(%f,%f,%f)", pos[0], pos[1], pos[2]);
					Warning(" total %d warnings for too many styles", stylewarningcount);
				}
				return;
			}
			else if(style_index == numstyles)
			{
				// Expand the arrays unless we hit ALLSTYLES, this is for reducing
				// memory footprint while processing large vertex counts
				int newcount = numstyles + 1;

				// Allocate new array for the vectors and copy original data
				vec3_t* pnew = new vec3_t[newcount];
				memcpy(pnew, sample_lightvectors, sizeof(vec3_t)*numstyles);
				memset(pnew[style_index], 0, sizeof(vec3_t));
				delete[] sample_lightvectors;
				sample_lightvectors = pnew;

				// Also for diffuse
				pnew = new vec3_t[newcount];
				memcpy(pnew, sample_diffuse, sizeof(vec3_t)*numstyles);
				memset(pnew[style_index], 0, sizeof(vec3_t));
				delete[] sample_diffuse;
				sample_diffuse = pnew;

				// And ambient
				pnew = new vec3_t[newcount];
				memcpy(pnew, sample_ambient, sizeof(vec3_t)*numstyles);
				memset(pnew[style_index], 0, sizeof(vec3_t));
				delete[] sample_ambient;
				sample_ambient = pnew;

				// Expand unique styles array
				byte* pnewstyles = new byte[newcount];
				memcpy(pnewstyles, styles, sizeof(byte)*numstyles);
				pnewstyles[style_index] = style;
				delete[] styles;
				styles = pnewstyles;

				// Set new style count
				numstyles = newcount;
			}

			// Add the bump maps components as well
			VectorCopy(directions[style], sample_lightvectors[style_index]);
			VectorAdd(sample_diffuse[style_index], adds_diffuse[style], sample_diffuse[style_index]);
			VectorAdd(sample_ambient[style_index], adds_ambient[style], sample_ambient[style_index]);
		}
		else
		{
			if (VectorMaximum(adds[style]) > g_maxdiscardedlight + NORMAL_EPSILON)
			{
				ThreadLock();
				if (VectorMaximum(adds[style]) > g_maxdiscardedlight + NORMAL_EPSILON)
				{
					g_maxdiscardedlight = VectorMaximum(adds[style]);
					VectorCopy(pos, g_maxdiscardedpos);
				}
				ThreadUnlock();
			}
		}
	}
}