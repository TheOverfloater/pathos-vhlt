/*
===============================================
Pathos Engine - Created by Andrew Stephen "Overfloater" Lucas

Copyright 2016
All Rights Reserved.
===============================================
*/

#include <math.h>
#include <vector>
#include <Windows.h>

#include "studio_util.h"
#include "scriplib.h"
#include "studio.h"
#include "log.h"
#include "mathlib.h"

// Quaternion and vector arrays used for bone transforms
vec3_t g_bonePositions1[MAXSTUDIOBONES];
vec4_t g_boneQuaternions1[MAXSTUDIOBONES];
vec3_t g_bonePositions2[MAXSTUDIOBONES];
vec4_t g_boneQuaternions2[MAXSTUDIOBONES];
vec3_t g_bonePositions3[MAXSTUDIOBONES];
vec4_t g_boneQuaternions3[MAXSTUDIOBONES];
vec3_t g_bonePositions4[MAXSTUDIOBONES];
vec4_t g_boneQuaternions4[MAXSTUDIOBONES];
vec3_t g_bonePositions5[MAXSTUDIOBONES];
vec4_t g_boneQuaternions5[MAXSTUDIOBONES];

// Used for bone transform calculations
float	g_boneMatrix[3][4];

// Internal rotation matrix
float g_rotationMatrix[3][4];

//=============================================================================
// DotProduct
//
//=============================================================================
inline float DotProduct4( const float* pv1, const float *pv2 )
{
	return (pv1[0] * pv2[0] + pv1[1] * pv2[1] + pv1[2] * pv2[2] + pv1[3] * pv2[3]);
}

//=============================================================================
// QuaternionSlerp
//
//=============================================================================
void QuaternionSlerp( const vec4_t p, vec4_t q, float t, vec4_t qt )
{
	int i;
	float omega, cosom, sinom, sclp, sclq;

	// decide if one of the quaternions is backwards
	float a = 0;
	float b = 0;
	for (i = 0; i < 4; i++) {
		a += (p[i]-q[i])*(p[i]-q[i]);
		b += (p[i]+q[i])*(p[i]+q[i]);
	}
	if (a > b) {
		for (i = 0; i < 4; i++) {
			q[i] = -q[i];
		}
	}

	cosom = p[0]*q[0] + p[1]*q[1] + p[2]*q[2] + p[3]*q[3];

	if ((1.0 + cosom) > 0.00000001) {
		if ((1.0 - cosom) > 0.00000001) {
			omega = acos( cosom );
			sinom = sin( omega );
			sclp = sin( (1.0 - t)*omega) / sinom;
			sclq = sin( t*omega ) / sinom;
		}
		else {
			sclp = 1.0 - t;
			sclq = t;
		}
		for (i = 0; i < 4; i++) {
			qt[i] = sclp * p[i] + sclq * q[i];
		}
	}
	else {
		qt[0] = -p[1];
		qt[1] = p[0];
		qt[2] = -p[3];
		qt[3] = p[2];
		sclp = sin( (1.0 - t) * 0.5 * M_PI);
		sclq = sin( t * 0.5 * M_PI);
		for (i = 0; i < 3; i++) {
			qt[i] = sclp * p[i] + sclq * qt[i];
		}
	}
}

//=============================================================================
// R_ConcatTransforms
//
//=============================================================================
void R_ConcatTransforms (const float in1[3][4], const float in2[3][4], float out[3][4])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
				in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
				in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
				in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] +
				in1[0][2] * in2[2][3] + in1[0][3];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
				in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
				in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
				in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] +
				in1[1][2] * in2[2][3] + in1[1][3];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
				in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
				in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
				in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] +
				in1[2][2] * in2[2][3] + in1[2][3];
}

//=============================================================================
// VectorTransform
//
//=============================================================================
void VectorTransform (const vec_t* pin1, const float in2[3][4], vec3_t& out)
{
	out[0] = DotProduct(pin1, in2[0]) + in2[0][3];
	out[1] = DotProduct(pin1, in2[1]) +	in2[1][3];
	out[2] = DotProduct(pin1, in2[2]) +	in2[2][3];
}

