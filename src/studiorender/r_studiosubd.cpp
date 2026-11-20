// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

// Purpose: Support for mapping from a quad mesh to Bicubic Patches, as a means
//         of rendering approximate Catmull-Clark subdivision surfaces

import ks.core.dbg;
import ks.core.vprof;
import ks.math.compressed_vector;
import ks.math.fltx4;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.ssequaternion;
import ks.math.vector;
import ks.math.vector4d;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.iconvar;
#include "studio.h"
#include "studiorendercontext.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imesh.h"
import std;
#include "studiorender.h"
#include "optimize.h"

#include "r_studiosubd_patches.h"

#ifdef _DEBUG
// Temporary debug arrays
extern std::vector< Vector4D > g_DebugCornerPositions;
extern std::vector< Vector4D > g_DebugEdgePositions;
extern std::vector< Vector4D > g_DebugInteriorPositions;
#endif

//
//  Check out CL# 584588 for an SSE-ized version of the older versions of these
//	routines, which came from an older MS doc, by way of the DX10 SDK
//

static void R_TransformVert( const Vector *pSrcPos,	matrix3x4_t *pSkinMat, Vector4DAligned &pos )
{
	CVProfScope _( "R_TransformVert", 0, "SubD Rendering", false, BUDGETFLAG_OTHER );

	// NOTE: Could add SSE stuff here, if we knew what SSE stuff could make it faster
	pos.x  = pSrcPos->x  * (*pSkinMat)[0][0] + pSrcPos->y  * (*pSkinMat)[0][1] + pSrcPos->z  * (*pSkinMat)[0][2] + (*pSkinMat)[0][3];
	pos.y  = pSrcPos->x  * (*pSkinMat)[1][0] + pSrcPos->y  * (*pSkinMat)[1][1] + pSrcPos->z  * (*pSkinMat)[1][2] + (*pSkinMat)[1][3];
	pos.z  = pSrcPos->x  * (*pSkinMat)[2][0] + pSrcPos->y  * (*pSkinMat)[2][1] + pSrcPos->z  * (*pSkinMat)[2][2] + (*pSkinMat)[2][3];
	pos.w = 1.0f;
}


// This function is duplicate code ****
static matrix3x4_t *ComputeSkinMatrixSSE( mstudioboneweight_t &boneweights, matrix3x4_t *pPoseToWorld, matrix3x4_t &scratchMatrix )
{
	CVProfScope _( "ComputeSkinMatrixSSE", 0, "SubD Rendering", false, BUDGETFLAG_OTHER );

	// NOTE: pPoseToWorld, being cache aligned, doesn't need explicit initialization
	extern matrix3x4_t *ComputeSkinMatrix( mstudioboneweight_t &boneweights, matrix3x4_t *pPoseToWorld, matrix3x4_t &scratchMatrix );
	return ComputeSkinMatrix( boneweights, pPoseToWorld, scratchMatrix );

	contract_assert( 0 );
	return nullptr;
}

#ifdef _DEBUG
static ConVar mat_tess_dump( "mat_tess_dump", "0", FCVAR_CHEAT );
#endif

void CStudioRender::SkinSubDCage( mstudiovertex_t *pVertices, int nNumVertices,
								  matrix3x4_t *pPoseToWorld, CCachedRenderData &vertexCache,
								  unsigned short* pGroupToMesh, fltx4 *vOutput, bool bDoFlex )
{
	CVProfScope _( "CStudioRender::SkinSubDCage", 0, "SubD Rendering", false, BUDGETFLAG_OTHER );

	Vector *pSrcPos;
	matrix3x4_t *pSkinMat, temp [[gnu::aligned(16)]];

	contract_assert( nNumVertices > 0 );

	for ( int j=0; j < nNumVertices; ++j )
	{
		mstudiovertex_t &vert = pVertices[pGroupToMesh[j]];

		pSkinMat = ComputeSkinMatrixSSE( vert.m_BoneWeights, pPoseToWorld, temp );

		if ( bDoFlex && vertexCache.IsVertexFlexed( pGroupToMesh[j] ) )
		{
			CachedPosNormTan_t* pFlexedVertex = vertexCache.GetFlexVertex( pGroupToMesh[j] );
			pSrcPos = &pFlexedVertex->m_Position.AsVector3D();

			// Copy strange signed, 0..3 wrinkle tangent-flip encoding over to tangent.w
			pFlexedVertex->m_TangentS.w = pFlexedVertex->m_Position.w;
		}
		else // non-flexed case
		{
			pSrcPos = &vert.m_vecPosition;
		}

		// Transform into world space
		Vector4DAligned vTemp;
		R_TransformVert( pSrcPos, pSkinMat, *(Vector4DAligned*)&vTemp );
		vOutput[j] = LoadAlignedSIMD( (float *) &vTemp );
	}
}

