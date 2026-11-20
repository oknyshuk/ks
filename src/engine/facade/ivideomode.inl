import ks.core.basetypes;
import ks.core.types;
import ks.tier1.byte_writer;
import ks.tier1.keyvalues;
import ks.tier1.kvpacker;
import ks.tier1.timeutils;
import ks.tier1.uniqueid;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// CVideoMode_Common calls the overrides of CVideoMode_MaterialSystem; IVideoMode forwards to it.
// Included by sys_getmodes.cpp.

void CVideoMode_Common::ChangeDisplaySettingsToFullscreen( int nWidth, int nHeight, int nBPP, bool bDesktopFriendlyFullscreen )
{ static_cast<CVideoMode_MaterialSystem*>( this )->ChangeDisplaySettingsToFullscreen( nWidth, nHeight, nBPP, bDesktopFriendlyFullscreen ); }
bool CVideoMode_Common::Init()
{ return static_cast<CVideoMode_MaterialSystem*>( this )->Init(); }
void CVideoMode_Common::ReadScreenPixels( int x, int y, int w, int h, void *pBuffer, ImageFormat format )
{ static_cast<CVideoMode_MaterialSystem*>( this )->ReadScreenPixels( x, y, w, h, pBuffer, format ); }
void CVideoMode_Common::ReleaseFullScreen()
{ static_cast<CVideoMode_MaterialSystem*>( this )->ReleaseFullScreen(); }
void CVideoMode_Common::ReleaseVideo()
{ static_cast<CVideoMode_MaterialSystem*>( this )->ReleaseVideo(); }
void CVideoMode_Common::RestoreVideo()
{ static_cast<CVideoMode_MaterialSystem*>( this )->RestoreVideo(); }
void CVideoMode_Common::Shutdown()
{ static_cast<CVideoMode_MaterialSystem*>( this )->Shutdown(); }

bool IVideoMode::Init()
{ return static_cast<CVideoMode_MaterialSystem*>(this)->Init(); }
void IVideoMode::Shutdown( void )
{ static_cast<CVideoMode_MaterialSystem*>(this)->Shutdown(); }
void IVideoMode::DrawStartupGraphic()
{ static_cast<CVideoMode_MaterialSystem*>(this)->DrawStartupGraphic(); }
bool IVideoMode::CreateGameWindow( int nWidth, int nHeight, bool bWindowed, bool bNoWindowBorder )
{ return static_cast<CVideoMode_MaterialSystem*>(this)->CreateGameWindow( nWidth, nHeight, bWindowed, bNoWindowBorder ); }
void IVideoMode::SetGameWindow( void *hWnd )
{ static_cast<CVideoMode_MaterialSystem*>(this)->SetGameWindow( hWnd ); }
bool IVideoMode::SetMode( int nWidth, int nHeight, bool bWindowed, bool bNoWindowBorder )
{ return static_cast<CVideoMode_MaterialSystem*>(this)->SetMode( nWidth, nHeight, bWindowed, bNoWindowBorder ); }
int IVideoMode::GetModeCount( void )
{ return static_cast<CVideoMode_MaterialSystem*>(this)->GetModeCount(); }
struct vmode_s * IVideoMode::GetMode( int num )
{ return static_cast<CVideoMode_MaterialSystem*>(this)->GetMode( num ); }
void IVideoMode::UpdateWindowPosition( void )
{ static_cast<CVideoMode_MaterialSystem*>(this)->UpdateWindowPosition(); }
void IVideoMode::RestoreVideo( void )
{ static_cast<CVideoMode_MaterialSystem*>(this)->RestoreVideo(); }
void IVideoMode::ReleaseVideo( void )
{ static_cast<CVideoMode_MaterialSystem*>(this)->ReleaseVideo(); }
void IVideoMode::DrawNullBackground( void *hdc, int w, int h )
{ static_cast<CVideoMode_MaterialSystem*>(this)->DrawNullBackground( hdc, w, h ); }
void IVideoMode::InvalidateWindow()
{ static_cast<CVideoMode_MaterialSystem*>(this)->InvalidateWindow(); }
int IVideoMode::GetModeWidth() const
{ return static_cast<const CVideoMode_MaterialSystem*>(this)->GetModeWidth(); }
int IVideoMode::GetModeHeight() const
{ return static_cast<const CVideoMode_MaterialSystem*>(this)->GetModeHeight(); }
bool IVideoMode::IsWindowedMode() const
{ return static_cast<const CVideoMode_MaterialSystem*>(this)->IsWindowedMode(); }
bool IVideoMode::NoWindowBorder() const
{ return static_cast<const CVideoMode_MaterialSystem*>(this)->NoWindowBorder(); }
const vrect_t & IVideoMode::GetClientViewRect() const
{ return static_cast<const CVideoMode_MaterialSystem*>(this)->GetClientViewRect(); }
void IVideoMode::SetClientViewRect( const vrect_t &viewRect )
{ static_cast<CVideoMode_MaterialSystem*>(this)->SetClientViewRect( viewRect ); }
void IVideoMode::MarkClientViewRectDirty()
{ static_cast<CVideoMode_MaterialSystem*>(this)->MarkClientViewRectDirty(); }
void IVideoMode::TakeSnapshotTGA( const char *pFileName )
{ static_cast<CVideoMode_MaterialSystem*>(this)->TakeSnapshotTGA( pFileName ); }
void IVideoMode::TakeSnapshotTGARect( const char *pFilename, int x, int y, int w, int h, int resampleWidth, int resampleHeight, bool bPFM, CubeMapFaceIndex_t faceIndex )
{ static_cast<CVideoMode_MaterialSystem*>(this)->TakeSnapshotTGARect( pFilename, x, y, w, h, resampleWidth, resampleHeight, bPFM, faceIndex ); }
void IVideoMode::WriteMovieFrame( const MovieInfo_t& info )
{ static_cast<CVideoMode_MaterialSystem*>(this)->WriteMovieFrame( info ); }
void IVideoMode::TakeSnapshotJPEG( const char *pFileName, int quality )
{ static_cast<CVideoMode_MaterialSystem*>(this)->TakeSnapshotJPEG( pFileName, quality ); }
bool IVideoMode::TakeSnapshotJPEGToBuffer( ks::byte_writer& buf, int quality )
{ return static_cast<CVideoMode_MaterialSystem*>(this)->TakeSnapshotJPEGToBuffer( buf, quality ); }
bool IVideoMode::SyncToActualWindowSize()
{ return static_cast<CVideoMode_MaterialSystem*>(this)->SyncToActualWindowSize(); }
void IVideoMode::OnWindowSizeChanged( int nNewWidth, int nNewHeight )
{ static_cast<CVideoMode_MaterialSystem*>(this)->OnWindowSizeChanged( nNewWidth, nNewHeight ); }

