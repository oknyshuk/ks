import ks.core.platform;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IPlayerManager class to PlayerManager. Included by the source that defines PlayerManager.

IPlayerLocal * IPlayerManager::GetLocalPlayer( int iController )
{ return static_cast<PlayerManager*>(this)->GetLocalPlayer( iController ); }
IPlayerFriend * IPlayerManager::GetFriendByXUID( XUID xuid )
{ return static_cast<PlayerManager*>(this)->GetFriendByXUID( xuid ); }
IPlayer * IPlayerManager::FindPlayer( XUID xuid )
{ return static_cast<PlayerManager*>(this)->FindPlayer( xuid ); }
