// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the ISplitScreen class to CSplitScreen. Included by cl_splitscreen.cpp.

bool ISplitScreen::Init()
{ return static_cast<CSplitScreen*>(this)->Init(); }
void ISplitScreen::Shutdown()
{ static_cast<CSplitScreen*>(this)->Shutdown(); }
bool ISplitScreen::AddSplitScreenUser( int nSlot, int nPlayerIndex )
{ return static_cast<CSplitScreen*>(this)->AddSplitScreenUser( nSlot, nPlayerIndex ); }
bool ISplitScreen::AddBaseUser( int nSlot, int nPlayerIndex )
{ return static_cast<CSplitScreen*>(this)->AddBaseUser( nSlot, nPlayerIndex ); }
bool ISplitScreen::RemoveSplitScreenUser( int nSlot, int nPlayerIndex )
{ return static_cast<CSplitScreen*>(this)->RemoveSplitScreenUser( nSlot, nPlayerIndex ); }
int ISplitScreen::GetActiveSplitScreenPlayerSlot()
{ return static_cast<CSplitScreen*>(this)->GetActiveSplitScreenPlayerSlot(); }
int ISplitScreen::SetActiveSplitScreenPlayerSlot( int nSlot )
{ return static_cast<CSplitScreen*>(this)->SetActiveSplitScreenPlayerSlot( nSlot ); }
bool ISplitScreen::IsValidSplitScreenSlot( int nSlot )
{ return static_cast<CSplitScreen*>(this)->IsValidSplitScreenSlot( nSlot ); }
int ISplitScreen::FirstValidSplitScreenSlot()
{ return static_cast<CSplitScreen*>(this)->FirstValidSplitScreenSlot(); }
int ISplitScreen::NextValidSplitScreenSlot( int nPreviousSlot )
{ return static_cast<CSplitScreen*>(this)->NextValidSplitScreenSlot( nPreviousSlot ); }
int ISplitScreen::GetNumSplitScreenPlayers()
{ return static_cast<CSplitScreen*>(this)->GetNumSplitScreenPlayers(); }
int ISplitScreen::GetSplitScreenPlayerEntity( int nSlot )
{ return static_cast<CSplitScreen*>(this)->GetSplitScreenPlayerEntity( nSlot ); }
INetChannel * ISplitScreen::GetSplitScreenPlayerNetChan( int nSlot )
{ return static_cast<CSplitScreen*>(this)->GetSplitScreenPlayerNetChan( nSlot ); }
bool ISplitScreen::IsDisconnecting( int nSlot )
{ return static_cast<CSplitScreen*>(this)->IsDisconnecting( nSlot ); }
void ISplitScreen::SetDisconnecting( int nSlot, bool bState )
{ static_cast<CSplitScreen*>(this)->SetDisconnecting( nSlot, bState ); }
bool ISplitScreen::SetLocalPlayerIsResolvable( char const *pchContext, int nLine, bool bResolvable )
{ return static_cast<CSplitScreen*>(this)->SetLocalPlayerIsResolvable( pchContext, nLine, bResolvable ); }
bool ISplitScreen::IsLocalPlayerResolvable()
{ return static_cast<CSplitScreen*>(this)->IsLocalPlayerResolvable(); }

