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
#include "vbmbvh.h"
#include "studio_util.h"

void DumpSMD_BVH( const vbmheader_t* pvbmheader, CVBMBVH::bvhsubmodel_t* pbvhsubmodel )
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

	for(int i = 0; i < pbvhsubmodel->triangles.size(); i++)
	{
		CVBMBVH::bvhtriangle_t& triangle = pbvhsubmodel->triangles[i];
		const vbmtexture_t* ptexture = pvbmheader->getTexture(triangle.skinref);

		fprintf(pf, "%s.tga\n", ptexture->name);
		for(Uint32 k = 0; k < 3; k++)
		{
			int vertexindex = triangle.vertindexes[k];
			CVBMBVH::bvhvertex_t& vertex = pbvhsubmodel->vertexes[vertexindex];

			fprintf(pf, "  0   %.4f  %.4f  %.4f  %.4f  %.4f  %.4f  %.4f  %.4f\n",
			vertex.origin[0], vertex.origin[1], vertex.origin[2],
			triangle.normal[0], triangle.normal[1], triangle.normal[2],
			0, 0);
		}
	}

	fprintf(pf, "end\n");
	fclose(pf);
}

//===========================
// Constructor
// 
//===========================
CVBMBVH::CVBMBVH( const vbmheader_t* pvbmheader, const studiohdr_t* pstudioheader, int sequence, float scale, int cacheindex ):
	m_pVBMheader(pvbmheader),
	m_pStudioHeader(pstudioheader),
	m_sequenceIndex(sequence),
	m_modelScale(scale),
	m_cacheIndex(cacheindex)
{
	VectorClear(m_mins);
	VectorClear(m_maxs);
}

//===========================
// Destructor
// 
//===========================
CVBMBVH::~CVBMBVH( void )
{
	if(!m_pBodyPartsVector.empty())
	{
		for(int i = 0; i < m_pBodyPartsVector.size(); i++)
			delete m_pBodyPartsVector[i];

		m_pBodyPartsVector.clear();
	}
}

