#pragma once
import ks.core.basetypes;
import ks.math.compressed_vector;
import ks.math.vector;
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#if !defined ( EFXH )
#define EFXH

#include "iefx.h"

class IMaterial;
struct dlight_t;

class CVEfx : public IVEfx
{
public:
	~CVEfx() {}

	int			Draw_DecalIndexFromName	( char *name );
	void		DecalShoot				( int textureIndex, int entity, const model_t *model, const Vector& model_origin, const QAngle& model_angles, const Vector& position, const Vector *saxis, int flags, const Vector *pNormal = nullptr, int nAdditionalDecalFlags = 0 );
	void		DecalColorShoot			( int textureIndex, int entity, const model_t *model, const Vector& model_origin, const QAngle& model_angles, const Vector& position, const Vector *saxis, int flags, const color32 &rgbaColor, const Vector *pNormal = nullptr, int nAdditionalDecalFlags = 0 );
	void		PlayerDecalShoot		( IMaterial *material, void *userdata, int entity, const model_t *model, const Vector& model_origin, const QAngle& model_angles, 
													const Vector& position, const Vector *saxis, int flags, const color32 &rgbaColor, int nAdditionalDecalFlags );
	dlight_t	*CL_AllocDlight			( int key );
	dlight_t	*CL_AllocElight			( int key );
	int			CL_GetActiveDLights		( dlight_t *pList[MAX_DLIGHTS] );
	const char *Draw_DecalNameFromIndex	( int nIndex );
	dlight_t    *GetElightByKey			( int key );
};

extern CVEfx *g_pEfx;

#endif
