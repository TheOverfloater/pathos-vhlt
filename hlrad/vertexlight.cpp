#include <stdint.h>
#include <vector>
#include <string>
#include <stdlib.h>
#include <string.h>

#include "qrad.h"
#include "vertexlight.h"
#include "studio.h"
#include "meshtrace.h"
#include "aldformat.h"
#include "vldformat.h"
#include "miniz.h"

// hacky fix, without vector.h vbmformat.h wont work and vector needs nanmask and including common would require more includes
#ifndef NANMASK
#define NANMASK 0x7F800000
#endif

#include "../../common/vector.h"
#include "../../shared/vbmformat.h"

extern model_t models[MAX_MODELS];
extern int num_models;

extern void GatherVertexLight(const vec3_t pos, const byte* const pvs, const vec3_t normal, vec3_t* sample, vec3_t* sample_ambient, vec3_t* sample_diffuse, vec3_t* sample_lightvectors);

struct model_vertex_lights_t
{
	int model_idx;
	int numverts;
	vec3_t* lightdata[NB_SURF_LIGHTMAP_LAYERS];

	model_vertex_lights_t(int idx, int verts) : model_idx(idx), numverts(verts)
	{
		for (int i = 0; i < NB_SURF_LIGHTMAP_LAYERS; i++)
		{
			lightdata[i] = new vec3_t[numverts];
			memset(lightdata[i], 0, sizeof(vec3_t) * numverts);
		}
	}

	~model_vertex_lights_t()
	{
		for (int i = 0; i < NB_SURF_LIGHTMAP_LAYERS; i++)
		{
			delete[] lightdata[i];
		}
	}
};

static std::vector<model_vertex_lights_t*> g_model_vertex_lights;