//===========================
// Set up VBH object
// 
//===========================
bool CVBMBVH::SetupBVH( CVBMCache& vbmCache )
{
	if(!m_pVBMheader)
	{
		Warning("No VBM data set.");
		return false;
	}

	if(!m_pStudioHeader)
	{
		Warning("No studiomodel data set.");
		return false;
	}

	// Set up bone transform matrices
	std::vector<bonematrix_t> bonetransform(m_pStudioHeader->numbones);
	std::vector<bonematrix_t> weightbonetransform(m_pStudioHeader->numbones);

	vec3_t origin;
	VectorClear(origin);

	vec3_t angles;
	VectorClear(angles);

	float controllers[4];
	for(int i = 0; i < m_pStudioHeader->numbonecontrollers; i++)
		VBM_SetController(m_pStudioHeader, i, 0, controllers);

	VBM_SetupBones(m_pStudioHeader, m_pVBMheader, origin, angles, m_sequenceIndex, m_modelScale, 0, controllers, bonetransform, weightbonetransform);

	// Get vertexes and indexes arrays
	const unsigned int* pindexes = m_pVBMheader->getIndexes();
	const vbmvertex_t* pvertexes = m_pVBMheader->getVertexes();

	for(int i = 0; i < 3; i++)
	{
		m_mins[i] = MAX_FLOAT_VALUE;
		m_maxs[i] = -MAX_FLOAT_VALUE;
	}

	// Set up textures
	m_pMaterialFilesVector.resize(m_pVBMheader->numtextures);
	for(int i = 0; i < m_pVBMheader->numtextures; i++)
	{
		const vbmtexture_t* pvbmtexture = m_pVBMheader->getTexture(i);
		if(pvbmtexture->index == -1)
			continue;

		const pmffile_t* ppmffile = vbmCache.GetMaterialScriptByIndex(pvbmtexture->index);
		if(ppmffile)
			m_pMaterialFilesVector[i] = ppmffile;
	}

	// Allocate bodyparts
	m_pBodyPartsVector.resize(m_pVBMheader->numbodyparts);
	for(int i = 0; i < m_pVBMheader->numbodyparts; i++)
	{
		const vbmbodypart_t* psrcbodypart = m_pVBMheader->getBodyPart(i);
		bvhbodypart_t* pdestbodypart = new bvhbodypart_t();
		m_pBodyPartsVector[i] = pdestbodypart;

		pdestbodypart->name.assign(psrcbodypart->name);
		pdestbodypart->base = psrcbodypart->base;

		// Create submodels
		pdestbodypart->submodels.resize(psrcbodypart->numsubmodels);
		for(int j = 0; j < psrcbodypart->numsubmodels; j++)
		{
			const vbmsubmodel_t* psrcsubmodel = psrcbodypart->getSubmodel(m_pVBMheader, j);

			// Count triangles
			int numindexes = 0;
			for(int k = 0; k < psrcsubmodel->nummeshes; k++)
			{
				const vbmmesh_t* pmesh = psrcsubmodel->getMesh(m_pVBMheader, k);
				numindexes += pmesh->num_indexes;
			}

			// Create entry
			int numtriangles = numindexes / 3;
			bvhsubmodel_t* pdestsubmodel = new bvhsubmodel_t(numtriangles);
			pdestbodypart->submodels[j] = pdestsubmodel;

			// If no data was present, leave submodel blank
			if(!numindexes)
				continue;

			// Collect unique vertexes
			int triangleindex = 0;
			int numvertexeindexes = 0;
			std::vector<int> vertexindexes(numindexes);
			std::vector<int> vertmeshindexes(numindexes);
			std::vector<std::vector<int>> bonemap(psrcsubmodel->nummeshes);

			const vbmmesh_t* pbonemesh;
			for(int k = 0; k < psrcsubmodel->nummeshes; k++)
			{
				const vbmmesh_t* pmesh = psrcsubmodel->getMesh(m_pVBMheader, k);
				if(pmesh->numbones > 0)
					pbonemesh = pmesh;

				const byte* pbones = pbonemesh->getBones(m_pVBMheader);
				bonemap[k].resize(pbonemesh->numbones);
				for(int l = 0; l < pbonemesh->numbones; l++)
					bonemap[k][l] = pbones[l];
				
				for(int l = 0; l < pmesh->num_indexes; l += 3)
				{
					for(int m = 0; m < 3; m++)
					{
						int vertexindex = pindexes[pmesh->start_index + l + m];

						int n = 0;
						for(; n < numvertexeindexes; n++)
						{
							if(vertexindexes[n] == vertexindex)
								break;
						}

						if(n == numvertexeindexes)
						{
							vertexindexes[n] = vertexindex;
							vertmeshindexes[n] = k;
							numvertexeindexes++;
						}

						// We need to reverse triangle orientation for BVH processing
						pdestsubmodel->triangles[triangleindex].vertindexes[(2 - m)] = n;
					}

					pdestsubmodel->triangles[triangleindex].skinref = pmesh->skinref;
					triangleindex++;
				}
			}

			// Clean this up
			for(int k = 0; k < 3; k++)
			{
				pdestsubmodel->mins[k] = MAX_FLOAT_VALUE;
				pdestsubmodel->maxs[k] = -MAX_FLOAT_VALUE;
			}

			// Create final verts
			pdestsubmodel->setVertexCount(numvertexeindexes);
			for(int k = 0; k < numvertexeindexes; k++)
			{
				const vbmvertex_t* pvertex = &pvertexes[vertexindexes[k]];
				int meshindex = vertmeshindexes[k];

				float weights[MAX_VBM_BONEWEIGHTS];
				for(int l = 0; l < MAX_VBM_BONEWEIGHTS; l++)
					weights[l] = pvertex->boneweights[l] / 255.0;

				VBM_NormalizeWeights(weights, MAX_VBM_BONEWEIGHTS);

				vec3_t finalPosition;
				VectorClear(finalPosition);
				for(int l = 0; l < MAX_VBM_BONEWEIGHTS; l++)
				{
					if(!pvertex->boneweights[l])
						continue;

					int meshboneindex = pvertex->boneindexes[l] / 3;
					int realboneindex = bonemap[meshindex][meshboneindex];

					vec3_t tmp;
					VectorTransform(pvertex->origin, weightbonetransform[realboneindex].matrix, tmp);
					VectorMA(finalPosition, weights[l], tmp, finalPosition);
				}

				VectorCopy(finalPosition, pdestsubmodel->vertexes[k].origin);

				// Update mins/maxs
				for(int l = 0; l < 3; l++)
				{
					if(pdestsubmodel->mins[l] > finalPosition[l])
						pdestsubmodel->mins[l] = finalPosition[l];

					if(pdestsubmodel->maxs[l] < finalPosition[l])
						pdestsubmodel->maxs[l] = finalPosition[l];
				}

				for(int l = 0; l < 2; l++)
					pdestsubmodel->vertexes[k].texcoords[l] = pvertex->texcoord[l];
			}

			// Set up triangle data
			int trianglecount = pdestsubmodel->triangles.size();
			for(int k = 0; k < trianglecount; k++)
			{
				bvhtriangle_t& tri = pdestsubmodel->triangles[k];

				const bvhvertex_t& v1 = pdestsubmodel->vertexes[tri.vertindexes[0]];
				const bvhvertex_t& v2 = pdestsubmodel->vertexes[tri.vertindexes[1]];
				const bvhvertex_t& v3 = pdestsubmodel->vertexes[tri.vertindexes[2]];

				VectorSubtract(v2.origin, v1.origin, tri.edge00);
				VectorSubtract(v3.origin, v1.origin, tri.edge02);
				CrossProduct(tri.edge00, tri.edge02, tri.normal);
				VectorNormalize(tri.normal);

				tri.distance = DotProduct(v2.origin, tri.normal);

				int l = 0;
				for(; l < 3; l++)
				{
					if(tri.normal[l] < 0)
						tri.signbits |= (1 << l);

					if(fabs(tri.normal[l]) == 1.0)
					{
						tri.planetype = l;
						break;
					}
				}

				if(l == 3)
				{
					if(tri.normal[0] >= tri.normal[1] && tri.normal[0] >= tri.normal[2])
						tri.planetype = PLANE_AX;
					else if(tri.normal[1] >= tri.normal[0] && tri.normal[1] >= tri.normal[2])
						tri.planetype = PLANE_AY;
					else
						tri.planetype = PLANE_AZ;
				}

				VectorAdd(v1.origin, v2.origin, tri.centroid);
				VectorAdd(tri.centroid, v3.origin, tri.centroid);
				VectorScale(tri.centroid, 0.3333f, tri.centroid);
			}

			// Create the BVH itself
			bvhnode_t* pbvhrootnode = new bvhnode_t();
			pbvhrootnode->triindexes.resize(trianglecount);
			for(int k = 0; k < trianglecount; k++)
				pbvhrootnode->triindexes[k] = k;

			pbvhrootnode->index = pdestsubmodel->pbvhnodes.size();
			pdestsubmodel->pbvhnodes.push_back(pbvhrootnode);

			// Create nodes
			UpdateBVHNodeBounds(pdestsubmodel, pbvhrootnode);
			SubdivideBVHNode(pdestsubmodel, pbvhrootnode);

			// Update entire BVH's mins/maxs
			for(int k = 0; k < 3; k++)
			{
				if(pdestsubmodel->mins[k] < m_mins[k])
					m_mins[k] = pdestsubmodel->mins[k];

				if(pdestsubmodel->maxs[k] > m_maxs[k])
					m_maxs[k] = pdestsubmodel->maxs[k];
			}
		}
	}

	return true;
}

