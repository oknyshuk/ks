// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of IVertexBuffer, IIndexBuffer and IMesh to the empty mesh (and the queued ones of the
// material system), selected by the kind. Included by shaderapiempty.cpp.

// IVertexBuffer
int IVertexBuffer::VertexCount() const
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: return static_cast<const CEmptyMesh *>( this )->VertexCount();
	case MeshKind::QueuedMesh: return static_cast<const CMatQueuedMesh *>( this )->VertexCount();
	default: break;
	}
	std::unreachable();
}
VertexFormat_t IVertexBuffer::GetVertexFormat() const
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: return static_cast<const CEmptyMesh *>( this )->GetVertexFormat();
	case MeshKind::QueuedMesh: return static_cast<const CMatQueuedMesh *>( this )->GetVertexFormat();
	default: break;
	}
	std::unreachable();
}
bool IVertexBuffer::IsDynamic() const
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: return static_cast<const CEmptyMesh *>( this )->IsDynamic();
	case MeshKind::QueuedMesh: return static_cast<const CMatQueuedMesh *>( this )->IsDynamic();
	default: break;
	}
	std::unreachable();
}
void IVertexBuffer::BeginCastBuffer( VertexFormat_t format )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->BeginCastBuffer( format ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->BeginCastBuffer( format ); return;
	default: break;
	}
	std::unreachable();
}
void IVertexBuffer::EndCastBuffer()
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->EndCastBuffer(); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->EndCastBuffer(); return;
	default: break;
	}
	std::unreachable();
}
int IVertexBuffer::GetRoomRemaining() const
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: return static_cast<const CEmptyMesh *>( this )->GetRoomRemaining();
	case MeshKind::QueuedMesh: return static_cast<const CMatQueuedMesh *>( this )->GetRoomRemaining();
	default: break;
	}
	std::unreachable();
}
bool IVertexBuffer::Lock( int nVertexCount, bool bAppend, VertexDesc_t &desc )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: return static_cast<CEmptyMesh *>( this )->Lock( nVertexCount, bAppend, desc );
	case MeshKind::QueuedMesh: return static_cast<CMatQueuedMesh *>( this )->Lock( nVertexCount, bAppend, desc );
	default: break;
	}
	std::unreachable();
}
void IVertexBuffer::Unlock( int nVertexCount, VertexDesc_t &desc )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->Unlock( nVertexCount, desc ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->Unlock( nVertexCount, desc ); return;
	default: break;
	}
	std::unreachable();
}
void IVertexBuffer::Spew( int nVertexCount, const VertexDesc_t &desc )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->Spew( nVertexCount, desc ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->Spew( nVertexCount, desc ); return;
	default: break;
	}
	std::unreachable();
}
void IVertexBuffer::ValidateData( int nVertexCount, const VertexDesc_t & desc )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->ValidateData( nVertexCount, desc ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->ValidateData( nVertexCount, desc ); return;
	default: break;
	}
	std::unreachable();
}