void BuildVertexLights()
{
	Log("Building Vertex Lighting for studio models...\n");

	int total_vertices = 0;

	for (int i = 0; i < num_models; i++)
	{
		model_t* m = &models[i];

		if (!m->bVertexLight)
		{
			continue;
		}

		char vbmPath[_MAX_PATH];
		strcpy(vbmPath, m->name);
		char* ext = strrchr(vbmPath, '.');
		if (ext) 
			strcpy(ext, ".vbm");

		FILE* fvbm = fopen(vbmPath, "rb");
		if (!fvbm)
		{
			Log("Error: Could not load VBM for vertex lighting: %s\n", vbmPath);
			continue;
		}

		fseek(fvbm, 0, SEEK_END);
		long vbmSize = ftell(fvbm);
		fseek(fvbm, 0, SEEK_SET);

		byte* pVbmData = (byte*)malloc(vbmSize);
		if (!pVbmData)
		{
			fclose(fvbm);
			continue;
		}
		fread(pVbmData, vbmSize, 1, fvbm);
		fclose(fvbm);

		vbmheader_t* vbmHeader = (vbmheader_t*)pVbmData;
		if (vbmHeader->id != VBM_HEADER)
		{
			Log("Error: %s is not a valid VBM file.\n", vbmPath);
			free(pVbmData);
			continue;
		}

		int num_model_verts = vbmHeader->numverts;
		if (num_model_verts == 0)
		{
			free(pVbmData);
			continue;
		}

		model_vertex_lights_t* model_lights = new model_vertex_lights_t(i, num_model_verts);
		g_model_vertex_lights.push_back(model_lights);
		total_vertices += num_model_verts;

		matrix3x4 bone_transforms;
		m->mesh.AngleMatrix(m->angles, m->origin, m->scale, bone_transforms);

		const vbmvertex_t* pVbmVerts = (vbmvertex_t*)(pVbmData + vbmHeader->vertexoffset);

		for (int l = 0; l < num_model_verts; l++)
		{
			vec3_t world_pos, world_norm;

			m->mesh.VectorTransform(pVbmVerts[l].origin, bone_transforms, world_pos);

			vec3_t temp_norm;
			m->mesh.VectorTransform(pVbmVerts[l].normal, bone_transforms, temp_norm);
			VectorSubtract(temp_norm, m->origin, world_norm);
			VectorNormalize(world_norm);

			dleaf_t* leaf = PointInLeaf(world_pos);
			byte pvs[(MAX_MAP_LEAFS + 7) / 8];
			if (leaf->visofs != -1)
				DecompressVis(&g_dvisdata[leaf->visofs], pvs, sizeof(pvs));
			else
				memset(pvs, 0xFF, sizeof(pvs));

			vec3_t samples[ALLSTYLES], samples_amb[ALLSTYLES], samples_diff[ALLSTYLES], samples_vecs[ALLSTYLES];
			memset(samples, 0, sizeof(samples));
			if (g_bumpmaps)
			{
				memset(samples_amb, 0, sizeof(samples_amb));
				memset(samples_diff, 0, sizeof(samples_diff));
				memset(samples_vecs, 0, sizeof(samples_vecs));
			}

			GatherVertexLight(world_pos, pvs, world_norm, samples, samples_amb, samples_diff, samples_vecs);

			VectorCopy(samples[0], model_lights->lightdata[LIGHTMAP_LAYER_DEFAULT][l]);

			if (g_bumpmaps)
			{
				VectorCopy(samples_amb[0], model_lights->lightdata[LIGHTMAP_LAYER_AMBIENT][l]);
				VectorCopy(samples_diff[0], model_lights->lightdata[LIGHTMAP_LAYER_DIFFUSE][l]);
				VectorCopy(samples_vecs[0], model_lights->lightdata[LIGHTMAP_LAYER_VECTORS][l]);
			}
		}

		free(pVbmData);
	}

	if (total_vertices == 0)
	{
		return;
	}

	g_dvertexlightdatasize = total_vertices * 3;
	hlassume(g_dvertexlightdatasize <= g_max_map_lightdata, assume_MAX_MAP_LIGHTING);

	g_dvertexlightdata = (byte*)malloc(g_dvertexlightdatasize);
	if (g_bumpmaps)
	{
		g_dvertexlightdata_ambient = (byte*)malloc(g_dvertexlightdatasize);
		g_dvertexlightdata_diffuse = (byte*)malloc(g_dvertexlightdatasize);
		g_dvertexlightdata_vectors = (byte*)malloc(g_dvertexlightdatasize);
	}

	int current_offset = 0;
	for (int i = 0; i < num_models; i++)
	{
		model_t* m = &models[i];
		model_vertex_lights_t* found_model = nullptr;
		for (auto& ml : g_model_vertex_lights)
		{
			if (ml->model_idx == i)
			{
				found_model = ml;
				break;
			}
		}

		if (found_model)
		{
			int data_size = found_model->numverts * 3;

			for (int v = 0; v < found_model->numverts; v++)
			{
				for (int c = 0; c < 3; c++)
				{
					float val = found_model->lightdata[SURF_LIGHTMAP_DEFAULT][v][c];
					g_dvertexlightdata[current_offset + (v * 3) + c] = (byte)qmax(0, qmin(255, (int)(val * 255.0f)));
				}
			}

			if (g_bumpmaps)
			{
				for (int v = 0; v < found_model->numverts; v++)
				{
					for (int c = 0; c < 3; c++)
					{
						float amb = found_model->lightdata[SURF_LIGHTMAP_AMBIENT][v][c];
						float diff = found_model->lightdata[SURF_LIGHTMAP_DIFFUSE][v][c];

						g_dvertexlightdata_ambient[current_offset + (v * 3) + c] = (byte)qmax(0, qmin(255, (int)(amb * 255.0f)));
						g_dvertexlightdata_diffuse[current_offset + (v * 3) + c] = (byte)qmax(0, qmin(255, (int)(diff * 255.0f)));

						float vec = found_model->lightdata[SURF_LIGHTMAP_VECTORS][v][c];
						g_dvertexlightdata_vectors[current_offset + (v * 3) + c] = (byte)qmax(0, qmin(255, (int)((vec + 1.0f) * 127.5f)));
					}
				}
			}

			char szOffset[32];
			sprintf(szOffset, "%d", current_offset);
			SetKeyValue(m->pEntity, "vlight_offset", szOffset);

			current_offset += data_size;
		}
	}

	for (auto& ml : g_model_vertex_lights)
	{
		delete ml;
	}
	g_model_vertex_lights.clear();
}

