// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

import std;


import ks.core.basetypes;
import ks.core.color;
import ks.core.dbg;
import ks.core.platform;
import ks.core.threadtools;
import ks.core.types;
import ks.core.vprof;
import ks.core.wchartypes;
import ks.math.compressed_vector;
import ks.math.lightdesc;
import ks.math.mathlib;
import ks.math.polyhedron;
import ks.math.vector;
import ks.math.vector4d;
import ks.math.vmatrix;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.envelope;
import ks.tier1.exprevaluator;
import ks.tier1.functors;
import ks.tier1.iconvar;
import ks.tier1.interfaces;
import ks.tier1.memstack;
import ks.vstdlib.callqueue;
import ks.vstdlib.jobthread;

#include "tier2/tier2.h"
#include "bitmap/imageformat.h"
#include "bitmap/tgawriter.h"
#include "bitmap/tgaloader.h"
#include "datacache/idatacache.h"
#include "filesystem.h"
#include "pixelwriter.h"

#include "materialsystem_global.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imesh.h"
#include "materialsystem/IColorCorrection.h"

#include "imaterialinternal.h"
#include "imaterialsysteminternal.h"

#include "itextureinternal.h"


#include "cmatqueuedrendercontext.h"
#include "cmatqueuedmesh.h"
#include "cmaterialsystem.h" // @HACKHACK

ConVar mat_report_queue_status( "mat_report_queue_status", "0", FCVAR_MATERIAL_SYSTEM_THREAD );


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Used for caching off the memory pointers, and vertex/index data info for a
// CMatQueuedMesh.
//-----------------------------------------------------------------------------
class CCachedPerFrameMeshData : public ICachedPerFrameMeshData
{
public:
	CMatQueuedMesh::MST_MeshInfo_t m_meshInfo;

	CCachedPerFrameMeshData()
	{
		m_meshInfo.m_pMaterial = nullptr;
	}

	~CCachedPerFrameMeshData()
	{
	}
};

// ICachedPerFrameMeshData is only ever a CCachedPerFrameMeshData
void ICachedPerFrameMeshData::Free()
{
	delete static_cast<CCachedPerFrameMeshData *>( this );
}

//----------------------------------------------------------------------------
// Static members
//----------------------------------------------------------------------------
unsigned short CMatQueuedMesh::gm_ScratchIndexBuffer;


//----------------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------------
CMatQueuedMesh::CMatQueuedMesh( CMatQueuedRenderContext *pOwner, IMatRenderContextInternal *pHardwareContext, bool bFlexMesh )
 :	IMesh( MeshKind::QueuedMesh ),
	m_pLateBoundMesh( &m_pMSTActualMesh ),
	m_pOwner( pOwner ), 
	m_pCallQueue( pOwner->GetCallQueueInternal() ),
	m_pHardwareContext( pHardwareContext ),
	m_pVertexData( nullptr ),
	m_pIndexData( nullptr ),
	m_nVerts( 0 ),
	m_nIndices( 0 ),
	m_VertexSize( 0 ),
	m_Type( MATERIAL_TRIANGLES ),
	m_pVertexOverride( nullptr ),
	m_pIndexOverride ( nullptr ),
	m_pMSTActualMesh( nullptr ),
	m_nMSTActualVertexOffsetInBytes( 0 ),
	m_VertexFormat( 0 ),
	m_bFlexMesh( bFlexMesh ),
	m_bCanSetAuxMeshes( false ),
	m_nFlags( 0 ),
	m_bExternalIB( false ),
	m_bExternalVB( false )
{
}


//----------------------------------------------------------------------------
//
// Methods that run in the material system thread
//
//----------------------------------------------------------------------------


//----------------------------------------------------------------------------
// Gets the external mesh + points it at the already-allocated data
//----------------------------------------------------------------------------
IMesh *CMatQueuedMesh::MST_SetupExternalMesh( const MST_MeshInfo_t &info )
{
	return nullptr;
}


//-----------------------------------------------------------------------------
// Copies queued vertex buffer data into the actual dynamic buffer
//-----------------------------------------------------------------------------
[[gnu::always_inline]] inline void CMatQueuedMesh::MST_CopyDynamicVB( const MeshDesc_t &desc, const uint8 *pVertexData, std::size_t nSizeInBytes )
{
	void *pDest;
	if ( desc.m_VertexSize_Position != 0 )
	{
		pDest = desc.m_pPosition;
	}
	else
	{
#define FindMin( desc, pCurrent, tag )	( ( desc.m_VertexSize_##tag != 0 ) ? std::min( (void *)(pCurrent), (void *)(desc.m_p##tag) )  : (void *)(pCurrent) )

		pDest = (void *)((byte *)~0);

		pDest = FindMin( desc, pDest, BoneWeight );
		pDest = FindMin( desc, pDest, BoneMatrixIndex );
		pDest = FindMin( desc, pDest, Normal );
		pDest = FindMin( desc, pDest, Color );
		pDest = FindMin( desc, pDest, Specular );
		pDest = FindMin( desc, pDest, TangentS );
		pDest = FindMin( desc, pDest, TangentT );
		pDest = FindMin( desc, pDest, Wrinkle );

		for ( int i = 0; i < VERTEX_MAX_TEXTURE_COORDINATES; i++ )
		{
			if ( desc.m_VertexSize_TexCoord[i] && desc.m_pTexCoord[i] < pDest )
			{
				pDest = desc.m_pTexCoord[i];
			}
		}

#undef FindMin
	}

	contract_assert( pDest );
	if ( pDest )
	{
		std::memcpy( (byte *)pDest, pVertexData, nSizeInBytes );
	}
}


//-----------------------------------------------------------------------------
// Copies queued index buffer data into the actual dynamic buffer
//-----------------------------------------------------------------------------
[[gnu::always_inline]] inline void CMatQueuedMesh::MST_CopyDynamicIB( const MeshDesc_t &desc, const uint16 *pIndexData, int nIndices )
{
	if ( !desc.m_nFirstVertex )
	{
		// AssertMsg(desc.m_pIndices & 0x03 == 0,"desc.m_pIndices is misaligned in CMatQueuedMesh::ExecuteDefferedBuild\n");
		std::memcpy( (byte *)desc.m_pIndices, (const byte *)pIndexData, nIndices * sizeof(*pIndexData) );
		return;
	}

	uint16 tempIndices[16];

	int i = 0;
	if ( (std::size_t)desc.m_pIndices % 4 == 2 )
	{
		desc.m_pIndices[i] = pIndexData[i] + desc.m_nFirstVertex;
		i++;
	}
	while ( i < nIndices )
	{
		int nToCopy = std::min<std::common_type_t<long unsigned int, int>>( std::size( tempIndices ), nIndices - i );
		for ( int j = 0; j < nToCopy; j++ )
		{
			tempIndices[j] = pIndexData[i+j] + desc.m_nFirstVertex;
		}
		std::memcpy( (byte *)(desc.m_pIndices + i), (byte *)tempIndices, nToCopy * sizeof(uint16)  );
		i += nToCopy;
	}
}


//-----------------------------------------------------------------------------
// Gets the actual dynamic buffer (if necessary), and copies queued data into it
//-----------------------------------------------------------------------------
IMesh *CMatQueuedMesh::MST_SetupDynamicMesh( const MST_MeshInfo_t &info, IMesh *pExternalMesh )
{
	IMesh *pVertexOverride = info.m_pVertexOverride;
	IMesh *pIndexOverride = info.m_pIndexOverride;

	int nVerticesToLock = info.m_nVertexCount;
	int nIndicesToLock = info.m_nIndexCount;

	// Gets the dynamic mesh
	IMesh *pDynamicMesh;
	if ( !m_bFlexMesh )
	{
		pDynamicMesh = m_pHardwareContext->GetDynamicMeshEx( info.m_VertexFormat, ( ( info.m_nFlags & MQM_BUFFERED ) != 0 ), pVertexOverride, pIndexOverride, info.m_pMaterial );
	}
	else
	{
		pDynamicMesh = m_pHardwareContext->GetFlexMesh();
	}

	// Copies the buffers into the actual dynamic mesh
	if ( !nVerticesToLock && !nIndicesToLock )
		return pDynamicMesh;

	// Primitive type doesn't get sent down until the draw call.  
	// Because we lock based upon the primitive type (whether we're points or instanced quads), we
	// don't have a valid primitive type for our dynamic mesh, so set it here.  This will get
	// redundantly set on the draw call.
	pDynamicMesh->SetPrimitiveType( info.m_nPrimitiveType );

	MeshDesc_t desc;
	pDynamicMesh->LockMesh( nVerticesToLock, nIndicesToLock, desc );
	m_nMSTActualVertexOffsetInBytes = desc.m_nFirstVertex * desc.m_ActualVertexSize;
	if ( nVerticesToLock && desc.m_ActualVertexSize ) // if !desc.m_ActualVertexSize, device lost
	{
		MST_CopyDynamicVB( desc, info.m_pVertexData, info.m_nVertexCount * info.m_nVertexSizeInBytes );
	}

	if ( nIndicesToLock && ( info.m_pIndexData != &gm_ScratchIndexBuffer ) && desc.m_nIndexSize )
	{
		MST_CopyDynamicIB( desc, info.m_pIndexData, info.m_nIndexCount );
	}

	pDynamicMesh->UnlockMesh( nVerticesToLock, nIndicesToLock, desc );
	return pDynamicMesh;
}


