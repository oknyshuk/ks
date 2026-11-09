// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IMatchTitle class to CMatchTitle. Included by the source that defines CMatchTitle.

uint64 IMatchTitle::GetTitleID()
{ return static_cast<CMatchTitle*>(this)->GetTitleID(); }
uint64 IMatchTitle::GetTitleServiceID()
{ return static_cast<CMatchTitle*>(this)->GetTitleServiceID(); }
uint64 IMatchTitle::GetTitleSettingsFlags()
{ return static_cast<CMatchTitle*>(this)->GetTitleSettingsFlags(); }
void IMatchTitle::PrepareNetStartupParams( void *pNetStartupParams )
{ static_cast<CMatchTitle*>(this)->PrepareNetStartupParams( pNetStartupParams ); }
int IMatchTitle::GetTotalNumPlayersSupported()
{ return static_cast<CMatchTitle*>(this)->GetTotalNumPlayersSupported(); }
char const * IMatchTitle::GetGuestPlayerName( int iUserIndex )
{ return static_cast<CMatchTitle*>(this)->GetGuestPlayerName( iUserIndex ); }
TitleDataFieldsDescription_t const * IMatchTitle::DescribeTitleDataStorage()
{ return static_cast<CMatchTitle*>(this)->DescribeTitleDataStorage(); }
TitleAchievementsDescription_t const * IMatchTitle::DescribeTitleAchievements()
{ return static_cast<CMatchTitle*>(this)->DescribeTitleAchievements(); }
TitleAvatarAwardsDescription_t const * IMatchTitle::DescribeTitleAvatarAwards()
{ return static_cast<CMatchTitle*>(this)->DescribeTitleAvatarAwards(); }
void IMatchTitle::PrepareClientForConnect( KeyValues *pSettings )
{ static_cast<CMatchTitle*>(this)->PrepareClientForConnect( pSettings ); }
bool IMatchTitle::StartServerMap( KeyValues *pSettings )
{ return static_cast<CMatchTitle*>(this)->StartServerMap( pSettings ); }
TitleDlcDescription_t const * IMatchTitle::DescribeTitleDlcs()
{ return static_cast<CMatchTitle*>(this)->DescribeTitleDlcs(); }
void IMatchTitle::RunFrame()
{ static_cast<CMatchTitle*>(this)->RunFrame(); }