static void FinalizeVertexLightData(int lightdatasize, byte*& plightdataptr, int& lightdatasize_actual, int& compression_level, int& compression_type, int* plightdatasize_out)
{
	if (g_nocompress || lightdatasize == 0)
	{
		lightdatasize_actual = lightdatasize;
		compression_type = PBSPV2_LMAP_COMPRESSION_NONE;
		compression_level = 0;
		if (plightdatasize_out) 
			*plightdatasize_out = lightdatasize;
		return;
	}

	// Compress using miniz
	int destsize = compressBound(lightdatasize);
	byte* pdestination = (byte*)malloc(destsize);

	int comp_level;
	switch (g_compressionlevel)
	{
	case COMPRESSION_LEVEL_BEST_SPEED: 
		comp_level = MZ_BEST_SPEED; 
		break;
	case COMPRESSION_LEVEL_BEST_COMPRESSION: 
		comp_level = MZ_BEST_COMPRESSION; 
		break;
	case COMPRESSION_LEVEL_UBER_COMPRESSION: 
		comp_level = MZ_UBER_COMPRESSION; 
		break;
	default: 
		comp_level = MZ_DEFAULT_LEVEL; 
		break;
	}

	mz_ulong resultsize = destsize;
	int result = compress2(pdestination, &resultsize, (const byte*)plightdataptr, lightdatasize, comp_level);
	if (result != MZ_OK)
	{
		Error("%s - Failed to compress vertex lightmap data, compress returned %d.\n", __FUNCTION__, result);
		return;
	}

	free(plightdataptr);
	plightdataptr = (byte*)malloc(resultsize);
	memcpy(plightdataptr, pdestination, resultsize);
	free(pdestination);

	lightdatasize_actual = resultsize;
	compression_level = comp_level;
	compression_type = PBSPV2_LMAP_COMPRESSION_MINIZ;
	if (plightdatasize_out) 
		*plightdatasize_out = lightdatasize;
}

void FinalizeVertexLightBuffers()
{
	if (g_dvertexlightdatasize == 0) 
		return;

	Log("Compressing vertex lightmap data\n");

	int lightsize_orig = g_dvertexlightdatasize;

	FinalizeVertexLightData(lightsize_orig, g_dvertexlightdata, g_dvertexlightdatasize_actual, g_dvertexlightdata_compression_level, g_dvertexlightdata_compression, &g_dvertexlightdatasize);
	Log("  default: %d -> %d bytes\n", lightsize_orig, g_dvertexlightdatasize_actual);

	if (g_bumpmaps)
	{
		FinalizeVertexLightData(lightsize_orig, g_dvertexlightdata_ambient, g_dvertexlightdatasize_ambient_actual, g_dvertexlightdata_ambient_compression_level, g_dvertexlightdata_ambient_compression, nullptr);
		Log("  ambient: %d -> %d bytes\n", lightsize_orig, g_dvertexlightdatasize_ambient_actual);

		FinalizeVertexLightData(lightsize_orig, g_dvertexlightdata_diffuse, g_dvertexlightdatasize_diffuse_actual, g_dvertexlightdata_diffuse_compression_level, g_dvertexlightdata_diffuse_compression, nullptr);
		Log("  diffuse: %d -> %d bytes\n", lightsize_orig, g_dvertexlightdatasize_diffuse_actual);

		FinalizeVertexLightData(lightsize_orig, g_dvertexlightdata_vectors, g_dvertexlightdatasize_vectors_actual, g_dvertexlightdata_vectors_compression_level, g_dvertexlightdata_vectors_compression, nullptr);
		Log("  vectors: %d -> %d bytes\n", lightsize_orig, g_dvertexlightdatasize_vectors_actual);
	}
}

