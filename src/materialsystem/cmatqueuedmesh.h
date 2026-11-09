#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

// The meshes and index buffer a queued render context hands out.  The shader API libraries dispatch
// to them as they do to their own meshes (see MeshKind in materialsystem/imesh.h).

import ks.tier1;
import std;
#include "materialsystem/imesh.h"
#include "imatrendercontextinternal.h"
#include "imaterialsysteminternal.h"

class CMatQueuedRenderContext;
class IMaterialInternal;

#define Unsupported( funcName ) ExecuteOnce( [&]{ Msg( "CMatQueuedRenderContext: %s is unsupported\n", #funcName ); } )
#define FATAL_QUEUE 1

#ifdef FATAL_QUEUE
#define CannotSupport() ExecuteOnce( [&]{ Msg( "Called function that cannot be supported\n" ); } ); ExecuteOnce( [&]{ DebuggerBreakIfDebugging(); } )
#else
#define CannotSupport() ExecuteOnce( [&]{ Msg( "Called function that cannot be supported\n" ); } )
#endif

//-----------------------------------------------------------------------------
class CMatQueuedMesh : public IMesh
{
	friend class IVertexBuffer;
	friend class IIndexBuffer;
	friend class IMesh;
	// Methods of IVertexBuffer, called from the main (client) thread
public:
	int VertexCount() const;
	VertexFormat_t GetVertexFormat() const;
	bool IsDynamic() const												{ return true; } 
	void BeginCastBuffer( VertexFormat_t format )						{ CannotSupport(); }
	void EndCastBuffer( )												{ CannotSupport(); }
	int GetRoomRemaining() const										{ CannotSupport(); return 0; }
	void * AccessRawHardwareDataStream( uint8 nRawStreamIndex, uint32 numBytes, uint32 uiFlags, void *pvContext ) {	CannotSupport(); return nullptr; }
	bool Lock( int nVertexCount, bool bAppend, VertexDesc_t &desc )		{ CannotSupport(); return false; }
	void Unlock( int nVertexCount, VertexDesc_t &desc )					{ CannotSupport(); }
	void Spew( int nVertexCount, const VertexDesc_t &desc )				{ }
	void ValidateData( int nVertexCount, const VertexDesc_t & desc )	{ }

	// Methods of IIndexBuffer, called from the main (client) thread
public:
	int IndexCount() const;
	MaterialIndexFormat_t IndexFormat() const												{ CannotSupport(); return MATERIAL_INDEX_FORMAT_16BIT; }
	void BeginCastBuffer( MaterialIndexFormat_t format )									{ CannotSupport(); }
	bool Lock( int nMaxIndexCount, bool bAppend, IndexDesc_t& desc )						{ CannotSupport(); return false; }
	void Unlock( int nWrittenIndexCount, IndexDesc_t& desc )								{ CannotSupport(); }
	void ModifyBegin( bool bReadOnly, int nFirstIndex, int nIndexCount, IndexDesc_t& desc )	{ CannotSupport(); }
	void ModifyEnd( IndexDesc_t& desc )														{ CannotSupport(); }
	void Spew( int nIndexCount, const IndexDesc_t & desc )									{ }
	void ValidateData( int nIndexCount, const IndexDesc_t &desc )							{ }
	IMesh *GetMesh()																		{ return this; }

