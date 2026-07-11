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
#include "vbmcache.h"
#include "tga.h"
#include "dds.h"
#include "studio_util.h"

void DumpSMD_VertexCache( const vbmheader_t* pvbmheader, CVBMCache::vertexcache_t* pvertexcache )
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
	
	const unsigned int* pindexes = pvbmheader->getIndexes();
	for(int i = 0; i < pvbmheader->numbodyparts; i++)
	{
		const vbmbodypart_t* pbodypart = pvbmheader->getBodyPart(i);
		for(int j = 0; j < pbodypart->numsubmodels; j++)
		{
			const vbmsubmodel_t* psubmodel = pbodypart->getSubmodel(pvbmheader, j);
			for(int k = 0; k < psubmodel->nummeshes; k++)
			{
				const vbmmesh_t* pmesh = psubmodel->getMesh(pvbmheader, k);
				const vbmtexture_t* ptexture = pvbmheader->getTexture(pmesh->skinref);

				for(int l = 0; l < pmesh->num_indexes; l += 3)
				{
					fprintf(pf, "%s.tga\n", ptexture->name);
					for(int m = 0; m < 3; m++)
					{
						int vertexindex = pindexes[pmesh->start_index + l + m];
						const CVBMCache::cachevertex_t& vertex = pvertexcache->pvertexes[vertexindex];

						fprintf(pf, "  0   %.4f  %.4f  %.4f  %.4f  %.4f  %.4f  %.4f  %.4f\n",
						vertex.origin[0], vertex.origin[1], vertex.origin[2],
						vertex.normal[0], vertex.normal[1], vertex.normal[2],
						0, 0);
					}
				}
			}
		}
	}

	fprintf(pf, "end\n");
	fclose(pf);
}

// Object definition
CVBMCache gVBMCache;

//===========================
// Constructor
// 
//===========================
CVBMCache::CVBMCache( void )
{
}

//===========================
// Destructor
// 
//===========================
CVBMCache::~CVBMCache( void )
{
	// Clean up anything we still got in memory
	Cleanup();
}
//===========================
// Loads a model for an entity
// 
//===========================
const CVBMCache::vbmcache_t* CVBMCache::LoadModel( const char* pstrModelFile )
{
	if(!m_pVBMCacheVector.empty())
	{
		for(int i = 0; i < m_pVBMCacheVector.size(); i++)
		{
			vbmcache_t* pcache = m_pVBMCacheVector[i];
			if(!pcache->name.compare(pstrModelFile))
				return pcache;
		}
	}

	std::string strfilepath(pstrModelFile);
	std::string::size_type offset = strfilepath.find('.', 0);
	if(offset != std::string::npos)
		strfilepath.erase(offset, strfilepath.length() - offset);

	// Load the MDL file
	std::string modelpath;
	modelpath = m_modDirPath + "/" + strfilepath + ".mdl";

	FILE* pf = fopen(modelpath.c_str(), "rb");
	if(!pf)
	{
		Warning("Failed to open '%s' for reading, check the path you set for '-moddir'.", modelpath.c_str());
		return nullptr;
	}

	fseek(pf, 0, SEEK_END);
	int size = ftell(pf);
	fseek(pf, 0, SEEK_SET);

	byte* pstudiobuffer = new byte[size];
	fread(pstudiobuffer, sizeof(byte)*size, 1, pf);
	fclose(pf);

	studiohdr_t* pstudiohdr = (studiohdr_t*)pstudiobuffer;
	if(pstudiohdr->id != IDSTUDIOHEADER)
	{
		Warning("Wrong studiomodel ID '%d' encountered on MDL file '%s', '%d' expected.", pstudiohdr->id, modelpath.c_str(), IDSTUDIOHEADER);
		delete[] pstudiobuffer;
		return nullptr;
	}

	if(pstudiohdr->version != STUDIO_VERSION)
	{
		Warning("Wrong studiomodel version '%d' encountered on MDL file '%s', '%d' expected.", pstudiohdr->version, modelpath.c_str(), STUDIO_VERSION);
		delete[] pstudiobuffer;
		return nullptr;
	}

	// Now load the VBM file
	modelpath = m_modDirPath + "/" + strfilepath + ".vbm";

	pf = fopen(modelpath.c_str(), "rb");
	if(!pf)
	{
		Warning("Failed to open '%s' for reading, check the path you set for '-moddir'.", modelpath.c_str());
		delete[] pstudiobuffer;
		return nullptr;
	}

	fseek(pf, 0, SEEK_END);
	size = ftell(pf);
	fseek(pf, 0, SEEK_SET);

	byte* pvbmbuffer = new byte[size];
	fread(pvbmbuffer, sizeof(byte)*size, 1, pf);
	fclose(pf);

	vbmheader_t* pvbmhdr = (vbmheader_t*)pvbmbuffer;
	if(pvbmhdr->id != VBM_HEADER)
	{
		Warning("Wrong VBM header ID '%d' encountered on VBM file '%s', '%d' expected.", pvbmhdr->id, modelpath.c_str(), VBM_HEADER);
		delete[] pstudiobuffer;
		delete[] pvbmbuffer;
		return nullptr;
	}

	vbmcache_t* pcache = new vbmcache_t();
	pcache->name.assign(pstrModelFile);
	pcache->pstudiohdr = pstudiohdr;
	pcache->pvbmheader = pvbmhdr;
	pcache->cacheindex = m_pVBMCacheVector.size();
	m_pVBMCacheVector.push_back(pcache);

	// Load textures for this model
	LoadModelTextures(pcache);

	return pcache;
}