//=============================================================================
// VectorInverseTransform
//
//=============================================================================
void VectorInverseTransform( const vec_t* pvec, const float (*pmatrix)[4], vec3_t& out )
{
	// We first have to subtract the position, THEN inverse rotate
	vec3_t tmp;
	for(int i = 0; i < 3; i++)
		tmp[i] = pvec[i] - pmatrix[i][3];

	out[0] = tmp[0]*pmatrix[0][0] + tmp[1]*pmatrix[1][0] + tmp[2]*pmatrix[2][0];
	out[1] = tmp[0]*pmatrix[0][1] + tmp[1]*pmatrix[1][1] + tmp[2]*pmatrix[2][1];
	out[2] = tmp[0]*pmatrix[0][2] + tmp[1]*pmatrix[1][2] + tmp[2]*pmatrix[2][2];
}

//=============================================================================
// VectorRotate
//
//=============================================================================
void VectorRotate (const vec3_t in1, const float in2[3][4], vec3_t& out)
{
	out[0] = DotProduct(in1, in2[0]);
	out[1] = DotProduct(in1, in2[1]);
	out[2] = DotProduct(in1, in2[2]);
}

//=============================================================================
// VectorIRotate
//
//=============================================================================
void VectorIRotate (const float *in1, const float in2[3][4], float *out)
{
	out[0] = in1[0]*in2[0][0] + in1[1]*in2[1][0] + in1[2]*in2[2][0];
	out[1] = in1[0]*in2[0][1] + in1[1]*in2[1][1] + in1[2]*in2[2][1];
	out[2] = in1[0]*in2[0][2] + in1[1]*in2[1][2] + in1[2]*in2[2][2];
}

//=============================================
// @brief
//
//=============================================
void AngleMatrix( const float* angles, float (*pmatrix)[4] )
{
	float angle = angles[YAW]*(M_PI*2/360);
	float sy = sin(angle);
	float cy = cos(angle);

	angle = angles[PITCH]*(M_PI*2/360);
	float sp = sin(angle);
	float cp = cos(angle);

	angle = angles[ROLL]*(M_PI*2/360);
	float sr = sin(angle);
	float cr = cos(angle);

	pmatrix[0][0] = cp*cy;
	pmatrix[1][0] = cp*sy;
	pmatrix[2][0] = -sp;

	pmatrix[0][1] = sr*sp*cy+cr*-sy;
	pmatrix[1][1] = sr*sp*sy+cr*cy;
	pmatrix[2][1] = sr*cp;

	pmatrix[0][2] = (cr*sp*cy+-sr*-sy);
	pmatrix[1][2] = (cr*sp*sy+-sr*cy);
	pmatrix[2][2] = cr*cp;

	pmatrix[0][3] = 0;
	pmatrix[1][3] = 0;
	pmatrix[2][3] = 0;
}

//=============================================
// @brief Takes a quaternion, and turns it into a 3x4 matrix
//
// @param quaternion Input quaternion
// @param pmatrix Result 3x4 matrix
//=============================================
inline void QuaternionMatrix( const vec4_t& quaternion, float (*pmatrix)[4] )
{
	pmatrix[0][0] = 1.0 - 2.0 * quaternion[1] * quaternion[1] - 2.0 * quaternion[2] * quaternion[2];
	pmatrix[1][0] = 2.0 * quaternion[0] * quaternion[1] + 2.0 * quaternion[3] * quaternion[2];
	pmatrix[2][0] = 2.0 * quaternion[0] * quaternion[2] - 2.0 * quaternion[3] * quaternion[1];

	pmatrix[0][1] = 2.0 * quaternion[0] * quaternion[1] - 2.0 * quaternion[3] * quaternion[2];
	pmatrix[1][1] = 1.0 - 2.0 * quaternion[0] * quaternion[0] - 2.0 * quaternion[2] * quaternion[2];
	pmatrix[2][1] = 2.0 * quaternion[1] * quaternion[2] + 2.0 * quaternion[3] * quaternion[0];

	pmatrix[0][2] = 2.0 * quaternion[0] * quaternion[2] + 2.0 * quaternion[3] * quaternion[1];
	pmatrix[1][2] = 2.0 * quaternion[1] * quaternion[2] - 2.0 * quaternion[3] * quaternion[0];
	pmatrix[2][2] = 1.0 - 2.0 * quaternion[0] * quaternion[0] - 2.0 * quaternion[1] * quaternion[1];
}

