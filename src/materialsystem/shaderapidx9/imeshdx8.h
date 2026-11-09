#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "meshbase.h"
#include "shaderapi/ishaderapi.h"

class IMeshMgr
{
public:
	// Initialize, shutdown
	void Init();
	void Shutdown();

	// Task switch...
	void ReleaseBuffers();
	void RestoreBuffers();

	// Releases all dynamic vertex buffers
	void DestroyVertexBuffers();

	// Discards the dynamic vertex and index buffer
	void DiscardVertexBuffers();

	// Creates, destroys static meshes
	IMesh*	CreateStaticMesh( VertexFormat_t vertexFormat, const char *pTextureBudgetGroup, IMaterial *pMaterial = nullptr, VertexStreamSpec_t *pStreamSpec = nullptr );
	void	DestroyStaticMesh( IMesh* pMesh );

	// Gets at the dynamic mesh
	IMesh*	GetDynamicMesh( IMaterial* pMaterial, VertexFormat_t vertexFormat, int nHWSkinBoneCount, bool buffered = true,
		IMesh* pVertexOverride = 0, IMesh* pIndexOverride = 0);


// ------------ New Vertex/Index Buffer interface ----------------------------
	// Do we need support for bForceTempMesh and bSoftwareVertexShader?
	// I don't think we use bSoftwareVertexShader anymore. .need to look into bForceTempMesh.
	IVertexBuffer *CreateVertexBuffer( ShaderBufferType_t type, VertexFormat_t fmt, int nVertexCount, const char *pBudgetGroup );
	IIndexBuffer *CreateIndexBuffer( ShaderBufferType_t indexBufferType, MaterialIndexFormat_t fmt, int nIndexCount, const char *pBudgetGroup );
	void DestroyVertexBuffer( IVertexBuffer * );
	void DestroyIndexBuffer( IIndexBuffer * );
	// Do we need to specify the stream here in the case of locking multiple dynamic VBs on different streams?
	IVertexBuffer *GetDynamicVertexBuffer( int streamID, VertexFormat_t vertexFormat, bool bBuffered = true );
	IIndexBuffer *GetDynamicIndexBuffer( );
	void BindVertexBuffer( int streamID, IVertexBuffer *pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions = 1 );
	void BindIndexBuffer( IIndexBuffer *pIndexBuffer, int nOffsetInBytes );
	void Draw( MaterialPrimitiveType_t primitiveType, int nFirstIndex, int nIndexCount );
// ------------ End ----------------------------
	VertexFormat_t GetCurrentVertexFormat( void ) const;
	void RenderPassWithVertexAndIndexBuffers( const unsigned char *pInstanceCommandBuffer );
	void DrawInstancedPrims( const unsigned char *pInstanceCommandBuffer );

	// Computes the vertex format
	VertexFormat_t ComputeVertexFormat( unsigned int flags, 
			int numTexCoords, int* pTexCoordDimensions, int numBoneWeights,
			int userDataSize ) const;

	// Returns the number of buffers...
	int BufferCount() const;

	// Use fat vertices (for tools)
	void UseFatVertices( bool bUseFat );

	// Returns the number of vertices + indices we can render using the dynamic mesh
	// Passing true in the second parameter will return the max # of vertices + indices
	// we can use before a flush is provoked and may return different values 
	// if called multiple times in succession. 
	// Passing false into the second parameter will return
	// the maximum possible vertices + indices that can be rendered in a single batch
	void GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices );

	// Returns the max number of vertices we can render for a given material
	int GetMaxVerticesToRender( IMaterial *pMaterial );
	int GetMaxIndicesToRender( );
	IMesh *GetFlexMesh();

	void ComputeVertexDescription( unsigned char* pBuffer, VertexFormat_t vertexFormat, MeshDesc_t& desc ) const;

	int VertexFormatSize(  VertexFormat_t vertexFormat ) const;

	IVertexBuffer *GetDynamicVertexBuffer( IMaterial *pMaterial, bool buffered = true );

	void MarkUnusedVertexFields( unsigned int nFlags, int nTexCoordCount, bool *pUnusedTexCoords );

	void DrawInstances( int nInstanceCount, const MeshInstanceData_t *pInstances );

};