//-----------------------------------------------------------------------------
// Main entry point for setting up the dynamic buffers on the material thread
//-----------------------------------------------------------------------------
void CMatQueuedMesh::MST_BuildDynamicBuffers( const MST_MeshInfo_t &info )
{
	// Think about the 4 cases each for VB and IB:
	// 1) External, 2) Dynamic, 3) Override, 4) Empty
	// We identify them this way:
	//		if info.m_pVertex/IndexOverride, it's an override
	//		else if info.m_nVertex/IndexCount == 0 it's empty
	//		else if info.m_bExternalIB/VB, it's external
	//		else it's dynamic
	// In general, any time we encounter an external buffer, we always have to make one.
	// Any time we encounter a dynamic buffer, we have to make one.
	// The strange cases are 
	//	* one is ext, the other is dynamic. 
	//		In that case, we make both and pass the external as an override to the dynamic.
	//	* Both are overrides, or one is an override and the other is empty
	//		In that case, we make only a dynamic and use the override(s)
	//	* Both are empty
	//		This is an illegal case.

	// We need data at least *somewhere* for this to make sense.
	contract_assert( info.m_pVertexOverride || info.m_nVertexCount || info.m_pIndexOverride || info.m_nIndexCount );

	// First, see if we need any external buffers. Then see if we need
	// dynamic buffers also
	IMesh *pExternalMesh = MST_SetupExternalMesh( info );
	m_pMSTActualMesh = MST_SetupDynamicMesh( info, pExternalMesh );

	// At this point, we're done with the data. We can free it.
	if ( info.m_pVertexData && !info.m_bExternalVB )
	{
		m_pOwner->FreeVertices( info.m_pVertexData, info.m_nVertexCount, info.m_nVertexSizeInBytes );
	}
	if ( info.m_pIndexData && !info.m_bExternalIB && ( info.m_pIndexData != &gm_ScratchIndexBuffer ) )
	{
		m_pOwner->FreeIndices( (uint8*)info.m_pIndexData, info.m_nIndexCount, sizeof(uint16) );
	}

	if ( info.m_pMaterial )
	{
		info.m_pMaterial->DecrementReferenceCount();
	}
}


//-----------------------------------------------------------------------------
// Main entry point for calling draw
//-----------------------------------------------------------------------------
void CMatQueuedMesh::MST_Draw( const MST_DrawInfo_t &info )
{
	m_pMSTActualMesh->SetPrimitiveType( info.m_Type );
	m_pMSTActualMesh->Draw( info.m_nFirstIndex, info.m_nIndexCount );
}


//-----------------------------------------------------------------------------
// Main entry point for calling draw modulated
//-----------------------------------------------------------------------------
void CMatQueuedMesh::MST_DrawModulated( const MST_DrawModulatedInfo_t &info )
{
	m_pMSTActualMesh->SetPrimitiveType( info.m_Type );
	m_pMSTActualMesh->DrawModulated( info.m_vecDiffuseModulation, info.m_nFirstIndex, info.m_nIndexCount );
}


//-----------------------------------------------------------------------------
//
// Methods called by the main (client) thread
//
//-----------------------------------------------------------------------------
bool CMatQueuedMesh::OnGetDynamicMesh( VertexFormat_t vertexFormat, unsigned flags, IMesh* pVertexOverride, IMesh* pIndexOverride, IMaterialInternal *pMaterial, int nHWSkinBoneCount )
{
	FreeBuffers();

	m_pVertexOverride = pVertexOverride;
	m_pIndexOverride = pIndexOverride;
	m_nFlags = flags;

	if ( !m_bFlexMesh )
	{
		if ( pVertexOverride )
		{
			m_VertexFormat = pVertexOverride->GetVertexFormat();
		}
		else
		{
			// Remove VERTEX_FORMAT_COMPRESSED from the material's format (dynamic meshes don't
			// support compression, and all materials should support uncompressed verts too)
			VertexFormat_t materialFormat = pMaterial->GetVertexFormat() & ~VERTEX_FORMAT_COMPRESSED;
			m_VertexFormat = ( vertexFormat != 0 ) ? vertexFormat : materialFormat;

			if ( vertexFormat != 0 )
			{
				int nVertexFormatBoneWeights = NumBoneWeights( vertexFormat );
				if ( nHWSkinBoneCount < nVertexFormatBoneWeights )
				{
					nHWSkinBoneCount = nVertexFormatBoneWeights;
				}
			}
			// Force the requested number of bone weights
			m_VertexFormat &= ~VERTEX_BONE_WEIGHT_MASK;
			m_VertexFormat |= VERTEX_BONEWEIGHT( nHWSkinBoneCount );
			if ( nHWSkinBoneCount > 0 )
			{
				m_VertexFormat |= VERTEX_BONE_INDEX;
			}
		}
	}
	else
	{
		m_VertexFormat = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_USE_EXACT_FORMAT;
		if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 92 )
		{
			m_VertexFormat |= VERTEX_WRINKLE;
		}
	}

	m_VertexSize = g_pShaderAPI->VertexFormatSize( m_VertexFormat );
	return true;
}

int CMatQueuedMesh::VertexCount() const
{
	return m_VertexSize ? m_nVerts : 0;
}

int CMatQueuedMesh::IndexCount() const
{
	return m_nIndices;
}

void CMatQueuedMesh::SetPrimitiveType( MaterialPrimitiveType_t type )
{
	// NOTE: Have to just hold onto the type here. We might not actually
	// have our meshes set up in the material system thread at this point
	// because we don't know if it's an external or a dynamic mesh
	// until unlock.
	m_Type = type;
//		m_pCallQueue->QueueCall( m_pLateBoundMesh, &IMesh::SetPrimitiveType, type );
}

void CMatQueuedMesh::LockMesh( int numVerts, int numIndices, MeshDesc_t& desc, MeshBuffersAllocationSettings_t *pSettings )
{
	if ( m_pVertexOverride )
	{
		numVerts = 0;
	}
	if ( m_pIndexOverride )
	{
		numIndices = 0;
	}

	m_nVerts = numVerts;
	m_nIndices = numIndices;

	if( numVerts > 0 )
	{
		contract_assert( m_VertexSize );
		contract_assert( !m_pVertexData );
		m_pVertexData = (byte *)m_pOwner->AllocVertices( numVerts, m_VertexSize, &m_bExternalVB );
		contract_assert( (uintp)m_pVertexData % 16 == 0 );

		// Compute the vertex index..
		desc.m_nFirstVertex = 0;
		static_cast< VertexDesc_t* >( &desc )->m_nOffset = 0;
		// Set up the mesh descriptor
		g_pShaderAPI->ComputeVertexDescription( m_pVertexData, m_VertexFormat, desc );
	}
	else
	{
		m_bExternalVB = false;
		desc.m_nFirstVertex = 0;
		static_cast< VertexDesc_t* >( &desc )->m_nOffset = 0;
		// Set up the mesh descriptor
		g_pShaderAPI->ComputeVertexDescription( 0, 0, desc );
	}

	if ( m_Type != MATERIAL_POINTS && numIndices > 0 )
	{
		contract_assert( !m_pIndexData );
		m_pIndexData = (uint16*)m_pOwner->AllocIndices( numIndices, sizeof(uint16), &m_bExternalIB );
		desc.m_pIndices = m_pIndexData;
		desc.m_nIndexSize = 1;
		desc.m_nFirstIndex = 0;
		static_cast< IndexDesc_t* >( &desc )->m_nOffset = 0;
	}
	else
	{
		m_bExternalIB = false;
		desc.m_pIndices = &gm_ScratchIndexBuffer;
		desc.m_nIndexSize = 0;
		desc.m_nFirstIndex = 0;
		static_cast< IndexDesc_t* >( &desc )->m_nOffset = 0;
	}
}