//=============================================
// @brief Blends two quaternions together into an output quaternion
//
// @param q1 First quaternion
// @param q2 Second quaternion
// @param interp A value from 0-1 determining how the quaternions are blended together
// @param outq Reference to vec4_t to hold the resulting quaternion
//=============================================
inline void QuaternionBlend( const vec4_t& q1, const vec4_t& q2, float interp, vec4_t& outq )
{
	float a = 0;
	float b = 0;

	vec4_t quat1;
	for(int i = 0; i < 4; i++)
	{
		a += (q1[i]-q2[i])*(q1[i]-q2[i]);
		b += (q1[i]+q2[i])*(q1[i]+q2[i]);
	}

	if(a > b)
	{
		for(int i = 0; i < 4; i++)
			quat1[i] = -q1[i];
	}
	else
	{
		for(int i = 0; i < 4; i++)
			quat1[i] = q1[i];
	}

	float sclq1;
	float sclq2;
	const float cosom = DotProduct4(quat1, q2);
	if(1.0+cosom > 0.000001f)
	{
		if(1.0-cosom > 0.00001f)
		{
			const float omega = acos(cosom);
			const float sinom = sin(omega);
			sclq1 = sin((1.0f-interp)*omega)/sinom;
			sclq2 = sin(interp*omega)/sinom;
		}
		else
		{
			sclq1 = 1.0-interp;
			sclq2 = interp;
		}
	}
	else
	{
		quat1[0] = -q2[1];
		quat1[1] = q2[0];
		quat1[2] = -q2[3];
		quat1[3] = q2[2];

		sclq1 = sin((1.0f-interp) * (0.5f*M_PI));
		sclq2 = sin(interp*(0.5f*M_PI));
	}

	for(int i = 0; i < 4; i++)
		outq[i] = sclq1*quat1[i] + sclq2*q2[i];
}

//=============================================
// @brief Takes an angle and turns it into a quaternion
//
// @param angles Input angles
// @param quaternion Result quaternion
//=============================================
inline void AngleQuaternion( const vec3_t& angles, vec4_t& quaternion )
{
	// FIXME: rescale the inputs to 1/2 angle
	float angle = angles[2] * 0.5;
	float sy = sin(angle);
	float cy = cos(angle);
	angle = angles[1] * 0.5;
	float sp = sin(angle);
	float cp = cos(angle);
	angle = angles[0] * 0.5;
	float sr = sin(angle);
	float cr = cos(angle);

	quaternion[0] = sr*cp*cy-cr*sp*sy; // X
	quaternion[1] = cr*sp*cy+sr*cp*sy; // Y
	quaternion[2] = cr*cp*sy-sr*sp*cy; // Z
	quaternion[3] = cr*cp*cy+sr*sp*sy; // W
}

