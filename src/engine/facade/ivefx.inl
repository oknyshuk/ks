import ks.core.basetypes;
import ks.math.compressed_vector;
import ks.math.vector;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IVEfx class to CVEfx. Included by r_efx.cpp.

int IVEfx::Draw_DecalIndexFromName( char *name )
{ return static_cast<CVEfx*>(this)->Draw_DecalIndexFromName( name ); }
void IVEfx::DecalShoot( int textureIndex, int entity, const model_t *model, const Vector& model_origin, const QAngle& model_angles, const Vector& position, const Vector *saxis, int flags, const Vector *pNormal, int nAdditionalDecalFlags )
{ static_cast<CVEfx*>(this)->DecalShoot( textureIndex, entity, model, model_origin, model_angles, position, saxis, flags, pNormal, nAdditionalDecalFlags ); }
void IVEfx::DecalColorShoot( int textureIndex, int entity, const model_t *model, const Vector& model_origin, const QAngle& model_angles, const Vector& position, const Vector *saxis, int flags, const color32 &rgbaColor, const Vector *pNormal, int nAdditionalDecalFlags )
{ static_cast<CVEfx*>(this)->DecalColorShoot( textureIndex, entity, model, model_origin, model_angles, position, saxis, flags, rgbaColor, pNormal, nAdditionalDecalFlags ); }
void IVEfx::PlayerDecalShoot( IMaterial *material, void *userdata, int entity, const model_t *model, const Vector& model_origin, const QAngle& model_angles, const Vector& position, const Vector *saxis, int flags, const color32 &rgbaColor, int nAdditionalDecalFlags )
{ static_cast<CVEfx*>(this)->PlayerDecalShoot( material, userdata, entity, model, model_origin, model_angles, position, saxis, flags, rgbaColor, nAdditionalDecalFlags ); }
dlight_t * IVEfx::CL_AllocDlight( int key )
{ return static_cast<CVEfx*>(this)->CL_AllocDlight( key ); }
dlight_t * IVEfx::CL_AllocElight( int key )
{ return static_cast<CVEfx*>(this)->CL_AllocElight( key ); }
int IVEfx::CL_GetActiveDLights( dlight_t *pList[MAX_DLIGHTS] )
{ return static_cast<CVEfx*>(this)->CL_GetActiveDLights( pList ); }
const char * IVEfx::Draw_DecalNameFromIndex( int nIndex )
{ return static_cast<CVEfx*>(this)->Draw_DecalNameFromIndex( nIndex ); }
dlight_t * IVEfx::GetElightByKey( int key )
{ return static_cast<CVEfx*>(this)->GetElightByKey( key ); }