//===========================
// Update bounds of a BVH node
// 
//===========================
void CVBMBVH::UpdateBVHNodeBounds( bvhsubmodel_t* pdestsubmodel, bvhnode_t* pnode )
{
	for(int i = 0; i < 3; i++)
	{
		pnode->mins[i] = MAX_FLOAT_VALUE;
		pnode->maxs[i] = -MAX_FLOAT_VALUE;
	}

	for(int i = 0; i < pnode->triindexes.size(); i++)
	{
		int triindex = pnode->triindexes[i];
		bvhtriangle_t& tri = pdestsubmodel->triangles[triindex];

		for(int j = 0; j < 3; j++)
		{
			bvhvertex_t& vertex = pdestsubmodel->vertexes[tri.vertindexes[j]];
			
			for(int k = 0; k < 3; k++)
			{
				if(pnode->mins[k] > vertex.origin[k])
					pnode->mins[k] = vertex.origin[k];

				if(pnode->maxs[k] < vertex.origin[k])
					pnode->maxs[k] = vertex.origin[k];
			}
		}
	}
}

//===========================
// Subdivides a BVH node
// 
//===========================
void CVBMBVH::SubdivideBVHNode( bvhsubmodel_t* pdestsubmodel, bvhnode_t* pnode )
{
	// Find our longest axis
	vec3_t extents;
	VectorSubtract(pnode->maxs, pnode->mins, extents);

	int j = 0;
	for(int i = 1; i < 3; i++)
	{
		if(extents[i] > extents[j])
			j = i;
	}

	// Assign triangles from parent to children based on split position
	float splitPosition = pnode->mins[j] + extents[j] * 0.5;
	
	std::vector<int> leftNodeTriangles;
	std::vector<int> rightNodeTriangles;
	for(int i = 0; i < pnode->triindexes.size(); i++)
	{
		int triangleIndex = pnode->triindexes[i];
		bvhtriangle_t& triangle = pdestsubmodel->triangles[i];
		if(triangle.centroid[j] < splitPosition)
			leftNodeTriangles.push_back(triangleIndex);
		else
			rightNodeTriangles.push_back(triangleIndex);
	}

	if(leftNodeTriangles.empty() || rightNodeTriangles.empty())
	{
		// If left or right node is empty, we're in a leaf node
		pnode->isleaf = true;
		return;
	}

	// Create left child node
	int leftChildIndex = pdestsubmodel->pbvhnodes.size();
	pnode->childnodes[0] = leftChildIndex;

	bvhnode_t* pLeftChild = new bvhnode_t();
	pLeftChild->index = leftChildIndex;
	pLeftChild->triindexes = leftNodeTriangles;
	pdestsubmodel->pbvhnodes.push_back(pLeftChild);

	UpdateBVHNodeBounds(pdestsubmodel, pLeftChild);

	// Create right child node
	int rightChildIndex = pdestsubmodel->pbvhnodes.size();
	pnode->childnodes[1] = rightChildIndex;

	bvhnode_t* pRightChild = new bvhnode_t();
	pRightChild->index = rightChildIndex;
	pRightChild->triindexes = rightNodeTriangles;
	pdestsubmodel->pbvhnodes.push_back(pRightChild);

	UpdateBVHNodeBounds(pdestsubmodel, pRightChild);

	// Clear this node, as it's not a leaf node
	pnode->triindexes.clear();
	pnode->isleaf = false;

	// Recurse further down the children
	SubdivideBVHNode(pdestsubmodel, pLeftChild);
	SubdivideBVHNode(pdestsubmodel, pRightChild);
}

