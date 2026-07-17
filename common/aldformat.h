/*
===============================================
Pathos Engine - Created by Andrew Stephen "Overfloater" Lucas

Copyright 2016
All Rights Reserved.
===============================================
*/

#ifndef ALDHEADER_H
#define ALDHEADER_H

#define ALD_HEADER_ENCODED		(('D'<<24)+('L'<<16)+('A'<<8)+'P')
#define ALD_HEADER_VERSION		3

// Lightmap layers
enum surf_lmap_layers_t
{
	SURF_LIGHTMAP_DEFAULT = 0,
	SURF_LIGHTMAP_VECTORS,
	SURF_LIGHTMAP_AMBIENT,
	SURF_LIGHTMAP_DIFFUSE,

	// Must be last
	NB_SURF_LIGHTMAP_LAYERS
};

// Baked vertex lighting layers
enum baked_vertexlight_layers_t
{
	VERTEX_LIGHTING_VECTORS = 0,
	VERTEX_LIGHTING_AMBIENT,
	VERTEX_LIGHTING_DIFFUSE,

	// Must be last
	NB_BAKED_VERTEXLIGHT_LAYERS
};

enum lightgriddata_layers_t
{
	LIGHTGRID_LAYER_VECTORS = 0,
	LIGHTGRID_LAYER_AMBIENT,
	LIGHTGRID_LAYER_DIFFUSE,

	// Must be last
	NB_LIGHTGRID_DATA_LAYERS
};

enum aldlumptype_t 
{
	ALD_LUMP_NIGHTDATA_BUMP = 1, // nighttime light data for bsp
	ALD_LUMP_EDD_NIGHTDATA_NOBUMP, // nighttime light data for edd(not used in Pathos, only kept for legacy support)
	ALD_LUMP_EXTERNAL_BUMP, // externally stored regular BSP lightdata(not used, legacy support)
	ALD_LUMP_EXTERNAL_NOBUMP, // external data with no bump info(not used, legacy support)
	ALD_LUMP_NIGHTDATA_NOBUMP, // nighttime data with no bump
	ALD_LUMP_EDD_NIGHTDATA_BUMP, // edd data with no bump info(not used in Pathos, only kept for legacy support)
	ALD_LUMP_DAYLIGHT_RETURN_DATA_NOBUMP,
	ALD_LUMP_DAYLIGHT_RETURN_DATA_BUMP
};

enum aldcompression_t
{
	ALD_COMPRESSION_NONE = 0,
	ALD_COMPRESSION_MINIZ
};

struct aldheader_t
{
	aldheader_t():
		header(0),
		version(0),
		flags(0),
		lumpoffset(0),
		numlumps(0),
		lightdatasize(0),
		vertexlightdatasize(0),
		lightgridsampledatasize(0)
		{}

	int header;
	int version;
	int flags;

	int lumpoffset;
	int numlumps;
	int lightdatasize;
	int vertexlightdatasize;
	int lightgridsampledatasize;
};

struct aldlump_t
{
	aldlump_t():
		type(0)
	{
		for(int i = 0; i < NB_SURF_LIGHTMAP_LAYERS; i++)
			lmaplayeroffsets[i] = 0;

		for(int i = 0; i < NB_BAKED_VERTEXLIGHT_LAYERS; i++)
			vertexlightlayeroffsets[i] = 0;

		for(int i = 0; i < NB_LIGHTGRID_DATA_LAYERS; i++)
			lightgridlayeroffsets[i] = 0;
	}

	// Type of ALD lump
	int type;
	// The offsets to each layer of lightmap
	int lmaplayeroffsets[NB_SURF_LIGHTMAP_LAYERS];
	// The offsets to each layer of baked vertex lighting
	int vertexlightlayeroffsets[NB_BAKED_VERTEXLIGHT_LAYERS];
	// The offsets to each layer of light grid sample data
	int lightgridlayeroffsets[NB_LIGHTGRID_DATA_LAYERS];
};

struct aldlayer_t
{
	aldlayer_t():
		compression(0),
		compressionlevel(0),
		dataoffset(0),
		datasize(0)
	{}

	int compression;
	int compressionlevel;
	int dataoffset;
	int datasize;
};
#endif