void CMatQueuedMesh::UnlockMesh( int numVerts, int numIndices, MeshDesc_t& desc )
{
	if ( m_pVertexData && numVerts < m_nVerts )
	{
		m_pVertexData = m_pOwner->ReallocVertices( m_pVertexData, m_nVerts, numVerts, m_VertexSize, m_bExternalVB );
		m_nVerts = numVerts;
	}

	if ( m_pIndexData && numIndices < m_nIndices )
	{
		m_pIndexData = (uint16*)m_pOwner->ReallocIndices( (byte*)m_pIndexData, m_nIndices, numIndices, sizeof(uint16), m_bExternalIB );
		m_nIndices = numIndices;
	}

	// Once we've unlocked the mesh, fire it off to the materialsystem thread.
	// The rules of mesh locking is that once Lock() is called, there's no
	// guarantee that any of the previous mesh still exists. However, since
	// flex meshes + dynamic meshes are separate concepts, the *flex* mesh can
	// remain if the dynamic mesh is locked, hence the queued mat render context
	// maintains two queued meshes, one for the flex, one for normal.
	QueueBuild( );
}


//-----------------------------------------------------------------------------
// Queues the buffers up to be 
//-----------------------------------------------------------------------------
void CMatQueuedMesh::QueueBuild( )
{
	if ( !m_pVertexOverride && !m_nVerts && !m_pIndexOverride && !m_nIndices )
		return;

	MST_MeshInfo_t info;
	info.m_pMaterial = m_pOwner->GetCurrentMaterialInternal();
	info.m_nFlags = 0; //m_nFlags;

	info.m_pVertexData = m_pVertexData;
	info.m_nVertexCount = m_nVerts;
	info.m_VertexFormat = m_VertexFormat;
	info.m_nVertexSizeInBytes = m_VertexSize;
	info.m_pVertexOverride = m_pVertexOverride;
	info.m_bExternalVB = m_bExternalVB;

	info.m_pIndexData = m_pIndexData;
	info.m_nIndexCount = m_nIndices;
	info.m_bExternalIB = m_bExternalIB;
	info.m_pIndexOverride = m_pIndexOverride;
	info.m_nPrimitiveType = m_Type;
	contract_assert( info.m_pIndexData || ( m_nIndices == 0 ) );
					  
	if ( info.m_pMaterial )
	{
		info.m_pMaterial->IncrementReferenceCount();
	}

	m_pCallQueue->QueueCall( this, &CMatQueuedMesh::MST_BuildDynamicBuffers, info );
	m_bCanSetAuxMeshes = true;
}


//-----------------------------------------------------------------------------
// Associates flex/color meshes with the dynamic mesh.
//-----------------------------------------------------------------------------
void CMatQueuedMesh::SetColorMesh( IMesh *pColorMesh, int nVertexOffset )
{
	// This only works on the main dynamic mesh
	contract_assert( !m_bFlexMesh );

	// This cannot be called in between the call to GetDynamicMesh + UnlockMesh;
	// the late bound mesh is in an indeterminant state during that point
	contract_assert( m_bCanSetAuxMeshes );
	m_pCallQueue->QueueCall( m_pLateBoundMesh, &IMesh::SetColorMesh, pColorMesh, nVertexOffset );
}

void CMatQueuedMesh::SetFlexMesh( IMesh *pMesh, int nVertexOffset )
{
	// This only works on the main dynamic mesh
	contract_assert( !m_bFlexMesh );

	// This cannot be called in between the call to GetDynamicMesh + UnlockMesh;
	// the late bound mesh is in an indeterminant state during that point
	contract_assert( m_bCanSetAuxMeshes );
	m_pCallQueue->QueueCall( m_pLateBoundMesh, &IMesh::SetFlexMesh, pMesh, nVertexOffset );
}

void CMatQueuedMesh::DisableFlexMesh()
{
	// This only works on the color mesh
	contract_assert( !m_bFlexMesh );

	// This cannot be called in between the call to GetDynamicMesh + UnlockMesh;
	// the late bound mesh is in an indeterminant state during that point
	contract_assert( m_bCanSetAuxMeshes );
	m_pCallQueue->QueueCall( m_pLateBoundMesh, &IMesh::DisableFlexMesh );
}

void CMatQueuedMesh::Draw( int firstIndex, int numIndices )
{
	if ( ( !m_nVerts && !m_pVertexOverride ) && ( !m_nIndices && !m_pIndexOverride ) )
	{
		MarkAsDrawn();
		return;
	}

	if ( ( m_Type == MATERIAL_INSTANCED_QUADS ) || ( m_Type == MATERIAL_POINTS ) )
	{
		if ( !m_nVerts && !m_pVertexOverride )
		{
			MarkAsDrawn();
			return;
		}
	}

	MST_DrawInfo_t info;
	info.m_Type = m_Type;
	info.m_nFirstIndex = firstIndex;
	info.m_nIndexCount = numIndices;

	m_pCallQueue->QueueCall( this, &CMatQueuedMesh::MST_Draw, info );
}

void CMatQueuedMesh::DrawModulated( const Vector4D &vecDiffuseModulation, int firstIndex, int numIndices )
{
	if ( ( !m_nVerts && !m_pVertexOverride ) && ( !m_nIndices && !m_pIndexOverride ) )
	{
		MarkAsDrawn();
		return;
	}

	if ( ( m_Type == MATERIAL_INSTANCED_QUADS ) || ( m_Type == MATERIAL_POINTS ) )
	{
		if ( !m_nVerts && !m_pVertexOverride )
		{
			MarkAsDrawn();
			return;
		}
	}

	MST_DrawModulatedInfo_t info;
	info.m_Type = m_Type;
	info.m_vecDiffuseModulation = vecDiffuseModulation;
	info.m_nFirstIndex = firstIndex;
	info.m_nIndexCount = numIndices;

	m_pCallQueue->QueueCall( this, &CMatQueuedMesh::MST_DrawModulated, info );
}

ICachedPerFrameMeshData *CMatQueuedMesh::GetCachedPerFrameMeshData()
{
	// Short-circuit if we have no vertices
	if ( m_nVerts == 0 )
		return nullptr;

	CCachedPerFrameMeshData *pNewPerFrameData = new CCachedPerFrameMeshData();

	MST_MeshInfo_t &info = pNewPerFrameData->m_meshInfo;
	info.m_pMaterial = m_pOwner->GetCurrentMaterialInternal();
	info.m_nFlags = 0;

	info.m_pVertexData = m_pVertexData;
	info.m_nVertexCount = m_nVerts;
	info.m_VertexFormat = m_VertexFormat;
	info.m_nVertexSizeInBytes = m_VertexSize;
	info.m_pVertexOverride = m_pVertexOverride;
	info.m_bExternalVB = m_bExternalVB;

	info.m_pIndexData = m_pIndexData;
	info.m_nIndexCount = m_nIndices;
	info.m_bExternalIB = m_bExternalIB;
	info.m_pIndexOverride = m_pIndexOverride;
	info.m_nPrimitiveType = m_Type;
	contract_assert( info.m_pIndexData || ( m_nIndices == 0 ) );

	return pNewPerFrameData;
}

void CMatQueuedMesh::ReconstructFromCachedPerFrameMeshData( ICachedPerFrameMeshData *pData )
{
	CCachedPerFrameMeshData *pCachedData = (CCachedPerFrameMeshData*)pData;

	m_pVertexData = pCachedData->m_meshInfo.m_pVertexData;
	m_nVerts = pCachedData->m_meshInfo.m_nVertexCount;
	m_VertexFormat = pCachedData->m_meshInfo.m_VertexFormat;
	m_pVertexOverride = pCachedData->m_meshInfo.m_pVertexOverride;
	m_bExternalVB = pCachedData->m_meshInfo.m_bExternalVB;

	m_pIndexData = pCachedData->m_meshInfo.m_pIndexData;
	m_nIndices = pCachedData->m_meshInfo.m_nIndexCount;
	m_bExternalIB = pCachedData->m_meshInfo.m_bExternalIB;
	m_pIndexOverride = pCachedData->m_meshInfo.m_pIndexOverride;
	m_Type = pCachedData->m_meshInfo.m_nPrimitiveType;

	if( pCachedData->m_meshInfo.m_pVertexOverride || pCachedData->m_meshInfo.m_nVertexCount || pCachedData->m_meshInfo.m_pIndexOverride || pCachedData->m_meshInfo.m_nIndexCount )
	{
		if ( pCachedData->m_meshInfo.m_pMaterial )
		{
			pCachedData->m_meshInfo.m_pMaterial->IncrementReferenceCount();
		}

		m_pCallQueue->QueueCall( this, &CMatQueuedMesh::MST_BuildDynamicBuffers, pCachedData->m_meshInfo );
		m_bCanSetAuxMeshes = true;
	}
}

