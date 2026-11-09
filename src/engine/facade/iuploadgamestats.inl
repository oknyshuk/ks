// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IUploadGameStats class to CUploadGameStats. Included by sv_uploadgamestats.cpp.

bool IUploadGameStats::UploadGameStats( char const *mapname, unsigned int blobversion, unsigned int blobsize, const void *pvBlobData )
{ return static_cast<CUploadGameStats*>(this)->UploadGameStats( mapname, blobversion, blobsize, pvBlobData ); }
void IUploadGameStats::InitConnection( void )
{ static_cast<CUploadGameStats*>(this)->InitConnection(); }
void IUploadGameStats::UpdateConnection( void )
{ static_cast<CUploadGameStats*>(this)->UpdateConnection(); }
bool IUploadGameStats::IsGameStatsLoggingEnabled()
{ return static_cast<CUploadGameStats*>(this)->IsGameStatsLoggingEnabled(); }
void IUploadGameStats::GetPseudoUniqueId( char *buf, std::size_t bufsize )
{ static_cast<CUploadGameStats*>(this)->GetPseudoUniqueId( buf, bufsize ); }
bool IUploadGameStats::IsCyberCafeUser( void )
{ return static_cast<CUploadGameStats*>(this)->IsCyberCafeUser(); }
bool IUploadGameStats::IsHDREnabled( void )
{ return static_cast<CUploadGameStats*>(this)->IsHDREnabled(); }

