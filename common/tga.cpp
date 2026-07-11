/*
===============================================
Pathos Engine - Created by Andrew "Overfloater" Lucas

Copyright 2016
All Rights Reserved.

===============================================
*/

#include "cmdlib.h"
#include "tga.h"
#include "scriplib.h"
#include "log.h"


//=============================================
//
// Function:
//=============================================
void COM_FlipTexture( int width, int height, int bpp, bool fliph, bool flipv, byte*& pdata )
{
	// Flip vertically and/or horizontally if needed
	int outputSize = width*height*bpp;
	byte *pflipped = new byte[outputSize];
	for(int i = 0; i < height; i++)
	{
		byte *dst = pflipped + i*width*bpp;

		const byte *src;
		if(flipv)
			src = pdata + (height-i-1)*width*bpp;
		else
			src = pdata + i*width*bpp;

		if(fliph)
		{
			for(int j = 0; j < width; j++)
				memcpy(&dst[j*bpp], &src[((width-j-1)*bpp)], sizeof(byte)*bpp);
		}
		else
			memcpy(dst, src, sizeof(byte)*width*bpp);
	}

	delete[] pdata;
	pdata = pflipped;
}

//=============================================
// @brief Tells if the input is a power of two value
//
// @param size Size to check
// @return TRUE if it's a power of two value, FALSE otherwise
//=============================================
bool COM_IsPowerOfTwo( int size )
{
	int _size = size;
	while(_size != 1)
	{
		if((_size % 2) != 0) 
			return false;

		_size /=2;
	}

	return true;
}

