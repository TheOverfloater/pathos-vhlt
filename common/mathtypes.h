#ifndef MATHTYPES_H__
#define MATHTYPES_H__
#include "cmdlib.h" //--vluzacn

#if _MSC_VER >= 1000
#pragma once
#endif

typedef unsigned char byte;

#ifdef DOUBLEVEC_T
typedef double vec_t;
#else
typedef float vec_t;
#endif
typedef vec_t   vec3_t[3];                                 // x,y,z
typedef vec_t   vec4_t[4];                                 // x,y,z, w

#ifndef M_PI
#define M_PI			3.14159265358979323846
#endif // M_PI

// Max floating point value
static constexpr float MAX_FLOAT_VALUE = 1e30f;

//=============================================
// @brief Converts bytes to short
//
// @param pdata Pointer to data in bytes
//=============================================
inline int COM_ByteToInt16( const byte *pdata )
{
	static byte bLittleEndian[2] = {1, 0};
	if(*(short*)bLittleEndian == 1)
		return (pdata[0]+(pdata[1]<<8));
	else
		return (pdata[1]+(pdata[0]<<8));
}

//=============================================
// @brief Converts bytes to unsigned short
//
// @param pdata Pointer to data in bytes
//=============================================
inline unsigned short COM_ByteToUint16( const byte *pdata )
{
	static byte littleEndian[2] = {1, 0};
	if(*(int*)littleEndian == 1)
		return (pdata[0]+(pdata[1]<<8));
	else
		return (pdata[1]+(pdata[0]<<8));
}

//=============================================
// @brief Converts bytes to int
//
// @param pdata Pointer to data in bytes
//=============================================
inline int COM_ByteToInt32( const byte *pdata )
{
	int iValue = pdata[0];
	iValue += (pdata[1]<<8);
	iValue += (pdata[2]<<16);
	iValue += (pdata[3]<<24);

	return iValue;
}

//=============================================
// @brief Converts bytes to unsigned int
//
// @param pdata Pointer to data in bytes
//=============================================
inline unsigned int COM_ByteToUint32( const byte *pdata )
{
	unsigned int iValue = pdata[0];
	iValue += (pdata[1]<<8);
	iValue += (pdata[2]<<16);
	iValue += (pdata[3]<<24);

	return iValue;
}


#endif //MATHTYPES_H__