inline unsigned short *InitializeTopologyIndexStruct( TopologyIndexStruct &quad, unsigned short *topologyIndex )
{
	quad.vtx1RingSize             = topologyIndex; topologyIndex += 4;
	quad.vtx1RingCenterQuadOffset = topologyIndex; topologyIndex += 4;
	quad.valences				  = topologyIndex; topologyIndex += 4;
	quad.minOneRingOffset		  = topologyIndex; topologyIndex += 4;
	quad.bndVtx					  = topologyIndex; topologyIndex += 4;
	quad.bndEdge				  = topologyIndex; topologyIndex += 4;
	quad.cornerVtx				  = topologyIndex; topologyIndex += 4;
	quad.loopGapAngle			  = topologyIndex; topologyIndex += 4;
	quad.nbCornerVtx			  = topologyIndex; topologyIndex += 4;
	quad.edgeBias				  = topologyIndex; topologyIndex += 8;
	quad.vUV0					  = topologyIndex; topologyIndex += 4;
	quad.vUV1					  = topologyIndex; topologyIndex += 4;
	quad.vUV2					  = topologyIndex; topologyIndex += 4;
	quad.vUV3					  = topologyIndex; topologyIndex += 4;
	quad.oneRing                  = topologyIndex; 
	topologyIndex += quad.vtx1RingSize[0]+quad.vtx1RingSize[1]+quad.vtx1RingSize[2]+quad.vtx1RingSize[3];

	return topologyIndex;
}

static ConVar mat_tessellation_update_buffers( "mat_tessellation_update_buffers", "1", FCVAR_CHEAT );
static ConVar mat_tessellation_cornertangents( "mat_tessellation_cornertangents", "1", FCVAR_CHEAT );
static ConVar mat_tessellation_accgeometrytangents( "mat_tessellation_accgeometrytangents", "0", FCVAR_CHEAT );

#ifdef _DEBUG

bool NotQuiteEqual( Vector4D &vA, Vector4D &vB )
{
	float flEpsilon = 0.05f;
	Vector4D vDelta = vA - vB;
	float flDist = sqrt( vDelta.x * vDelta.x + vDelta.y * vDelta.y + vDelta.z * vDelta.z );
	bool bSameVector = ( vA.x == vB.x ) && ( vA.y == vB.y ) && ( vA.z == vB.z );

	return ( flDist < flEpsilon ) && !bSameVector;
}

void DumpDebugPositions()
{

	for ( int i=0; i< std::ssize( g_DebugCornerPositions ); i++ )
	{
		bool bCrack = false;
		for ( int j=0; j< std::ssize( g_DebugCornerPositions ); j++ )
		{
			if ( NotQuiteEqual( g_DebugCornerPositions[i], g_DebugCornerPositions[j] ) )
			{
				bCrack = true;
				contract_assert( 0 );
			}
		}

		DevMsg( "%s C - %.15f, %.15f, %.15f\n", bCrack ? "*** " : "    ", g_DebugCornerPositions[i].x, g_DebugCornerPositions[i].y, g_DebugCornerPositions[i].z );
	}

	for ( int i=0; i< std::ssize( g_DebugEdgePositions ); i++ )
	{
		bool bCrack = false;
		for ( int j=0; j< std::ssize( g_DebugEdgePositions ); j++ )
		{
			if ( NotQuiteEqual( g_DebugEdgePositions[i], g_DebugEdgePositions[j] ) )
			{
				bCrack = true;
			}
		}

		DevMsg( "%s E - %.15f, %.15f, %.15f\n", bCrack ? "*** " : "    ", g_DebugEdgePositions[i].x, g_DebugEdgePositions[i].y, g_DebugEdgePositions[i].z );
	}

	for ( int i=0; i< std::ssize( g_DebugInteriorPositions ); i++ )
	{
		bool bCrack = false;
		for ( int j=0; j< std::ssize( g_DebugInteriorPositions ); j++ )
		{
			if ( NotQuiteEqual( g_DebugInteriorPositions[i], g_DebugInteriorPositions[j] ) )
			{
				bCrack = true;
			}
		}

		DevMsg( "%s I - %.15f, %.15f, %.15f\n", bCrack ? "*** " : "    ", g_DebugInteriorPositions[i].x, g_DebugInteriorPositions[i].y, g_DebugInteriorPositions[i].z );
	}
}