//=============================================
//
//
//=============================================
void VBM_CalculateBoneAdjustments( const studiohdr_t* phdr, float dadt, std::vector<float>& padj, const float* pcontroller1, const float* pcontroller2, byte mouth )
{
	// Get pointer to bone controller
	for(int i = 0; i < phdr->numbonecontrollers; i++)
	{
		const mstudiobonecontroller_t* pbonecontroller = phdr->getBoneController(i);

		float value = 0;
		int index = pbonecontroller->index;
		if(index <= 3)
		{
			if(pbonecontroller->type & STUDIO_RLOOP)
			{
				if(fabs(pcontroller1[i]-pcontroller2[i]) > 128.0f)
				{
					int a = (static_cast<int>(pcontroller1[i])+128)%256;
					int b = (static_cast<int>(pcontroller2[i])+128)%256;

					value = ((a*dadt + b*(1.0f-dadt)) - 128) * (360.0f/256.0f) + pbonecontroller->start;
				}
				else
				{
					value = (pcontroller1[i]*dadt + pcontroller2[i]*(1.0f-dadt))*(360.0f/256.0f)+pbonecontroller->start;
				}
			}
			else
			{
				value = (pcontroller1[i]*dadt+pcontroller2[i]*(1.0-dadt))/255.0f;
				if(value < 0)
					value = 0;
				else if(value > 1)
					value = 1;

				value = (1.0f-value)*pbonecontroller->start + value*pbonecontroller->end;
			}
		}
		else
		{
			value = static_cast<float>(mouth)/64.0f;
			if(value < 0)
				value = 0;
			else if(value > 1)
				value = 1;

			value = (1.0-value) * pbonecontroller->start + value*pbonecontroller->end;
		}

		switch(pbonecontroller->type & STUDIO_TYPES)
		{
		case STUDIO_XR:
		case STUDIO_YR:
		case STUDIO_ZR:
			padj[i] = value*(M_PI/180.0f);
			break;
		case STUDIO_X:
		case STUDIO_Y:
		case STUDIO_Z:
			padj[i] = value;
			break;
		default:
			break;
		}
	}
}

//=============================================
//
//
//=============================================
void VBM_InterpolateBones( const studiohdr_t* phdr, const vec4_t* quaternions1, vec3_t positions1[], vec4_t quaternions2[], vec3_t positions2[], float interpolant, vec4_t outquaternions[], vec3_t outpositions[] )
{
	// Cap interpolant
	float interp = interpolant;
	if(interp < 0)
		interp = 0;
	else if(interp > 1)
		interp = 1;

	float ninterp = 1.0 - interp;
	for(int i = 0; i < phdr->numbones; i++)
	{
		const mstudiobone_t* pbone = phdr->getBone(i);

		// Don't blend non-blended bones
		if(pbone->flags & STUDIO_DONT_BLEND)
		{
			for(int j = 0; j < 4; j++)
				outquaternions[i][j] = quaternions1[i][j];

			for(int j = 0; j < 3; j++)
				outpositions[i][j] = positions1[i][j];
		}
		else
		{
			// Blend the quaternions
			QuaternionBlend(quaternions1[i], quaternions2[i], interp, outquaternions[i]);
		
			// Blend the positions
			for(int j = 0; j < 3; j++)
				outpositions[i][j] = positions1[i][j] * ninterp + positions2[i][j] * interp;
		}
	}
}

//=============================================
//
//
//=============================================
const mstudioanim_t* VBM_GetAnimation( const studiohdr_t* phdr, const mstudioseqdesc_t* psequencedesc )
{
	if(psequencedesc->seqgroup != 0)
		return nullptr;

	const mstudioseqgroup_t* pseqgroup = phdr->getSequenceGroup(psequencedesc->seqgroup);
	return reinterpret_cast<const mstudioanim_t*>(reinterpret_cast<const byte*>(phdr) + pseqgroup->data + psequencedesc->animindex);
}

