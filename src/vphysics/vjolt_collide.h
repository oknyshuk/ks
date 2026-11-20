// SPDX-License-Identifier: MIT
// Copyright 2022 Joshua Ashton and Josh Dowell (Slartibarty)

//=================================================================================================
//
// CPhysCollide and friends!
//
//=================================================================================================

#pragma once

//-------------------------------------------------------------------------------------------------

// Dummy helper class to go back and forth.
// Does not and will not contain *any* data.
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/ConvexShape.h>
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.polyhedron;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.interfaces;
#include "vphysics_interface.h"
import std;

class CPhysCollide
{
	~CPhysCollide() = delete;

public:
	JPH::Shape* ToShape()
	{
		return reinterpret_cast<JPH::Shape*>(this);
	}

	const JPH::Shape *ToShape() const
	{
		return reinterpret_cast< const JPH::Shape * >( this );
	}

	//-------------------------------------------------------------------------------------------------

	static CPhysCollide *FromShape( JPH::Shape *pCollide )
	{
		return reinterpret_cast< CPhysCollide * >( pCollide );
	}

	static const CPhysCollide *FromShape( const JPH::Shape *pCollide )
	{
		return reinterpret_cast< const CPhysCollide * >( pCollide );
	}
};

//-------------------------------------------------------------------------------------------------

// Dummy helper class to go back and forth.
// Does not and will not contain *any* data.
class CPhysConvex
{
	~CPhysConvex() = delete;

public:
	JPH::ConvexShape* ToConvexShape()
	{
		return reinterpret_cast< JPH::ConvexShape* >( this );
	}

	const JPH::ConvexShape *ToConvexShape() const
	{
		return reinterpret_cast< const JPH::ConvexShape * >( this );
	}

	//-------------------------------------------------------------------------------------------------

	static CPhysConvex *FromConvexShape( JPH::ConvexShape *pCollide )
	{
		return reinterpret_cast< CPhysConvex * >( pCollide );
	}

	static const CPhysConvex *FromConvexShape( const JPH::ConvexShape *pCollide )
	{
		return reinterpret_cast< const CPhysConvex * >( pCollide );
	}

	//-------------------------------------------------------------------------------------------------

	CPhysCollide *ToPhysCollide()
	{
		return reinterpret_cast< CPhysCollide * >( this );
	}
};

//-------------------------------------------------------------------------------------------------

class CPhysPolysoup
{
public:
	JPH::TriangleList Triangles;
};

//-------------------------------------------------------------------------------------------------

// Josh: Suprise! This is not an app system! Just an interface...
class JoltPhysicsCollision final : public IPhysicsCollision 
{
public:
	CPhysConvex		*ConvexFromVerts( Vector **pVerts, int vertCount );
	CPhysConvex		*ConvexFromPlanes( float *pPlanes, int planeCount, float mergeDistance );
	float			ConvexVolume( CPhysConvex *pConvex );

	float			ConvexSurfaceArea( CPhysConvex *pConvex );
	void			SetConvexGameData( CPhysConvex *pConvex, unsigned int gameData );
	void			ConvexFree( CPhysConvex *pConvex );
	CPhysConvex		*BBoxToConvex( const Vector &mins, const Vector &maxs );
	CPhysConvex		*ConvexFromConvexPolyhedron( const CPolyhedron &ConvexPolyhedron );
	void			ConvexesFromConvexPolygon( const Vector &vPolyNormal, const Vector *pPoints, int iPointCount, CPhysConvex **pOutput );

	CPhysPolysoup	*PolysoupCreate();
	void			PolysoupDestroy( CPhysPolysoup *pSoup );
	void			PolysoupAddTriangle( CPhysPolysoup *pSoup, const Vector &a, const Vector &b, const Vector &c, int materialIndex7bits );
	CPhysCollide	*ConvertPolysoupToCollide( CPhysPolysoup *pSoup, bool useMOPP );
	
	CPhysCollide	*ConvertConvexToCollide( CPhysConvex **pConvex, int convexCount );
	CPhysCollide	*ConvertConvexToCollideParams( CPhysConvex **pConvex, int convexCount, const convertconvexparams_t &convertParams );
	void			DestroyCollide( CPhysCollide *pCollide );