//=============================================
// @brief Loads a TGA file and returns it's data
//
// @param pfile Pointer to raw file data
// @param pdata Destination pointer for image data
// @param width Reference to texture width variable
// @param height Reference to texture height variable
// @param bpp Reference to texture bit depth variable
// @param size To hold the output data size
// @return TRUE if successfully loaded, FALSE otherwise
//=============================================
bool TGA_Load( const char* pstrFilename, const byte* pfile, byte*& pdata, int& width, int& height, int& bpp, int& size )
{
	// Set basic information
	const tga_header_t *ptrTgaHeader = reinterpret_cast<const tga_header_t *>(pfile);
	if(ptrTgaHeader->datatypecode != TGA_DATATYPE_RGB
		&& ptrTgaHeader->datatypecode != TGA_DATATYPE_RLE_RGB 
		&& ptrTgaHeader->datatypecode != TGA_DATATYPE_COLORMAPPED 
		&& ptrTgaHeader->datatypecode != TGA_DATATYPE_RLE_COLORMAPPED
		&& ptrTgaHeader->datatypecode != TGA_DATATYPE_GRAYSCALE
		&& ptrTgaHeader->datatypecode != TGA_DATATYPE_RLE_GRAYSCALE
		|| ptrTgaHeader->bitsperpixel != 24 
		&& ptrTgaHeader->bitsperpixel != 32
		&& ptrTgaHeader->bitsperpixel != 8)
	{
		Warning("%s is using a non-supported format. Only 24 bit and 32 bit true color formats are supported.\n", pstrFilename);
		return false;
	}

	int tgaWidth = COM_ByteToInt16(ptrTgaHeader->width);
	int tgaHeight = COM_ByteToInt16(ptrTgaHeader->height);
	int tgaBpp = ptrTgaHeader->bitsperpixel/8;

	if(!COM_IsPowerOfTwo(tgaWidth) || !COM_IsPowerOfTwo(tgaHeight))
	{
		Warning("%s is not a power of two texture.\n", pstrFilename);
		return false;
	}

	// Determine sizes
	int nbPixels = tgaWidth*tgaHeight;
	int inputSize = nbPixels*tgaBpp;
	int outputSize = nbPixels*4;

	// Allocate the conversion data
	byte* pout = new byte[outputSize];

	// Load based on type
	const byte *pcur = pfile + sizeof(tga_header_t);
	if(ptrTgaHeader->datatypecode == TGA_DATATYPE_RGB)
	{
		// Uncompressed TGA
		for(int i = 0, j = 0; i < inputSize; i += tgaBpp, j += 4)
		{
			pout[j] = pcur[i+2];
			pout[j+1] = pcur[i+1];
			pout[j+2] = pcur[i];

			if(tgaBpp == 3)
				pout[j+3] = 255;
			else
				pout[j+3] = pcur[i+3];
		}
	}
	else if(ptrTgaHeader->datatypecode == TGA_DATATYPE_RLE_RGB)
	{
		// RLE Compression
		int i = 0;
		while(i < outputSize)
		{
			if((*pcur) & 0x80)
			{
				byte length = *pcur-127;
				pcur++;

				for(int j = 0; j < length; j++, i += 4)
				{
					pout[i] = pcur[2];
					pout[i+1] = pcur[1];
					pout[i+2] = pcur[0];

					if(tgaBpp == 3)
						pout[i+3] = 255;
					else
						pout[i+3] = pcur[3];
				}
					
				pcur += tgaBpp;
			}
			else
			{
				byte length = *pcur+1;
				pcur++;

				for(int j = 0; j < length; j++, i += 4, pcur += tgaBpp)
				{
					pout[i] = pcur[2];
					pout[i+1] = pcur[1];
					pout[i+2] = pcur[0];

					if(tgaBpp == 3)
						pout[i+3] = 255;
					else
						pout[i+3] = pcur[3];
				}
			}
		}
	}
	else if(ptrTgaHeader->datatypecode == TGA_DATATYPE_COLORMAPPED)
	{
		int colormapDepth = ptrTgaHeader->colourmapdepth;
		if(colormapDepth != 24 && colormapDepth != 32)
		{
			Warning("TGA %s uses an unsupported color map depth of %d.\n", pstrFilename, colormapDepth);
			delete[] pout;
			return false;
		}

		int colormapBpp = colormapDepth / 8;
		int colormapLength = COM_ByteToInt16(ptrTgaHeader->colourmaplength);
		int colormapOrigin = COM_ByteToInt16(ptrTgaHeader->colourmaporigin);
		int colorMapSize = colormapLength * colormapBpp;

		const byte* pcolormap = pcur;
		const byte* ppixeldata = pcur + colorMapSize;

		// Uncompressed TGA
		for(int i = 0, j = 0; i < inputSize; i++, j += 4)
		{
			const byte* pcolor = pcolormap + (colormapOrigin + ppixeldata[i]) * colormapBpp;

			pout[j] = pcolor[2];
			pout[j+1] = pcolor[1];
			pout[j+2] = pcolor[0];

			if(colormapBpp == 3)
				pout[j+3] = 255;
			else
				pout[j+3] = pcolor[3];
		}
	}
	else if(ptrTgaHeader->datatypecode == TGA_DATATYPE_RLE_COLORMAPPED)
	{
		int colormapDepth = ptrTgaHeader->colourmapdepth;
		if(colormapDepth != 24 && colormapDepth != 32)
		{
			Warning("TGA %s uses an unsupported color map depth of %d.\n", pstrFilename, colormapDepth);
			delete[] pout;
			return false;
		}

		int colormapBpp = colormapDepth / 8;
		int colormapLength = COM_ByteToInt16(ptrTgaHeader->colourmaplength);
		int colormapOrigin = COM_ByteToInt16(ptrTgaHeader->colourmaporigin);
		int colorMapSize = colormapLength * colormapBpp;

		const byte* pcolormap = pcur;
		const byte* ppixeldata = pcur + colorMapSize;

		// RLE Compression
		int i = 0;
		while(i < outputSize)
		{
			if((*ppixeldata) & 0x80)
			{
				byte length = (*ppixeldata)-127;
				ppixeldata++;

				const byte* pcolor = pcolormap + (colormapOrigin + (*ppixeldata)) * colormapBpp;
				for(int j = 0; j < length; j++, i += 4)
				{
					pout[i] = pcolor[2];
					pout[i+1] = pcolor[1];
					pout[i+2] = pcolor[0];

					if(colormapBpp == 3)
						pout[i+3] = 255;
					else
						pout[i+3] = pcolor[3];
				}
					
				ppixeldata++;
			}
			else
			{
				byte length = (*ppixeldata)+1;
				ppixeldata++;

				for(int j = 0; j < length; j++, i += 4, ppixeldata++)
				{
					const byte* pcolor = pcolormap + (colormapOrigin + (*ppixeldata)) * colormapBpp;

					pout[i] = pcolor[2];
					pout[i+1] = pcolor[1];
					pout[i+2] = pcolor[0];

					if(colormapBpp == 3)
						pout[i+3] = 255;
					else
						pout[i+3] = pcolor[3];
				}
			}
		}
	}
	else if(ptrTgaHeader->datatypecode == TGA_DATATYPE_GRAYSCALE)
	{
		if(ptrTgaHeader->bitsperpixel != 8)
		{
			Warning("TGA %s is a greyscale image with an unsupported pixel depth of %d.\n", pstrFilename, (ptrTgaHeader->bitsperpixel*8));
			delete[] pout;
			return false;
		}

		// Uncompressed TGA
		for(int i = 0, j = 0; i < inputSize; i++, j += 4)
		{
			byte greyColorValue = pcur[i];
			for(int k = 0; k < 3; k++)
				pout[j+k] = greyColorValue;
			
			pout[j+3] = 255;
		}
	}
	else if(ptrTgaHeader->datatypecode == TGA_DATATYPE_RLE_GRAYSCALE)
	{
		if(ptrTgaHeader->bitsperpixel != 8)
		{
			Warning("TGA %s is a greyscale image with an unsupported pixel depth of %d.\n", pstrFilename, (ptrTgaHeader->bitsperpixel*8));
			delete[] pout;
			return false;
		}

		// RLE Compression
		int i = 0;
		while(i < outputSize)
		{
			if((*pcur) & 0x80)
			{
				byte length = (*pcur)-127;
				pcur++;

				byte greyColorValue = (*pcur);
				for(int j = 0; j < length; j++, i += 4)
				{
					for(int k = 0; k < 3; k++)
						pout[i+k] = greyColorValue;

					pout[i+3] = 255;
				}
					
				pcur++;
			}
			else
			{
				byte length = (*pcur)+1;
				pcur++;

				for(int j = 0; j < length; j++, i += 4, pcur++)
				{
					byte greyColorValue = (*pcur);
					for(int k = 0; k < 3; k++)
						pout[i+k] = greyColorValue;

					pout[i+3] = 255;;
				}
			}
		}
	}
	else
	{
		Warning("TGA %s uses an unsupported datatype.\n", pstrFilename);
		delete[] pout;
		return false;
	}

	// Flip vertically and/or horizontally if needed
	if(!(ptrTgaHeader->imagedescriptor & 5) || (ptrTgaHeader->imagedescriptor & 4))
		COM_FlipTexture(tgaWidth, tgaHeight, 4, (ptrTgaHeader->imagedescriptor & 4) ? true : false, (ptrTgaHeader->imagedescriptor & 5) ? false : true, pout);

	// Set the output data
	pdata = pout;
	size = outputSize;
	width = tgaWidth;
	height = tgaHeight;
	bpp = tgaBpp;

	return true;
}