//===========================
// Load a texture for a model
// 
//===========================
model_texture_t* CVBMCache::LoadTexture( vbmtexture_t* ptexture, const Char* pstrBaseDirectory, const Char* pstrTexturePath )
{
	if(!m_pTexturesVector.empty())
	{
		for(int i = 0; i < m_pTexturesVector.size(); i++)
		{
			model_texture_t* ptexture = m_pTexturesVector[i];
			if(!ptexture->name.compare(pstrTexturePath))
				return ptexture;
		}	
	}

	// Try to load the actual texture file itself
	char fullPath[MAX_PATH];
	sprintf(fullPath, "%s/textures/%s", pstrBaseDirectory, pstrTexturePath);

	FILE* pf = fopen(fullPath, "rb");
	if(!pf)
	{
		if(!stricmp(&fullPath[strlen(fullPath)-4], ".dds"))
		{
			strcpy(&fullPath[strlen(fullPath)-4], ".tga");
			pf = fopen(fullPath, "rb");
		}

		if(!pf)
		{
			Warning("Failed to open '%s' for reading.", pstrTexturePath);
			return nullptr;
		}
	}

	fseek(pf, 0, SEEK_END);
	int fileSize = ftell(pf);
	fseek(pf, 0, SEEK_SET);

	byte* pdata = new byte[fileSize];
	fread(pdata, fileSize, 1, pf);
	fclose(pf);

	byte* poutdata = nullptr;
	int datasize = 0;

	dds_compression_t ddscompression;
	int width = 0;
	int height = 0;
	int bpp = 0;

	texture_format_t textureformat = GetTextureFileFormat(fullPath);
	if(textureformat == TX_FORMAT_UNDEFINED)
	{
		Warning("'%s' is not a known file format.", fullPath);
		delete[] pdata;
		return nullptr;
	}
	else if(textureformat == TX_FORMAT_TGA)
	{
		if(!TGA_Load(fullPath, pdata, poutdata, width, height, bpp, datasize))
		{
			Warning("Failed to load TGA image file '%s'.", fullPath);

			delete[] pdata;
			return nullptr;
		}
	}
	else if(textureformat == TX_FORMAT_DDS)
	{
		if(!DDS_Load(fullPath, pdata, poutdata, width, height, bpp, datasize, ddscompression))
		{
			Warning("Failed to load DDS image file '%s'.", fullPath);
			delete[] pdata;
			return nullptr;
		}
	}

	model_texture_t* pnew = new model_texture_t();
	pnew->bpp = bpp;
	pnew->width = width;
	pnew->height = height;
	pnew->index = m_pTexturesVector.size();
	pnew->name.assign(pstrTexturePath);
	pnew->pdata = poutdata;

	m_pTexturesVector.push_back(pnew);
	ptexture->index = pnew->index;
	return pnew;
}

