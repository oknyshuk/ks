#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// NOTE: These are fixed length to make it easy to fill these out without memory allocation or storage
import std;
import ks.math;

const int MAX_VIRTUAL_TRIANGLES = 1024;
struct virtualmeshlist_t
{
	Vector			*pVerts;
	int				indexCount;
	int				triangleCount;
	int				vertexCount;
	int				surfacePropsIndex;
	byte			*pHull;
	unsigned short	indices[MAX_VIRTUAL_TRIANGLES*3];
};

struct virtualmeshtrianglelist_t
{
	int				triangleCount;
	unsigned short	triangleIndices[MAX_VIRTUAL_TRIANGLES*3];
};

class IVirtualMeshEvent
{
public:
	void GetVirtualMesh( void *userData, virtualmeshlist_t *pList );
	void GetWorldspaceBounds( void *userData, Vector *pMins, Vector *pMaxs );
	void GetTrianglesInSphere( void *userData, const Vector &center, float radius, virtualmeshtrianglelist_t *pList );
};
struct virtualmeshparams_t
{
	IVirtualMeshEvent	*pMeshEventHandler;
	void				*userData;
	bool				buildOuterHull;
};