// IIndexBuffer
int IIndexBuffer::IndexCount() const
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: return static_cast<const CEmptyMesh *>( this )->IndexCount();
	case MeshKind::QueuedMesh: return static_cast<const CMatQueuedMesh *>( this )->IndexCount();
	case MeshKind::QueuedIndexBuffer: return static_cast<const CMatQueuedIndexBuffer *>( this )->IndexCount();
	default: break;
	}
	std::unreachable();
}
MaterialIndexFormat_t IIndexBuffer::IndexFormat() const
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: return static_cast<const CEmptyMesh *>( this )->IndexFormat();
	case MeshKind::QueuedMesh: return static_cast<const CMatQueuedMesh *>( this )->IndexFormat();
	case MeshKind::QueuedIndexBuffer: return static_cast<const CMatQueuedIndexBuffer *>( this )->IndexFormat();
	default: break;
	}
	std::unreachable();
}
bool IIndexBuffer::IsDynamic() const
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: return static_cast<const CEmptyMesh *>( this )->IsDynamic();
	case MeshKind::QueuedMesh: return static_cast<const CMatQueuedMesh *>( this )->IsDynamic();
	case MeshKind::QueuedIndexBuffer: return static_cast<const CMatQueuedIndexBuffer *>( this )->IsDynamic();
	default: break;
	}
	std::unreachable();
}
void IIndexBuffer::BeginCastBuffer( MaterialIndexFormat_t format )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->BeginCastBuffer( format ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->BeginCastBuffer( format ); return;
	case MeshKind::QueuedIndexBuffer: static_cast<CMatQueuedIndexBuffer *>( this )->BeginCastBuffer( format ); return;
	default: break;
	}
	std::unreachable();
}
void IIndexBuffer::EndCastBuffer()
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->EndCastBuffer(); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->EndCastBuffer(); return;
	case MeshKind::QueuedIndexBuffer: static_cast<CMatQueuedIndexBuffer *>( this )->EndCastBuffer(); return;
	default: break;
	}
	std::unreachable();
}
int IIndexBuffer::GetRoomRemaining() const
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: return static_cast<const CEmptyMesh *>( this )->GetRoomRemaining();
	case MeshKind::QueuedMesh: return static_cast<const CMatQueuedMesh *>( this )->GetRoomRemaining();
	case MeshKind::QueuedIndexBuffer: return static_cast<const CMatQueuedIndexBuffer *>( this )->GetRoomRemaining();
	default: break;
	}
	std::unreachable();
}
bool IIndexBuffer::Lock( int nMaxIndexCount, bool bAppend, IndexDesc_t &desc )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: return static_cast<CEmptyMesh *>( this )->Lock( nMaxIndexCount, bAppend, desc );
	case MeshKind::QueuedMesh: return static_cast<CMatQueuedMesh *>( this )->Lock( nMaxIndexCount, bAppend, desc );
	case MeshKind::QueuedIndexBuffer: return static_cast<CMatQueuedIndexBuffer *>( this )->Lock( nMaxIndexCount, bAppend, desc );
	default: break;
	}
	std::unreachable();
}
void IIndexBuffer::Unlock( int nWrittenIndexCount, IndexDesc_t &desc )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->Unlock( nWrittenIndexCount, desc ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->Unlock( nWrittenIndexCount, desc ); return;
	case MeshKind::QueuedIndexBuffer: static_cast<CMatQueuedIndexBuffer *>( this )->Unlock( nWrittenIndexCount, desc ); return;
	default: break;
	}
	std::unreachable();
}
void IIndexBuffer::ModifyBegin( bool bReadOnly, int nFirstIndex, int nIndexCount, IndexDesc_t& desc )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->ModifyBegin( bReadOnly, nFirstIndex, nIndexCount, desc ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->ModifyBegin( bReadOnly, nFirstIndex, nIndexCount, desc ); return;
	case MeshKind::QueuedIndexBuffer: static_cast<CMatQueuedIndexBuffer *>( this )->ModifyBegin( bReadOnly, nFirstIndex, nIndexCount, desc ); return;
	default: break;
	}
	std::unreachable();
}
void IIndexBuffer::ModifyEnd( IndexDesc_t& desc )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->ModifyEnd( desc ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->ModifyEnd( desc ); return;
	case MeshKind::QueuedIndexBuffer: static_cast<CMatQueuedIndexBuffer *>( this )->ModifyEnd( desc ); return;
	default: break;
	}
	std::unreachable();
}
void IIndexBuffer::Spew( int nIndexCount, const IndexDesc_t &desc )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->Spew( nIndexCount, desc ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->Spew( nIndexCount, desc ); return;
	case MeshKind::QueuedIndexBuffer: static_cast<CMatQueuedIndexBuffer *>( this )->Spew( nIndexCount, desc ); return;
	default: break;
	}
	std::unreachable();
}
void IIndexBuffer::ValidateData( int nIndexCount, const IndexDesc_t &desc )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->ValidateData( nIndexCount, desc ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->ValidateData( nIndexCount, desc ); return;
	case MeshKind::QueuedIndexBuffer: static_cast<CMatQueuedIndexBuffer *>( this )->ValidateData( nIndexCount, desc ); return;
	default: break;
	}
	std::unreachable();
}
IMesh* IIndexBuffer::GetMesh()
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: return static_cast<CEmptyMesh *>( this )->GetMesh();
	case MeshKind::QueuedMesh: return static_cast<CMatQueuedMesh *>( this )->GetMesh();
	case MeshKind::QueuedIndexBuffer: return static_cast<CMatQueuedIndexBuffer *>( this )->GetMesh();
	default: break;
	}
	std::unreachable();
}