//===========================
// Load textures for a model
// 
//===========================
void CVBMCache::LoadModelTextures( vbmcache_t* pvbmcache )
{
	char filePath[MAX_PATH];

	for(int i = 0; i < pvbmcache->pvbmheader->numtextures; i++)
	{
		vbmtexture_t* pvbmtexture = pvbmcache->pvbmheader->getTexture(i);

		// Get the model basename
		char modelBaseName[MAX_PARSE_LENGTH];
		COM_Basename(pvbmcache->name.c_str(), modelBaseName);

		// Get the texture basename
		char textureBaseName[MAX_PARSE_LENGTH];
		COM_Basename(pvbmtexture->name, textureBaseName);

		sprintf(filePath, "textures/models/%s/%s.pmf", modelBaseName, textureBaseName);
		pmffile_t* ppmffile = LoadPMFFile(m_modDirPath.c_str(), filePath);

		// Only bother with alphatested textures
		if(!ppmffile)
		{
			pvbmtexture->index = -1;
			continue;
		}

		model_texture_t* ptexture = LoadTexture(pvbmtexture, m_modDirPath.c_str(), ppmffile->texturename.c_str());
		if(ptexture)
			ppmffile->ptexture = ptexture;

		pvbmtexture->index = ppmffile->index;
	}
}