//=============================================
//
//
//=============================================
void VBM_CalculateBoneQuaternion( int frame, float interpolant, const mstudiobone_t* pbone, const mstudioanim_t* panimation, const std::vector<float>& padj, vec4_t& quaternion )
{
	vec3_t angle1, angle2;

	for (int i = 0; i < 3; i++)
	{
		if (panimation->offset[i+3] == 0)
		{
			angle2[i] = angle1[i] = pbone->value[i+3]; // default;
		}
		else
		{
			const mstudioanimvalue_t* panimvalue = panimation->getAnimationValue(i+3);
			int j = frame;
			if (panimvalue->num.total < panimvalue->num.valid)
				j = 0;

			while (panimvalue->num.total <= j)
			{
				j -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
				if (panimvalue->num.total < panimvalue->num.valid)
					j = 0;
			}

			if (panimvalue->num.valid > j)
			{
				angle1[i] = panimvalue[j+1].value;

				if (panimvalue->num.valid > j + 1)
				{
					angle2[i] = panimvalue[j+2].value;
				}
				else
				{
					if (panimvalue->num.total > j + 1)
						angle2[i] = angle1[i];
					else
						angle2[i] = panimvalue[panimvalue->num.valid+2].value;
				}
			}
			else
			{
				angle1[i] = panimvalue[panimvalue->num.valid].value;
				if (panimvalue->num.total > j + 1)
					angle2[i] = angle1[i];
				else
					angle2[i] = panimvalue[panimvalue->num.valid + 2].value;
			}
			angle1[i] = pbone->value[i+3] + angle1[i] * pbone->scale[i+3];
			angle2[i] = pbone->value[i+3] + angle2[i] * pbone->scale[i+3];
		}

		if (pbone->bonecontroller[i+3] != -1)
		{
			angle1[i] += padj[pbone->bonecontroller[i+3]];
			angle2[i] += padj[pbone->bonecontroller[i+3]];
		}
	}

	if (!VectorCompare( angle1, angle2 ))
	{
		vec4_t q1, q2;
		AngleQuaternion( angle1, q1 );
		AngleQuaternion( angle2, q2 );
		QuaternionBlend( q1, q2, interpolant, quaternion );
	}
	else
	{
		AngleQuaternion( angle1, quaternion );
	}
}

//=============================================
//
//
//=============================================
void VBM_CalculateBonePosition( int frame, float interpolant, const mstudiobone_t* pbone, const mstudioanim_t* panimation, const std::vector<float>& padj, vec3_t& outpos )
{
	for (int i = 0; i < 3; i++)
	{
		outpos[i] = pbone->value[i]; // default;
		if (panimation->offset[i] != 0)
		{
			const mstudioanimvalue_t* panimvalue = panimation->getAnimationValue(i);
			int j = frame;
			if (panimvalue->num.total < panimvalue->num.valid)
				j = 0;

			// find span of values that includes the frame we want
			while (panimvalue->num.total <= j)
			{
				j -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
				if (panimvalue->num.total < panimvalue->num.valid)
					j = 0;
			}

			// if we're inside the span
			if (panimvalue->num.valid > j)
			{
				if (panimvalue->num.valid > j + 1)
					outpos[i] += (static_cast<Double>(panimvalue[j+1].value) * (1.0 - interpolant) + interpolant * static_cast<Double>(panimvalue[j+2].value)) * pbone->scale[i];
				else
					outpos[i] += static_cast<Double>(panimvalue[j+1].value) * pbone->scale[i];
			}
			else
			{
				// are we at the end of the repeating values section and there'interpolant another section with data?
				if (panimvalue->num.total <= j + 1)
					outpos[i] += (static_cast<Double>(panimvalue[panimvalue->num.valid].value) * (1.0 - interpolant) + interpolant * static_cast<Double>(panimvalue[panimvalue->num.valid + 2].value)) * pbone->scale[i];
				else
					outpos[i] += static_cast<Double>(panimvalue[panimvalue->num.valid].value) * pbone->scale[i];
			}
		}

		if ( pbone->bonecontroller[i] != -1 && !padj.empty() )
			outpos[i] += padj[pbone->bonecontroller[i]];
	}
}

//=============================================
//
//
//=============================================
float VBM_EstimateInterpolant( float time, float animtime, float prevanimtime )
{
	float interpolant = 1.0;
	if(animtime >= prevanimtime + 0.01f)
	{
		interpolant = time - animtime;
		interpolant /= 0.1f;
		if(interpolant > 2.0)
			interpolant = 2.0;
	}

	return interpolant;
}