void CMatQueuedMesh::MarkAsDrawn()
{
	if ( m_bCanSetAuxMeshes )
	{
		m_pCallQueue->QueueCall( m_pLateBoundMesh, &IMesh::MarkAsDrawn );
	}
	FreeBuffers();
}

void CMatQueuedMesh::FreeBuffers()
{
	if ( m_pIndexData && ( m_pIndexData != &gm_ScratchIndexBuffer ) )
	{
		m_pOwner->FreeIndices( (byte*)m_pIndexData, m_nIndices, sizeof(uint16) );
	}
	m_nIndices = 0;
	m_pIndexData = nullptr;

	if ( m_pVertexData )
	{
		m_pOwner->FreeVertices( m_pVertexData, m_nVerts, m_VertexSize );
	}

	m_nVerts = 0;
	m_VertexFormat = 0;
	m_VertexSize = 0;
	m_pVertexData = nullptr;
	m_bCanSetAuxMeshes = false;
	m_nFlags = 0;
	m_bExternalIB = false;
	m_bExternalVB = false;
}

VertexFormat_t CMatQueuedMesh::GetVertexFormat() const
{
	return m_VertexFormat;
}


//-----------------------------------------------------------------------------
// Queued index buffer
//-----------------------------------------------------------------------------
//----------------------------------------------------------------------------
// Constructor, destructor
//----------------------------------------------------------------------------
CMatQueuedIndexBuffer::CMatQueuedIndexBuffer( CMatQueuedRenderContext *pOwner, IMatRenderContextInternal *pHardwareContext ) :
	IIndexBuffer( MeshKind::QueuedIndexBuffer ),
	m_pOwner( pOwner ), 
	m_pCallQueue( pOwner->GetCallQueueInternal() ),
	m_pHardwareContext( pHardwareContext ),
	m_pIndexData( nullptr ),
	m_nIndices( 0 ),
	m_nIndexFormat( MATERIAL_INDEX_FORMAT_UNKNOWN ),
	m_nRTStartIndex( -1 ),
	m_pRTDynamicIndexBuffer( 0 ),
	m_bExternalIB( false )
{
}

CMatQueuedIndexBuffer::~CMatQueuedIndexBuffer()
{
	if ( m_pIndexData )
	{
		FreeIndexData( m_nIndices, m_nIndexFormat, m_pIndexData );
		m_pIndexData = nullptr;
		m_nIndices = 0;
	}
}

const void *CMatQueuedIndexBuffer::GetIndexData() const
{ 
	return m_pIndexData; 
}

int CMatQueuedIndexBuffer::IndexCount() const
{
	return m_nIndices;
}

MaterialIndexFormat_t CMatQueuedIndexBuffer::IndexFormat() const
{
	return m_nIndexFormat;
}

bool CMatQueuedIndexBuffer::IsDynamic() const
{
	// Queued index buffers are only interesting for dynamic index buffers
	// static ones need not use this
	return true;
}

void CMatQueuedIndexBuffer::BeginCastBuffer( MaterialIndexFormat_t format )
{
	// Recasting this buffer has the effect of causing us to not need this index data any more
	ReleaseBuffer();
	m_nIndexFormat = format;
}

void CMatQueuedIndexBuffer::EndCastBuffer()
{
	ReleaseBuffer();
	m_nIndexFormat = MATERIAL_INDEX_FORMAT_UNKNOWN;
}

// Returns the number of indices that can still be written into the buffer
int CMatQueuedIndexBuffer::GetRoomRemaining() const
{
	return m_pOwner->GetMaxIndicesToRender() - m_nIndices;
}

int CMatQueuedIndexBuffer::RT_GetIndexStart() const
{
	return m_nRTStartIndex;
}

IIndexBuffer* CMatQueuedIndexBuffer::RT_GetDynamicIndexBuffer()
{
	return m_pRTDynamicIndexBuffer;
}


void CMatQueuedIndexBuffer::FreeIndexData( int nIndexCount, MaterialIndexFormat_t fmt, void *pIndexData )
{
	if ( pIndexData )
	{
		contract_assert( fmt != MATERIAL_INDEX_FORMAT_UNKNOWN );
		int nIndexSize = ( fmt == MATERIAL_INDEX_FORMAT_16BIT ) ? sizeof(uint16) : sizeof(uint32);
		m_pOwner->FreeIndices( (byte*)pIndexData, nIndexCount, nIndexSize );
	}
}

void CMatQueuedIndexBuffer::RT_CopyIndexData( int nIndexCount, MaterialIndexFormat_t fmt, void *pIndexData, bool bIsExternal )
{

	IndexDesc_t desc;
	contract_assert( !m_pRTDynamicIndexBuffer );
	m_pRTDynamicIndexBuffer = m_pHardwareContext->GetDynamicIndexBuffer();
	if ( !m_pRTDynamicIndexBuffer->Lock( nIndexCount, false, desc ) )
	{
		m_pRTDynamicIndexBuffer = nullptr;
		m_nRTStartIndex = -1;
		return;
	}

	int nIndexSize = sizeof( uint16 ) * desc.m_nIndexSize;
	m_nRTStartIndex = desc.m_nOffset / nIndexSize;
	if ( pIndexData && desc.m_nIndexSize )
	{
		std::memcpy( (byte *)desc.m_pIndices, (byte *)pIndexData, nIndexCount * nIndexSize );
	}
	m_pRTDynamicIndexBuffer->Unlock( nIndexCount, desc );
}

void CMatQueuedIndexBuffer::RT_FreeIndexData( int nIndexCount, MaterialIndexFormat_t fmt, void *pIndexData )
{
	FreeIndexData( nIndexCount, fmt, pIndexData );
	m_pRTDynamicIndexBuffer = nullptr;
	m_nRTStartIndex = -1;
}

void CMatQueuedIndexBuffer::ReleaseBuffer()
{
	if ( m_pIndexData )
	{
		m_pCallQueue->QueueCall( this, &CMatQueuedIndexBuffer::RT_FreeIndexData, m_nIndices, m_nIndexFormat, m_pIndexData );
		m_pIndexData = 0;
		m_nIndices = 0;
		m_bExternalIB = false;
	}
}

bool CMatQueuedIndexBuffer::Lock( int nMaxIndexCount, bool bAppend, IndexDesc_t &desc )
{
	// Locking this buffer has the effect of causing us to not need this index data any more
	ReleaseBuffer();

	// Append mode is not supported. We need to kill it altogether.
	if ( bAppend )
		return false;

	m_nIndices = nMaxIndexCount;
	int nIndexSize = ( m_nIndexFormat == MATERIAL_INDEX_FORMAT_16BIT ) ? sizeof(uint16) : sizeof(uint32);
	m_pIndexData = m_pOwner->AllocIndices( nMaxIndexCount, nIndexSize, &m_bExternalIB );
	desc.m_pIndices = (uint16*)m_pIndexData;
	desc.m_nIndexSize = nIndexSize >> 1;
	desc.m_nFirstIndex = 0;
	desc.m_nOffset = 0;
	return ( m_pIndexData != 0 );
}

void CMatQueuedIndexBuffer::Unlock( int nWrittenIndexCount, IndexDesc_t &desc )
{
	if ( m_pIndexData && nWrittenIndexCount < m_nIndices )
	{
		m_pIndexData = m_pOwner->ReallocIndices( (byte*)m_pIndexData, m_nIndices, nWrittenIndexCount, desc.m_nIndexSize * sizeof(uint16), m_bExternalIB );
	}
	m_nIndices = nWrittenIndexCount;

	// Unlocking this buffer has the effect of queuing a call to 
	// write these indices into the dynamic indexbuffer, caching off the
	// base index it was written into
	if ( m_pIndexData )
	{
		m_pCallQueue->QueueCall( this, &CMatQueuedIndexBuffer::RT_CopyIndexData, m_nIndices, m_nIndexFormat, m_pIndexData, m_bExternalIB );
	}
}

void CMatQueuedIndexBuffer::ModifyBegin( bool bReadOnly, int nFirstIndex, int nIndexCount, IndexDesc_t& desc )
{
	CannotSupport();
}

