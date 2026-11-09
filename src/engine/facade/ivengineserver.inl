// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IVEngineServer class to CVEngineServer. Included by vengineserver_impl.cpp.

void IVEngineServer::ChangeLevel( const char *s1, const char *s2 )
{ static_cast<CVEngineServer*>(this)->ChangeLevel( s1, s2 ); }
int IVEngineServer::IsMapValid( const char *filename )
{ return static_cast<CVEngineServer*>(this)->IsMapValid( filename ); }
bool IVEngineServer::IsDedicatedServer( void )
{ return static_cast<CVEngineServer*>(this)->IsDedicatedServer(); }
int IVEngineServer::IsInEditMode( void )
{ return static_cast<CVEngineServer*>(this)->IsInEditMode(); }
KeyValues* IVEngineServer::GetLaunchOptions( void )
{ return static_cast<CVEngineServer*>(this)->GetLaunchOptions(); }
int IVEngineServer::PrecacheModel( const char *s, bool preload )
{ return static_cast<CVEngineServer*>(this)->PrecacheModel( s, preload ); }
int IVEngineServer::PrecacheSentenceFile( const char *s, bool preload )
{ return static_cast<CVEngineServer*>(this)->PrecacheSentenceFile( s, preload ); }
int IVEngineServer::PrecacheDecal( const char *name, bool preload )
{ return static_cast<CVEngineServer*>(this)->PrecacheDecal( name, preload ); }
int IVEngineServer::PrecacheGeneric( const char *s, bool preload )
{ return static_cast<CVEngineServer*>(this)->PrecacheGeneric( s, preload ); }
bool IVEngineServer::IsModelPrecached( char const *s ) const
{ return static_cast<const CVEngineServer*>(this)->IsModelPrecached( s ); }
bool IVEngineServer::IsDecalPrecached( char const *s ) const
{ return static_cast<const CVEngineServer*>(this)->IsDecalPrecached( s ); }
bool IVEngineServer::IsGenericPrecached( char const *s ) const
{ return static_cast<const CVEngineServer*>(this)->IsGenericPrecached( s ); }
int IVEngineServer::GetClusterForOrigin( const Vector &org )
{ return static_cast<CVEngineServer*>(this)->GetClusterForOrigin( org ); }
int IVEngineServer::GetPVSForCluster( int cluster, int outputpvslength, unsigned char *outputpvs )
{ return static_cast<CVEngineServer*>(this)->GetPVSForCluster( cluster, outputpvslength, outputpvs ); }
bool IVEngineServer::CheckOriginInPVS( const Vector &org, const unsigned char *checkpvs, int checkpvssize )
{ return static_cast<CVEngineServer*>(this)->CheckOriginInPVS( org, checkpvs, checkpvssize ); }
bool IVEngineServer::CheckBoxInPVS( const Vector &mins, const Vector &maxs, const unsigned char *checkpvs, int checkpvssize )
{ return static_cast<CVEngineServer*>(this)->CheckBoxInPVS( mins, maxs, checkpvs, checkpvssize ); }
int IVEngineServer::GetPlayerUserId( const edict_t *e )
{ return static_cast<CVEngineServer*>(this)->GetPlayerUserId( e ); }
const char * IVEngineServer::GetPlayerNetworkIDString( const edict_t *e )
{ return static_cast<CVEngineServer*>(this)->GetPlayerNetworkIDString( e ); }
bool IVEngineServer::IsUserIDInUse( int userID )
{ return static_cast<CVEngineServer*>(this)->IsUserIDInUse( userID ); }
int IVEngineServer::GetLoadingProgressForUserID( int userID )
{ return static_cast<CVEngineServer*>(this)->GetLoadingProgressForUserID( userID ); }
int IVEngineServer::GetEntityCount( void )
{ return static_cast<CVEngineServer*>(this)->GetEntityCount(); }
INetChannelInfo* IVEngineServer::GetPlayerNetInfo( int playerIndex )
{ return static_cast<CVEngineServer*>(this)->GetPlayerNetInfo( playerIndex ); }
edict_t * IVEngineServer::CreateEdict( int iForceEdictIndex )
{ return static_cast<CVEngineServer*>(this)->CreateEdict( iForceEdictIndex ); }
void IVEngineServer::RemoveEdict( edict_t *e )
{ static_cast<CVEngineServer*>(this)->RemoveEdict( e ); }
void * IVEngineServer::PvAllocEntPrivateData( long cb )
{ return static_cast<CVEngineServer*>(this)->PvAllocEntPrivateData( cb ); }
void IVEngineServer::FreeEntPrivateData( void *pEntity )
{ static_cast<CVEngineServer*>(this)->FreeEntPrivateData( pEntity ); }
void IVEngineServer::EmitAmbientSound( int entindex, const Vector &pos, const char *samp, float vol, soundlevel_t soundlevel, int fFlags, int pitch, float delay )
{ static_cast<CVEngineServer*>(this)->EmitAmbientSound( entindex, pos, samp, vol, soundlevel, fFlags, pitch, delay ); }
void IVEngineServer::FadeClientVolume( const edict_t *pEdict, float fadePercent, float fadeOutSeconds, float holdTime, float fadeInSeconds )
{ static_cast<CVEngineServer*>(this)->FadeClientVolume( pEdict, fadePercent, fadeOutSeconds, holdTime, fadeInSeconds ); }
int IVEngineServer::SentenceGroupPick( int groupIndex, char *name, int nameBufLen )
{ return static_cast<CVEngineServer*>(this)->SentenceGroupPick( groupIndex, name, nameBufLen ); }
int IVEngineServer::SentenceGroupPickSequential( int groupIndex, char *name, int nameBufLen, int sentenceIndex, int reset )
{ return static_cast<CVEngineServer*>(this)->SentenceGroupPickSequential( groupIndex, name, nameBufLen, sentenceIndex, reset ); }
int IVEngineServer::SentenceIndexFromName( const char *pSentenceName )
{ return static_cast<CVEngineServer*>(this)->SentenceIndexFromName( pSentenceName ); }
const char * IVEngineServer::SentenceNameFromIndex( int sentenceIndex )
{ return static_cast<CVEngineServer*>(this)->SentenceNameFromIndex( sentenceIndex ); }
int IVEngineServer::SentenceGroupIndexFromName( const char *pGroupName )
{ return static_cast<CVEngineServer*>(this)->SentenceGroupIndexFromName( pGroupName ); }
const char * IVEngineServer::SentenceGroupNameFromIndex( int groupIndex )
{ return static_cast<CVEngineServer*>(this)->SentenceGroupNameFromIndex( groupIndex ); }
float IVEngineServer::SentenceLength( int sentenceIndex )
{ return static_cast<CVEngineServer*>(this)->SentenceLength( sentenceIndex ); }
void IVEngineServer::ServerCommand( const char *str )
{ static_cast<CVEngineServer*>(this)->ServerCommand( str ); }
void IVEngineServer::ServerExecute( void )
{ static_cast<CVEngineServer*>(this)->ServerExecute(); }
void IVEngineServer::LightStyle( int style, const char *val )
{ static_cast<CVEngineServer*>(this)->LightStyle( style, val ); }
void IVEngineServer::StaticDecal( const Vector &originInEntitySpace, int decalIndex, int entityIndex, int modelIndex, bool lowpriority )
{ static_cast<CVEngineServer*>(this)->StaticDecal( originInEntitySpace, decalIndex, entityIndex, modelIndex, lowpriority ); }
void IVEngineServer::Message_DetermineMulticastRecipients( bool usepas, const Vector& origin, CPlayerBitVec& playerbits )
{ static_cast<CVEngineServer*>(this)->Message_DetermineMulticastRecipients( usepas, origin, playerbits ); }
bf_write * IVEngineServer::EntityMessageBegin( int ent_index, ServerClass * ent_class, bool reliable )
{ return static_cast<CVEngineServer*>(this)->EntityMessageBegin( ent_index, ent_class, reliable ); }
void IVEngineServer::MessageEnd( void )
{ static_cast<CVEngineServer*>(this)->MessageEnd(); }
void IVEngineServer::SendUserMessage( IRecipientFilter& filter, int message, const void *data, int size )
{ static_cast<CVEngineServer*>(this)->SendUserMessage( filter, message, data, size ); }
void IVEngineServer::ClientPrintf( edict_t *pEdict, const char *szMsg )
{ static_cast<CVEngineServer*>(this)->ClientPrintf( pEdict, szMsg ); }
void IVEngineServer::SetView( const edict_t *pClient, const edict_t *pViewent )
{ static_cast<CVEngineServer*>(this)->SetView( pClient, pViewent ); }
void IVEngineServer::CrosshairAngle( const edict_t *pClient, float pitch, float yaw )
{ static_cast<CVEngineServer*>(this)->CrosshairAngle( pClient, pitch, yaw ); }
void IVEngineServer::GetGameDir( char *szGetGameDir, int maxlength )
{ static_cast<CVEngineServer*>(this)->GetGameDir( szGetGameDir, maxlength ); }
int IVEngineServer::CompareFileTime( const char *filename1, const char *filename2, int *iCompare )
{ return static_cast<CVEngineServer*>(this)->CompareFileTime( filename1, filename2, iCompare ); }
bool IVEngineServer::LockNetworkStringTables( bool lock )
{ return static_cast<CVEngineServer*>(this)->LockNetworkStringTables( lock ); }
edict_t * IVEngineServer::CreateFakeClient( const char *netname )
{ return static_cast<CVEngineServer*>(this)->CreateFakeClient( netname ); }
const char * IVEngineServer::GetClientConVarValue( int clientIndex, const char *name )
{ return static_cast<CVEngineServer*>(this)->GetClientConVarValue( clientIndex, name ); }
const char * IVEngineServer::ParseFile( const char *data, char *token, int maxlen )
{ return static_cast<CVEngineServer*>(this)->ParseFile( data, token, maxlen ); }
bool IVEngineServer::CopyFile( const char *source, const char *destination )
{ return static_cast<CVEngineServer*>(this)->CopyFile( source, destination ); }
void IVEngineServer::ResetPVS( byte *pvs, int pvssize )
{ static_cast<CVEngineServer*>(this)->ResetPVS( pvs, pvssize ); }
void IVEngineServer::AddOriginToPVS( const Vector &origin )
{ static_cast<CVEngineServer*>(this)->AddOriginToPVS( origin ); }
void IVEngineServer::SetAreaPortalState( int portalNumber, int isOpen )
{ static_cast<CVEngineServer*>(this)->SetAreaPortalState( portalNumber, isOpen ); }
void IVEngineServer::PlaybackTempEntity( IRecipientFilter& filter, float delay, const void *pSender, const SendTable *pST, int classID )
{ static_cast<CVEngineServer*>(this)->PlaybackTempEntity( filter, delay, pSender, pST, classID ); }
int IVEngineServer::CheckHeadnodeVisible( int nodenum, const byte *pvs, int vissize )
{ return static_cast<CVEngineServer*>(this)->CheckHeadnodeVisible( nodenum, pvs, vissize ); }
int IVEngineServer::CheckAreasConnected( int area1, int area2 )
{ return static_cast<CVEngineServer*>(this)->CheckAreasConnected( area1, area2 ); }
int IVEngineServer::GetArea( const Vector &origin )
{ return static_cast<CVEngineServer*>(this)->GetArea( origin ); }
void IVEngineServer::GetAreaBits( int area, unsigned char *bits, int buflen )
{ static_cast<CVEngineServer*>(this)->GetAreaBits( area, bits, buflen ); }
bool IVEngineServer::GetAreaPortalPlane( Vector const &vViewOrigin, int portalKey, VPlane *pPlane )
{ return static_cast<CVEngineServer*>(this)->GetAreaPortalPlane( vViewOrigin, portalKey, pPlane ); }
bool IVEngineServer::LoadGameState( char const *pMapName, bool createPlayers )
{ return static_cast<CVEngineServer*>(this)->LoadGameState( pMapName, createPlayers ); }
void IVEngineServer::LoadAdjacentEnts( const char *pOldLevel, const char *pLandmarkName )
{ static_cast<CVEngineServer*>(this)->LoadAdjacentEnts( pOldLevel, pLandmarkName ); }
void IVEngineServer::ClearSaveDir()
{ static_cast<CVEngineServer*>(this)->ClearSaveDir(); }
const char* IVEngineServer::GetMapEntitiesString()
{ return static_cast<CVEngineServer*>(this)->GetMapEntitiesString(); }
client_textmessage_t * IVEngineServer::TextMessageGet( const char *pName )
{ return static_cast<CVEngineServer*>(this)->TextMessageGet( pName ); }
void IVEngineServer::LogPrint( const char *msg )
{ static_cast<CVEngineServer*>(this)->LogPrint( msg ); }
bool IVEngineServer::IsLogEnabled()
{ return static_cast<CVEngineServer*>(this)->IsLogEnabled(); }
void IVEngineServer::BuildEntityClusterList( edict_t *pEdict, PVSInfo_t *pPVSInfo )
{ static_cast<CVEngineServer*>(this)->BuildEntityClusterList( pEdict, pPVSInfo ); }
void IVEngineServer::SolidMoved( edict_t *pSolidEnt, ICollideable *pSolidCollide, const Vector* pPrevAbsOrigin, bool testSurroundingBoundsOnly )
{ static_cast<CVEngineServer*>(this)->SolidMoved( pSolidEnt, pSolidCollide, pPrevAbsOrigin, testSurroundingBoundsOnly ); }
void IVEngineServer::TriggerMoved( edict_t *pTriggerEnt, bool testSurroundingBoundsOnly )
{ static_cast<CVEngineServer*>(this)->TriggerMoved( pTriggerEnt, testSurroundingBoundsOnly ); }
ISpatialPartition * IVEngineServer::CreateSpatialPartition( const Vector& worldmin, const Vector& worldmax )
{ return static_cast<CVEngineServer*>(this)->CreateSpatialPartition( worldmin, worldmax ); }
const std::bitset<MAX_EDICTS>* IVEngineServer::GetEntityTransmitBitsForClient( int iClientIndex )
{ return static_cast<CVEngineServer*>(this)->GetEntityTransmitBitsForClient( iClientIndex ); }
bool IVEngineServer::IsPaused()
{ return static_cast<CVEngineServer*>(this)->IsPaused(); }
float IVEngineServer::GetTimescale( void ) const
{ return static_cast<const CVEngineServer*>(this)->GetTimescale(); }
void IVEngineServer::ForceExactFile( const char *s )
{ static_cast<CVEngineServer*>(this)->ForceExactFile( s ); }
void IVEngineServer::ForceModelBounds( const char *s, const Vector &mins, const Vector &maxs )
{ static_cast<CVEngineServer*>(this)->ForceModelBounds( s, mins, maxs ); }
void IVEngineServer::ClearSaveDirAfterClientLoad()
{ static_cast<CVEngineServer*>(this)->ClearSaveDirAfterClientLoad(); }
void IVEngineServer::SetFakeClientConVarValue( edict_t *pEntity, const char *cvar, const char *value )
{ static_cast<CVEngineServer*>(this)->SetFakeClientConVarValue( pEntity, cvar, value ); }
void IVEngineServer::ForceSimpleMaterial( const char *s )
{ static_cast<CVEngineServer*>(this)->ForceSimpleMaterial( s ); }
bool IVEngineServer::IsLevelMainMenuBackground( void )
{ return static_cast<CVEngineServer*>(this)->IsLevelMainMenuBackground(); }
void IVEngineServer::SetAreaPortalStates( const int *portalNumbers, const int *isOpen, int nPortals )
{ static_cast<CVEngineServer*>(this)->SetAreaPortalStates( portalNumbers, isOpen, nPortals ); }
void IVEngineServer::NotifyEdictFlagsChange( int iEdict )
{ static_cast<CVEngineServer*>(this)->NotifyEdictFlagsChange( iEdict ); }
const CCheckTransmitInfo* IVEngineServer::GetPrevCheckTransmitInfo( edict_t *pPlayerEdict )
{ return static_cast<CVEngineServer*>(this)->GetPrevCheckTransmitInfo( pPlayerEdict ); }
CSharedEdictChangeInfo* IVEngineServer::GetSharedEdictChangeInfo()
{ return static_cast<CVEngineServer*>(this)->GetSharedEdictChangeInfo(); }
void IVEngineServer::AllowImmediateEdictReuse()
{ static_cast<CVEngineServer*>(this)->AllowImmediateEdictReuse(); }
bool IVEngineServer::IsInternalBuild( void )
{ return static_cast<CVEngineServer*>(this)->IsInternalBuild(); }
IChangeInfoAccessor * IVEngineServer::GetChangeAccessor( const edict_t *pEdict )
{ return static_cast<CVEngineServer*>(this)->GetChangeAccessor( pEdict ); }
char const * IVEngineServer::GetMostRecentlyLoadedFileName()
{ return static_cast<CVEngineServer*>(this)->GetMostRecentlyLoadedFileName(); }
char const * IVEngineServer::GetSaveFileName()
{ return static_cast<CVEngineServer*>(this)->GetSaveFileName(); }
void IVEngineServer::CleanUpEntityClusterList( PVSInfo_t *pPVSInfo )
{ static_cast<CVEngineServer*>(this)->CleanUpEntityClusterList( pPVSInfo ); }
void IVEngineServer::SetAchievementMgr( IAchievementMgr *pAchievementMgr )
{ static_cast<CVEngineServer*>(this)->SetAchievementMgr( pAchievementMgr ); }
IAchievementMgr * IVEngineServer::GetAchievementMgr()
{ return static_cast<CVEngineServer*>(this)->GetAchievementMgr(); }
int IVEngineServer::GetAppID()
{ return static_cast<CVEngineServer*>(this)->GetAppID(); }
bool IVEngineServer::IsLowViolence()
{ return static_cast<CVEngineServer*>(this)->IsLowViolence(); }
bool IVEngineServer::IsAnyClientLowViolence()
{ return static_cast<CVEngineServer*>(this)->IsAnyClientLowViolence(); }
QueryCvarCookie_t IVEngineServer::StartQueryCvarValue( edict_t *pPlayerEntity, const char *pName )
{ return static_cast<CVEngineServer*>(this)->StartQueryCvarValue( pPlayerEntity, pName ); }
void IVEngineServer::InsertServerCommand( const char *str )
{ static_cast<CVEngineServer*>(this)->InsertServerCommand( str ); }
bool IVEngineServer::GetPlayerInfo( int ent_num, player_info_t *pinfo )
{ return static_cast<CVEngineServer*>(this)->GetPlayerInfo( ent_num, pinfo ); }
bool IVEngineServer::IsClientFullyAuthenticated( edict_t *pEdict )
{ return static_cast<CVEngineServer*>(this)->IsClientFullyAuthenticated( pEdict ); }
void IVEngineServer::SetDedicatedServerBenchmarkMode( bool bBenchmarkMode )
{ static_cast<CVEngineServer*>(this)->SetDedicatedServerBenchmarkMode( bBenchmarkMode ); }
bool IVEngineServer::IsSplitScreenPlayer( int ent_num )
{ return static_cast<CVEngineServer*>(this)->IsSplitScreenPlayer( ent_num ); }
edict_t * IVEngineServer::GetSplitScreenPlayerAttachToEdict( int ent_num )
{ return static_cast<CVEngineServer*>(this)->GetSplitScreenPlayerAttachToEdict( ent_num ); }
int IVEngineServer::GetNumSplitScreenUsersAttachedToEdict( int ent_num )
{ return static_cast<CVEngineServer*>(this)->GetNumSplitScreenUsersAttachedToEdict( ent_num ); }
edict_t * IVEngineServer::GetSplitScreenPlayerForEdict( int ent_num, int nSlot )
{ return static_cast<CVEngineServer*>(this)->GetSplitScreenPlayerForEdict( ent_num, nSlot ); }
bool IVEngineServer::IsOverrideLoadGameEntsOn()
{ return static_cast<CVEngineServer*>(this)->IsOverrideLoadGameEntsOn(); }
void IVEngineServer::ForceFlushEntity( int iEntity )
{ static_cast<CVEngineServer*>(this)->ForceFlushEntity( iEntity ); }
ISPSharedMemory * IVEngineServer::GetSinglePlayerSharedMemorySpace( const char *szName, int ent_num )
{ return static_cast<CVEngineServer*>(this)->GetSinglePlayerSharedMemorySpace( szName, ent_num ); }
void * IVEngineServer::AllocLevelStaticData( std::size_t bytes )
{ return static_cast<CVEngineServer*>(this)->AllocLevelStaticData( bytes ); }
int IVEngineServer::GetClusterCount()
{ return static_cast<CVEngineServer*>(this)->GetClusterCount(); }
int IVEngineServer::GetAllClusterBounds( bbox_t *pBBoxList, int maxBBox )
{ return static_cast<CVEngineServer*>(this)->GetAllClusterBounds( pBBoxList, maxBBox ); }
bool IVEngineServer::IsCreatingReslist()
{ return static_cast<CVEngineServer*>(this)->IsCreatingReslist(); }
void IVEngineServer::Pause( bool bPause, bool bForce )
{ static_cast<CVEngineServer*>(this)->Pause( bPause, bForce ); }
void IVEngineServer::SetTimescale( float flTimescale )
{ static_cast<CVEngineServer*>(this)->SetTimescale( flTimescale ); }
void IVEngineServer::SetGamestatsData( void *pGamestatsData )
{ static_cast<CVEngineServer*>(this)->SetGamestatsData( pGamestatsData ); }
void * IVEngineServer::GetGamestatsData()
{ return static_cast<CVEngineServer*>(this)->GetGamestatsData(); }
const CPlayerId * IVEngineServer::GetClientPlayerID( const edict_t *pPlayerEdict, bool bRequireFullyAuthenticated )
{ return static_cast<CVEngineServer*>(this)->GetClientPlayerID( pPlayerEdict, bRequireFullyAuthenticated ); }
void IVEngineServer::HostValidateSession()
{ static_cast<CVEngineServer*>(this)->HostValidateSession(); }
void IVEngineServer::RefreshScreenIfNecessary()
{ static_cast<CVEngineServer*>(this)->RefreshScreenIfNecessary(); }
bool IVEngineServer::HasPaintmap()
{ return static_cast<CVEngineServer*>(this)->HasPaintmap(); }
bool IVEngineServer::SpherePaintSurface( const model_t *pModel, const Vector& vPosition, BYTE color, float flSphereRadius, float flPaintCoatPercent )
{ return static_cast<CVEngineServer*>(this)->SpherePaintSurface( pModel, vPosition, color, flSphereRadius, flPaintCoatPercent ); }
void IVEngineServer::SphereTracePaintSurface( const model_t *pModel, const Vector& vPosition, const Vector& vContactNormal, float flSphereRadius, std::vector< BYTE >& surfColor )
{ static_cast<CVEngineServer*>(this)->SphereTracePaintSurface( pModel, vPosition, vContactNormal, flSphereRadius, surfColor ); }
void IVEngineServer::RemoveAllPaint()
{ static_cast<CVEngineServer*>(this)->RemoveAllPaint(); }
void IVEngineServer::PaintAllSurfaces( BYTE color )
{ static_cast<CVEngineServer*>(this)->PaintAllSurfaces( color ); }
void IVEngineServer::RemovePaint( const model_t* pModel )
{ static_cast<CVEngineServer*>(this)->RemovePaint( pModel ); }
void IVEngineServer::ClientCommandKeyValues( edict_t *pEdict, KeyValues *pCommand )
{ static_cast<CVEngineServer*>(this)->ClientCommandKeyValues( pEdict, pCommand ); }
uint64 IVEngineServer::GetClientXUID( edict_t *pPlayerEdict )
{ return static_cast<CVEngineServer*>(this)->GetClientXUID( pPlayerEdict ); }
bool IVEngineServer::IsActiveApp()
{ return static_cast<CVEngineServer*>(this)->IsActiveApp(); }
void IVEngineServer::SetNoClipEnabled( bool bEnabled )
{ static_cast<CVEngineServer*>(this)->SetNoClipEnabled( bEnabled ); }
void IVEngineServer::GetPaintmapDataRLE( std::vector< uint32 > &data )
{ static_cast<CVEngineServer*>(this)->GetPaintmapDataRLE( data ); }
void IVEngineServer::LoadPaintmapDataRLE( const std::vector< uint32 > &data )
{ static_cast<CVEngineServer*>(this)->LoadPaintmapDataRLE( data ); }
void IVEngineServer::SendPaintmapDataToClient( edict_t *pPlayerEdict )
{ static_cast<CVEngineServer*>(this)->SendPaintmapDataToClient( pPlayerEdict ); }
float IVEngineServer::GetLatencyForChoreoSounds()
{ return static_cast<CVEngineServer*>(this)->GetLatencyForChoreoSounds(); }
CrossPlayPlatform_t IVEngineServer::GetClientCrossPlayPlatform( int ent_num )
{ return static_cast<CVEngineServer*>(this)->GetClientCrossPlayPlatform( ent_num ); }
void IVEngineServer::EnsureInstanceBaseline( int ent_num )
{ static_cast<CVEngineServer*>(this)->EnsureInstanceBaseline( ent_num ); }
bool IVEngineServer::ReserveServerForQueuedGame( char const *szReservationPayload )
{ return static_cast<CVEngineServer*>(this)->ReserveServerForQueuedGame( szReservationPayload ); }
bool IVEngineServer::GetEngineHltvInfo( CEngineHltvInfo_t &info )
{ return static_cast<CVEngineServer*>(this)->GetEngineHltvInfo( info ); }
void IVEngineServer::AddHltvRelayProxyWhitelist( uint32 a, uint32 b, uint32 c, uint32 d, uint32 numbits )
{ static_cast<CVEngineServer*>(this)->AddHltvRelayProxyWhitelist( a, b, c, d, numbits ); }
int IVEngineServer::GetServerVersion() const
{ return static_cast<const CVEngineServer*>(this)->GetServerVersion(); }
void IVEngineServer::UpdateHltvExternalViewers( uint32 numTotalViewers, uint32 numLinkedViewers )
{ static_cast<CVEngineServer*>(this)->UpdateHltvExternalViewers( numTotalViewers, numLinkedViewers ); }
bool IVEngineServer::WasShutDownRequested() const
{ return static_cast<const CVEngineServer*>(this)->WasShutDownRequested(); }
bool IVEngineServer::StartClientHltvReplay( int nClientIndex, const HltvReplayParams_t &params )
{ return static_cast<CVEngineServer*>(this)->StartClientHltvReplay( nClientIndex, params ); }
void IVEngineServer::StopClientHltvReplay( int nClientIndex )
{ static_cast<CVEngineServer*>(this)->StopClientHltvReplay( nClientIndex ); }
int IVEngineServer::GetClientHltvReplayDelay( int nClientIndex )
{ return static_cast<CVEngineServer*>(this)->GetClientHltvReplayDelay( nClientIndex ); }
bool IVEngineServer::HasHltvReplay()
{ return static_cast<CVEngineServer*>(this)->HasHltvReplay(); }
bool IVEngineServer::ClientCanStartHltvReplay( int nClientIndex )
{ return static_cast<CVEngineServer*>(this)->ClientCanStartHltvReplay( nClientIndex ); }
void IVEngineServer::ClientResetReplayRequestTime( int nClientIndex )
{ static_cast<CVEngineServer*>(this)->ClientResetReplayRequestTime( nClientIndex ); }
bool IVEngineServer::AnyClientsInHltvReplayMode()
{ return static_cast<CVEngineServer*>(this)->AnyClientsInHltvReplayMode(); }
int IVEngineServer::GetLocalClientIndex( void )
{ return static_cast<CVEngineServer*>(this)->GetLocalClientIndex(); }

void IVEngineServer::ClientCommand( edict_t *pEdict, const char *szFmt, ... )
{
	std::va_list args;
	__builtin_va_start( args, szFmt );
	static_cast<CVEngineServer*>(this)->ClientCommandV( pEdict, szFmt, args );
	__builtin_va_end( args );
}
void IVEngineServer::Con_NPrintf( int pos, const char *fmt, ... )
{
	std::va_list args;
	__builtin_va_start( args, fmt );
	static_cast<CVEngineServer*>(this)->Con_NPrintfV( pos, fmt, args );
	__builtin_va_end( args );
}
void IVEngineServer::Con_NXPrintf( const struct con_nprint_s *info, const char *fmt, ... )
{
	std::va_list args;
	__builtin_va_start( args, fmt );
	static_cast<CVEngineServer*>(this)->Con_NXPrintfV( info, fmt, args );
	__builtin_va_end( args );
}
void IVEngineServer::DestroySpatialPartition( ISpatialPartition *pPartition )
{ static_cast<CVEngineServer*>(this)->DestroySpatialPartition( pPartition ); }
