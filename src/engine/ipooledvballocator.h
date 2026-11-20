#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2007 Valve Corporation

// Purpose: This file contains a little interface to deal with pooled vertex buffer allocations
// (which is used to allow multiple meshes to own sub-ranges within a single vertex buffer)

//-----------------------------------------------------------------------------
// Pooled VB allocator abstract base class
//-----------------------------------------------------------------------------
#include "materialsystem/imesh.h"
import ks.math.polyhedron;

class IPooledVBAllocator
{
public:


	// Allocate the shared vertex buffer
	bool			Init( VertexFormat_t format, int numVerts );
	// Free the shared vertex buffer (after Deallocate is called for all sub-allocs)
	void			Clear();

	// Get the shared mesh (vertex buffer) from which sub-allocations are made
	IMesh			*GetSharedMesh();

	// Get a pointer to the start of the vertex buffer data
	void			*GetVertexBufferBase();
	int				GetNumVertsAllocated(); 

	// Allocate a sub-range of 'numVerts' from free space in the shared vertex buffer
	// (returns the byte offset from the start of the VB to the new allocation)
	int				Allocate( int numVerts );
	void			Deallocate( int offset, int numVerts );
};
