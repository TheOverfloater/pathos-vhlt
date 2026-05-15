/*
===============================================
Pathos Engine - Created by Andrew Stephen "Overfloater" Lucas

Copyright 2016
All Rights Reserved.
===============================================
*/

#ifndef VLDFORMAT_H
#define VLDFORMAT_H

#include "datatypes.h"

#define VLD_HEADER_ENCODED		(('D'<<24)+('L'<<16)+('V'<<8)+'P')
#define VLD_HEADER_VERSION		1

#define VLD_MAX_LAYERS          4

enum vld_datatype_t
{
	VLD_DATA_NIGHTSTAGE = 0,
	VLD_DATA_DAYLIGHT_RETURN
};

enum vldlumptype_t 
{
	VLD_LUMP_NIGHTDATA_BUMP = 1,
	VLD_LUMP_NIGHTDATA_NOBUMP,
	VLD_LUMP_DAYLIGHT_RETURN_DATA_NOBUMP,
	VLD_LUMP_DAYLIGHT_RETURN_DATA_BUMP
};

struct vldheader_t
{
	vldheader_t():
		header(0),
		version(0),
		flags(0),
		lumpoffset(0),
		numlumps(0),
		vertexdatasize(0)
		{}

	Int32 header;
	Int32 version;
	Int32 flags;

	Int32 lumpoffset;
	Int32 numlumps;
	Int32 vertexdatasize;
};

struct vldlump_t
{
	vldlump_t():
		type(0)
	{
		for(Uint32 i = 0; i < VLD_MAX_LAYERS; i++)
			layeroffsets[i] = 0;
	}

	Int32 type;
	Int32 layeroffsets[VLD_MAX_LAYERS];
};

struct vldlayer_t
{
	vldlayer_t():
		compression(0),
		compressionlevel(0),
		dataoffset(0),
		datasize(0)
	{}

	Int32 compression;
	Int32 compressionlevel;
	Int32 dataoffset;
	Int32 datasize;
};

#endif