	// Methods of IMesh, called from the main (client) thread
public:
	void SetPrimitiveType( MaterialPrimitiveType_t type );
	void Draw( int firstIndex = -1, int numIndices = 0 );
	void SetColorMesh( IMesh *pColorMesh, int nVertexOffset );
	void Draw( CPrimList *pLists, int nLists )																					{ CannotSupport(); }
	void CopyToMeshBuilder( int iStartVert, int nVerts, int iStartIndex, int nIndices, int indexOffset, CMeshBuilder &builder )	{ CannotSupport(); }
	void Spew( int numVerts, int numIndices, const MeshDesc_t & desc )															{ }
	void ValidateData( int numVerts, int numIndices, const MeshDesc_t & desc )													{ }
	void LockMesh( int numVerts, int numIndices, MeshDesc_t& desc, MeshBuffersAllocationSettings_t *pSettings = 0 );
	void ModifyBegin( int firstVertex, int numVerts, int firstIndex, int numIndices, MeshDesc_t& desc )							{ CannotSupport(); }
	void ModifyEnd( MeshDesc_t& desc )																							{ CannotSupport(); }
	void UnlockMesh( int numVerts, int numIndices, MeshDesc_t& desc );
	void ModifyBeginEx( bool bReadOnly, int firstVertex, int numVerts, int firstIndex, int numIndices, MeshDesc_t& desc )		{ CannotSupport(); }
	void SetFlexMesh( IMesh *pMesh, int nVertexOffset );
	void DisableFlexMesh();
	void MarkAsDrawn();
	void DrawModulated( const Vector4D &vecDiffuseModulation, int firstIndex = -1, int numIndices = 0 );
	unsigned int ComputeMemoryUsed() { return 0; }
	ICachedPerFrameMeshData *GetCachedPerFrameMeshData();
	void ReconstructFromCachedPerFrameMeshData( ICachedPerFrameMeshData *pData );

	// Other methods called from the main (client) thread
public:
	CMatQueuedMesh( CMatQueuedRenderContext *pOwner, IMatRenderContextInternal *pHardwareContext, bool bFlexMesh );
	byte *GetVertexData()				{ return m_pVertexData; }
	uint16 *GetIndexData()				{ return m_pIndexData; }
	void InvalidateAuxMeshSet()			{ m_bCanSetAuxMeshes = false; }
	int GetVertexSize()					{ return m_VertexSize; }
	bool OnGetDynamicMesh( VertexFormat_t vertexFormat, unsigned flags, IMesh* pVertexOverride, IMesh* pIndexOverride, IMaterialInternal *pMaterial, int nHWSkinBoneCount );
	void QueueBuild( );
	void FreeBuffers();

	// These methods are called from the material system thread
	// Using the prefix MST_ to indicate this.
public:
	struct MST_MeshInfo_t
	{
		IMaterial *m_pMaterial;
		VertexFormat_t m_VertexFormat;
		uint8 m_nFlags;
		bool m_bExternalVB : 1;
		bool m_bExternalIB : 1;
		IMesh* m_pVertexOverride;
		IMesh* m_pIndexOverride;
		uint8 *m_pVertexData;
		int m_nVertexCount;
		int m_nVertexSizeInBytes;
		uint16 *m_pIndexData;
		int m_nIndexCount;
		MaterialPrimitiveType_t m_nPrimitiveType;
	};

	struct MST_DrawInfo_t
	{
		MaterialPrimitiveType_t m_Type;
		int m_nFirstIndex;
		int m_nIndexCount;
	};

	struct MST_DrawModulatedInfo_t
	{
		MaterialPrimitiveType_t m_Type;
		Vector4D m_vecDiffuseModulation;
		int m_nFirstIndex;
		int m_nIndexCount;
	};

	IMesh *MST_DetachActualMesh()		{ IMesh *p = m_pMSTActualMesh; m_pMSTActualMesh = nullptr; return p; }
	IMesh *MST_GetActualMesh()			{ return m_pMSTActualMesh; }
	IMesh *MST_SetupExternalMesh( const MST_MeshInfo_t &info );
	IMesh *MST_SetupDynamicMesh( const MST_MeshInfo_t &info, IMesh *pExternalMesh );
	int MST_GetActualVertexOffsetInBytes() const { return m_nMSTActualVertexOffsetInBytes; }
	void MST_CopyDynamicVB( const MeshDesc_t &desc, const uint8 *pVertexData, std::size_t nSizeInBytes );
	void MST_CopyDynamicIB( const MeshDesc_t &desc, const uint16 *pIndexData, int nIndices );
	void MST_BuildDynamicBuffers( const MST_MeshInfo_t &info );
	void MST_Draw( const MST_DrawInfo_t &info );
	void MST_DrawModulated( const MST_DrawModulatedInfo_t &info );

