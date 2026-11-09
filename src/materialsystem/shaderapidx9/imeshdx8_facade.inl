// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMeshMgr facade class to the one mesh manager.
// Included by meshdx8.cpp, which defines MeshMgrImpl().

// IMeshMgr
void IMeshMgr::Init()
{ MeshMgrImpl().Init(); }
void IMeshMgr::Shutdown()
{ MeshMgrImpl().Shutdown(); }
void IMeshMgr::ReleaseBuffers()
{ MeshMgrImpl().ReleaseBuffers(); }
void IMeshMgr::RestoreBuffers()
{ MeshMgrImpl().RestoreBuffers(); }
void IMeshMgr::DestroyVertexBuffers()
{ MeshMgrImpl().DestroyVertexBuffers(); }
void IMeshMgr::DiscardVertexBuffers()
{ MeshMgrImpl().DiscardVertexBuffers(); }
IMesh* IMeshMgr::CreateStaticMesh( VertexFormat_t vertexFormat, const char *pTextureBudgetGroup, IMaterial *pMaterial, VertexStreamSpec_t *pStreamSpec )
{ return MeshMgrImpl().CreateStaticMesh( vertexFormat, pTextureBudgetGroup, pMaterial, pStreamSpec ); }
void IMeshMgr::DestroyStaticMesh( IMesh* pMesh )
{ MeshMgrImpl().DestroyStaticMesh( pMesh ); }
IMesh* IMeshMgr::GetDynamicMesh( IMaterial* pMaterial, VertexFormat_t vertexFormat, int nHWSkinBoneCount, bool buffered, IMesh* pVertexOverride, IMesh* pIndexOverride )
{ return MeshMgrImpl().GetDynamicMesh( pMaterial, vertexFormat, nHWSkinBoneCount, buffered, pVertexOverride, pIndexOverride ); }
IVertexBuffer * IMeshMgr::CreateVertexBuffer( ShaderBufferType_t type, VertexFormat_t fmt, int nVertexCount, const char *pBudgetGroup )
{ return MeshMgrImpl().CreateVertexBuffer( type, fmt, nVertexCount, pBudgetGroup ); }
IIndexBuffer * IMeshMgr::CreateIndexBuffer( ShaderBufferType_t indexBufferType, MaterialIndexFormat_t fmt, int nIndexCount, const char *pBudgetGroup )
{ return MeshMgrImpl().CreateIndexBuffer( indexBufferType, fmt, nIndexCount, pBudgetGroup ); }
void IMeshMgr::DestroyVertexBuffer( IVertexBuffer * a0 )
{ MeshMgrImpl().DestroyVertexBuffer( a0 ); }
void IMeshMgr::DestroyIndexBuffer( IIndexBuffer * a0 )
{ MeshMgrImpl().DestroyIndexBuffer( a0 ); }
IVertexBuffer * IMeshMgr::GetDynamicVertexBuffer( int streamID, VertexFormat_t vertexFormat, bool bBuffered )
{ return MeshMgrImpl().GetDynamicVertexBuffer( streamID, vertexFormat, bBuffered ); }
IIndexBuffer * IMeshMgr::GetDynamicIndexBuffer()
{ return MeshMgrImpl().GetDynamicIndexBuffer(); }
void IMeshMgr::BindVertexBuffer( int streamID, IVertexBuffer *pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions )
{ MeshMgrImpl().BindVertexBuffer( streamID, pVertexBuffer, nOffsetInBytes, nFirstVertex, nVertexCount, fmt, nRepetitions ); }
void IMeshMgr::BindIndexBuffer( IIndexBuffer *pIndexBuffer, int nOffsetInBytes )
{ MeshMgrImpl().BindIndexBuffer( pIndexBuffer, nOffsetInBytes ); }
void IMeshMgr::Draw( MaterialPrimitiveType_t primitiveType, int nFirstIndex, int nIndexCount )
{ MeshMgrImpl().Draw( primitiveType, nFirstIndex, nIndexCount ); }
VertexFormat_t IMeshMgr::GetCurrentVertexFormat( void ) const
{ return MeshMgrImpl().GetCurrentVertexFormat(); }
void IMeshMgr::RenderPassWithVertexAndIndexBuffers( const unsigned char *pInstanceCommandBuffer )
{ MeshMgrImpl().RenderPassWithVertexAndIndexBuffers( pInstanceCommandBuffer ); }
void IMeshMgr::DrawInstancedPrims( const unsigned char *pInstanceCommandBuffer )
{ MeshMgrImpl().DrawInstancedPrims( pInstanceCommandBuffer ); }
VertexFormat_t IMeshMgr::ComputeVertexFormat( unsigned int flags, int numTexCoords, int* pTexCoordDimensions, int numBoneWeights, int userDataSize ) const
{ return MeshMgrImpl().ComputeVertexFormat( flags, numTexCoords, pTexCoordDimensions, numBoneWeights, userDataSize ); }
int IMeshMgr::BufferCount() const
{ return MeshMgrImpl().BufferCount(); }
void IMeshMgr::UseFatVertices( bool bUseFat )
{ MeshMgrImpl().UseFatVertices( bUseFat ); }
void IMeshMgr::GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices )
{ MeshMgrImpl().GetMaxToRender( pMesh, bMaxUntilFlush, pMaxVerts, pMaxIndices ); }
int IMeshMgr::GetMaxVerticesToRender( IMaterial *pMaterial )
{ return MeshMgrImpl().GetMaxVerticesToRender( pMaterial ); }
int IMeshMgr::GetMaxIndicesToRender()
{ return MeshMgrImpl().GetMaxIndicesToRender(); }
IMesh * IMeshMgr::GetFlexMesh()
{ return MeshMgrImpl().GetFlexMesh(); }
void IMeshMgr::ComputeVertexDescription( unsigned char* pBuffer, VertexFormat_t vertexFormat, MeshDesc_t& desc ) const
{ MeshMgrImpl().ComputeVertexDescription( pBuffer, vertexFormat, desc ); }
int IMeshMgr::VertexFormatSize( VertexFormat_t vertexFormat ) const
{ return MeshMgrImpl().VertexFormatSize( vertexFormat ); }
IVertexBuffer * IMeshMgr::GetDynamicVertexBuffer( IMaterial *pMaterial, bool buffered )
{ return MeshMgrImpl().GetDynamicVertexBuffer( pMaterial, buffered ); }
void IMeshMgr::MarkUnusedVertexFields( unsigned int nFlags, int nTexCoordCount, bool *pUnusedTexCoords )
{ MeshMgrImpl().MarkUnusedVertexFields( nFlags, nTexCoordCount, pUnusedTexCoords ); }
void IMeshMgr::DrawInstances( int nInstanceCount, const MeshInstanceData_t *pInstances )
{ MeshMgrImpl().DrawInstances( nInstanceCount, pInstances ); }