//===========================
// Perform line trace
// 
//===========================
bool CVBMBVH::TraceLine( const vec3_t& start, const vec3_t& end, int body, int skin, float* poutfraction, vec3_t* poutnormal, vec3_t* poutposition, float* poutplanedistance ) const
{
	// We need this structure, as we cannot have threads changing/accessing member variables
	bvhthreadinfo_t threadinfo;

	// Set thread specific data
	VectorClear(threadinfo.planenormal);
	VectorCopy(end, threadinfo.endposition);
	VectorSubtract(end, start, threadinfo.normdirection);

	// Set basic stuff in thread data
	threadinfo.distance = threadinfo.basedistance = VectorNormalize(threadinfo.normdirection);
	threadinfo.tracefraction = 1.0;

	for(int i = 0; i < m_pBodyPartsVector.size(); i++)
	{
		// Select submodel to use
		const bvhsubmodel_t* psubmodel = SetupModel(i, body);

		// Don't bother if it's empty
		if(psubmodel->pbvhnodes.empty())
			continue;

		const bvhnode_t* prootnode = psubmodel->pbvhnodes[0];
		RecurseTreePointTrace(threadinfo, psubmodel, start, end, prootnode);
		if(!threadinfo.numtriangles)
			continue;

		for(int j = 0; j < threadinfo.numtriangles; j++)
		{
			vec3_t impactPosition;
			vec3_t impactNormal;
			float planeDistance;
			float fraction = 1.0;

			// Get triangle index to get triangle, and triangle's texture
			int triangleIndex = threadinfo.trianglesvector[j];
			const bvhtriangle_t& triangle = psubmodel->triangles[triangleIndex];

			// The triangle will only have a texture if the texture has alphatest set
			const pmffile_t* ppmffile;
			if(triangle.skinref != -1)
				ppmffile = m_pMaterialFilesVector[triangle.skinref];
			else
				ppmffile = nullptr;
			
			if(ppmffile && (ppmffile->noshadow || ppmffile->transparent))
				continue;

			bool testResult;
			if(ppmffile && ppmffile->alphatest && ppmffile->ptexture && ppmffile->ptexture->pdata)
				testResult = TestLineTriangleIntersectWithAlpha(threadinfo, start, psubmodel->vertexes, &triangle, ppmffile->ptexture, impactPosition, impactNormal, planeDistance, fraction);
			else
				testResult = TestLineTriangleIntersect(threadinfo, start, end, psubmodel->vertexes, &triangle, impactPosition, impactNormal, planeDistance, fraction);

			if(testResult)
			{
				if(fraction < threadinfo.tracefraction)
				{
					threadinfo.tracefraction = fraction;
					VectorCopy(impactPosition, threadinfo.endposition);
					VectorCopy(impactNormal, threadinfo.planenormal);
					threadinfo.planedistance = planeDistance;
				}
			}
		}

		// Reset this
		threadinfo.numtriangles = 0;
	}

	// Set any outputs
	if(poutfraction)
		(*poutfraction) = threadinfo.tracefraction;

	if(poutnormal)
		VectorCopy(threadinfo.planenormal, (*poutnormal));

	if(poutposition)
		VectorCopy(threadinfo.endposition, (*poutposition));

	if(poutplanedistance)
		(*poutplanedistance) = threadinfo.planedistance;

	// Tell if we impacted anything at all
	return (threadinfo.tracefraction < 1.0) ? true : false;
}