// IMesh
void IMesh::SetPrimitiveType( MaterialPrimitiveType_t type )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->SetPrimitiveType( type ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->SetPrimitiveType( type ); return;
	default: break;
	}
	std::unreachable();
}
void IMesh::Draw( int nFirstIndex, int nIndexCount )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->Draw( nFirstIndex, nIndexCount ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->Draw( nFirstIndex, nIndexCount ); return;
	default: break;
	}
	std::unreachable();
}
void IMesh::SetColorMesh( IMesh *pColorMesh, int nVertexOffset )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->SetColorMesh( pColorMesh, nVertexOffset ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->SetColorMesh( pColorMesh, nVertexOffset ); return;
	default: break;
	}
	std::unreachable();
}
void IMesh::Draw( CPrimList *pLists, int nLists )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->Draw( pLists, nLists ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->Draw( pLists, nLists ); return;
	default: break;
	}
	std::unreachable();
}
void IMesh::CopyToMeshBuilder( int iStartVert, int nVerts, int iStartIndex, int nIndices, int indexOffset, CMeshBuilder &builder )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->CopyToMeshBuilder( iStartVert, nVerts, iStartIndex, nIndices, indexOffset, builder ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->CopyToMeshBuilder( iStartVert, nVerts, iStartIndex, nIndices, indexOffset, builder ); return;
	default: break;
	}
	std::unreachable();
}
void IMesh::Spew( int nVertexCount, int nIndexCount, const MeshDesc_t &desc )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->Spew( nVertexCount, nIndexCount, desc ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->Spew( nVertexCount, nIndexCount, desc ); return;
	default: break;
	}
	std::unreachable();
}
void IMesh::ValidateData( int nVertexCount, int nIndexCount, const MeshDesc_t &desc )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->ValidateData( nVertexCount, nIndexCount, desc ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->ValidateData( nVertexCount, nIndexCount, desc ); return;
	default: break;
	}
	std::unreachable();
}
void IMesh::LockMesh( int nVertexCount, int nIndexCount, MeshDesc_t &desc, MeshBuffersAllocationSettings_t *pSettings )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->LockMesh( nVertexCount, nIndexCount, desc, pSettings ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->LockMesh( nVertexCount, nIndexCount, desc, pSettings ); return;
	default: break;
	}
	std::unreachable();
}
void IMesh::ModifyBegin( int nFirstVertex, int nVertexCount, int nFirstIndex, int nIndexCount, MeshDesc_t& desc )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->ModifyBegin( nFirstVertex, nVertexCount, nFirstIndex, nIndexCount, desc ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->ModifyBegin( nFirstVertex, nVertexCount, nFirstIndex, nIndexCount, desc ); return;
	default: break;
	}
	std::unreachable();
}
void IMesh::ModifyEnd( MeshDesc_t& desc )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->ModifyEnd( desc ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->ModifyEnd( desc ); return;
	default: break;
	}
	std::unreachable();
}
void IMesh::UnlockMesh( int nVertexCount, int nIndexCount, MeshDesc_t &desc )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->UnlockMesh( nVertexCount, nIndexCount, desc ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->UnlockMesh( nVertexCount, nIndexCount, desc ); return;
	default: break;
	}
	std::unreachable();
}
void IMesh::ModifyBeginEx( bool bReadOnly, int nFirstVertex, int nVertexCount, int nFirstIndex, int nIndexCount, MeshDesc_t &desc )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->ModifyBeginEx( bReadOnly, nFirstVertex, nVertexCount, nFirstIndex, nIndexCount, desc ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->ModifyBeginEx( bReadOnly, nFirstVertex, nVertexCount, nFirstIndex, nIndexCount, desc ); return;
	default: break;
	}
	std::unreachable();
}
void IMesh::SetFlexMesh( IMesh *pMesh, int nVertexOffset )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->SetFlexMesh( pMesh, nVertexOffset ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->SetFlexMesh( pMesh, nVertexOffset ); return;
	default: break;
	}
	std::unreachable();
}
void IMesh::DisableFlexMesh()
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->DisableFlexMesh(); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->DisableFlexMesh(); return;
	default: break;
	}
	std::unreachable();
}
void IMesh::MarkAsDrawn()
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->MarkAsDrawn(); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->MarkAsDrawn(); return;
	default: break;
	}
	std::unreachable();
}
void IMesh::DrawModulated( const Vector4D &diffuseModulation, int nFirstIndex, int nIndexCount )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->DrawModulated( diffuseModulation, nFirstIndex, nIndexCount ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->DrawModulated( diffuseModulation, nFirstIndex, nIndexCount ); return;
	default: break;
	}
	std::unreachable();
}
unsigned int IMesh::ComputeMemoryUsed()
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: return static_cast<CEmptyMesh *>( this )->ComputeMemoryUsed();
	case MeshKind::QueuedMesh: return static_cast<CMatQueuedMesh *>( this )->ComputeMemoryUsed();
	default: break;
	}
	std::unreachable();
}
void * IMesh::AccessRawHardwareDataStream( uint8 nRawStreamIndex, uint32 numBytes, uint32 uiFlags, void *pvContext )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: return static_cast<CEmptyMesh *>( this )->AccessRawHardwareDataStream( nRawStreamIndex, numBytes, uiFlags, pvContext );
	case MeshKind::QueuedMesh: return static_cast<CMatQueuedMesh *>( this )->AccessRawHardwareDataStream( nRawStreamIndex, numBytes, uiFlags, pvContext );
	default: break;
	}
	std::unreachable();
}
ICachedPerFrameMeshData * IMesh::GetCachedPerFrameMeshData()
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: return static_cast<CEmptyMesh *>( this )->GetCachedPerFrameMeshData();
	case MeshKind::QueuedMesh: return static_cast<CMatQueuedMesh *>( this )->GetCachedPerFrameMeshData();
	default: break;
	}
	std::unreachable();
}
void IMesh::ReconstructFromCachedPerFrameMeshData( ICachedPerFrameMeshData *pData )
{
	switch ( Kind() )
	{
	case MeshKind::EmptyMesh: static_cast<CEmptyMesh *>( this )->ReconstructFromCachedPerFrameMeshData( pData ); return;
	case MeshKind::QueuedMesh: static_cast<CMatQueuedMesh *>( this )->ReconstructFromCachedPerFrameMeshData( pData ); return;
	default: break;
	}
	std::unreachable();
}