//===========================
// Load textures for a model
// 
//===========================
pmffile_t* CVBMCache::LoadPMFFile(  const Char* pstrBaseDirectory, const Char* pstrPMFPath, bool isloadingfromalias )
{
	// Look in array first
	pmffile_t* pPMFFile = nullptr;
	for(int i = 0; i < m_pPMFFilesVector.size(); i++)
	{
		if(!m_pPMFFilesVector[i]->filepath.compare(pstrPMFPath))
			return m_pPMFFilesVector[i];
	}

	// Look in alias mappings too
	if(!pPMFFile)
	{
		for(int i = 0; i < m_pAliasMappingsVector.size(); i++)
		{
			if(!m_pAliasMappingsVector[i]->filepath.compare(pstrPMFPath))
				return m_pAliasMappingsVector[i]->ppmffile;
		}
	}

	// Get path to the VBM file
	char fullPath[MAX_PATH];
	sprintf(fullPath, "%s/%s", pstrBaseDirectory, pstrPMFPath);

	FILE* pf = fopen(fullPath, "rb");
	if(!pf)
	{
		Warning("Failed to open '%s' for reading.", fullPath);
		return nullptr;
	}

	fseek(pf, 0, SEEK_END);
	int size = ftell(pf);
	fseek(pf, 0, SEEK_SET);

	byte* pdata = new byte[size + 1];
	fread(pdata, sizeof(byte)*size, 1, pf);
	pdata[size] = '\0';
	fclose(pf);

	const char* pfile = reinterpret_cast<const char*>(pdata);

	// True if this is an alias script
	static char aliasscriptpath[MAX_PARSE_LENGTH];
	bool isaliasscript = false;

	// Make sure the syntax is valid
	static char token[MAX_PARSE_LENGTH];
	static char value[MAX_PARSE_LENGTH];

	const char* pchar = COM_Parse(pfile, token);
	if(!pchar)
	{
		Warning("Unexpected EOF in '%s'.", fullPath);
		delete[] pdata;
		return nullptr;
	}

	// Make sure the name is present
	if(!strcmp(token, "$alias"))
	{
		// Alias script
		isaliasscript = true;
	}
	else if(strcmp(token, "$texture"))
	{
		Warning("Expected $texture or $alias token, got '%s' instead in '%s'.", token, fullPath);
		delete[] pdata;
		return nullptr;
	}

	// Prevent infinite recursion
	if(isloadingfromalias && isaliasscript)
	{
		Warning("Could not load '%s', because it is an alias script linked to by another alias script.", fullPath);
		delete[] pdata;
		return nullptr;
	}

	// Seek out the opening bracket
	pchar = COM_Parse(pchar, token);
	if(!pchar)
	{
		Warning("Unexpected EOF in '%s'.", fullPath);
		delete[] pdata;
		return nullptr;
	}

	// Make sure the bracket is present
	if(strcmp(token, "{"))
	{
		Warning("Expected { token, got '%s' instead in '%s'.", token, fullPath);
		delete[] pdata;
		return nullptr;
	}

	// Only allocate if not an alias script
	pmffile_t* ppmffile = nullptr;
	if(!isaliasscript)
	{
		// Allocate a new material object
		ppmffile = new pmffile_t();

		// Set basic info
		ppmffile->filepath = fullPath;
		ppmffile->index = m_pPMFFilesVector.size();
	}

	// Holds text for a line
	static char line[MAX_LINE_LENGTH];

	// Parse the file line by line
	const char* pstr = pchar;
	while(true)
	{
		if(!pstr)
		{
			Warning("Unexpected EOF in '%s', missing } token at end.", fullPath);
			if(ppmffile) 
				delete ppmffile;
			delete[] pdata;
			break;
		}

		// Read the line in
		pstr = COM_ReadLine(pstr, line);
		if(!strlen(line))
			continue;

		// Skip comments
		if(!strncmp(line, "//", 2))
			continue;

		// Parse fields
		pchar = COM_Parse(line, token);
		if(!strcmp(token, "}"))
			break;

		if(isaliasscript)
		{
			// Make sure the token is valid
			if(strcmp(token, "$scriptfile"))
			{
				Warning("Expected $scriptfile token, got '%s' instead in '%s'.", token, fullPath);
				if(ppmffile) 
					delete ppmffile;
				delete[] pdata;
				return nullptr;
			}

			if(!pstr)
			{
				Warning("Parameter specification for '%s' is incomplete in '%s'.", token, fullPath);
				continue;
			}

			// Read in the value token
			pchar = COM_Parse(pchar, aliasscriptpath);
		}
		else
		{
			// Identify the field
			if(!strcmp(token, "$alphatest"))
				ppmffile->alphatest = true;
			else if(!strcmp(token, "$noradshadows"))
				ppmffile->noshadow = true;
			else if(!strcmp(token, "$additive")
				|| !strcmp(token, "$alphablend"))
				ppmffile->transparent;
			else if( !strcmp(token, "$chrome")
				|| !strcmp(token, "$cubemaps")
				|| !strcmp(token, "$fullbright")
				|| !strcmp(token, "$nodecal")
				|| !strcmp(token, "$nomipmaps")
				|| !strcmp(token, "$clamp")
				|| !strcmp(token, "$eyeglint")
				|| !strcmp(token, "$scope")
				|| !strcmp(token, "$noimpactfx")
				|| !strcmp(token, "$nostepsound")
				|| !strcmp(token, "$nofacecull")
				|| !strcmp(token, "$dt_scalex")
				|| !strcmp(token, "$dt_scaley")
				|| !strcmp(token, "$int_width")
				|| !strcmp(token, "$int_height")
				|| !strcmp(token, "$cubemapstrength")
				|| !strcmp(token, "$container")
				|| !strcmp(token, "$spec")
				|| !strcmp(token, "$scopescale")
				|| !strcmp(token, "$phong_exp"))
			{
				// Do nothing, we don't use these
				continue;
			}
			else if(!strcmp(token, "$alpha"))
			{
				// Not needed
				continue;
			}
			else if(!strcmp(token, "$texture"))
			{
				if(!pchar)
				{
					Warning("$texture command is incomplete in '%s'.", fullPath);
					continue;
				}

				// Read in the type token
				pchar = COM_Parse(pchar, token);
				if(!pchar)
				{
					Warning("$texture command is incomplete in '%s'.", fullPath);
					continue;
				}

				// Diffuse texture only
				if(strcmp(token, "diffuse"))
					continue;

				// Parse in the file path token
				pchar = COM_Parse(pchar, token);
				ppmffile->texturename = token;
			}
			else if(!strcmp(token, "$material"))
			{
				// Not needed
				continue;
			}
			else if(strlen(token) > 0)
			{
				Warning("Unknown field '%s' in '%s'.", token, fullPath);
			}
		}
	}

	// Release data we held
	delete[] pdata;

	if(isaliasscript)
	{
		char aliasBasePath[MAX_PATH];
		strcpy(aliasBasePath, pstrBaseDirectory);
		strcat(aliasBasePath, "/textures");

		// Load alias script, but prevent infinite recursion
		ppmffile = LoadPMFFile(aliasBasePath, aliasscriptpath, true);
		if(!ppmffile)
			return nullptr;

		// Add to mappings
		alias_mapping_t* pmapping = new alias_mapping_t;
		pmapping->filepath.assign(pstrPMFPath);
		pmapping->ppmffile = ppmffile;

		m_pAliasMappingsVector.push_back(pmapping);
		return ppmffile;
	}
	else
	{
		// Check for errors
		if(ppmffile->texturename.empty())
		{
			Warning("No diffuse texture specified in '%s'.", fullPath);
			delete ppmffile;
			return nullptr;
		}

		// Add to the list
		m_pPMFFilesVector.push_back(ppmffile);
		return ppmffile;
	}
}