	// Member variables accessible from the material thread
private:
	int m_nMSTActualVertexOffsetInBytes;
	IMesh *m_pMSTActualMesh;

	// Member variables accessible from the main thread
private:

	CLateBoundPtr<IMesh> m_pLateBoundMesh;

	CMatQueuedRenderContext *m_pOwner;
	CMatCallQueue *m_pCallQueue;
	IMatRenderContextInternal *m_pHardwareContext;

	// The vertex format we're using...
	VertexFormat_t m_VertexFormat;

	byte *m_pVertexData;
	uint16 *m_pIndexData;

	int m_nVerts;
	int m_nIndices;

	unsigned short m_VertexSize;
	uint8 m_nFlags;
	bool m_bExternalVB;
	bool m_bExternalIB;
	bool m_bFlexMesh;
	bool m_bCanSetAuxMeshes;
	MaterialPrimitiveType_t m_Type;

	IMesh *m_pVertexOverride;
	IMesh *m_pIndexOverride;

	static unsigned short gm_ScratchIndexBuffer;
};

enum MatQueuedMeshFlags_t
{
	MQM_BUFFERED	= ( 1 << 0 ),
};

//-----------------------------------------------------------------------------
class CMatQueuedIndexBuffer : public IIndexBuffer
{
	friend class IIndexBuffer;
	// Inherited from IIndexBuffer
public:
	int IndexCount() const;
	MaterialIndexFormat_t IndexFormat() const;
	bool IsDynamic() const;
	void BeginCastBuffer( MaterialIndexFormat_t format );
	void EndCastBuffer();
	int GetRoomRemaining() const;
	bool Lock( int nMaxIndexCount, bool bAppend, IndexDesc_t &desc );
	void Unlock( int nWrittenIndexCount, IndexDesc_t &desc );
	void ModifyBegin( bool bReadOnly, int nFirstIndex, int nIndexCount, IndexDesc_t& desc );
	void ModifyEnd( IndexDesc_t& desc );
	void Spew( int nIndexCount, const IndexDesc_t &desc );
	void ValidateData( int nIndexCount, const IndexDesc_t &desc );
	IMesh *GetMesh() { return nullptr; }
	
	// Other public methods, accessible from the main thread
public:
	CMatQueuedIndexBuffer( CMatQueuedRenderContext *pOwner, IMatRenderContextInternal *pHardwareContext );
	~CMatQueuedIndexBuffer();
	const void *GetIndexData() const;

	// These methods are expected to only be accessed from the render thread
public:
	int RT_GetIndexStart() const;
	IIndexBuffer* RT_GetDynamicIndexBuffer();

private:
	void ReleaseBuffer();
	void FreeIndexData( int nIndexCount, MaterialIndexFormat_t fmt, void *pIndexData );

	// These methods run in the render thread
	void RT_CopyIndexData( int nIndexCount, MaterialIndexFormat_t fmt, void *pIndexData, bool bIsExternal );
	void RT_FreeIndexData( int nIndexCount, MaterialIndexFormat_t fmt, void *pIndexData );

	CMatQueuedRenderContext *m_pOwner;
	CMatCallQueue *m_pCallQueue;
	IMatRenderContextInternal *m_pHardwareContext;
	void *m_pIndexData;
	int m_nIndices;
	bool m_bExternalIB;
	MaterialIndexFormat_t m_nIndexFormat;

	// This must only be accessed from the render thread
	int m_nRTStartIndex;
	IIndexBuffer *m_pRTDynamicIndexBuffer;
};
