// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IGame class to CGame. Included by sys_mainwind.cpp.

bool IGame::Init( void *pvInstance )
{ return static_cast<CGame*>(this)->Init( pvInstance ); }
bool IGame::Shutdown( void )
{ return static_cast<CGame*>(this)->Shutdown(); }
bool IGame::CreateGameWindow( void )
{ return static_cast<CGame*>(this)->CreateGameWindow(); }
void IGame::DestroyGameWindow( void )
{ static_cast<CGame*>(this)->DestroyGameWindow(); }
void IGame::SetGameWindow( void* hWnd )
{ static_cast<CGame*>(this)->SetGameWindow( hWnd ); }
bool IGame::InputAttachToGameWindow()
{ return static_cast<CGame*>(this)->InputAttachToGameWindow(); }
void IGame::InputDetachFromGameWindow()
{ static_cast<CGame*>(this)->InputDetachFromGameWindow(); }
void IGame::PlayStartupVideos( void )
{ static_cast<CGame*>(this)->PlayStartupVideos(); }
void* IGame::GetMainWindow( void )
{ return static_cast<CGame*>(this)->GetMainWindow(); }
void** IGame::GetMainWindowAddress( void )
{ return static_cast<CGame*>(this)->GetMainWindowAddress(); }
void IGame::GetDesktopInfo( int &width, int &height, int &refreshrate )
{ static_cast<CGame*>(this)->GetDesktopInfo( width, height, refreshrate ); }
void IGame::SetWindowXY( int x, int y )
{ static_cast<CGame*>(this)->SetWindowXY( x, y ); }
void IGame::SetWindowSize( int w, int h )
{ static_cast<CGame*>(this)->SetWindowSize( w, h ); }
void IGame::GetWindowRect( int *x, int *y, int *w, int *h )
{ static_cast<CGame*>(this)->GetWindowRect( x, y, w, h ); }
bool IGame::IsActiveApp( void )
{ return static_cast<CGame*>(this)->IsActiveApp(); }
void IGame::DispatchAllStoredGameMessages()
{ static_cast<CGame*>(this)->DispatchAllStoredGameMessages(); }
void IGame::OnScreenSizeChanged( int nOldWidth, int nOldHeight )
{ static_cast<CGame*>(this)->OnScreenSizeChanged( nOldWidth, nOldHeight ); }

