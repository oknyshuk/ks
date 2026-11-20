import ks.core.basetypes;
import ks.core.wchartypes;
import ks.math.compressed_vector;
import ks.math.vector;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IStaticPropMgr class to CStaticPropMgr. Included by staticpropmgr.cpp.

void IStaticPropMgr::CreateVPhysicsRepresentations( IPhysicsEnvironment *physenv, IVPhysicsKeyHandler *pDefaults, void *pGameData )
{ static_cast<CStaticPropMgr*>(this)->CreateVPhysicsRepresentations( physenv, pDefaults, pGameData ); }
void IStaticPropMgr::TraceRayAgainstStaticProp( const Ray_t& ray, int staticPropIndex, trace_t& tr )
{ static_cast<CStaticPropMgr*>(this)->TraceRayAgainstStaticProp( ray, staticPropIndex, tr ); }
bool IStaticPropMgr::IsStaticProp( IHandleEntity *pHandleEntity ) const
{ return static_cast<const CStaticPropMgr*>(this)->IsStaticProp( pHandleEntity ); }
bool IStaticPropMgr::IsStaticProp( CBaseHandle handle ) const
{ return static_cast<const CStaticPropMgr*>(this)->IsStaticProp( handle ); }
ICollideable * IStaticPropMgr::GetStaticPropByIndex( int propIndex )
{ return static_cast<CStaticPropMgr*>(this)->GetStaticPropByIndex( propIndex ); }
void IStaticPropMgr::AddDecalToStaticProp( const Vector& rayStart, const Vector& rayEnd, int staticPropIndex, int decalIndex, bool doTrace, trace_t& tr, void *pvProxyUserData, const Vector* saxis, int nAdditionalDecalFlags )
{ static_cast<CStaticPropMgr*>(this)->AddDecalToStaticProp( rayStart, rayEnd, staticPropIndex, decalIndex, doTrace, tr, pvProxyUserData, saxis, nAdditionalDecalFlags ); }
void IStaticPropMgr::AddShadowToStaticProp( unsigned short shadowHandle, IClientRenderable* pRenderable )
{ static_cast<CStaticPropMgr*>(this)->AddShadowToStaticProp( shadowHandle, pRenderable ); }
void IStaticPropMgr::RemoveAllShadowsFromStaticProp( IClientRenderable* pRenderable )
{ static_cast<CStaticPropMgr*>(this)->RemoveAllShadowsFromStaticProp( pRenderable ); }
void IStaticPropMgr::GetStaticPropMaterialColorAndLighting( trace_t* pTrace, int staticPropIndex, Vector& lighting, Vector& matColor )
{ static_cast<CStaticPropMgr*>(this)->GetStaticPropMaterialColorAndLighting( pTrace, staticPropIndex, lighting, matColor ); }
void IStaticPropMgr::GetAllStaticProps( std::vector< ICollideable * > *pOutput )
{ static_cast<CStaticPropMgr*>(this)->GetAllStaticProps( pOutput ); }
void IStaticPropMgr::GetAllStaticPropsInAABB( const Vector &vMins, const Vector &vMaxs, std::vector< ICollideable * > *pOutput )
{ static_cast<CStaticPropMgr*>(this)->GetAllStaticPropsInAABB( vMins, vMaxs, pOutput ); }
void IStaticPropMgr::GetAllStaticPropsInOBB( const Vector &ptOrigin, const Vector &vExtent1, const Vector &vExtent2, const Vector &vExtent3, std::vector< ICollideable * > *pOutput )
{ static_cast<CStaticPropMgr*>(this)->GetAllStaticPropsInOBB( ptOrigin, vExtent1, vExtent2, vExtent3, pOutput ); }
void IStaticPropMgr::DrawStaticProps( IClientRenderable **pProps, const RenderableInstance_t *pInstances, int count, bool bShadowDepth, bool drawVCollideWireframe )
{ static_cast<CStaticPropMgr*>(this)->DrawStaticProps( pProps, pInstances, count, bShadowDepth, drawVCollideWireframe ); }
void IStaticPropMgr::GetLightingOrigins( Vector *pLightingOrigins, int nOriginStride, int nCount, IClientRenderable **ppRenderable, int nRenderableStride )
{ static_cast<CStaticPropMgr*>(this)->GetLightingOrigins( pLightingOrigins, nOriginStride, nCount, ppRenderable, nRenderableStride ); }

bool IStaticPropMgrEngine::Init()
{ return static_cast<CStaticPropMgr*>(this)->Init(); }
void IStaticPropMgrEngine::Shutdown()
{ static_cast<CStaticPropMgr*>(this)->Shutdown(); }
void IStaticPropMgrEngine::LevelInit()
{ static_cast<CStaticPropMgr*>(this)->LevelInit(); }
void IStaticPropMgrEngine::LevelInitClient()
{ static_cast<CStaticPropMgr*>(this)->LevelInitClient(); }
void IStaticPropMgrEngine::LevelShutdownClient()
{ static_cast<CStaticPropMgr*>(this)->LevelShutdownClient(); }
void IStaticPropMgrEngine::LevelShutdown()
{ static_cast<CStaticPropMgr*>(this)->LevelShutdown(); }
void IStaticPropMgrEngine::RecomputeStaticLighting()
{ static_cast<CStaticPropMgr*>(this)->RecomputeStaticLighting(); }
bool IStaticPropMgrEngine::IsPropInPVS( IHandleEntity *pHandleEntity, const byte *pVis ) const
{ return static_cast<const CStaticPropMgr*>(this)->IsPropInPVS( pHandleEntity, pVis ); }
void IStaticPropMgrEngine::DisableCSMRenderingForStaticProp( int staticPropIndex )
{ static_cast<CStaticPropMgr*>(this)->DisableCSMRenderingForStaticProp( staticPropIndex ); }
ICollideable * IStaticPropMgrEngine::GetStaticProp( IHandleEntity *pHandleEntity )
{ return static_cast<CStaticPropMgr*>(this)->GetStaticProp( pHandleEntity ); }
LightCacheHandle_t IStaticPropMgrEngine::GetLightCacheHandleForStaticProp( IHandleEntity *pHandleEntity )
{ return static_cast<CStaticPropMgr*>(this)->GetLightCacheHandleForStaticProp( pHandleEntity ); }
bool IStaticPropMgrEngine::IsStaticProp( IHandleEntity *pHandleEntity ) const
{ return static_cast<const CStaticPropMgr*>(this)->IsStaticProp( pHandleEntity ); }
bool IStaticPropMgrEngine::IsStaticProp( CBaseHandle handle ) const
{ return static_cast<const CStaticPropMgr*>(this)->IsStaticProp( handle ); }
int IStaticPropMgrEngine::GetStaticPropIndex( IHandleEntity *pHandleEntity ) const
{ return static_cast<const CStaticPropMgr*>(this)->GetStaticPropIndex( pHandleEntity ); }
void IStaticPropMgrEngine::ConfigureSystemLevel( int nCPULevel, int nGPULevel )
{ static_cast<CStaticPropMgr*>(this)->ConfigureSystemLevel( nCPULevel, nGPULevel ); }
void IStaticPropMgrEngine::RestoreStaticProps()
{ static_cast<CStaticPropMgr*>(this)->RestoreStaticProps(); }