//=============================================
// @brief Returns the format for a filename extension
//
// @return Format identifier
//=============================================
CVBMCache::texture_format_t CVBMCache::GetTextureFileFormat( const Char* pstrFilename )
{
	if(!stricmp(pstrFilename + strlen(pstrFilename) - 3, "tga"))
		return TX_FORMAT_TGA;
	else if(!stricmp(pstrFilename + strlen(pstrFilename) - 3, "dds"))
		return TX_FORMAT_DDS;
	else
		return TX_FORMAT_UNDEFINED;
}

//===========================
// Process vbm mesh into vertex cache
// 
//===========================
void CVBMCache::ProcessVBMMesh( const vbmheader_t* pvbmheader, std::vector<bonematrix_t>& weightbonetransform, const vbmmesh_t* pmesh, const vbmmesh_t* pbonemesh, const vbmvertex_t* pvertexes, const unsigned int* pindexes, vertexcache_t* pcache )
{
	const byte* pboneindexes = pbonemesh->getBones(pvbmheader);

	for(int k = 0; k < pmesh->num_indexes; k++)
	{
		int vertexindex = pindexes[pmesh->start_index + k];
		const vbmvertex_t* pvertex = &pvertexes[vertexindex];

		vec3_t finalPosition;
		VectorClear(finalPosition);

		vec3_t finalNormal;
		VectorClear(finalNormal);

		float weights[MAX_VBM_BONEWEIGHTS];
		for(int l = 0; l < MAX_VBM_BONEWEIGHTS; l++)
			weights[l] = pvertex->boneweights[l] / 255.0;

		VBM_NormalizeWeights(weights, MAX_VBM_BONEWEIGHTS);

		for(int l = 0; l < MAX_VBM_BONEWEIGHTS; l++)
		{
			if(!pvertex->boneweights[l])
				continue;

			int meshboneindex = pvertex->boneindexes[l] / 3;
			int realboneindex = pboneindexes[meshboneindex];

			vec3_t tmp;
			float weight = weights[l];
			VectorTransform(pvertex->origin, weightbonetransform[realboneindex].matrix, tmp);
			VectorMA(finalPosition, weight, tmp, finalPosition);

			VectorRotate(pvertex->normal, weightbonetransform[realboneindex].matrix, tmp);
			VectorMA(finalNormal, weight, tmp, finalNormal);
		}

		pcache->setvertex(vertexindex, finalPosition, finalNormal);

	}
}