#endif // _DEBUG

void GenerateWorldSpacePatches( float *pSubDBuff, int nNumPatches, unsigned short *pTopologyIndices, fltx4 *pWSVertices, bool bRegularPatch )
{
	CVProfScope _( "CStudioRender::GenerateWorldSpacePatches", 0, "SubD Rendering", false, BUDGETFLAG_OTHER );

	TopologyIndexStruct quad;
	unsigned short *nextPatchIndices = InitializeTopologyIndexStruct( quad, pTopologyIndices );

	set_ShowACCGeometryTangents(mat_tessellation_accgeometrytangents.GetBool());
	set_UseCornerTangents(mat_tessellation_cornertangents.GetBool());

	Vector4D Geo[16] [[gnu::aligned(16)]];
	Vector4D TanU[12] [[gnu::aligned(16)]];
	Vector4D TanV[12] [[gnu::aligned(16)]];

#ifdef _DEBUG
	if ( mat_tess_dump.GetBool() )
	{
		// Debug Arrays
		g_DebugCornerPositions.reserve( nNumPatches * 4 );
		g_DebugEdgePositions.reserve( nNumPatches * 8 );
		g_DebugInteriorPositions.reserve( nNumPatches * 4 );

		// Empty the arrays this time around
		g_DebugCornerPositions.clear();
		g_DebugEdgePositions.clear();
		g_DebugInteriorPositions.clear();
	}
#endif

	for( int p = 0; p < nNumPatches; p++ )
	{
		ComputeACCAllPatches( pWSVertices, &quad, Geo, TanU, TanV, bRegularPatch );

		for ( int i=0; i < 16; i++ )
		{
			pSubDBuff[ i * 3 + 0 ] = Geo[i].x;
			pSubDBuff[ i * 3 + 1 ] = Geo[i].y;
			pSubDBuff[ i * 3 + 2 ] = Geo[i].z;

		}

		for ( int i=0; i<12; i++ )
		{
			pSubDBuff[ i * 3 + 0 + 48 ] = TanU[ i ].x;
			pSubDBuff[ i * 3 + 1 + 48 ] = TanU[ i ].y;
			pSubDBuff[ i * 3 + 2 + 48 ] = TanU[ i ].z;
		}

		for ( int i=0; i<12; i++ )
		{
			pSubDBuff[ i * 3 + 0 + 84 ] = TanV[ i ].x;
			pSubDBuff[ i * 3 + 1 + 84 ] = TanV[ i ].y;
			pSubDBuff[ i * 3 + 2 + 84 ] = TanV[ i ].z;
		}

		pSubDBuff += 120; // 30 * sizeof( float )

		nextPatchIndices = InitializeTopologyIndexStruct( quad, nextPatchIndices );
	}

#ifdef _DEBUG
	if ( mat_tess_dump.GetBool() )
	{
		// These should be a particular size
		contract_assert( std::ssize( g_DebugCornerPositions ) == ( nNumPatches * 4 ) );
		contract_assert( std::ssize( g_DebugEdgePositions ) == ( nNumPatches * 8 ) );
		contract_assert( std::ssize( g_DebugInteriorPositions ) == ( nNumPatches * 4 ) );

		DumpDebugPositions();
		mat_tess_dump.SetValue( 0 );		// Turn back off
	}
#endif

}

//-----------------------------------------------------------------------------------
// Top level function for mapping a quad mesh to an array of Bicubic Bezier patches
//-----------------------------------------------------------------------------------
void CStudioRender::GenerateBicubicPatches( mstudiomesh_t* pmesh, studiomeshgroup_t* pGroup, bool bDoFlex )
{
  	contract_assert( 0 );
}


// Transform Tangent vector
static void R_TransformTangent( const Vector4D *pSrcTangentS, matrix3x4_t *pSkinMat, Vector4DAligned &tangentS )
{
	CVProfScope _( "R_TransformTangent", 0, "SubD Rendering", false, BUDGETFLAG_OTHER );

	tangentS.x = pSrcTangentS->x * (*pSkinMat)[0][0] + pSrcTangentS->y * (*pSkinMat)[0][1]	+ pSrcTangentS->z * (*pSkinMat)[0][2];
	tangentS.y = pSrcTangentS->x * (*pSkinMat)[1][0] + pSrcTangentS->y * (*pSkinMat)[1][1]	+ pSrcTangentS->z * (*pSkinMat)[1][2];
	tangentS.z = pSrcTangentS->x * (*pSkinMat)[2][0] + pSrcTangentS->y * (*pSkinMat)[2][1]	+ pSrcTangentS->z * (*pSkinMat)[2][2];
	tangentS.w = pSrcTangentS->w;
}

