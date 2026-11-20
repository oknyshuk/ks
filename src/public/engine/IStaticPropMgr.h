#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import std;
#include "basehandle.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct vcollide_t;
struct Ray_t;
class IClientRenderable;
class CGameTrace;
typedef CGameTrace trace_t;
class IVPhysicsKeyHandler;
class IPhysicsEnvironment;
class ICollideable;
struct RenderableInstance_t;


//-----------------------------------------------------------------------------
// Interface versions for static props
//-----------------------------------------------------------------------------
inline constexpr const char *INTERFACEVERSION_STATICPROPMGR_CLIENT = "StaticPropMgrClient005";
inline constexpr const char *INTERFACEVERSION_STATICPROPMGR_SERVER = "StaticPropMgrServer002";


//-----------------------------------------------------------------------------
// Interface for static props
//-----------------------------------------------------------------------------
class IStaticPropMgr
{
public:
	// Create physics representations of props
	void	CreateVPhysicsRepresentations( IPhysicsEnvironment *physenv, IVPhysicsKeyHandler *pDefaults, void *pGameData );

	// Purpose: Trace a ray against the specified static Prop. Returns point of intersection in trace_t
	void	TraceRayAgainstStaticProp( const Ray_t& ray, int staticPropIndex, trace_t& tr );

	// Is a base handle a static prop?
	bool	IsStaticProp( IHandleEntity *pHandleEntity ) const;
	bool	IsStaticProp( CBaseHandle handle ) const;

	// returns a collideable interface to static props
	ICollideable *GetStaticPropByIndex( int propIndex );

	// Adds decals to static props, returns point of decal in trace_t
	void	AddDecalToStaticProp( const Vector& rayStart, const Vector& rayEnd,
		int staticPropIndex, int decalIndex, bool doTrace, trace_t& tr, void *pvProxyUserData = nullptr, const Vector* saxis = nullptr, int nAdditionalDecalFlags = 0 );

	// Adds/removes shadows from static props
	void	AddShadowToStaticProp( unsigned short shadowHandle, IClientRenderable* pRenderable );
	void	RemoveAllShadowsFromStaticProp( IClientRenderable* pRenderable );

	// Gets the lighting + material color of a static prop
	void	GetStaticPropMaterialColorAndLighting( trace_t* pTrace,
		int staticPropIndex, Vector& lighting, Vector& matColor );

	//Changes made specifically to support the Portal mod (smack Dave Kircher if something breaks) (Added separately to both client and server to not mess with versioning)
	//===================================================================
	void GetAllStaticProps( std::vector< ICollideable * > *pOutput ); //testing function that will eventually be removed
	void GetAllStaticPropsInAABB( const Vector &vMins, const Vector &vMaxs, std::vector< ICollideable * > *pOutput ); //get all static props that exist wholly or partially in an AABB
	void GetAllStaticPropsInOBB( const Vector &ptOrigin, const Vector &vExtent1, const Vector &vExtent2, const Vector &vExtent3, std::vector< ICollideable * > *pOutput ); //get all static props that exist wholly or partially in an OBB
	//===================================================================

	void DrawStaticProps( IClientRenderable **pProps, const RenderableInstance_t *pInstances, int count, bool bShadowDepth, bool drawVCollideWireframe );

	// Returns the lighting origins of a number of static props
	void GetLightingOrigins( Vector *pLightingOrigins, int nOriginStride, int nCount, IClientRenderable **ppRenderable, int nRenderableStride ); 
};

// One object serves both sides of the engine: the client and the server game code use the same class.
using IStaticPropMgrClient = IStaticPropMgr;
using IStaticPropMgrServer = IStaticPropMgr;