//===========================
// Build vertex cache
// 
//===========================
CVBMCache::vertexcache_t* CVBMCache::BuildVertexCache( const vbmcache_t* pvbmcache, const vec3_t& origin, const vec3_t& angles, int sequence, float scale )
{
	// Set up bone transform matrices
	int numbones = pvbmcache->pstudiohdr->numbones;
	std::vector<bonematrix_t> bonetransform(numbones);
	std::vector<bonematrix_t> weightbonetransform(numbones);

	float controllers[4] = {0};
	for(int i = 0; i < pvbmcache->pstudiohdr->numbonecontrollers; i++)
		VBM_SetController(pvbmcache->pstudiohdr, i, 0, controllers);

	VBM_SetupBones(pvbmcache->pstudiohdr, pvbmcache->pvbmheader, origin, angles, sequence, scale, 0, controllers, bonetransform, weightbonetransform);

	// Create the actual entry and transform vertices
	const vbmvertex_t* pvbmvertexes = pvbmcache->pvbmheader->getVertexes();
	const unsigned int* pvbmindexes = pvbmcache->pvbmheader->getIndexes();

	vertexcache_t* pnew = new vertexcache_t(pvbmcache->pvbmheader->numverts);
	for(int i = 0; i < pvbmcache->pvbmheader->numbodyparts; i++)
	{
		const vbmbodypart_t* pbodypart = pvbmcache->pvbmheader->getBodyPart(i);
		for(int j = 0; j < pbodypart->numsubmodels; j++)
		{
			const vbmsubmodel_t* psubmodel = pbodypart->getSubmodel(pvbmcache->pvbmheader, j);

			// Process any LODs
			for(int k = 0; k < psubmodel->numlods; k++)
			{
				const vbmlod_t* plod = psubmodel->getLOD(pvbmcache->pvbmheader, k);
				const vbmsubmodel_t* plodsubmodel = plod->getSubmodel(pvbmcache->pvbmheader);
				const vbmmesh_t* pbonemesh = nullptr;

				for(int l = 0; l < plodsubmodel->nummeshes; l++)
				{
					const vbmmesh_t* pmesh = plodsubmodel->getMesh(pvbmcache->pvbmheader, l);
					if(pmesh->numbones)
						pbonemesh = pmesh;

					ProcessVBMMesh(pvbmcache->pvbmheader, weightbonetransform, pmesh, pbonemesh, pvbmvertexes, pvbmindexes, pnew);
				}
			}

			// Process main submodel
			const vbmmesh_t* pbonemesh = nullptr;
			for(int l = 0; l < psubmodel->nummeshes; l++)
			{
				const vbmmesh_t* pmesh = psubmodel->getMesh(pvbmcache->pvbmheader, l);
				if(pmesh->numbones)
					pbonemesh = pmesh;

				ProcessVBMMesh(pvbmcache->pvbmheader, weightbonetransform, pmesh, pbonemesh, pvbmvertexes, pvbmindexes, pnew);
			}
		}
	}

	pnew->index = m_pVertexCacheVector.size();
	m_pVertexCacheVector.push_back(pnew);

	return pnew;
}