// Transforms per-vertex tangent vector, copies texture coordinates etc into dynamic VB
void CStudioRender::SoftwareProcessQuadMesh( mstudiomesh_t* pmesh, CMeshBuilder& meshBuilder, 
											 int numFaces, unsigned short* pGroupToMesh,
											 unsigned short *pTopologyIndices, bool bTangentSpace, bool bDoFlex )
{
	CVProfScope _( "CStudioRender::SoftwareProcessQuadMesh", 0, "SubD Rendering", false, BUDGETFLAG_OTHER );

	Vector4D *pStudioTangentS = nullptr;

	QuadTessVertex_t quadVertex [[gnu::aligned(16)]];

	// QuadTessVertex_t currently has the following map:
	// +-----------------------------------+
	// |  tanX  |  tanY  |  tanZ  | sBWrnk | <- Tangent in .xyz, Binormal sign flip bit plus wrinkle in .w
	// +-----------------------------------+
	// |  tcU0  |  tcV0  |  tcU1  |  tcV1  | <- Interior TC, Parametric V Edge TC
	// +-----------------------------------+
	// |  tcU2  |  tcV2  |  tcU3  |  tcV3  | <- Parametric U Edge TC, Corner TC
	// +-----------------------------------+

	quadVertex.m_vTangent.Init( 1.0f, 0.0f, 0.0f, 1.0f );

	matrix3x4_t *pSkinMat, matTemp [[gnu::aligned(16)]];

	contract_assert( numFaces > 0 );

	const mstudio_meshvertexdata_t *pVertData = pmesh->GetVertexData( m_pStudioHdr );
	contract_assert( pVertData );
	if ( !pVertData )
		return;

	mstudiovertex_t *pVertices = pVertData->Vertex( 0 );


	if ( bTangentSpace )
	{
		pStudioTangentS = pVertData->TangentS( 0 );
	}

	TopologyIndexStruct quad;
	unsigned short *nextPatchIndices = InitializeTopologyIndexStruct( quad, pTopologyIndices );

	for ( int i=0; i < numFaces; ++i )						// Run over faces
	{
		int patchCorner = 0;


		for ( int j=0; j < 4; ++j )							// Four verts per face
		{
			int idx = quad.oneRing[patchCorner];
			mstudiovertex_t &vert = pVertices[idx];

			if ( bTangentSpace )
			{
				pSkinMat = ComputeSkinMatrixSSE( vert.m_BoneWeights, m_PoseToWorld, matTemp );

				if ( bDoFlex && m_VertexCache.IsVertexFlexed( idx ) )
				{
					CachedPosNormTan_t* pFlexedVertex = m_VertexCache.GetFlexVertex( idx );
					R_TransformTangent( &(pFlexedVertex->m_TangentS), pSkinMat, *(Vector4DAligned*)&quadVertex.m_vTangent );
				}
				else // non-flexed case
				{
					R_TransformTangent( &pStudioTangentS[idx], pSkinMat, *(Vector4DAligned*)&quadVertex.m_vTangent );
					quadVertex.m_vTangent.w *= 2; // non-flexed vertex should have wrinkle of -2 or +2
				}
			}

			// Store 4 texcoords per quad corner
			quadVertex.m_vUV01.x = pVertices[ quad.vUV0[j] ].m_vecTexCoord.x;
			quadVertex.m_vUV01.y = pVertices[ quad.vUV0[j] ].m_vecTexCoord.y;
			quadVertex.m_vUV01.z = pVertices[ quad.vUV1[j] ].m_vecTexCoord.x;
			quadVertex.m_vUV01.w = pVertices[ quad.vUV1[j] ].m_vecTexCoord.y;
			quadVertex.m_vUV23.x = pVertices[ quad.vUV2[j] ].m_vecTexCoord.x;
			quadVertex.m_vUV23.y = pVertices[ quad.vUV2[j] ].m_vecTexCoord.y;
			quadVertex.m_vUV23.z = pVertices[ quad.vUV3[j] ].m_vecTexCoord.x;
			quadVertex.m_vUV23.w = pVertices[ quad.vUV3[j] ].m_vecTexCoord.y;

			meshBuilder.FastQuadVertexSSE( quadVertex );

			patchCorner += quad.vtx1RingSize[j];
		}

		nextPatchIndices = InitializeTopologyIndexStruct( quad, nextPatchIndices );
	}

	meshBuilder.FastAdvanceNVertices( numFaces * 4 );
}
