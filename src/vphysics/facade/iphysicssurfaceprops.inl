// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IPhysicsSurfaceProps class to JoltPhysicsSurfaceProps. Included by the source that defines JoltPhysicsSurfaceProps.

int IPhysicsSurfaceProps::ParseSurfaceData( const char *pFilename, const char *pTextfile )
{ return static_cast<JoltPhysicsSurfaceProps*>(this)->ParseSurfaceData( pFilename, pTextfile ); }
int IPhysicsSurfaceProps::SurfacePropCount() const
{ return static_cast<const JoltPhysicsSurfaceProps*>(this)->SurfacePropCount(); }
int IPhysicsSurfaceProps::GetSurfaceIndex( const char *pSurfacePropName ) const
{ return static_cast<const JoltPhysicsSurfaceProps*>(this)->GetSurfaceIndex( pSurfacePropName ); }
void IPhysicsSurfaceProps::GetPhysicsProperties( int surfaceDataIndex, float *density, float *thickness, float *friction, float *elasticity ) const
{ static_cast<const JoltPhysicsSurfaceProps*>(this)->GetPhysicsProperties( surfaceDataIndex, density, thickness, friction, elasticity ); }
surfacedata_t * IPhysicsSurfaceProps::GetSurfaceData( int surfaceDataIndex )
{ return static_cast<JoltPhysicsSurfaceProps*>(this)->GetSurfaceData( surfaceDataIndex ); }
const char * IPhysicsSurfaceProps::GetString( unsigned short stringTableIndex ) const
{ return static_cast<const JoltPhysicsSurfaceProps*>(this)->GetString( stringTableIndex ); }
const char * IPhysicsSurfaceProps::GetPropName( int surfaceDataIndex ) const
{ return static_cast<const JoltPhysicsSurfaceProps*>(this)->GetPropName( surfaceDataIndex ); }
void IPhysicsSurfaceProps::SetWorldMaterialIndexTable( int *pMapArray, int mapSize )
{ static_cast<JoltPhysicsSurfaceProps*>(this)->SetWorldMaterialIndexTable( pMapArray, mapSize ); }
void IPhysicsSurfaceProps::GetPhysicsParameters( int surfaceDataIndex, surfacephysicsparams_t *pParamsOut ) const
{ static_cast<const JoltPhysicsSurfaceProps*>(this)->GetPhysicsParameters( surfaceDataIndex, pParamsOut ); }