//=============================================
//
//
//=============================================
void VBM_CalculateRotations( const studiohdr_t* phdr, float time, float animtime, float prevanimtime, vec3_t positions[], vec4_t quaternions[], const mstudioseqdesc_t* pseqdesc, const mstudioanim_t* panim, float frame, const float* pcontroller1, const float* pcontroller2, byte mouth )
{
	// Cap frame value
	float _frame = frame;
	if(_frame > (pseqdesc->numframes-1))
		_frame = 0;
	else if(_frame < -0.01f)
		_frame = -0.01f;

	// Calculate interpolant value
	const int intframe = static_cast<int>(_frame);
	const float dadt = VBM_EstimateInterpolant(time, animtime, prevanimtime);
	const float interp = _frame - intframe;
	
	// Estimate controller values
	std::vector<float> controlleradj(phdr->numbonecontrollers);
	for(int i = 0; i < phdr->numbonecontrollers; i++)
		controlleradj[i] = 0;

	VBM_CalculateBoneAdjustments(phdr, dadt, controlleradj, pcontroller1, pcontroller2, mouth);

	// Calculate quaternions and bone positions for each bone
	for(int i = 0; i < phdr->numbones; i++)
	{
		const mstudiobone_t* pbone = phdr->getBone(i);

		VBM_CalculateBoneQuaternion(intframe, interp, pbone, &panim[i], controlleradj, quaternions[i]);
		VBM_CalculateBonePosition(intframe, interp, pbone, &panim[i], controlleradj, positions[i]);
	}

	// Remove movement from specific sequence motion types
	if(pseqdesc->motiontype & STUDIO_X)
		positions[pseqdesc->motionbone][0] = 0.0f;
	if(pseqdesc->motiontype & STUDIO_Y)
		positions[pseqdesc->motionbone][1] = 0.0f;
	if(pseqdesc->motiontype & STUDIO_Z)
		positions[pseqdesc->motionbone][2] = 0.0f;
}

//=============================================
//
//=============================================
void VBM_SetupBones( const studiohdr_t* pstudiohdr, const vbmheader_t* pvbmheader, const vec3_t& origin, const vec3_t& angles, int sequence, float scale, float frame, float* pcontrollers, std::vector<bonematrix_t>& bonetransformsvector, std::vector<bonematrix_t>& weightbonetransformsvector )
{
	if(bonetransformsvector.empty() || bonetransformsvector.size() < pstudiohdr->numbones)
		bonetransformsvector.resize(pstudiohdr->numbones);

	vec3_t _angles;
	_angles[PITCH] = -angles[PITCH];
	_angles[YAW] = angles[YAW];
	_angles[ROLL] = angles[ROLL];

	AngleMatrix(angles, g_rotationMatrix);

	for(int i = 0; i < 3; i++)
		g_rotationMatrix[i][3] = origin[i];

	// Apply scale to models that require it
	if(scale != 0)
	{
		for(int i = 0; i < 3; i++)
		{
			for(int j = 0; j < 3; j++)
				g_rotationMatrix[i][j] *= scale;
		}
	}

	// Get sequence info
	int _sequence = sequence;
	if(_sequence < 0 || _sequence >= pstudiohdr->numseq)
		_sequence = 0;

	const mstudioseqdesc_t* pseqdesc = pstudiohdr->getSequence(_sequence);

	// Get animation and calc rotations
	const mstudioanim_t* panim = VBM_GetAnimation(pstudiohdr, pseqdesc);
	if(!panim)
	{
		Warning("Pathos does not support models with sequence groups. Model '%s' not managed.\n", __FUNCTION__, pstudiohdr->name);
		return;
	}

	// Calculate rotations
	float animtime = 0;
	float blending[2] = { 0 };
	float time = 0;

	VBM_CalculateRotations(pstudiohdr, time, animtime, 0, g_bonePositions1, g_boneQuaternions1, pseqdesc, panim, frame, pcontrollers, pcontrollers, 0);

	// Manage blending
	if(pseqdesc->numblends > 1)
	{
		panim += pstudiohdr->numbones;
		VBM_CalculateRotations(pstudiohdr, time, animtime, 0, g_bonePositions2, g_boneQuaternions2, pseqdesc, panim, frame, pcontrollers, pcontrollers, 0);
		float interp = blending[0]/255.0f;

		VBM_InterpolateBones(pstudiohdr, g_boneQuaternions1, g_bonePositions1, g_boneQuaternions2, g_bonePositions2, interp, g_boneQuaternions1, g_bonePositions1);

		if(pseqdesc->numblends == 4)
		{
			panim += pstudiohdr->numbones;
			VBM_CalculateRotations(pstudiohdr, time, animtime,0, g_bonePositions3, g_boneQuaternions3, pseqdesc, panim, frame, pcontrollers, pcontrollers, 0);

			panim += pstudiohdr->numbones;
			VBM_CalculateRotations(pstudiohdr, time, animtime, 0, g_bonePositions4, g_boneQuaternions4, pseqdesc, panim, frame, pcontrollers, pcontrollers, 0);

			interp = blending[0]/255.0f;
			VBM_InterpolateBones(pstudiohdr, g_boneQuaternions3, g_bonePositions3, g_boneQuaternions4, g_bonePositions4, interp, g_boneQuaternions3, g_bonePositions3);

			interp = blending[1]/255.0f;
			VBM_InterpolateBones(pstudiohdr, g_boneQuaternions1, g_bonePositions1, g_boneQuaternions3, g_bonePositions3, interp, g_boneQuaternions1, g_bonePositions1);
		}
	}

	// Calculate bone matrices
	for(int i = 0; i < pstudiohdr->numbones; i++)
	{
		const mstudiobone_t* pbone = pstudiohdr->getBone(i);
		QuaternionMatrix(g_boneQuaternions1[i], g_boneMatrix);

		for(int j = 0; j < 3; j++)
			g_boneMatrix[j][3] = g_bonePositions1[i][j];

		if(pbone->parent == -1)
			R_ConcatTransforms(g_rotationMatrix, g_boneMatrix, bonetransformsvector[i].matrix);
		else
			R_ConcatTransforms(bonetransformsvector[pbone->parent].matrix, g_boneMatrix, bonetransformsvector[i].matrix);
	}

	// Rotate by the inverse bind pose
	if(pvbmheader)
	{
		for(int i = 0; i < pvbmheader->numboneinfo; i++)
		{
			const vbmboneinfo_t* pvbmbone = pvbmheader->getBoneInfo(i);
			R_ConcatTransforms(bonetransformsvector[i].matrix, pvbmbone->bindtransform, weightbonetransformsvector[i].matrix);
		}
	}
}

