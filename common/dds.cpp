/*
===============================================
Pathos Engine - Created by Andrew "Overfloater" Lucas

Copyright 2016
All Rights Reserved.
===============================================
*/

#include "dds.h"
#include "s3tc.h"

extern bool COM_IsPowerOfTwo( int size );

//=============================================
// @brief Loads a DDS file and returns it's data
//
// @param pfile Pointer to raw file data
// @param pdata Destination pointer for image data
// @param width Reference to texture width variable
// @param height Reference to texture height variable
// @param bpp Reference to texture bit depth variable
// @param size Reference to data size variable
// @param compression Reference to texture compression variable
// @return TRUE if successfully loaded, FALSE otherwise
//=============================================
bool DDS_Load( const char* pstrFilename, const byte* pfile, byte*& pdata, int& width, int& height, int& bpp, int& size, dds_compression_t& compression )
{
	const dds_header_t *pDDSHeader = reinterpret_cast<const dds_header_t*>(pfile);

	// Read information
	int ddsFlags = COM_ByteToUint32(pDDSHeader->bFlags);
	int ddsMagic = COM_ByteToUint32(pDDSHeader->bMagic);
	int ddsFourCC = COM_ByteToUint32(pDDSHeader->bPFFourCC);
	int ddsPFFlags = COM_ByteToUint32(pDDSHeader->bPFFlags);
	int ddsLinSize = COM_ByteToUint32(pDDSHeader->bPitchOrLinearSize);
	int ddsSize = COM_ByteToUint32(pDDSHeader->bSize);

	int ddsWidth = COM_ByteToUint32(pDDSHeader->bWidth);
	int ddsHeight = COM_ByteToUint32(pDDSHeader->bHeight);

	if(!COM_IsPowerOfTwo(ddsWidth) || !COM_IsPowerOfTwo(ddsHeight))
	{
		Warning("%s is not a power of two texture.\n", pstrFilename);
		return false;
	}
		
	if(ddsMagic != DDS_MAGIC || ddsSize != 124 || !(ddsFlags & DDSD_PIXELFORMAT)
		|| !(ddsFlags & DDSD_CAPS) || !(ddsPFFlags & DDPF_FOURCC))
	{
		Warning("Incorrect DDS format on %s.\n", pstrFilename);
		return false;
	}

	if(ddsFourCC == D3DFMT_DXT1)
		compression = DDS_COMPRESSION_DXT1;
	else if(ddsFourCC == D3DFMT_DXT5)
		compression = DDS_COMPRESSION_DXT5;
	else
	{
		Warning("Incorrect compression on: %s. Only DXT1 and DXT5 DDS files are supported.\n", pstrFilename);
		return false;
	}

	// Set output data
	width = ddsWidth;
	height = ddsHeight;
	bpp = 4;
	size = width * height * 4;

	const byte* pImageData = (pfile + DDS_DATA_OFFSET);
	byte* pRGBAData = new byte[size];
	if(compression == DDS_COMPRESSION_DXT1)
		BlockDecompressImageDXT1(width, height, pImageData, (unsigned long*)pRGBAData);
	else
		BlockDecompressImageDXT5(width, height, pImageData, (unsigned long*)pRGBAData);

	return true;
}