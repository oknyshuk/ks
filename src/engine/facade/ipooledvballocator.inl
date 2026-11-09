// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IPooledVBAllocator class to CPooledVBAllocator_ColorMesh. Included by l_studio.cpp.

bool IPooledVBAllocator::Init( VertexFormat_t format, int numVerts )
{ return static_cast<CPooledVBAllocator_ColorMesh*>(this)->Init( format, numVerts ); }
void IPooledVBAllocator::Clear()
{ static_cast<CPooledVBAllocator_ColorMesh*>(this)->Clear(); }
IMesh * IPooledVBAllocator::GetSharedMesh()
{ return static_cast<CPooledVBAllocator_ColorMesh*>(this)->GetSharedMesh(); }
void * IPooledVBAllocator::GetVertexBufferBase()
{ return static_cast<CPooledVBAllocator_ColorMesh*>(this)->GetVertexBufferBase(); }
int IPooledVBAllocator::GetNumVertsAllocated()
{ return static_cast<CPooledVBAllocator_ColorMesh*>(this)->GetNumVertsAllocated(); }
int IPooledVBAllocator::Allocate( int numVerts )
{ return static_cast<CPooledVBAllocator_ColorMesh*>(this)->Allocate( numVerts ); }
void IPooledVBAllocator::Deallocate( int offset, int numVerts )
{ static_cast<CPooledVBAllocator_ColorMesh*>(this)->Deallocate( offset, numVerts ); }

