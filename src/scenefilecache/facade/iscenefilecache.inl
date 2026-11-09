// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the ISceneFileCache class to CSceneFileCache. Included by SceneFileCache.cpp.

std::size_t ISceneFileCache::GetSceneBufferSize( char const *filename )
{ return static_cast<CSceneFileCache*>(this)->GetSceneBufferSize( filename ); }
bool ISceneFileCache::GetSceneData( char const *filename, byte *buf, std::size_t bufsize )
{ return static_cast<CSceneFileCache*>(this)->GetSceneData( filename, buf, bufsize ); }
bool ISceneFileCache::GetSceneCachedData( char const *pFilename, SceneCachedData_t *pData )
{ return static_cast<CSceneFileCache*>(this)->GetSceneCachedData( pFilename, pData ); }
short ISceneFileCache::GetSceneCachedSound( int iScene, int iSound )
{ return static_cast<CSceneFileCache*>(this)->GetSceneCachedSound( iScene, iSound ); }
const char * ISceneFileCache::GetSceneString( short stringId )
{ return static_cast<CSceneFileCache*>(this)->GetSceneString( stringId ); }
void ISceneFileCache::Reload()
{ static_cast<CSceneFileCache*>(this)->Reload(); }
bool ISceneFileCache::Connect( CreateInterfaceFn factory )
{ return static_cast<CSceneFileCache*>(this)->Connect( factory ); }
void ISceneFileCache::Disconnect()
{ static_cast<CSceneFileCache*>(this)->Disconnect(); }
void *ISceneFileCache::QueryInterface( const char *pInterfaceName )
{ return static_cast<CSceneFileCache*>(this)->QueryInterface( pInterfaceName ); }
InitReturnVal_t ISceneFileCache::Init()
{ return static_cast<CSceneFileCache*>(this)->Init(); }
void ISceneFileCache::Shutdown()
{ static_cast<CSceneFileCache*>(this)->Shutdown(); }