//===========================
// Return texture by index
// 
//===========================
const model_texture_t* CVBMCache::GetTextureByIndex( int index )
{
	if(index < 0 || index >= m_pTexturesVector.size())
		return nullptr;
	else
		return m_pTexturesVector[index];
}

//===========================
// Return material file by index
// 
//===========================
const pmffile_t* CVBMCache::GetMaterialScriptByIndex( int index )
{
	if(index < 0 || index >= m_pPMFFilesVector.size())
		return nullptr;
	else
		return m_pPMFFilesVector[index];
}

//===========================
// Create BVH for entity
// 
//===========================
CVBMBVH* CVBMCache::BuildBVHForEntity( const vbmcache_t* pvbmcache, int sequence, float scale )
{
	for(int i = 0; i < m_pVBMBVHVector.size(); i++)
	{
		CVBMBVH* pBVH = m_pVBMBVHVector[i];
		if(pBVH->GetVBMHeader() == pvbmcache->pvbmheader && pBVH->GetStudioeader() == pvbmcache->pstudiohdr
			&& pBVH->GetScale() == scale && pBVH->GetSequence() == sequence)
		{
			return pBVH;
		}
	}

	int cacheindex = m_pVBMBVHVector.size();
	CVBMBVH* pBVH = new CVBMBVH(pvbmcache->pvbmheader, pvbmcache->pstudiohdr, sequence, scale, cacheindex);
	if(!pBVH->SetupBVH(*this))
	{
		delete pBVH;
		return nullptr;
	}

	m_pVBMBVHVector.push_back(pBVH);
	return pBVH;
}

//===========================
// Return vertex cache for index
// 
//===========================
CVBMCache::vbmcache_t* CVBMCache::GetVBMCacheByIndex( int index )
{
	if(index < 0 || index >= m_pVBMCacheVector.size())
		return nullptr;
	else
		return m_pVBMCacheVector[index];
}

//===========================
// Return vertex cache for index
// 
//===========================
CVBMCache::vertexcache_t* CVBMCache::GetVertexCacheByIndex( int index )
{
	if(index < 0 || index >= m_pVertexCacheVector.size())
		return nullptr;
	else
		return m_pVertexCacheVector[index];
}

//===========================
// Return vertex cache for index
// 
//===========================
CVBMBVH* CVBMCache::GetBVHByIndex( int index )
{
	if(index < 0 || index >= m_pVBMBVHVector.size())
		return nullptr;
	else
		return m_pVBMBVHVector[index];
}

//===========================
// Cleans up the class, removing all entries
// 
//===========================
void CVBMCache::Cleanup( void )
{
	if(!m_pVBMCacheVector.empty())
	{
		for(int i = 0; i < m_pVBMCacheVector.size(); i++)
			delete m_pVBMCacheVector[i];

		m_pVBMCacheVector.clear();
	}

	if(!m_pVBMBVHVector.empty())
	{
		for(int i = 0; i < m_pVBMBVHVector.size(); i++)
			delete m_pVBMBVHVector[i];

		m_pVBMBVHVector.clear();
	}

	if(!m_pPMFFilesVector.empty())
	{
		for(int i = 0; i < m_pPMFFilesVector.size(); i++)
			delete m_pPMFFilesVector[i];

		m_pPMFFilesVector.clear();
	}

	if(!m_pAliasMappingsVector.empty())
	{
		for(int i = 0; i < m_pAliasMappingsVector.size(); i++)
			delete m_pAliasMappingsVector[i];

		m_pAliasMappingsVector.clear();
	}

	if(!m_pTexturesVector.empty())
	{
		for(int i = 0; i < m_pTexturesVector.size(); i++)
			delete m_pTexturesVector[i];

		m_pTexturesVector.clear();
	}

	if(!m_pVertexCacheVector.empty())
	{
		for(int i = 0; i < m_pVertexCacheVector.size(); i++)
			delete m_pVertexCacheVector[i];

		m_pVertexCacheVector.clear();
	}
}