	int				CollideSize( CPhysCollide *pCollide );
	int				CollideWrite( char *pDest, CPhysCollide *pCollide, bool bSwap = false );
	CPhysCollide	*UnserializeCollide( char *pBuffer, int size, int index );

	float			CollideVolume( CPhysCollide *pCollide );
	float			CollideSurfaceArea( CPhysCollide *pCollide );

	Vector			CollideGetExtent( const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, const Vector &direction );

	void			CollideGetAABB( Vector *pMins, Vector *pMaxs, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles );

	void			CollideGetMassCenter( CPhysCollide *pCollide, Vector *pOutMassCenter );
	void			CollideSetMassCenter( CPhysCollide *pCollide, const Vector &massCenter );
	Vector			CollideGetOrthographicAreas( const CPhysCollide *pCollide );
	void			CollideSetOrthographicAreas( CPhysCollide *pCollide, const Vector &areas );

	int				CollideIndex( const CPhysCollide *pCollide );

	CPhysCollide	*BBoxToCollide( const Vector &mins, const Vector &maxs );
	int				GetConvexesUsedInCollideable( const CPhysCollide *pCollideable, CPhysConvex **pOutputArray, int iOutputArrayLimit );


	void			TraceBox( const Vector &start, const Vector &end, const Vector &mins, const Vector &maxs, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr );
	void			TraceBox( const Ray_t &ray, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr );
	void			TraceBox( const Ray_t &ray, unsigned int contentsMask, IConvexInfo *pConvexInfo, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr );

	void			TraceCollide( const Vector &start, const Vector &end, const CPhysCollide *pSweepCollide, const QAngle &sweepAngles, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr );

	bool			IsBoxIntersectingCone( const Vector &boxAbsMins, const Vector &boxAbsMaxs, const truncatedcone_t &cone );

	void			VCollideLoad( vcollide_t *pOutput, int solidCount, const char *pBuffer, int size, bool swap = false );
	void			VCollideUnload( vcollide_t *pVCollide );

	IVPhysicsKeyParser	*VPhysicsKeyParserCreate( const char *pKeyData );
	IVPhysicsKeyParser	*VPhysicsKeyParserCreate( vcollide_t *pVCollide ) ;
	void				VPhysicsKeyParserDestroy( IVPhysicsKeyParser *pParser );

	int				CreateDebugMesh( CPhysCollide const *pCollisionModel, Vector **outVerts );
	void			DestroyDebugMesh( int vertCount, Vector *outVerts );

	ICollisionQuery *CreateQueryModel( CPhysCollide *pCollide );
	void			DestroyQueryModel( ICollisionQuery *pQuery );

	IPhysicsCollision *ThreadContextCreate();
	void			ThreadContextDestroy( IPhysicsCollision *pThreadContex );

	CPhysCollide	*CreateVirtualMesh( const virtualmeshparams_t &params );
	bool			SupportsVirtualMesh();


	bool			GetBBoxCacheSize( int *pCachedSize, int *pCachedCount );

	
	CPolyhedron		*PolyhedronFromConvex( CPhysConvex * const pConvex, bool bUseTempPolyhedron );

	void			OutputDebugInfo( const CPhysCollide *pCollide );
	unsigned int	ReadStat( int statID );

	float			CollideGetRadius( const CPhysCollide *pCollide ) ;

	void			*VCollideAllocUserData( vcollide_t *pVCollide, size_t userDataSize ) ;
	void			VCollideFreeUserData( vcollide_t *pVCollide ) ;
	void			VCollideCheck( vcollide_t *pVCollide, const char *pName ) ;

	bool			TraceBoxAA( const Ray_t &ray, const CPhysCollide *pCollide, trace_t *ptr ) ;

	void			DuplicateAndScale( vcollide_t *pOut, const vcollide_t *pIn, float flScale ) ;

public:
	static JoltPhysicsCollision& GetInstance() { return s_PhysicsCollision; }

private:
	static JoltPhysicsCollision s_PhysicsCollision;
};

const JPH::Shape *CreateCOMOverrideShape( const JPH::Shape* pShape, JPH::Vec3Arg comOverride );
