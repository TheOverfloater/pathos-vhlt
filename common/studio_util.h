/*
===============================================
Pathos Engine - Created by Andrew Stephen "Overfloater" Lucas

Copyright 2016
All Rights Reserved.
===============================================
*/

#ifndef STUDIO_UTIL_H
#define STUDIO_UTIL_H

#include <vector>

#include "mathtypes.h"
#include "studio.h"

struct bonematrix_t
{
	bonematrix_t()
	{
		memset(matrix, 0, sizeof(matrix));
	}

	float matrix[3][4];
};

enum
{
	PITCH = 0,	// up / down
	YAW,		// left / right
	ROLL		// fall over
};

extern void AngleMatrix( const float* angles, float (*pmatrix)[4] );
extern void VectorIRotate (const float *in1, const float in2[3][4], float *out);
extern void VectorRotate (const vec3_t in1, const float in2[3][4], vec3_t& out);
extern void VectorTransform (const vec_t* pin1, const float in2[3][4], vec3_t& out);
extern void VectorInverseTransform( const vec_t* pvec, const Float (*pmatrix)[4], vec3_t& out );
extern void R_ConcatTransforms (const float in1[3][4], const float in2[3][4], float out[3][4]);
extern void QuaternionSlerp( const vec4_t p, vec4_t q, float t, vec4_t qt );
extern const mstudioanim_t* VBM_GetAnimation( const studiohdr_t* phdr, const mstudioseqdesc_t* psequencedesc );
extern void VBM_SetupBones( const studiohdr_t* pstudiohdr, const vbmheader_t* pvbmheader, const vec3_t& origin, const vec3_t& angles, int sequence, float scale, float frame, float* pcontrollers, std::vector<bonematrix_t>& bonetransformsvector, std::vector<bonematrix_t>& weightbonetransformsvector );
extern bool IntersectBBoxPoint( const vec3_t& start, const vec3_t& end, const vec3_t& bbmins, const vec3_t& bbmaxs, const vec3_t& normalDirection );
extern void TransformMinsMaxs( const float (*protationmatrix)[4], const vec3_t& orig_mins, const vec3_t& orig_maxs, vec3_t& outmins, vec3_t& outmaxs );
extern void VBM_NormalizeWeights( float* pflweights, int maxweights );
extern float VBM_SetController( const studiohdr_t* pstudiohdr, int controllerindex, float value, float* pcontrollers );
#endif //STUDIO_UTIL_H
