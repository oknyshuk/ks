#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: The BSP tree leaf data system

import ks.core.types;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------

struct Ray_t;


//-----------------------------------------------------------------------------
// Interface needed by tree data to do its job
// 
// Note that anything that has convex spatial regions with leaves identified
// by indices can implement the ISpatialQuery. All you have to do is to implement
// a class that can answer the 5 questions in the Query interface about the
// spatial subdivision. For example, a K-D tree or a BSP tree could implement
// this interface
//
//-----------------------------------------------------------------------------

class ISpatialLeafEnumerator
{
public:
	// call back with a leaf and a context
	// The context is completely user defined; it's passed into the enumeration
	// function of ISpatialQuery.
	// This gets called	by the enumeration methods with each leaf
	// that passes the test; return true to continue enumerating,
	// false to stop

	virtual bool EnumerateLeaf( int leaf, intp context ) = 0;
};

class ISpatialQuery
{
public:
	// Returns the number of leaves
	virtual int LeafCount() const = 0;

	// Enumerates the leaves along a ray, box, etc.
	virtual bool EnumerateLeavesAtPoint( const Vector& pt, ISpatialLeafEnumerator* pEnum, intp context ) = 0;
	virtual bool EnumerateLeavesInBox( const Vector& mins, const Vector& maxs, ISpatialLeafEnumerator* pEnum, intp context ) = 0;
	virtual bool EnumerateLeavesInSphere( const Vector& center, float radius, ISpatialLeafEnumerator* pEnum, intp context ) = 0;
	virtual bool EnumerateLeavesAlongRay( Ray_t const& ray, ISpatialLeafEnumerator* pEnum, intp context ) = 0;
	virtual bool EnumerateLeavesInSphereWithFlagSet( const Vector& center, float radius, ISpatialLeafEnumerator* pEnum, intp context, int nFlagsCheck ) = 0;
	virtual int ListLeavesInBox( const Vector& mins, const Vector& maxs, unsigned short *pList, int listMax ) = 0;

	// Used to determine which leaves passed in (specified by leafcount, pLeafs, and nLeafStride ) 
	// are within radius flRadius of vecCenter and have the flag set.
	// The result is placed in the pLeafsInSphere array, which specifies _indices_ into the original pLeafs array
	// The number of leaves found within the sphere is the return value.
	// The caller is expected to have allocated at least nLeafCount pLeafsInSphere to place the results into
	virtual int ListLeavesInSphereWithFlagSet( int *pLeafsInSphere, const Vector& vecCenter, float flRadius, int nLeafCount, const uint16 *pLeafs, int nLeafStride = sizeof(uint16), int nFlagsCheck = 0xFFFFFFFF ) = 0;
};
