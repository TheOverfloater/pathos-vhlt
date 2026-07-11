/*
===============================================
Pathos Engine - Created by Andrew Stephen "Overfloater" Lucas

Copyright 2016
All Rights Reserved.
===============================================
*/

#ifndef VBMCACHE_H
#define VBMCACHE_H

#include <Windows.h>
#include "vbmbvh.h"
#include "mathtypes.h"
#include "vbmformat.h"
#include "studio.h"
#include "vbmbvh.h"
#include "studio_util.h"

#include <vector>
#include <string>

class CVBMBVH;

struct model_texture_t
{
	model_texture_t():
		index(-1),
		width(0),
		height(0),
		bpp(0),
		pdata(nullptr)
	{}

	~model_texture_t()
	{
		if(pdata)
			delete[] pdata;
	}

	std::string name;
	int index;
	int width;
	int height;
	int bpp;
	byte* pdata;
};

struct pmffile_t
{
	pmffile_t():
		index(-1),
		ptexture(nullptr),
		alphatest(false),
		noshadow(false),
		transparent(false)
	{}

	int index;
	std::string filepath;
	std::string texturename;
	model_texture_t* ptexture;

	bool alphatest;
	bool noshadow;
	bool transparent;
};

//===========================
// CVBMBVH
// 
//===========================
class CVBMCache
{
public:
	// Texture formats
	enum texture_format_t
	{
		TX_FORMAT_UNDEFINED = 0,
		TX_FORMAT_TGA,
		TX_FORMAT_DDS
	};

public:
	struct cachevertex_t
	{
		cachevertex_t()
		{
			memset(origin, 0, sizeof(origin));
			memset(normal, 0, sizeof(normal));
		}

		vec3_t origin;
		vec3_t normal;
	};

	struct vertexcache_t
	{
		vertexcache_t(int _numvertexes):
			index(-1),
			pvertexes(nullptr),
			numvertexes(_numvertexes)
		{
			pvertexes = new cachevertex_t[numvertexes];
		}
		~vertexcache_t()
		{
			if(pvertexes)
				delete[] pvertexes;
		}

		void setvertex(int index, vec3_t& position, vec3_t& normal)
		{
			if(index >= numvertexes)
			{
				assert(false);
				return;
			}

			cachevertex_t* pvertex = &pvertexes[index];
			VectorCopy(position, pvertex->origin);
			VectorCopy(normal, pvertex->normal);
		}

		int index;
		cachevertex_t* pvertexes;
		int numvertexes;
	};

	struct vbmcache_t
	{
		vbmcache_t():
			pstudiohdr(nullptr),
			pvbmheader(nullptr),
			cacheindex(-1)
		{}

		~vbmcache_t()
		{
			if(pstudiohdr)
				delete[] reinterpret_cast<byte*>(pstudiohdr);

			if(pvbmheader)
				delete[] reinterpret_cast<byte*>(pvbmheader);
		}

		std::string name;
		studiohdr_t* pstudiohdr;
		vbmheader_t* pvbmheader;
		int cacheindex;
	};

	struct alias_mapping_t
	{
		alias_mapping_t():
			ppmffile(nullptr)
		{}

		std::string filepath;
		pmffile_t* ppmffile;
	};

public:
	// Constructor
	CVBMCache( void );
	// Destructor
	~CVBMCache( void );

	// Loads a model for an entity
	const vbmcache_t* LoadModel( const char* pstrModelFile );
	// Build vertex cache
	vertexcache_t* BuildVertexCache( const vbmcache_t* pvbmcache, const vec3_t& origin, const vec3_t& angles, int sequence, float scale );
	// Create BVH for entity
	CVBMBVH* BuildBVHForEntity( const vbmcache_t* pvbmcache, int sequence, float scale );
	// Cleans up the class, removing all entries
	void Cleanup( void );

	// Set mod directory path
	void SetModDirectoryPath( const char* pstrPath ) { m_modDirPath = pstrPath; }

	// Get VBM cache by index
	vbmcache_t* GetVBMCacheByIndex( int index );
	// Return vertex cache for index
	vertexcache_t* GetVertexCacheByIndex( int index );
	// Return vertex cache for index
	CVBMBVH* GetBVHByIndex( int index );
	// Return texture by index
	const model_texture_t* GetTextureByIndex( int index );
	// Return material file by index
	const pmffile_t* GetMaterialScriptByIndex( int index );

private:
	// Load textures for a model
	void LoadModelTextures( vbmcache_t* pvbmcache );
	// Load textures for a model
	pmffile_t* LoadPMFFile( const Char* pstrBaseDirectory, const Char* pstrPMFPath, bool isloadingfromalias = false );
	// Load a texture for a model
	model_texture_t* LoadTexture( vbmtexture_t* ptexture, const Char* pstrBaseDirectory, const Char* pstrTexturePath );
	// Process vbm mesh into vertex cache
	void ProcessVBMMesh( const vbmheader_t* pvbmheader, std::vector<bonematrix_t>& weightbonetransform, const vbmmesh_t* pmesh, const vbmmesh_t* pbonemesh, const vbmvertex_t* pvertexes, const unsigned int* pindexes, vertexcache_t* pcache );

	// Returns the format for a filename extension
	texture_format_t GetTextureFileFormat( const Char* pstrFilename );

private:
	// Path to mod directory
	std::string m_modDirPath;
	// Cached VBMs
	std::vector<vbmcache_t*> m_pVBMCacheVector;
	// Vector of BVHs created for entities
	std::vector<class CVBMBVH*> m_pVBMBVHVector;

	// PMF files vector
	std::vector<pmffile_t*> m_pPMFFilesVector;
	// Alias file mapping vector
	std::vector<alias_mapping_t*> m_pAliasMappingsVector;
	// Textures vector
	std::vector<model_texture_t*> m_pTexturesVector;

	// Vertex caches for entities
	std::vector<vertexcache_t*> m_pVertexCacheVector;
};
extern CVBMCache gVBMCache;
#endif // VBMCACHE_H