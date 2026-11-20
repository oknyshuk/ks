import ks.tier1.interfaces;
import ks.tier1.keyvalues;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IDebugTextureInfo facade class to the implementation.
// Included by the TU that defines the implementation, which must declare DebugTextureInfoImpl() returning it.

// IDebugTextureInfo
void IDebugTextureInfo::EnableDebugTextureList( bool bEnable )
{ DebugTextureInfoImpl().EnableDebugTextureList( bEnable ); }
void IDebugTextureInfo::EnableGetAllTextures( bool bEnable )
{ DebugTextureInfoImpl().EnableGetAllTextures( bEnable ); }
KeyValues* IDebugTextureInfo::LockDebugTextureList( void )
{ return DebugTextureInfoImpl().LockDebugTextureList(); }
void IDebugTextureInfo::UnlockDebugTextureList( void )
{ DebugTextureInfoImpl().UnlockDebugTextureList(); }
int IDebugTextureInfo::GetTextureMemoryUsed( TextureMemoryType eTextureMemory )
{ return DebugTextureInfoImpl().GetTextureMemoryUsed( eTextureMemory ); }
bool IDebugTextureInfo::IsDebugTextureListFresh( int numFramesAllowed )
{ return DebugTextureInfoImpl().IsDebugTextureListFresh( numFramesAllowed ); }
bool IDebugTextureInfo::SetDebugTextureRendering( bool bEnable )
{ return DebugTextureInfoImpl().SetDebugTextureRendering( bEnable ); }

