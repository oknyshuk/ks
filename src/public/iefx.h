#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.basetypes;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import std;

struct model_t;
struct dlight_t;
class IMaterial;

#define	MAX_DLIGHTS		32

//-----------------------------------------------------------------------------
// Purpose: Exposes effects api to client .dll
//-----------------------------------------------------------------------------
class IVEfx
{
public:
	// Retrieve decal texture index from decal by name
	int				Draw_DecalIndexFromName	( char *name );

	// Apply decal. See decal_private.h for flags (FDECAL_*)
	// If normal is specified, don't decal surfaces antiparallel with normal
	void			DecalShoot				( int textureIndex, int entity, 
		const model_t *model, const Vector& model_origin, const QAngle& model_angles, 
		const Vector& position, const Vector *saxis, int flags, const Vector *pNormal = nullptr, int nAdditionalDecalFlags = 0 );

	// Apply colored decal. See decal_private.h for flags (FDECAL_*)
	void			DecalColorShoot				( int textureIndex, int entity, 
		const model_t *model, const Vector& model_origin, const QAngle& model_angles, 
		const Vector& position, const Vector *saxis, int flags, const color32 &rgbaColor, const Vector *pNormal = nullptr, int nAdditionalDecalFlags = 0 );

	void			PlayerDecalShoot( IMaterial *material, void *userdata, int entity, const model_t *model, 
		const Vector& model_origin, const QAngle& model_angles, 
		const Vector& position, const Vector *saxis, int flags, const color32 &rgbaColor, int nAdditionalDecalFlags );

	// Allocate a dynamic world light ( key is the entity to whom it is associated )
	dlight_t	*CL_AllocDlight			( int key );

	// Allocate a dynamic entity light ( key is the entity to whom it is associated )
	dlight_t	*CL_AllocElight			( int key );

	// Get a list of the currently-active dynamic lights.
	int CL_GetActiveDLights( dlight_t *pList[MAX_DLIGHTS] );

	// Retrieve decal texture name from decal by index
	const char *Draw_DecalNameFromIndex( int nIndex );

	// Given an elight key, find it. Does not search ordinary dlights. May return NULL.
	dlight_t   *GetElightByKey( int key );
};

inline constexpr const char *VENGINE_EFFECTS_INTERFACE_VERSION = "VEngineEffects001";