bool ExportVLDData(vld_datatype_t type)
{
	vldheader_t* poriginal = nullptr;

	// See if a file already exists
	char szpath[_MAX_PATH];
	strcpy(szpath, g_Mapname);
	strcat(szpath, ".vld");

	FILE* pfin = fopen(szpath, "rb");
	if (pfin)
	{
		fseek(pfin, 0, SEEK_END);
		int filesize = ftell(pfin);
		fseek(pfin, 0, SEEK_SET);

		byte* pbuffer = new byte[filesize];
		fread(pbuffer, filesize, 1, pfin);
		fclose(pfin);

		poriginal = (vldheader_t*)pbuffer;
	}

	if (poriginal && poriginal->vertexdatasize != g_dvertexlightdatasize)
	{
		Error("Error: Destination VLD file '%s' has an inconsistent vertex data size(%d bytes) compared to current output(%d bytes).\nOriginal file was deleted.\n", szpath, poriginal->vertexdatasize, g_dvertexlightdatasize);
		delete[](byte*)poriginal;
		poriginal = nullptr;
	}

	int compressionlevel;
	switch (g_compressionlevel)
	{
	case COMPRESSION_LEVEL_BEST_SPEED: 
		compressionlevel = MZ_BEST_SPEED; 
		break;
	case COMPRESSION_LEVEL_BEST_COMPRESSION: 
		compressionlevel = MZ_BEST_COMPRESSION; 
		break;
	case COMPRESSION_LEVEL_UBER_COMPRESSION: 
		compressionlevel = MZ_UBER_COMPRESSION; 
		break;
	default:
	case COMPRESSION_LEVEL_DEFAULT: 
		compressionlevel = MZ_DEFAULT_LEVEL; 
		break;
	}

	vldlumptype_t lumptype;
	switch (type)
	{
	case VLD_DATA_DAYLIGHT_RETURN:
		lumptype = g_bumpmaps ? VLD_LUMP_DAYLIGHT_RETURN_DATA_BUMP : VLD_LUMP_DAYLIGHT_RETURN_DATA_NOBUMP;
		break;
	case VLD_DATA_NIGHTSTAGE:
	default:
		lumptype = g_bumpmaps ? VLD_LUMP_NIGHTDATA_BUMP : VLD_LUMP_NIGHTDATA_NOBUMP;
		break;
	}

	int newnumlumps = 0;
	int totalsize = sizeof(vldheader_t);

	// Determine size for existing lumps we are keeping
	if (poriginal)
	{
		for (int i = 0; i < poriginal->numlumps; i++)
		{
			vldlump_t* plump = (vldlump_t*)((byte*)poriginal + poriginal->lumpoffset) + i;
			if (plump->type != lumptype)
			{
				totalsize += sizeof(vldlump_t);
				for (int j = 0; j < NB_SURF_LIGHTMAP_LAYERS; j++)
				{
					if (plump->layeroffsets[j] != 0)
					{
						vldlayer_t* player = (vldlayer_t*)((byte*)poriginal + plump->layeroffsets[j]);
						totalsize += sizeof(vldlayer_t) + player->datasize;
					}
				}
				newnumlumps++;
			}
		}
	}

	// Prepare data sources for the new lump
	byte* pdatasources[NB_SURF_LIGHTMAP_LAYERS];
	int datasizes_compressed[NB_SURF_LIGHTMAP_LAYERS];
	memset(pdatasources, 0, sizeof(pdatasources));
	memset(datasizes_compressed, 0, sizeof(datasizes_compressed));

	pdatasources[SURF_LIGHTMAP_DEFAULT] = g_dvertexlightdata;
	datasizes_compressed[SURF_LIGHTMAP_DEFAULT] = g_dvertexlightdatasize_actual;
	totalsize += sizeof(vldlump_t) + sizeof(vldlayer_t) + sizeof(byte) * g_dvertexlightdatasize_actual;

	int layernumber;
	if (g_bumpmaps)
	{
		pdatasources[SURF_LIGHTMAP_AMBIENT] = g_dvertexlightdata_ambient;
		datasizes_compressed[SURF_LIGHTMAP_AMBIENT] = g_dvertexlightdatasize_ambient_actual;
		totalsize += (sizeof(vldlayer_t) + sizeof(byte) * g_dvertexlightdatasize_ambient_actual);

		pdatasources[SURF_LIGHTMAP_DIFFUSE] = g_dvertexlightdata_diffuse;
		datasizes_compressed[SURF_LIGHTMAP_DIFFUSE] = g_dvertexlightdatasize_diffuse_actual;
		totalsize += (sizeof(vldlayer_t) + sizeof(byte) * g_dvertexlightdatasize_diffuse_actual);

		pdatasources[SURF_LIGHTMAP_VECTORS] = g_dvertexlightdata_vectors;
		datasizes_compressed[SURF_LIGHTMAP_VECTORS] = g_dvertexlightdatasize_vectors_actual;
		totalsize += (sizeof(vldlayer_t) + sizeof(byte) * g_dvertexlightdatasize_vectors_actual);

		layernumber = NB_SURF_LIGHTMAP_LAYERS;
	}
	else
	{
		layernumber = 1;
	}

	newnumlumps++;

	// Start building the file buffer
	int fileoffset = 0;
	byte* pfilebuffer = new byte[totalsize];
	memset(pfilebuffer, 0, totalsize);

	vldheader_t* pnewhdr = (vldheader_t*)pfilebuffer;
	fileoffset += sizeof(vldheader_t);

	pnewhdr->header = VLD_HEADER_ENCODED;
	pnewhdr->version = VLD_HEADER_VERSION;
	pnewhdr->numlumps = newnumlumps;
	pnewhdr->lumpoffset = fileoffset;
	pnewhdr->vertexdatasize = g_dvertexlightdatasize;

	vldlump_t* pnewlump_entry = nullptr;
	if (poriginal)
	{
		vldlump_t* pnewlumps_array = (vldlump_t*)(pfilebuffer + fileoffset);
		fileoffset += sizeof(vldlump_t) * newnumlumps;

		int j = 0;
		for (int i = 0; i < poriginal->numlumps; i++)
		{
			vldlump_t* psrclump = (vldlump_t*)((byte*)poriginal + poriginal->lumpoffset) + i;
			if (psrclump->type == lumptype) 
				continue;

			pnewlumps_array[j].type = psrclump->type;

			for (int k = 0; k < NB_SURF_LIGHTMAP_LAYERS; k++)
			{
				if (!psrclump->layeroffsets[k]) 
					continue;

				vldlayer_t* psrclayer = (vldlayer_t*)((byte*)poriginal + psrclump->layeroffsets[k]);

				pnewlumps_array[j].layeroffsets[k] = fileoffset;
				vldlayer_t* poutlayer = (vldlayer_t*)((byte*)pfilebuffer + fileoffset);
				fileoffset += sizeof(vldlayer_t);

				*poutlayer = *psrclayer;
				poutlayer->dataoffset = fileoffset;

				memcpy(pfilebuffer + fileoffset, (byte*)poriginal + psrclayer->dataoffset, psrclayer->datasize);
				fileoffset += psrclayer->datasize;
			}
			j++;
		}
		pnewlump_entry = &pnewlumps_array[j];
	}
	else
	{
		pnewlump_entry = (vldlump_t*)(pfilebuffer + fileoffset);
		fileoffset += sizeof(vldlump_t) * newnumlumps;
	}

	// Append the new lump data
	pnewlump_entry->type = lumptype;
	for (int i = 0; i < layernumber; i++)
	{
		pnewlump_entry->layeroffsets[i] = fileoffset;
		vldlayer_t* poutlayer = (vldlayer_t*)((byte*)pfilebuffer + fileoffset);
		fileoffset += sizeof(vldlayer_t);

		poutlayer->compression = g_nocompress ? 0 : 1;
		poutlayer->compressionlevel = compressionlevel;
		poutlayer->datasize = datasizes_compressed[i];
		poutlayer->dataoffset = fileoffset;

		memcpy(pfilebuffer + fileoffset, pdatasources[i], datasizes_compressed[i]);
		fileoffset += datasizes_compressed[i];
	}

	FILE* pf = fopen(szpath, "wb");
	if (!pf)
	{
		Log("Failed to open '%s' for writing.\n", szpath);
		delete[] pfilebuffer;
		return false;
	}

	fwrite(pfilebuffer, totalsize, 1, pf);
	fclose(pf);

	delete[] pfilebuffer;
	if (poriginal) 
		delete[](byte*)poriginal;

	Log("%s written(%d bytes).\n", szpath, totalsize);
	return true;
}