void CMatQueuedIndexBuffer::ModifyEnd( IndexDesc_t& desc )
{
	CannotSupport();
}

void CMatQueuedIndexBuffer::Spew( int nIndexCount, const IndexDesc_t &desc  )
{
}

void CMatQueuedIndexBuffer::ValidateData( int nIndexCount, const IndexDesc_t &desc )
{
}



//-----------------------------------------------------------------------------
//
// MatQueuedRenderContext starts here
//
//-----------------------------------------------------------------------------

void AllocateScratchRSXMemory()
{
}

void CMatQueuedRenderContext::Init( CMaterialSystem *pMaterialSystem, IMatRenderContextInternal *pHardwareContext )
{
	BaseClass::Init();

	m_pMaterialSystem = pMaterialSystem;
	m_pHardwareContext = pHardwareContext;

	m_pQueuedMesh = new CMatQueuedMesh( this, pHardwareContext, false );
	m_pQueuedFlexMesh = new CMatQueuedMesh( this, pHardwareContext, true );
	m_pQueuedIndexBuffer = new CMatQueuedIndexBuffer( this, pHardwareContext );


	unsigned int vertSize	= 16 * 1024 * 1024;
	unsigned int indSize	= 16 * 1024 * 1024;

	m_Vertices.Init( "CMatQueuedRenderContext::m_Vertices", vertSize, 128 * 1024 );
	m_Indices.Init( "CMatQueuedRenderContext::m_Indices",  indSize, 128 * 1024 );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::Shutdown()
{
	if ( !m_pHardwareContext )
		return;

	contract_assert( !m_pCurrentMaterial );

	delete m_pQueuedMesh;
	delete m_pQueuedFlexMesh;
	delete m_pQueuedIndexBuffer;
	m_pMaterialSystem = nullptr;
	m_pHardwareContext = nullptr;
	m_pQueuedMesh = nullptr;
	m_pQueuedFlexMesh = nullptr;
	m_pQueuedIndexBuffer = nullptr;

	m_Vertices.Term();
	m_Indices.Term();

	BaseClass::Shutdown();
	contract_assert( m_queue.Count() == 0 );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::CompactMemory()
{
	BaseClass::CompactMemory();

	m_Vertices.FreeAll();
	m_Indices.FreeAll();
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::CycleDynamicBuffers( )
{
}

void CMatQueuedRenderContext::BeginQueue( IMatRenderContextInternal *pInitialState )
{
	if ( !pInitialState )
	{
		pInitialState = m_pHardwareContext;
	}

	CycleDynamicBuffers();

	CMatRenderContextBase::InitializeFrom( static_cast<CMatRenderContextBase *>( pInitialState ) );
	g_pShaderAPI->GetBackBufferDimensions( m_WidthBackBuffer, m_HeightBackBuffer );
	m_FogMode = pInitialState->GetFogMode();
	m_nBoneCount = pInitialState->GetCurrentNumBones();
	pInitialState->GetFogDistances( &m_flFogStart, &m_flFogEnd, &m_flFogZ );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::EndQueue( bool bCallQueued )
{
	if ( bCallQueued )
	{
		CallQueued();
	}
	int i;

	if ( m_pCurrentMaterial )
	{
		m_pCurrentMaterial = nullptr;
	}

	if ( m_pUserDefinedLightmap )
	{
		m_pUserDefinedLightmap = nullptr;
	}

	if ( m_pLocalCubemapTexture )
	{
		m_pLocalCubemapTexture = nullptr;
	}

	for ( i = 0; i < MAX_FB_TEXTURES; i++ )
	{
		if ( m_pCurrentFrameBufferCopyTexture[i] )
		{
			m_pCurrentFrameBufferCopyTexture[i] = nullptr;
		}
	}

	for ( i = 0; i < std::ssize( m_RenderTargetStack ); i++ )
	{
		for ( int j = 0; j < MAX_RENDER_TARGETS; j++ )
		{
			if ( m_RenderTargetStack[i].m_pRenderTargets[j] )
			{
				m_RenderTargetStack[i].m_pRenderTargets[j] = nullptr;
			}
		}
	}

	m_RenderTargetStack.clear();

	m_ScissorRectStack.clear();
}


void CMatQueuedRenderContext::Bind( IMaterial *iMaterial, void *proxyData )
{
	if ( !iMaterial )
	{
		if( !g_pErrorMaterial )
			return;
		iMaterial = static_cast<IMaterialInternal *>( g_pErrorMaterial );
	}
	else
	{
		iMaterial = static_cast< IMaterialInternal*>( iMaterial )->GetRealTimeVersion(); //always work with the real time versions of materials internally
	}

	m_pCurrentMaterial = static_cast< IMaterialInternal*>( iMaterial );
	m_pCurrentProxyData = proxyData;

	if ( !m_pCurrentMaterial->HasQueueFriendlyProxies() )
	{
		// We've always gotta call the bind proxy (assuming there is one)
		// so we can copy off the material vars at this point.
		// However in case the material must have proxies bound on QMS then we don't call bind proxies
		// now and rely on queued material bind to setup proxies on QMS.
		m_pCurrentMaterial->CallBindProxy( proxyData, &m_queue );
	}

	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::Bind, iMaterial, proxyData );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::BeginRender()
{
	if ( ++m_iRenderDepth == 1 )
	{
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::BeginRender );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::EndRender()
{
	// This can fail if someone holds a render context pointer over a video mode change.  Find it if you hit this.
	contract_assert( m_pHardwareContext );
	if ( --m_iRenderDepth == 0 )
	{
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::EndRender );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::CallQueued( bool bTermAfterCall )
{
	if ( mat_report_queue_status.GetBool() )
	{
		Msg( "%d calls queued for %d bytes in parameters and overhead, %d bytes verts, %d bytes indices, %d bytes other\n", 
			m_queue.Count(), (int)m_queue.GetMemoryUsed(), Vertices().GetUsed(), Indices().GetUsed(), RenderDataSizeUsed() );
	}

	m_queue.CallQueued();


	m_Vertices.FreeAll( false );
	m_Indices.FreeAll( false );

	if ( bTermAfterCall )
	{
		Shutdown();
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::FlushQueued()
{
	m_queue.Flush();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
ICallQueue *CMatQueuedRenderContext::GetCallQueue()
{
	return &m_queue;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::SetRenderTargetEx( int nRenderTargetID, ITexture *pNewTarget ) 
{
	CMatRenderContextBase::SetRenderTargetEx( nRenderTargetID, pNewTarget );

	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetRenderTargetEx, nRenderTargetID, pNewTarget );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::GetRenderTargetDimensions( int &width, int &height) const
{
	// Target at top of stack
	ITexture *pTOS = m_RenderTargetStack.back().m_pRenderTargets[0];

	// If top of stack isn't the back buffer, get dimensions from the texture
	if ( pTOS != nullptr )
	{
		width = pTOS->GetActualWidth();
		height = pTOS->GetActualHeight();
	}
	else // otherwise, get them from the shader API
	{
		width = m_WidthBackBuffer;
		height = m_HeightBackBuffer;
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::Viewport( int x, int y, int width, int height )
{
	CMatRenderContextBase::Viewport( x, y, width, height );
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::Viewport, x, y, width, height );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::SetLights( int nCount, const LightDesc_t *pLights )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetLights, nCount, m_queue.CopyArray( pLights, nCount ) );
}

void CMatQueuedRenderContext::SetLightingState( const MaterialLightingState_t &state )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetLightingState, RefToVal( state ) );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::SetLightingOrigin( Vector vLightingOrigin )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetLightingOrigin, vLightingOrigin );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::SetAmbientLightCube( LightCube_t cube )
{
	// FIXME: does compiler do the right thing, is envelope needed?
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetAmbientLightCube, m_queue.CopyArray( &cube[0], 6 ) );
}

//-----------------------------------------------------------------------------
// Bone count
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::SetNumBoneWeights( int nBoneCount )
{
	m_nBoneCount = nBoneCount;
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetNumBoneWeights, nBoneCount );
}

int	CMatQueuedRenderContext::GetCurrentNumBones( ) const
{
	return m_nBoneCount;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::FogMode( MaterialFogMode_t fogMode )
{
	m_FogMode = fogMode;
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::FogMode, fogMode );
}

void CMatQueuedRenderContext::FogStart( float fStart )
{
	m_flFogStart = fStart;
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::FogStart, fStart );
}

void CMatQueuedRenderContext::FogEnd( float fEnd )
{
	m_flFogEnd = fEnd;
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::FogEnd, fEnd );
}

void CMatQueuedRenderContext::FogMaxDensity( float flMaxDensity )
{
	m_flFogMaxDensity = flMaxDensity;
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::FogMaxDensity, flMaxDensity );
}

void CMatQueuedRenderContext::SetFogZ( float fogZ )
{
	m_flFogZ = fogZ;
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetFogZ, fogZ );
}

MaterialFogMode_t CMatQueuedRenderContext::GetFogMode( void )
{
	return m_FogMode;
}

void CMatQueuedRenderContext::FogColor3f( float r, float g, float b )
{
	FogColor3ub( clamp( (int)(r * 255.0f), 0, 255 ), clamp( (int)(g * 255.0f), 0, 255 ), clamp( (int)(b * 255.0f), 0, 255 ) );
}

void CMatQueuedRenderContext::FogColor3fv( float const* rgb )
{
	FogColor3ub( clamp( (int)(rgb[0] * 255.0f), 0, 255 ), clamp( (int)(rgb[1] * 255.0f), 0, 255 ), clamp( (int)(rgb[2] * 255.0f), 0, 255 ) );
}

void CMatQueuedRenderContext::FogColor3ub( unsigned char r, unsigned char g, unsigned char b )
{
	m_FogColor.r = r;
	m_FogColor.g = g;
	m_FogColor.b = b;
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::FogColor3ub, r, g, b );
}

void CMatQueuedRenderContext::FogColor3ubv( unsigned char const* rgb )
{
	FogColor3ub( rgb[0], rgb[1], rgb[2] );
}

void CMatQueuedRenderContext::GetFogColor( unsigned char *rgb )
{
	rgb[0] = m_FogColor.r;
	rgb[1] = m_FogColor.g;
	rgb[2] = m_FogColor.b;
}

void CMatQueuedRenderContext::GetFogDistances( float *fStart, float *fEnd, float *fFogZ )
{
	if( fStart )
		*fStart = m_flFogStart;

	if( fEnd )
		*fEnd = m_flFogEnd;

	if( fFogZ )
		*fFogZ = m_flFogZ;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::GetViewport( int& x, int& y, int& width, int& height ) const
{
	// Verify valid top of RT stack
	contract_assert( !m_RenderTargetStack.empty() );

	// Grab the top of stack
	const RenderTargetStackElement_t& element = m_RenderTargetStack.back();

	// If either dimension is negative, set to full bounds of current target
	if ( (element.m_nViewW < 0) || (element.m_nViewH < 0) )
	{
		// Viewport origin at target origin
		x = y = 0;

		// If target is back buffer
		if ( element.m_pRenderTargets[0] == nullptr )
		{
			width = m_WidthBackBuffer;
			height = m_HeightBackBuffer;
		}
		else // if target is texture
		{
			width = element.m_pRenderTargets[0]->GetActualWidth();
			height = element.m_pRenderTargets[0]->GetActualHeight();
		}
	}
	else // use the bounds from the stack directly
	{
		x = element.m_nViewX;
		y = element.m_nViewY;
		width = element.m_nViewW;
		height = element.m_nViewH;
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::SyncToken( const char *p )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SyncToken, m_queue.Copy( p ) );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
IIndexBuffer* CMatQueuedRenderContext::GetDynamicIndexBuffer()
{
	return m_pQueuedIndexBuffer;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
IMesh* CMatQueuedRenderContext::GetDynamicMesh( bool buffered, IMesh* pVertexOverride, IMesh* pIndexOverride, IMaterial *pAutoBind )
{
	return GetDynamicMeshEx( 0, buffered, pVertexOverride, pIndexOverride, pAutoBind );
}

IMesh* CMatQueuedRenderContext::GetDynamicMeshEx( VertexFormat_t vertexFormat, bool bBuffered, IMesh* pVertexOverride, IMesh* pIndexOverride, IMaterial *pAutoBind )
{
	if( pAutoBind )
	{
		Bind( pAutoBind, nullptr );
	}

	if ( pVertexOverride && pIndexOverride )
	{
		// Use the new batch API
		DebuggerBreak();
		return nullptr;
	}

	if ( pVertexOverride )
	{
		if ( CompressionType( pVertexOverride->GetVertexFormat() ) != VERTEX_COMPRESSION_NONE )
		{
			// UNDONE: support compressed dynamic meshes if needed (pro: less VB memory, con: time spent compressing)
			DebuggerBreak();
			return nullptr;
		}
	}

	// For anything more than 1 bone, imply the last weight from the 1 - the sum of the others.
	int nCurrentBoneCount = GetCurrentNumBones();
	contract_assert( nCurrentBoneCount <= 4 );
	if ( nCurrentBoneCount > 1 )
	{
		--nCurrentBoneCount;
	}

	m_pQueuedMesh->OnGetDynamicMesh( vertexFormat, ( bBuffered ) ? MQM_BUFFERED : 0, pVertexOverride, pIndexOverride, GetCurrentMaterialInternal(), nCurrentBoneCount );
	return m_pQueuedMesh;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int CMatQueuedRenderContext::GetMaxVerticesToRender( IMaterial *pMaterial )
{
	pMaterial = ((IMaterialInternal *)pMaterial)->GetRealTimeVersion(); //always work with the real time version of materials internally.

	// Be conservative, assume no compression (in here, we don't know if the caller will used a compressed VB or not)
	// FIXME: allow the caller to specify which compression type should be used to compute size from the vertex format
	//        (this can vary between multiple VBs/Meshes using the same material)
	VertexFormat_t materialFormat = pMaterial->GetVertexFormat() & ~VERTEX_FORMAT_COMPRESSED;
	int nVertexFormatSize = g_pShaderAPI->VertexFormatSize( materialFormat );
	if ( nVertexFormatSize == 0 )
	{
		Warning( "bad vertex size for material %s\n", pMaterial->GetName() );
		return 65535;
	}

	int nDynamicVBSize = g_pShaderAPI->GetCurrentDynamicVBSize();
	int maxVerts = nDynamicVBSize / nVertexFormatSize;
	if ( maxVerts > 65535 )
	{
		maxVerts = 65535;
	}
	return maxVerts;
}

int CMatQueuedRenderContext::GetMaxIndicesToRender( )
{
	return INDEX_BUFFER_SIZE;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices )
{
	contract_assert( !bMaxUntilFlush );

	int nDynamicVBSize = g_pShaderAPI->GetCurrentDynamicVBSize();
	int nDynamicIBSize = INDEX_BUFFER_SIZE * sizeof(uint16);

	*pMaxVerts = nDynamicVBSize / m_pQueuedMesh->GetVertexSize();
	if ( *pMaxVerts > 65535 )
	{
		*pMaxVerts = 65535;
	}
	*pMaxIndices = nDynamicIBSize / sizeof(uint16);
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
IMesh *CMatQueuedRenderContext::GetFlexMesh()
{
	m_pQueuedFlexMesh->OnGetDynamicMesh( 0, 0, nullptr, nullptr, nullptr, 0 );
	return m_pQueuedFlexMesh;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
OcclusionQueryObjectHandle_t CMatQueuedRenderContext::CreateOcclusionQueryObject()
{
	OcclusionQueryObjectHandle_t h = g_pOcclusionQueryMgr->CreateOcclusionQueryObject();
	m_queue.QueueCall( g_pOcclusionQueryMgr, &COcclusionQueryMgr::OnCreateOcclusionQueryObject, h );
	return h;
}

int CMatQueuedRenderContext::OcclusionQuery_GetNumPixelsRendered( OcclusionQueryObjectHandle_t h )
{
	m_queue.QueueCall( g_pOcclusionQueryMgr, &COcclusionQueryMgr::OcclusionQuery_IssueNumPixelsRenderedQuery, h );
	return g_pOcclusionQueryMgr->OcclusionQuery_GetNumPixelsRendered( h, false );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::SetFlashlightState( const FlashlightState_t &s, const VMatrix &m )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetFlashlightState, RefToVal( s ), RefToVal( m ) );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CMatQueuedRenderContext::EnableClipping( bool bEnable )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::EnableClipping, bEnable );
	return BaseClass::EnableClipping( bEnable );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::UserClipTransform( const VMatrix &m )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::UserClipTransform, RefToVal( m ) );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::GetWindowSize( int &width, int &height ) const
{
	width = m_WidthBackBuffer;
	height = m_HeightBackBuffer;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::DrawScreenSpaceRectangle( 
	IMaterial *pMaterial,
	int destx, int desty,
	int width, int height,
	float src_texture_x0, float src_texture_y0,			// which texel you want to appear at
	// destx/y
	float src_texture_x1, float src_texture_y1,			// which texel you want to appear at
	// destx+width-1, desty+height-1
	int src_texture_width, int src_texture_height,		// needed for fixup
	void *pClientRenderable,
	int nXDice, int nYDice )							// Amount to tessellate the quad
{
	IMaterial *pRealTimeVersionMaterial = ((IMaterialInternal *)pMaterial)->GetRealTimeVersion();
	pRealTimeVersionMaterial->CallBindProxy( pClientRenderable, &m_queue );
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::DrawScreenSpaceRectangle, pMaterial, destx, desty, width, height, src_texture_x0, src_texture_y0, src_texture_x1, src_texture_y1,	src_texture_width, src_texture_height, pClientRenderable, nXDice, nYDice );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::LoadBoneMatrix( int i, const matrix3x4_t &m )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::LoadBoneMatrix, i, RefToVal( m ) );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::CopyRenderTargetToTextureEx( ITexture *pTexture, int i, Rect_t *pSrc, Rect_t *pDst )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::CopyRenderTargetToTextureEx, pTexture, i, ( pSrc ) ? &m_queue.Copy(*pSrc) : nullptr, ( pDst ) ? &m_queue.Copy(*pDst) : nullptr );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::CopyTextureToRenderTargetEx( int i, ITexture *pTexture, Rect_t *pSrc, Rect_t *pDst )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::CopyTextureToRenderTargetEx, i, pTexture, ks::envelope<Rect_t>(pSrc), ks::envelope<Rect_t>(pDst) );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CMatQueuedRenderContext::OnDrawMesh( IMesh *pMesh, int firstIndex, int numIndices )
{
	contract_assert( pMesh != m_pQueuedMesh );
	void (IMesh::*pfnDraw)( int, int) = &IMesh::Draw; // need assignment to disambiguate overloaded function
	m_queue.QueueCall( pMesh, pfnDraw, firstIndex, numIndices );
	return false;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CMatQueuedRenderContext::OnDrawMesh( IMesh *pMesh, CPrimList *pLists, int nLists )
{
	CMatRenderData< CPrimList > rdPrimList( this, nLists, pLists );
	m_queue.QueueCall( this, &CMatQueuedRenderContext::DeferredDrawPrimList, pMesh, rdPrimList.Base(), nLists );
	return false;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CMatQueuedRenderContext::OnDrawMeshModulated( IMesh *pMesh, const Vector4D& vecDiffuseModulation, int firstIndex, int numIndices )
{
	contract_assert( pMesh != m_pQueuedMesh );
	m_queue.QueueCall( pMesh, &IMesh::DrawModulated, vecDiffuseModulation, firstIndex, numIndices );
	return false;
}


void CMatQueuedRenderContext::DeferredDrawPrimList( IMesh *pMesh, CPrimList *pLists, int nLists )
{
	contract_assert( pMesh != m_pQueuedMesh );
	pMesh->Draw( pLists, nLists );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::DeferredSetFlexMesh( IMesh *pStaticMesh, int nVertexOffsetInBytes )
{
 	pStaticMesh->SetFlexMesh( m_pQueuedFlexMesh->MST_GetActualMesh(), m_pQueuedFlexMesh->MST_GetActualVertexOffsetInBytes() );
}

bool CMatQueuedRenderContext::OnSetFlexMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes )
{
	contract_assert( pMesh == m_pQueuedFlexMesh || !pMesh );
	if ( pMesh )
	{
		m_queue.QueueCall( this, &CMatQueuedRenderContext::DeferredSetFlexMesh, pStaticMesh, nVertexOffsetInBytes );
	}
	else
	{
		m_queue.QueueCall( pStaticMesh, &IMesh::SetFlexMesh, (IMesh *)nullptr, 0 );
	}
	return false;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CMatQueuedRenderContext::OnSetColorMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes )
{
	contract_assert( pStaticMesh != m_pQueuedMesh && pStaticMesh != m_pQueuedFlexMesh );
	m_queue.QueueCall( pStaticMesh, &IMesh::SetColorMesh, pMesh, nVertexOffsetInBytes );
	return false;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CMatQueuedRenderContext::OnSetPrimitiveType( IMesh *pMesh, MaterialPrimitiveType_t type )
{
	contract_assert( pMesh != m_pQueuedMesh && pMesh != m_pQueuedFlexMesh );
	m_queue.QueueCall( pMesh, &IMesh::SetPrimitiveType, type );
	return false;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
inline void CMatQueuedRenderContext::QueueMatrixSync()
{
	void (IMatRenderContext::*pfnLoadMatrix)( const VMatrix & ) = &IMatRenderContext::LoadMatrix; // need assignment to disambiguate overloaded function
	m_queue.QueueCall( m_pHardwareContext, pfnLoadMatrix, RefToVal( AccessCurrentMatrix() ) );
}

void CMatQueuedRenderContext::MatrixMode( MaterialMatrixMode_t mode )
{
	CMatRenderContextBase::MatrixMode( mode );
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::MatrixMode, mode );
}

void CMatQueuedRenderContext::PushMatrix()
{
	CMatRenderContextBase::PushMatrix();
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::PushMatrix );
}

void CMatQueuedRenderContext::PopMatrix()
{
	CMatRenderContextBase::PopMatrix();
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::PopMatrix );
}

void CMatQueuedRenderContext::LoadMatrix( const VMatrix& matrix )
{
	CMatRenderContextBase::LoadMatrix( matrix );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::LoadMatrix( const matrix3x4_t& matrix )
{
	CMatRenderContextBase::LoadMatrix( matrix );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::MultMatrix( const VMatrix& matrix )
{
	CMatRenderContextBase::MultMatrix( matrix );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::MultMatrix( const matrix3x4_t& matrix )
{
	CMatRenderContextBase::MultMatrix( VMatrix( matrix ) );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::MultMatrixLocal( const VMatrix& matrix )
{
	CMatRenderContextBase::MultMatrixLocal( matrix );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::MultMatrixLocal( const matrix3x4_t& matrix )
{
	CMatRenderContextBase::MultMatrixLocal( VMatrix( matrix ) );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::LoadIdentity()
{
	CMatRenderContextBase::LoadIdentity();
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::LoadIdentity );
}

void CMatQueuedRenderContext::Ortho( double left, double top, double right, double bottom, double zNear, double zFar )
{
	CMatRenderContextBase::Ortho( left, top, right, bottom, zNear, zFar );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::PerspectiveX( double flFovX, double flAspect, double flZNear, double flZFar )
{
	CMatRenderContextBase::PerspectiveX( flFovX, flAspect, flZNear, flZFar );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::PerspectiveOffCenterX( double flFovX, double flAspect, double flZNear, double flZFar, double bottom, double top, double left, double right )
{
	CMatRenderContextBase::PerspectiveOffCenterX( flFovX, flAspect, flZNear, flZFar, bottom, top, left, right );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::PickMatrix( int x, int y, int nWidth, int nHeight )
{
	CMatRenderContextBase::PickMatrix( x, y, nWidth, nHeight );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::Rotate( float flAngle, float x, float y, float z )
{
	CMatRenderContextBase::Rotate( flAngle, x, y, z );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::Translate( float x, float y, float z )
{
	CMatRenderContextBase::Translate( x, y, z );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::Scale( float x, float y, float z )
{
	CMatRenderContextBase::Scale( x, y, z );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::BeginBatch( IMesh* pIndices ) 
{
	contract_assert( pIndices == (IMesh *)m_pQueuedMesh );
	m_pQueuedMesh->InvalidateAuxMeshSet();
	m_queue.QueueCall( this, &CMatQueuedRenderContext::DeferredBeginBatch );
}

void CMatQueuedRenderContext::BindBatch( IMesh* pVertices, IMaterial *pAutoBind ) 
{
	contract_assert( pVertices != (IMesh *)m_pQueuedMesh );
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::BindBatch, pVertices, pAutoBind );
}

void CMatQueuedRenderContext::DrawBatch(MaterialPrimitiveType_t primType, int firstIndex, int numIndices )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::DrawBatch, primType, firstIndex, numIndices );
}

void CMatQueuedRenderContext::EndBatch()
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::EndBatch );
}

void CMatQueuedRenderContext::DeferredBeginBatch( )
{
	m_pHardwareContext->BeginBatch( m_pQueuedMesh->MST_DetachActualMesh() );
}


//-----------------------------------------------------------------------------
// Memory allocation calls for queued mesh, et. al.
//-----------------------------------------------------------------------------
byte *CMatQueuedRenderContext::AllocVertices( int nVerts, int nVertexSize, bool *pExternalVB )
{
	std::size_t nSizeInBytes = nVerts * nVertexSize;
	byte *pMemory = ( byte * )Vertices().Alloc( nSizeInBytes, false );

	*pExternalVB = false;

	contract_assert( pMemory );
	return pMemory;
}

byte *CMatQueuedRenderContext::AllocIndices( int nIndices, int nIndexSize, bool *pExternalIB )
{
	std::size_t nSizeInBytes = nIndices * nIndexSize;
	byte *pMemory = ( byte * )Indices().Alloc( nSizeInBytes, false );

	*pExternalIB = false;

	contract_assert( pMemory );
	return pMemory;
}

byte *CMatQueuedRenderContext::ReallocVertices( byte *pVerts, int nVertsOld, int nVertsNew, int nVertexSize, bool bExternalMemory )
{
	contract_assert( nVertsNew <= nVertsOld );

	if ( nVertsNew < nVertsOld )
	{
		unsigned nBytes = ( ( nVertsOld - nVertsNew ) * nVertexSize );
		CMemoryStack &stack = Vertices();
		stack.FreeToAllocPoint( stack.GetCurrentAllocPoint() - nBytes, false ); // memstacks 128 bit aligned
	}
	return pVerts;
}

byte *CMatQueuedRenderContext::ReallocIndices( byte *pIndices, int nIndicesOld, int nIndicesNew, int nIndexSize, bool bExternalMemory )
{
	contract_assert( nIndicesNew <= nIndicesOld );
	if ( nIndicesNew < nIndicesOld )
	{
		unsigned nBytes = ( ( nIndicesOld - nIndicesNew ) * nIndexSize );
		CMemoryStack &stack = Indices();
		stack.FreeToAllocPoint( stack.GetCurrentAllocPoint() - nBytes, false ); // memstacks 128 bit aligned
	}
	return pIndices;
}

void CMatQueuedRenderContext::FreeVertices( byte *pVerts, int nVerts, int nVertexSize )
{
	// free at end of call dispatch
}

void CMatQueuedRenderContext::FreeIndices( byte *pIndices, int nIndices, int nIndexSize )
{
	// free at end of call dispatch
}


//------------------------------------------------------------------------------
// Called from rendering thread, fixes up dynamic buffers
//------------------------------------------------------------------------------
void CMatQueuedRenderContext::DeferredDrawInstances( int nInstanceCount, const MeshInstanceData_t *pConstInstance )
{
	MeshInstanceData_t *pInstance = const_cast<MeshInstanceData_t*>( pConstInstance );

	// Adjust the instances pointing to the dynamic index buffer
	IIndexBuffer *pDynamicIndexBuffer = m_pQueuedIndexBuffer->RT_GetDynamicIndexBuffer();
	int nStartIndex = m_pQueuedIndexBuffer->RT_GetIndexStart();
	if ( ( nStartIndex < 0 ) || !pDynamicIndexBuffer )
		return;

	for ( int i = 0; i < nInstanceCount; ++i )
	{
		MeshInstanceData_t &instance = pInstance[i];

		// FIXME: Make dynamic vertex buffers work!
		contract_assert( !instance.m_pVertexBuffer->IsDynamic() );
		if ( !instance.m_pIndexBuffer->IsDynamic() )
			continue;
		
		instance.m_pIndexBuffer = pDynamicIndexBuffer;
		instance.m_nIndexOffset += nStartIndex;
	}

	m_pHardwareContext->DrawInstances( nInstanceCount, pConstInstance );
}


//------------------------------------------------------------------------------
// Draws instances with different meshes
//------------------------------------------------------------------------------
void CMatQueuedRenderContext::DrawInstances( int nInstanceCount, const MeshInstanceData_t *pInstance )
{
	CMatRenderData< MeshInstanceData_t > renderData( this );
	if ( !IsRenderData( pInstance ) )
	{
		renderData.Lock( nInstanceCount );
		std::memcpy( renderData.Base(), pInstance, nInstanceCount * sizeof(MeshInstanceData_t) );
		pInstance = renderData.Base();
	}

#ifdef _DEBUG
	for ( int i = 0; i < nInstanceCount; ++i )
	{
		contract_assert( !pInstance[i].m_pPoseToWorld || IsRenderData( pInstance[i].m_pPoseToWorld ) );
		contract_assert( !pInstance[i].m_pLightingState || IsRenderData( pInstance[i].m_pLightingState ) );
		contract_assert( !pInstance[i].m_pBoneRemap || IsRenderData( pInstance[i].m_pBoneRemap ) );
		contract_assert( !pInstance[i].m_pStencilState || IsRenderData( pInstance[i].m_pStencilState ) );
	}
#endif

	m_queue.QueueCall( this, &CMatQueuedRenderContext::DeferredDrawInstances, nInstanceCount, pInstance );
}


//------------------------------------------------------------------------------
// Color correction related methods
//------------------------------------------------------------------------------
ColorCorrectionHandle_t CMatQueuedRenderContext::AddLookup( const char *pName )
{
	MaterialLock_t hLock = m_pMaterialSystem->Lock();
	ColorCorrectionHandle_t hCC = ColorCorrectionSystem()->AddLookup( pName );
	m_pMaterialSystem->Unlock( hLock );
	return hCC;
}

bool CMatQueuedRenderContext::RemoveLookup( ColorCorrectionHandle_t handle )
{
	MaterialLock_t hLock = m_pMaterialSystem->Lock();
	bool bRemoved = ColorCorrectionSystem()->RemoveLookup( handle );
	m_pMaterialSystem->Unlock( hLock );
	return bRemoved;
}

ColorCorrectionHandle_t CMatQueuedRenderContext::FindLookup( const char *pName )
{
	MaterialLock_t hLock = m_pMaterialSystem->Lock();
	ColorCorrectionHandle_t hCC = ColorCorrectionSystem()->FindLookup( pName );
	m_pMaterialSystem->Unlock( hLock );
	return hCC;
}


void CMatQueuedRenderContext::LockLookup( ColorCorrectionHandle_t handle )
{
	MaterialLock_t hLock = m_pMaterialSystem->Lock();
	ColorCorrectionSystem()->LockLookup( handle );
	m_pMaterialSystem->Unlock( hLock );
}

void CMatQueuedRenderContext::LoadLookup( ColorCorrectionHandle_t handle, const char *pLookupName )
{
	MaterialLock_t hLock = m_pMaterialSystem->Lock();
	ColorCorrectionSystem()->LoadLookup( handle, pLookupName );
	m_pMaterialSystem->Unlock( hLock );
}

void CMatQueuedRenderContext::UnlockLookup( ColorCorrectionHandle_t handle )
{
	MaterialLock_t hLock = m_pMaterialSystem->Lock();
	ColorCorrectionSystem()->UnlockLookup( handle );
	m_pMaterialSystem->Unlock( hLock );
}

// NOTE: These are synchronous calls!  The rendering thread is stopped, the current queue is drained and the pixels are read
// NOTE: We should also have a queued read pixels in the API for doing mid frame reads (as opposed to screenshots)
void CMatQueuedRenderContext::ReadPixels( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, ITexture *pRenderTargetTexture )
{
	MaterialLock_t hLock = m_pMaterialSystem->Lock();
	g_pShaderAPI->ReadPixels( x, y, width, height, data, dstFormat, pRenderTargetTexture );
	m_pMaterialSystem->Unlock( hLock );
}

void CMatQueuedRenderContext::ReadPixelsAsync( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, ITexture *pRenderTargetTexture, CThreadEvent *pPixelsReadEvent )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::ReadPixelsAsync, x, y, width, height, data, dstFormat, pRenderTargetTexture, pPixelsReadEvent );
}

void CMatQueuedRenderContext::ReadPixelsAsyncGetResult( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, CThreadEvent *pGetResultEvent )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::ReadPixelsAsyncGetResult, x, y, width, height, data, dstFormat, pGetResultEvent );
}

void CMatQueuedRenderContext::ReadPixelsAndStretch( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *pBuffer, ImageFormat dstFormat, int nDstStride )
{
	MaterialLock_t hLock = m_pMaterialSystem->Lock();
	g_pShaderAPI->ReadPixels( pSrcRect, pDstRect, pBuffer, dstFormat, nDstStride );
	m_pMaterialSystem->Unlock( hLock );
}