//===========================
// Test intersection between a ray and a bvh bbox
// 
//===========================
bool IntersectBBoxPoint( const vec3_t& start, const vec3_t& end, const vec3_t& bbmins, const vec3_t& bbmaxs, const vec3_t& normalDirection )
{
	float tx1 = (bbmins[0] - start[0]) / normalDirection[0];
	float tx2 = (bbmaxs[0] - start[0]) / normalDirection[0];
	float tmin = min( tx1, tx2 );
	float tmax = max( tx1, tx2 );

	float ty1 = (bbmins[1] - start[1]) / normalDirection[1];
	float ty2 = (bbmaxs[1] - start[1]) / normalDirection[1];
	tmin = max( tmin, min( ty1, ty2 ) );
	tmax = min( tmax, max( ty1, ty2 ) );

	float tz1 = (bbmins[2] - start[2]) / normalDirection[2];
	float tz2 = (bbmaxs[2] - start[2]) / normalDirection[2];
	tmin = max( tmin, min( tz1, tz2 ) );
	tmax = min( tmax, max( tz1, tz2 ) );

	return tmax >= tmin && tmin < MAX_FLOAT_VALUE && tmax > 0;
}

//=============================================
// @brief Calculate mins/maxs for entity
//
//=============================================
void TransformMinsMaxs( const float (*protationmatrix)[4], const vec3_t& orig_mins, const vec3_t& orig_maxs, vec3_t& outmins, vec3_t& outmaxs )
{
	vec3_t temp;
	vec3_t bboxcorners[8];

	for (int i = 0; i < 8; i++)
	{
		if ( i & 1 ) 
			temp[0] = orig_mins[0];
		else 
			temp[0] = orig_maxs[0];

		if ( i & 2 ) 
			temp[1] = orig_mins[1];
		else 
			temp[1] = orig_maxs[1];

		if ( i & 4 ) 
			temp[2] = orig_mins[2];
		else 
			temp[2] = orig_maxs[2];

		VectorCopy( temp, bboxcorners[i] );
	}

	for (int i = 0; i < 8; i++ )
	{
		VectorCopy(bboxcorners[i], temp);
		VectorRotate(temp, protationmatrix, bboxcorners[i]);
	}

	// Set the bounding box
	vec3_t mins;
	vec3_t maxs;

	for(int i = 0; i < 3; i++)
	{
		mins[i] = MAX_FLOAT_VALUE;
		maxs[i] = -MAX_FLOAT_VALUE;
	}

	for(int i = 0; i < 8; i++)
	{
		// Mins
		if(bboxcorners[i][0] < mins[0]) 
			mins[0] = bboxcorners[i][0];
		if(bboxcorners[i][1] < mins[1]) 
			mins[1] = bboxcorners[i][1];
		if(bboxcorners[i][2] < mins[2]) 
			mins[2] = bboxcorners[i][2];

		// Maxs
		if(bboxcorners[i][0] > maxs[0]) 
			maxs[0] = bboxcorners[i][0];
		if(bboxcorners[i][1] > maxs[1]) 
			maxs[1] = bboxcorners[i][1];
		if(bboxcorners[i][2] > maxs[2]) 
			maxs[2] = bboxcorners[i][2];
	}

	for(int i = 0; i < 3; i++)
	{
		outmins[i] = mins[i] + protationmatrix[i][3];
		outmaxs[i] = maxs[i] + protationmatrix[i][3];
	}
}

