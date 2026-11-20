// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation
// Copyright (c) 2026 oknyshuk

export module ks.math.polyhedron;

import std;
import ks.platform;
import ks.simd;
export import ks.math.mathlib;

export extern "C++" {

struct Polyhedron_IndexedLine_t
{
	unsigned short iPointIndices[2];
};

struct Polyhedron_IndexedLineReference_t
{
	unsigned short iLineIndex;
	unsigned char iEndPointIndex; //since two polygons reference any one line, one needs to traverse the line backwards, this flags that behavior
};

struct Polyhedron_IndexedPolygon_t
{
	unsigned short iFirstIndex;
	unsigned short iIndexCount;
	Vector polyNormal;
};

class CPolyhedron
{
public:
	Vector *pVertices;
	Polyhedron_IndexedLine_t *pLines;
	Polyhedron_IndexedLineReference_t *pIndices;
	Polyhedron_IndexedPolygon_t *pPolygons;
	
	unsigned short iVertexCount;
	unsigned short iLineCount;
	unsigned short iIndexCount;
	unsigned short iPolygonCount;

	// Frees a polyhedron from Allocate(); does nothing for the temporary one.
	void Release( void );
	Vector Center( void ) const;

protected:
	CPolyhedron( void ) : m_bTemporary( false ) {}
	~CPolyhedron( void ) {}

	bool m_bTemporary; // the polyhedron returned by GetTempPolyhedron()
};

class CPolyhedron_AllocByNew : public CPolyhedron
{
public:
	static CPolyhedron_AllocByNew *Allocate( unsigned short iVertices, unsigned short iLines, unsigned short iIndices, unsigned short iPolygons ); //creates the polyhedron along with enough memory to hold all it's data in a single allocation

private:
	CPolyhedron_AllocByNew( void ) { }; //CPolyhedron_AllocByNew::Allocate() is the only way to create one of these.
};

CPolyhedron *GeneratePolyhedronFromPlanes( const float *pOutwardFacingPlanes, int iPlaneCount, float fOnPlaneEpsilon, bool bUseTemporaryMemory = false ); //be sure to polyhedron->Release()
CPolyhedron *ClipPolyhedron( const CPolyhedron *pExistingPolyhedron, const float *pOutwardFacingPlanes, int iPlaneCount, float fOnPlaneEpsilon, bool bUseTemporaryMemory = false ); //this does NOT modify/delete the existing polyhedron

CPolyhedron *GetTempPolyhedron( unsigned short iVertices, unsigned short iLines, unsigned short iIndices, unsigned short iPolygons ); //grab the temporary polyhedron. Avoids new/delete for quick work. Can only be in use by one chunk of code at a time

}
