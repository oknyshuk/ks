#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "gl_model_private.h"

#include "draw.h"
import ks.core.basetypes;
import ks.math.compressed_vector;
import ks.math.vector;

void				Decal_Init( void );
void				Decal_Shutdown( void );
IMaterial			*Draw_DecalMaterial( int index );
int					Draw_DecalIndexFromName( char *name, bool *found );
const char *		Draw_DecalNameFromIndex( int index );
void				Draw_DecalSetName( int decal, char *name );
void				R_DecalShoot( int textureIndex, int entity, const model_t *model, const Vector &position, const Vector *saxis, int flags, const color32 &rgbaColor );
int					Draw_DecalMax( void );
