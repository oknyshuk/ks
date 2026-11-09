// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IGameUIFuncs class to CGameUIFuncs. Included by sys_dll2.cpp.

bool IGameUIFuncs::IsKeyDown( const char *keyname, bool& isdown )
{ return static_cast<CGameUIFuncs*>(this)->IsKeyDown( keyname, isdown ); }
const char * IGameUIFuncs::GetBindingForButtonCode( ButtonCode_t code )
{ return static_cast<CGameUIFuncs*>(this)->GetBindingForButtonCode( code ); }
ButtonCode_t IGameUIFuncs::GetButtonCodeForBind( const char *pBind, int userId )
{ return static_cast<CGameUIFuncs*>(this)->GetButtonCodeForBind( pBind, userId ); }
void IGameUIFuncs::GetVideoModes( struct vmode_s **liststart, int *count )
{ static_cast<CGameUIFuncs*>(this)->GetVideoModes( liststart, count ); }
void IGameUIFuncs::SetFriendsID( uint friendsID, const char *friendsName )
{ static_cast<CGameUIFuncs*>(this)->SetFriendsID( friendsID, friendsName ); }
void IGameUIFuncs::GetDesktopResolution( int &width, int &height )
{ static_cast<CGameUIFuncs*>(this)->GetDesktopResolution( width, height ); }