//=============================================
// @brief Recurse down the tree with a point trace
//
//=============================================
void CVBMBVH::RecurseTreePointTrace( bvhthreadinfo_t& threadinfo, const bvhsubmodel_t* psubmodel, const vec3_t& start, const vec3_t& end, const bvhnode_t* pbvhnode ) const
{
	if(!IntersectBBoxPoint(start, end, pbvhnode->mins, pbvhnode->maxs, threadinfo.normdirection))
		return;

	if(pbvhnode->isleaf)
	{
		// If leaf, add triangles to the list
		AddBVHNodeTriangles(threadinfo, pbvhnode);
	}
	else
	{
		const bvhnode_t* pchildnode = psubmodel->pbvhnodes[pbvhnode->childnodes[0]];
		RecurseTreePointTrace(threadinfo, psubmodel, start, end, pchildnode);

		pchildnode = psubmodel->pbvhnodes[pbvhnode->childnodes[1]];
		RecurseTreePointTrace(threadinfo, psubmodel, start, end, pchildnode);	
	}
}

//=============================================
// @brief Add triangle to the list
//
//=============================================
void CVBMBVH::AddBVHNodeTriangles( bvhthreadinfo_t& threadinfo, const bvhnode_t* pbvhnode ) const
{
	for(int i = 0; i < pbvhnode->triindexes.size(); i++)
	{
		// Check if array needs to be extended
		if(threadinfo.numtriangles == threadinfo.trianglesvector.size())
		{
			int arraySize = threadinfo.trianglesvector.size()+TRIANGLE_INDEX_ALLOC_SIZE;
			threadinfo.trianglesvector.resize(arraySize);
		}

		threadinfo.trianglesvector[threadinfo.numtriangles] = pbvhnode->triindexes[i];
		threadinfo.numtriangles++;
	}
}