//=============================================
//
//
//=============================================
void VBM_NormalizeWeights( float* pflweights, int maxweights )
{
	// The idea to normalize weights like this came from
	// browsing Xash code ages ago, so thanks to Misha for the idea.
	// Not sure how necessary this is, but better safe than sorry.
	float totalweight = 0;
	for(int i = 0; i < maxweights; i++)
		totalweight += pflweights[i];

	const float scale = 1.0f/totalweight;
	for(int i = 0; i < maxweights; i++)
	{
		if(pflweights[i] > 0)
			pflweights[i] *= scale;
	}
}

//=============================================
// @brief
//
//=============================================
float VBM_SetController( const studiohdr_t* pstudiohdr, int controllerindex, float value, float* pcontrollers )
{
	// Get bone controller
	const mstudiobonecontroller_t* pbonecontrollers = reinterpret_cast<const mstudiobonecontroller_t*>(reinterpret_cast<const byte*>(pstudiohdr) + pstudiohdr->bonecontrollerindex);

	int i = 0;
	for(; i < pstudiohdr->numbonecontrollers; i++)
	{
		if(pbonecontrollers[i].index == static_cast<int>(controllerindex))
			break;
	}

	if(i == pstudiohdr->numbonecontrollers)
		return value;

	if(pbonecontrollers[i].type & (STUDIO_XR|STUDIO_YR|STUDIO_ZR))
	{
		if(pbonecontrollers[i].end < pbonecontrollers[i].start)
			value *= -1;

		if(pbonecontrollers[i].start + 359.0 >= pbonecontrollers[i].end)
		{
			if(value > ((pbonecontrollers[i].start + pbonecontrollers[i].end)/2.0) + 180)
				value -= 360;
			if(value < ((pbonecontrollers[i].start + pbonecontrollers[i].end)/2.0) - 180)
				value += 360;
		}
		else
		{
			if(value > 360)
				value -= static_cast<int>((value/360.0)*360.0);
			else if(value < 0)
				value += static_cast<int>(((value/-360.0)+1)*360.0);
		}
	}

	float controllersetting = 255*(value - pbonecontrollers[i].start)/(pbonecontrollers[i].end-pbonecontrollers[i].start);
	if(controllersetting < 0)
		controllersetting = 0;
	else if(controllersetting > 255)
		controllersetting = 255;

	pcontrollers[i] = controllersetting;

	return controllersetting*(1.0/255.0)*(pbonecontrollers[i].end-pbonecontrollers[i].start)+pbonecontrollers[i].start;
}
