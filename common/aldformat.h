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
#define ALD_VERSION				1

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

struct aldheader_t
{
	aldheader_t():
		header(0),
		flags(0),
		lumpoffset(0),
		numlumps(0)
		{}

	Int32 header;
	Int32 version;
	Int32 flags;

	Int32 lumpoffset;
	Int32 numlumps;
	Int32 lightdatasize;
};

struct aldlump_t
{
	aldlump_t():
		type(0)
	{
		for(Uint32 i = 0; i < NB_LIGHTMAP_LAYERS; i++)
			layeroffsets[i] = 0;
	}

	Int32 type;
	Int32 layeroffsets[NB_LIGHTMAP_LAYERS];
};
#endif