//=============================================
// @brief Check alpha value of a texture for collision
//
// Thanks to Meetem for his example code
//=============================================
bool CVBMBVH::TestLineTriangleIntersectWithAlpha( bvhthreadinfo_t& threadinfo, const vec3_t& start, const std::vector<bvhvertex_t>& vertexes, const bvhtriangle_t* ptriangle, const model_texture_t* ptexture, vec3_t& impactPosition, vec3_t& impactNormal, float& planeDistance, float& fraction ) const
{
	const bvhvertex_t& vertex0 = vertexes[ptriangle->vertindexes[0]];
	const bvhvertex_t& vertex1 = vertexes[ptriangle->vertindexes[1]];
	const bvhvertex_t& vertex2 = vertexes[ptriangle->vertindexes[2]];

	vec3_t ao, dao;
	VectorSubtract(start, vertex0.origin, ao);
	CrossProduct(ao, threadinfo.normdirection, dao);

	vec3_t normalVector;
	CrossProduct(ptriangle->edge00, ptriangle->edge02, normalVector);

	float determinant = -DotProduct(threadinfo.normdirection, normalVector);
	float invdeterminant = 1.0 / determinant;
	float dst = DotProduct(ao, normalVector) * invdeterminant;
	
	bool hit = (dst >= 0) && (dst < threadinfo.distance) && (determinant != 0) && abs(dst - threadinfo.distance) > 0.001f;
	if(!hit)
		return false;

	float u = DotProduct(ptriangle->edge02, dao) * invdeterminant;
	float v = -DotProduct(ptriangle->edge00, dao) * invdeterminant;
	float w = 1 - u - v;

	bool result = false;
	if(u >= 0 && v >= 0 && w >= 0)
	{
		int intcoords[2];
		float texcoords[2];

		int sizes[2] = { ptexture->width, ptexture->height };
		for(int i = 0; i < 2; i++)
		{
			texcoords[i] = (vertex0.texcoords[i] * w) + (vertex1.texcoords[i] * u) + (vertex2.texcoords[i] * v);
			texcoords[i] = texcoords[i] - floor(texcoords[i]);

			intcoords[i] = texcoords[i] * sizes[i];
			if(intcoords[i] < 0)
				intcoords[i] = 0;
			else if(intcoords[i] >= sizes[i])
				intcoords[i] = (sizes[i] - 1);
		}

		int offset1 = (intcoords[1] * ptexture->width + intcoords[0]) * 4;
		if(ptexture->pdata[offset1 + 3] > 128)
			result = true;
		else
			result = false;
	}

	if(result)
	{
		VectorMA(start, dst, threadinfo.normdirection, impactPosition);
		VectorCopy(ptriangle->normal, impactNormal);
		planeDistance = ptriangle->distance;
		fraction = dst / threadinfo.basedistance;
		threadinfo.distance = dst;
		return true;
	}
	else
	{
		return false;
	}
}

//=============================================
// @brief Perform a line test against a triangle
//
//=============================================
bool CVBMBVH::TestLineTriangleIntersect( bvhthreadinfo_t& threadinfo, const vec3_t& start, const vec3_t& end, const std::vector<bvhvertex_t>& vertexes, const bvhtriangle_t* ptriangle, vec3_t& impactPosition, vec3_t& impactNormal, float& planeDistance, float& fraction ) const
{
	const bvhvertex_t& vertex0 = vertexes[ptriangle->vertindexes[0]];
	const bvhvertex_t& vertex1 = vertexes[ptriangle->vertindexes[1]];
	const bvhvertex_t& vertex2 = vertexes[ptriangle->vertindexes[2]];

	vec3_t edge1, edge2;
	VectorSubtract(vertex1.origin, vertex0.origin, edge1);
	VectorSubtract(vertex2.origin, vertex0.origin, edge2);

	vec3_t h;
    CrossProduct( threadinfo.normdirection, edge2, h );

    float a = DotProduct( edge1, h );
    if (a > -0.0001f && a < 0.0001f) 
		return false; // ray parallel to triangle

    float f = 1 / a;
    vec3_t s;
	VectorSubtract(start, vertex0.origin, s);
    const float u = f * DotProduct( s, h );
    if (u < 0 || u > 1) 
		return false;

    vec3_t q;
	CrossProduct( s, edge1, q );
    float v = f * DotProduct( threadinfo.normdirection, q );
    if (v < 0 || u + v > 1) 
		return false;

    const float t = f * DotProduct( edge2, q );
	float tdiff = abs(t - threadinfo.distance);
    if (t > 0.001f && tdiff > 0.001f && t < threadinfo.distance)
	{
		VectorMA(start, t, threadinfo.normdirection, impactPosition);
		VectorCopy(ptriangle->normal, impactNormal);
		planeDistance = ptriangle->distance;
		fraction = t / threadinfo.basedistance;
		threadinfo.distance = t;
		return true;
	}
	else
	{
		return false;
	}
}

//===========================
// Set up a bodypart for processing
// 
//===========================
const CVBMBVH::bvhsubmodel_t* CVBMBVH::SetupModel( int bodypart, int bodyvalue ) const
{
	const bvhbodypart_t *pbodypart = m_pBodyPartsVector[bodypart];

	int index = bodyvalue / pbodypart->base;
	index = index % pbodypart->submodels.size();

	return pbodypart->submodels[index];
}