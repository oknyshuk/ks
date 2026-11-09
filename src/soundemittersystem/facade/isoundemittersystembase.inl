// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the ISoundEmitterSystemBase class to CSoundEmitterSystemBase. Included by soundemittersystembase.cpp.

int ISoundEmitterSystemBase::GetSoundIndex( const char *pName ) const
{ return static_cast<const CSoundEmitterSystemBase*>(this)->GetSoundIndex( pName ); }
bool ISoundEmitterSystemBase::IsValidIndex( int index )
{ return static_cast<CSoundEmitterSystemBase*>(this)->IsValidIndex( index ); }
int ISoundEmitterSystemBase::GetSoundCount( void )
{ return static_cast<CSoundEmitterSystemBase*>(this)->GetSoundCount(); }
const char * ISoundEmitterSystemBase::GetSoundName( int index )
{ return static_cast<CSoundEmitterSystemBase*>(this)->GetSoundName( index ); }
bool ISoundEmitterSystemBase::GetParametersForSound( const char *soundname, CSoundParameters& params, gender_t gender, bool isbeingemitted )
{ return static_cast<CSoundEmitterSystemBase*>(this)->GetParametersForSound( soundname, params, gender, isbeingemitted ); }
const char * ISoundEmitterSystemBase::GetWaveName( WaveNameId_t sym )
{ return static_cast<CSoundEmitterSystemBase*>(this)->GetWaveName( sym ); }
WaveNameId_t ISoundEmitterSystemBase::AddWaveName( const char *name )
{ return static_cast<CSoundEmitterSystemBase*>(this)->AddWaveName( name ); }
soundlevel_t ISoundEmitterSystemBase::LookupSoundLevel( const char *soundname )
{ return static_cast<CSoundEmitterSystemBase*>(this)->LookupSoundLevel( soundname ); }
const char * ISoundEmitterSystemBase::GetWavFileForSound( const char *soundname, char const *actormodel )
{ return static_cast<CSoundEmitterSystemBase*>(this)->GetWavFileForSound( soundname, actormodel ); }
const char * ISoundEmitterSystemBase::GetWavFileForSound( const char *soundname, gender_t gender )
{ return static_cast<CSoundEmitterSystemBase*>(this)->GetWavFileForSound( soundname, gender ); }
int ISoundEmitterSystemBase::CheckForMissingWavFiles( bool verbose )
{ return static_cast<CSoundEmitterSystemBase*>(this)->CheckForMissingWavFiles( verbose ); }
const char * ISoundEmitterSystemBase::GetSourceFileForSound( int index ) const
{ return static_cast<const CSoundEmitterSystemBase*>(this)->GetSourceFileForSound( index ); }
int ISoundEmitterSystemBase::First() const
{ return static_cast<const CSoundEmitterSystemBase*>(this)->First(); }
int ISoundEmitterSystemBase::Next( int i ) const
{ return static_cast<const CSoundEmitterSystemBase*>(this)->Next( i ); }
int ISoundEmitterSystemBase::InvalidIndex() const
{ return static_cast<const CSoundEmitterSystemBase*>(this)->InvalidIndex(); }
CSoundParametersInternal * ISoundEmitterSystemBase::InternalGetParametersForSound( int index )
{ return static_cast<CSoundEmitterSystemBase*>(this)->InternalGetParametersForSound( index ); }
bool ISoundEmitterSystemBase::AddSound( const char *soundname, const char *scriptfile, const CSoundParametersInternal& params )
{ return static_cast<CSoundEmitterSystemBase*>(this)->AddSound( soundname, scriptfile, params ); }
void ISoundEmitterSystemBase::RemoveSound( const char *soundname )
{ static_cast<CSoundEmitterSystemBase*>(this)->RemoveSound( soundname ); }
void ISoundEmitterSystemBase::MoveSound( const char *soundname, const char *newscript )
{ static_cast<CSoundEmitterSystemBase*>(this)->MoveSound( soundname, newscript ); }
void ISoundEmitterSystemBase::RenameSound( const char *soundname, const char *newname )
{ static_cast<CSoundEmitterSystemBase*>(this)->RenameSound( soundname, newname ); }
void ISoundEmitterSystemBase::UpdateSoundParameters( const char *soundname, const CSoundParametersInternal& params )
{ static_cast<CSoundEmitterSystemBase*>(this)->UpdateSoundParameters( soundname, params ); }
int ISoundEmitterSystemBase::GetNumSoundScripts() const
{ return static_cast<const CSoundEmitterSystemBase*>(this)->GetNumSoundScripts(); }
char const * ISoundEmitterSystemBase::GetSoundScriptName( int index ) const
{ return static_cast<const CSoundEmitterSystemBase*>(this)->GetSoundScriptName( index ); }
bool ISoundEmitterSystemBase::IsSoundScriptDirty( int index ) const
{ return static_cast<const CSoundEmitterSystemBase*>(this)->IsSoundScriptDirty( index ); }
int ISoundEmitterSystemBase::FindSoundScript( const char *name ) const
{ return static_cast<const CSoundEmitterSystemBase*>(this)->FindSoundScript( name ); }
void ISoundEmitterSystemBase::SaveChangesToSoundScript( int scriptindex )
{ static_cast<CSoundEmitterSystemBase*>(this)->SaveChangesToSoundScript( scriptindex ); }
void ISoundEmitterSystemBase::ExpandSoundNameMacros( CSoundParametersInternal& params, char const *wavename )
{ static_cast<CSoundEmitterSystemBase*>(this)->ExpandSoundNameMacros( params, wavename ); }
gender_t ISoundEmitterSystemBase::GetActorGender( char const *actormodel )
{ return static_cast<CSoundEmitterSystemBase*>(this)->GetActorGender( actormodel ); }
void ISoundEmitterSystemBase::GenderExpandString( char const *actormodel, char const *in, char *out, int maxlen )
{ static_cast<CSoundEmitterSystemBase*>(this)->GenderExpandString( actormodel, in, out, maxlen ); }
void ISoundEmitterSystemBase::GenderExpandString( gender_t gender, char const *in, char *out, int maxlen )
{ static_cast<CSoundEmitterSystemBase*>(this)->GenderExpandString( gender, in, out, maxlen ); }
bool ISoundEmitterSystemBase::IsUsingGenderToken( char const *soundname )
{ return static_cast<CSoundEmitterSystemBase*>(this)->IsUsingGenderToken( soundname ); }
unsigned int ISoundEmitterSystemBase::GetManifestFileTimeChecksum()
{ return static_cast<CSoundEmitterSystemBase*>(this)->GetManifestFileTimeChecksum(); }
bool ISoundEmitterSystemBase::GetParametersForSoundEx( const char *soundname, HSOUNDSCRIPTHASH& handle, CSoundParameters& params, gender_t gender, bool isbeingemitted )
{ return static_cast<CSoundEmitterSystemBase*>(this)->GetParametersForSoundEx( soundname, handle, params, gender, isbeingemitted ); }
soundlevel_t ISoundEmitterSystemBase::LookupSoundLevelByHandle( char const *soundname, HSOUNDSCRIPTHASH& handle )
{ return static_cast<CSoundEmitterSystemBase*>(this)->LookupSoundLevelByHandle( soundname, handle ); }
KeyValues * ISoundEmitterSystemBase::GetOperatorKVByHandle( HSOUNDSCRIPTHASH& handle )
{ return static_cast<CSoundEmitterSystemBase*>(this)->GetOperatorKVByHandle( handle ); }
char const * ISoundEmitterSystemBase::GetSoundNameForHash( HSOUNDSCRIPTHASH hash ) const
{ return static_cast<const CSoundEmitterSystemBase*>(this)->GetSoundNameForHash( hash ); }
int ISoundEmitterSystemBase::GetSoundIndexForHash( HSOUNDSCRIPTHASH hash ) const
{ return static_cast<const CSoundEmitterSystemBase*>(this)->GetSoundIndexForHash( hash ); }
HSOUNDSCRIPTHASH ISoundEmitterSystemBase::HashSoundName( char const *pchSndName ) const
{ return static_cast<const CSoundEmitterSystemBase*>(this)->HashSoundName( pchSndName ); }
bool ISoundEmitterSystemBase::IsValidHash( HSOUNDSCRIPTHASH hash ) const
{ return static_cast<const CSoundEmitterSystemBase*>(this)->IsValidHash( hash ); }
void ISoundEmitterSystemBase::DescribeSound( char const *soundname )
{ static_cast<CSoundEmitterSystemBase*>(this)->DescribeSound( soundname ); }
void ISoundEmitterSystemBase::Flush()
{ static_cast<CSoundEmitterSystemBase*>(this)->Flush(); }
void ISoundEmitterSystemBase::AddSoundsFromFile( const char *filename, bool bPreload, bool bAutoCache, bool bIsOverride )
{ static_cast<CSoundEmitterSystemBase*>(this)->AddSoundsFromFile( filename, bPreload, bAutoCache, bIsOverride ); }
bool ISoundEmitterSystemBase::Connect( CreateInterfaceFn factory )
{ return static_cast<CSoundEmitterSystemBase*>(this)->Connect( factory ); }
void ISoundEmitterSystemBase::Disconnect()
{ static_cast<CSoundEmitterSystemBase*>(this)->Disconnect(); }
void *ISoundEmitterSystemBase::QueryInterface( const char *pInterfaceName )
{ return static_cast<CSoundEmitterSystemBase*>(this)->QueryInterface( pInterfaceName ); }
InitReturnVal_t ISoundEmitterSystemBase::Init()
{ return static_cast<CSoundEmitterSystemBase*>(this)->Init(); }
void ISoundEmitterSystemBase::Shutdown()
{ static_cast<CSoundEmitterSystemBase*>(this)->Shutdown(); }

