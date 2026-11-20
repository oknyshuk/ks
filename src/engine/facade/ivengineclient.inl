import ks.core.basetypes;
import ks.core.memalloc;
import ks.core.platform;
import ks.core.types;
import ks.core.wchartypes;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.ssemath;
import ks.math.vector;
import ks.math.vmatrix;
import ks.tier1.icvar;
import ks.tier1.keyvalues;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of the IVEngineClient class to CEngineClient. Included by cdll_engine_int.cpp.

int IVEngineClient::GetIntersectingSurfaces( const model_t *model, const Vector &vCenter, const float radius, const bool bOnlyVisibleSurfaces, SurfInfo *pInfos, const int nMaxInfos )
{ return static_cast<CEngineClient*>(this)->GetIntersectingSurfaces( model, vCenter, radius, bOnlyVisibleSurfaces, pInfos, nMaxInfos ); }
Vector IVEngineClient::GetLightForPoint( const Vector &pos, bool bClamp )
{ return static_cast<CEngineClient*>(this)->GetLightForPoint( pos, bClamp ); }
IMaterial * IVEngineClient::TraceLineMaterialAndLighting( const Vector &start, const Vector &end, Vector &diffuseLightColor, Vector& baseColor )
{ return static_cast<CEngineClient*>(this)->TraceLineMaterialAndLighting( start, end, diffuseLightColor, baseColor ); }
const char * IVEngineClient::ParseFile( const char *data, char *token, int maxlen )
{ return static_cast<CEngineClient*>(this)->ParseFile( data, token, maxlen ); }
bool IVEngineClient::CopyLocalFile( const char *source, const char *destination )
{ return static_cast<CEngineClient*>(this)->CopyLocalFile( source, destination ); }
void IVEngineClient::GetScreenSize( int& width, int& height )
{ static_cast<CEngineClient*>(this)->GetScreenSize( width, height ); }
void IVEngineClient::ServerCmd( const char *szCmdString, bool bReliable )
{ static_cast<CEngineClient*>(this)->ServerCmd( szCmdString, bReliable ); }
void IVEngineClient::ClientCmd( const char *szCmdString )
{ static_cast<CEngineClient*>(this)->ClientCmd( szCmdString ); }
bool IVEngineClient::GetPlayerInfo( int ent_num, player_info_t *pinfo )
{ return static_cast<CEngineClient*>(this)->GetPlayerInfo( ent_num, pinfo ); }
int IVEngineClient::GetPlayerForUserID( int userID )
{ return static_cast<CEngineClient*>(this)->GetPlayerForUserID( userID ); }
client_textmessage_t * IVEngineClient::TextMessageGet( const char *pName )
{ return static_cast<CEngineClient*>(this)->TextMessageGet( pName ); }
bool IVEngineClient::Con_IsVisible( void )
{ return static_cast<CEngineClient*>(this)->Con_IsVisible(); }
int IVEngineClient::GetLocalPlayer( void )
{ return static_cast<CEngineClient*>(this)->GetLocalPlayer(); }
const model_t * IVEngineClient::LoadModel( const char *pName, bool bProp )
{ return static_cast<CEngineClient*>(this)->LoadModel( pName, bProp ); }
float IVEngineClient::GetLastTimeStamp( void )
{ return static_cast<CEngineClient*>(this)->GetLastTimeStamp(); }
CSentence * IVEngineClient::GetSentence( CAudioSource *pAudioSource )
{ return static_cast<CEngineClient*>(this)->GetSentence( pAudioSource ); }
float IVEngineClient::GetSentenceLength( CAudioSource *pAudioSource )
{ return static_cast<CEngineClient*>(this)->GetSentenceLength( pAudioSource ); }
bool IVEngineClient::IsStreaming( CAudioSource *pAudioSource ) const
{ return static_cast<const CEngineClient*>(this)->IsStreaming( pAudioSource ); }
void IVEngineClient::GetViewAngles( QAngle& va )
{ static_cast<CEngineClient*>(this)->GetViewAngles( va ); }
void IVEngineClient::SetViewAngles( QAngle& va )
{ static_cast<CEngineClient*>(this)->SetViewAngles( va ); }
int IVEngineClient::GetMaxClients( void )
{ return static_cast<CEngineClient*>(this)->GetMaxClients(); }
const char * IVEngineClient::Key_LookupBinding( const char *pBinding )
{ return static_cast<CEngineClient*>(this)->Key_LookupBinding( pBinding ); }
const char * IVEngineClient::Key_BindingForKey( ButtonCode_t code )
{ return static_cast<CEngineClient*>(this)->Key_BindingForKey( code ); }
void IVEngineClient::Key_SetBinding( ButtonCode_t code, const char *pBinding )
{ static_cast<CEngineClient*>(this)->Key_SetBinding( code, pBinding ); }
void IVEngineClient::StartKeyTrapMode( void )
{ static_cast<CEngineClient*>(this)->StartKeyTrapMode(); }
bool IVEngineClient::CheckDoneKeyTrapping( ButtonCode_t &code )
{ return static_cast<CEngineClient*>(this)->CheckDoneKeyTrapping( code ); }
bool IVEngineClient::IsInGame( void )
{ return static_cast<CEngineClient*>(this)->IsInGame(); }
bool IVEngineClient::IsConnected( void )
{ return static_cast<CEngineClient*>(this)->IsConnected(); }
bool IVEngineClient::IsDrawingLoadingImage( void )
{ return static_cast<CEngineClient*>(this)->IsDrawingLoadingImage(); }
void IVEngineClient::HideLoadingPlaque( void )
{ static_cast<CEngineClient*>(this)->HideLoadingPlaque(); }
void IVEngineClient::Con_NPrintf( int pos, const char *fmt, ... )
{
	std::va_list args;
	__builtin_va_start( args, fmt );
	static_cast<CEngineClient*>(this)->Con_NPrintfV( pos, fmt, args );
	__builtin_va_end( args );
}
void IVEngineClient::Con_NXPrintf( const struct con_nprint_s *info, const char *fmt, ... )
{
	std::va_list args;
	__builtin_va_start( args, fmt );
	static_cast<CEngineClient*>(this)->Con_NXPrintfV( info, fmt, args );
	__builtin_va_end( args );
}
int IVEngineClient::IsBoxVisible( const Vector& mins, const Vector& maxs )
{ return static_cast<CEngineClient*>(this)->IsBoxVisible( mins, maxs ); }
int IVEngineClient::IsBoxInViewCluster( const Vector& mins, const Vector& maxs )
{ return static_cast<CEngineClient*>(this)->IsBoxInViewCluster( mins, maxs ); }
bool IVEngineClient::CullBox( const Vector& mins, const Vector& maxs )
{ return static_cast<CEngineClient*>(this)->CullBox( mins, maxs ); }
void IVEngineClient::Sound_ExtraUpdate( void )
{ static_cast<CEngineClient*>(this)->Sound_ExtraUpdate(); }
const char * IVEngineClient::GetGameDirectory( void )
{ return static_cast<CEngineClient*>(this)->GetGameDirectory(); }
const VMatrix& IVEngineClient::WorldToScreenMatrix()
{ return static_cast<CEngineClient*>(this)->WorldToScreenMatrix(); }
const VMatrix& IVEngineClient::WorldToViewMatrix()
{ return static_cast<CEngineClient*>(this)->WorldToViewMatrix(); }
int IVEngineClient::GameLumpVersion( int lumpId ) const
{ return static_cast<const CEngineClient*>(this)->GameLumpVersion( lumpId ); }
int IVEngineClient::GameLumpSize( int lumpId ) const
{ return static_cast<const CEngineClient*>(this)->GameLumpSize( lumpId ); }
bool IVEngineClient::LoadGameLump( int lumpId, void* pBuffer, int size )
{ return static_cast<CEngineClient*>(this)->LoadGameLump( lumpId, pBuffer, size ); }
int IVEngineClient::LevelLeafCount() const
{ return static_cast<const CEngineClient*>(this)->LevelLeafCount(); }
ISpatialQuery* IVEngineClient::GetBSPTreeQuery()
{ return static_cast<CEngineClient*>(this)->GetBSPTreeQuery(); }
void IVEngineClient::LinearToGamma( float* linear, float* gamma )
{ static_cast<CEngineClient*>(this)->LinearToGamma( linear, gamma ); }
float IVEngineClient::LightStyleValue( int style )
{ return static_cast<CEngineClient*>(this)->LightStyleValue( style ); }
void IVEngineClient::ComputeDynamicLighting( const Vector& pt, const Vector* pNormal, Vector& color )
{ static_cast<CEngineClient*>(this)->ComputeDynamicLighting( pt, pNormal, color ); }
void IVEngineClient::GetAmbientLightColor( Vector& color )
{ static_cast<CEngineClient*>(this)->GetAmbientLightColor( color ); }
int IVEngineClient::GetDXSupportLevel()
{ return static_cast<CEngineClient*>(this)->GetDXSupportLevel(); }
bool IVEngineClient::SupportsHDR()
{ return static_cast<CEngineClient*>(this)->SupportsHDR(); }
void IVEngineClient::GetChapterName( char *pchBuff, int iMaxLength )
{ static_cast<CEngineClient*>(this)->GetChapterName( pchBuff, iMaxLength ); }
char const * IVEngineClient::GetLevelName( void )
{ return static_cast<CEngineClient*>(this)->GetLevelName(); }
char const * IVEngineClient::GetLevelNameShort( void )
{ return static_cast<CEngineClient*>(this)->GetLevelNameShort(); }
char const * IVEngineClient::GetMapGroupName( void )
{ return static_cast<CEngineClient*>(this)->GetMapGroupName(); }
struct IVoiceTweak_s * IVEngineClient::GetVoiceTweakAPI( void )
{ return static_cast<CEngineClient*>(this)->GetVoiceTweakAPI(); }
void IVEngineClient::SetVoiceCasterID( uint32 casterID )
{ static_cast<CEngineClient*>(this)->SetVoiceCasterID( casterID ); }
void IVEngineClient::EngineStats_BeginFrame( void )
{ static_cast<CEngineClient*>(this)->EngineStats_BeginFrame(); }
void IVEngineClient::EngineStats_EndFrame( void )
{ static_cast<CEngineClient*>(this)->EngineStats_EndFrame(); }
void IVEngineClient::FireEvents()
{ static_cast<CEngineClient*>(this)->FireEvents(); }
int IVEngineClient::GetLeavesArea( unsigned short *pLeaves, int nLeaves )
{ return static_cast<CEngineClient*>(this)->GetLeavesArea( pLeaves, nLeaves ); }
bool IVEngineClient::DoesBoxTouchAreaFrustum( const Vector &mins, const Vector &maxs, int iArea )
{ return static_cast<CEngineClient*>(this)->DoesBoxTouchAreaFrustum( mins, maxs, iArea ); }
int IVEngineClient::GetFrustumList( Frustum_t **pList, int listMax )
{ return static_cast<CEngineClient*>(this)->GetFrustumList( pList, listMax ); }
bool IVEngineClient::ShouldUseAreaFrustum( int area )
{ return static_cast<CEngineClient*>(this)->ShouldUseAreaFrustum( area ); }
void IVEngineClient::SetAudioState( const AudioState_t& state )
{ static_cast<CEngineClient*>(this)->SetAudioState( state ); }
int IVEngineClient::SentenceGroupPick( int groupIndex, char *name, int nameBufLen )
{ return static_cast<CEngineClient*>(this)->SentenceGroupPick( groupIndex, name, nameBufLen ); }
int IVEngineClient::SentenceGroupPickSequential( int groupIndex, char *name, int nameBufLen, int sentenceIndex, int reset )
{ return static_cast<CEngineClient*>(this)->SentenceGroupPickSequential( groupIndex, name, nameBufLen, sentenceIndex, reset ); }
int IVEngineClient::SentenceIndexFromName( const char *pSentenceName )
{ return static_cast<CEngineClient*>(this)->SentenceIndexFromName( pSentenceName ); }
const char * IVEngineClient::SentenceNameFromIndex( int sentenceIndex )
{ return static_cast<CEngineClient*>(this)->SentenceNameFromIndex( sentenceIndex ); }
int IVEngineClient::SentenceGroupIndexFromName( const char *pGroupName )
{ return static_cast<CEngineClient*>(this)->SentenceGroupIndexFromName( pGroupName ); }
const char * IVEngineClient::SentenceGroupNameFromIndex( int groupIndex )
{ return static_cast<CEngineClient*>(this)->SentenceGroupNameFromIndex( groupIndex ); }
float IVEngineClient::SentenceLength( int sentenceIndex )
{ return static_cast<CEngineClient*>(this)->SentenceLength( sentenceIndex ); }
void IVEngineClient::ComputeLighting( const Vector& pt, const Vector* pNormal, bool bClamp, Vector& color, Vector *pBoxColors )
{ static_cast<CEngineClient*>(this)->ComputeLighting( pt, pNormal, bClamp, color, pBoxColors ); }
void IVEngineClient::ActivateOccluder( int nOccluderIndex, bool bActive )
{ static_cast<CEngineClient*>(this)->ActivateOccluder( nOccluderIndex, bActive ); }
bool IVEngineClient::IsOccluded( int occlusionViewId, const Vector &vecAbsMins, const Vector &vecAbsMaxs )
{ return static_cast<CEngineClient*>(this)->IsOccluded( occlusionViewId, vecAbsMins, vecAbsMaxs ); }
int IVEngineClient::GetOcclusionViewId() const
{ return static_cast<const CEngineClient*>(this)->GetOcclusionViewId(); }
INetChannelInfo * IVEngineClient::GetNetChannelInfo( void )
{ return static_cast<CEngineClient*>(this)->GetNetChannelInfo(); }
void IVEngineClient::DebugDrawPhysCollide( const CPhysCollide *pCollide, IMaterial *pMaterial, const matrix3x4_t& transform, const color32 &color )
{ static_cast<CEngineClient*>(this)->DebugDrawPhysCollide( pCollide, pMaterial, transform, color ); }
void IVEngineClient::CheckPoint( const char *pName )
{ static_cast<CEngineClient*>(this)->CheckPoint( pName ); }
void IVEngineClient::DrawPortals()
{ static_cast<CEngineClient*>(this)->DrawPortals(); }
bool IVEngineClient::IsPlayingDemo( void )
{ return static_cast<CEngineClient*>(this)->IsPlayingDemo(); }
bool IVEngineClient::IsRecordingDemo( void )
{ return static_cast<CEngineClient*>(this)->IsRecordingDemo(); }
bool IVEngineClient::IsPlayingTimeDemo( void )
{ return static_cast<CEngineClient*>(this)->IsPlayingTimeDemo(); }
int IVEngineClient::GetDemoRecordingTick( void )
{ return static_cast<CEngineClient*>(this)->GetDemoRecordingTick(); }
int IVEngineClient::GetDemoPlaybackTick( void )
{ return static_cast<CEngineClient*>(this)->GetDemoPlaybackTick(); }
int IVEngineClient::GetDemoPlaybackStartTick( void )
{ return static_cast<CEngineClient*>(this)->GetDemoPlaybackStartTick(); }
float IVEngineClient::GetDemoPlaybackTimeScale( void )
{ return static_cast<CEngineClient*>(this)->GetDemoPlaybackTimeScale(); }
int IVEngineClient::GetDemoPlaybackTotalTicks( void )
{ return static_cast<CEngineClient*>(this)->GetDemoPlaybackTotalTicks(); }
bool IVEngineClient::IsPaused( void )
{ return static_cast<CEngineClient*>(this)->IsPaused(); }
float IVEngineClient::GetTimescale( void ) const
{ return static_cast<const CEngineClient*>(this)->GetTimescale(); }
bool IVEngineClient::IsTakingScreenshot( void )
{ return static_cast<CEngineClient*>(this)->IsTakingScreenshot(); }
bool IVEngineClient::IsHLTV( void )
{ return static_cast<CEngineClient*>(this)->IsHLTV(); }
bool IVEngineClient::IsLevelMainMenuBackground( void )
{ return static_cast<CEngineClient*>(this)->IsLevelMainMenuBackground(); }
void IVEngineClient::GetMainMenuBackgroundName( char *dest, int destlen )
{ static_cast<CEngineClient*>(this)->GetMainMenuBackgroundName( dest, destlen ); }
void IVEngineClient::SetOcclusionParameters( const OcclusionParams_t &params )
{ static_cast<CEngineClient*>(this)->SetOcclusionParameters( params ); }
void IVEngineClient::GetUILanguage( char *dest, int destlen )
{ static_cast<CEngineClient*>(this)->GetUILanguage( dest, destlen ); }
SkyboxVisibility_t IVEngineClient::IsSkyboxVisibleFromPoint( const Vector &vecPoint )
{ return static_cast<CEngineClient*>(this)->IsSkyboxVisibleFromPoint( vecPoint ); }
const char* IVEngineClient::GetMapEntitiesString()
{ return static_cast<CEngineClient*>(this)->GetMapEntitiesString(); }
bool IVEngineClient::IsInEditMode( void )
{ return static_cast<CEngineClient*>(this)->IsInEditMode(); }
float IVEngineClient::GetScreenAspectRatio( int viewportWidth, int viewportHeight )
{ return static_cast<CEngineClient*>(this)->GetScreenAspectRatio( viewportWidth, viewportHeight ); }
unsigned int IVEngineClient::GetEngineBuildNumber()
{ return static_cast<CEngineClient*>(this)->GetEngineBuildNumber(); }
const char * IVEngineClient::GetProductVersionString()
{ return static_cast<CEngineClient*>(this)->GetProductVersionString(); }
void IVEngineClient::GrabPreColorCorrectedFrame( int x, int y, int width, int height )
{ static_cast<CEngineClient*>(this)->GrabPreColorCorrectedFrame( x, y, width, height ); }
bool IVEngineClient::IsHammerRunning() const
{ return static_cast<const CEngineClient*>(this)->IsHammerRunning(); }
void IVEngineClient::ExecuteClientCmd( const char *szCmdString )
{ static_cast<CEngineClient*>(this)->ExecuteClientCmd( szCmdString ); }
bool IVEngineClient::MapHasHDRLighting( void )
{ return static_cast<CEngineClient*>(this)->MapHasHDRLighting(); }
bool IVEngineClient::MapHasLightMapAlphaData( void )
{ return static_cast<CEngineClient*>(this)->MapHasLightMapAlphaData(); }
int IVEngineClient::GetAppID()
{ return static_cast<CEngineClient*>(this)->GetAppID(); }
Vector IVEngineClient::GetLightForPointFast( const Vector &pos, bool bClamp )
{ return static_cast<CEngineClient*>(this)->GetLightForPointFast( pos, bClamp ); }
void IVEngineClient::ClientCmd_Unrestricted( const char *szCmdString, bool fromConsoleOrKeybind )
{ static_cast<CEngineClient*>(this)->ClientCmd_Unrestricted( szCmdString, fromConsoleOrKeybind ); }
void IVEngineClient::ClientCmd_Unrestricted( const char *szCmdString, bool fromConsoleOrKeybind, int nUserSlot, bool bCheckValidSlot )
{ static_cast<CEngineClient*>(this)->ClientCmd_Unrestricted( szCmdString, fromConsoleOrKeybind, nUserSlot, bCheckValidSlot ); }
void IVEngineClient::SetRestrictServerCommands( bool bRestrict )
{ static_cast<CEngineClient*>(this)->SetRestrictServerCommands( bRestrict ); }
void IVEngineClient::SetRestrictClientCommands( bool bRestrict )
{ static_cast<CEngineClient*>(this)->SetRestrictClientCommands( bRestrict ); }
void IVEngineClient::SetOverlayBindProxy( int iOverlayID, void *pBindProxy )
{ static_cast<CEngineClient*>(this)->SetOverlayBindProxy( iOverlayID, pBindProxy ); }
bool IVEngineClient::CopyFrameBufferToMaterial( const char *pMaterialName )
{ return static_cast<CEngineClient*>(this)->CopyFrameBufferToMaterial( pMaterialName ); }
void IVEngineClient::ReadConfiguration( const int iController, const bool readDefault )
{ static_cast<CEngineClient*>(this)->ReadConfiguration( iController, readDefault ); }
void IVEngineClient::SetAchievementMgr( IAchievementMgr *pAchievementMgr )
{ static_cast<CEngineClient*>(this)->SetAchievementMgr( pAchievementMgr ); }
IAchievementMgr * IVEngineClient::GetAchievementMgr()
{ return static_cast<CEngineClient*>(this)->GetAchievementMgr(); }
bool IVEngineClient::MapLoadFailed( void )
{ return static_cast<CEngineClient*>(this)->MapLoadFailed(); }
void IVEngineClient::SetMapLoadFailed( bool bState )
{ static_cast<CEngineClient*>(this)->SetMapLoadFailed( bState ); }
bool IVEngineClient::IsLowViolence()
{ return static_cast<CEngineClient*>(this)->IsLowViolence(); }
void IVEngineClient::StartXboxExitingProcess()
{ static_cast<CEngineClient*>(this)->StartXboxExitingProcess(); }
uint IVEngineClient::OnStorageDeviceAttached( int iController )
{ return static_cast<CEngineClient*>(this)->OnStorageDeviceAttached( iController ); }
void IVEngineClient::OnStorageDeviceDetached( int iController )
{ static_cast<CEngineClient*>(this)->OnStorageDeviceDetached( iController ); }
void IVEngineClient::WriteScreenshot( const char *pFilename )
{ static_cast<CEngineClient*>(this)->WriteScreenshot( pFilename ); }
void IVEngineClient::ResetDemoInterpolation( void )
{ static_cast<CEngineClient*>(this)->ResetDemoInterpolation(); }
int IVEngineClient::GetActiveSplitScreenPlayerSlot()
{ return static_cast<CEngineClient*>(this)->GetActiveSplitScreenPlayerSlot(); }
int IVEngineClient::SetActiveSplitScreenPlayerSlot( int slot )
{ return static_cast<CEngineClient*>(this)->SetActiveSplitScreenPlayerSlot( slot ); }
bool IVEngineClient::SetLocalPlayerIsResolvable( char const *pchContext, int nLine, bool bResolvable )
{ return static_cast<CEngineClient*>(this)->SetLocalPlayerIsResolvable( pchContext, nLine, bResolvable ); }
bool IVEngineClient::IsLocalPlayerResolvable()
{ return static_cast<CEngineClient*>(this)->IsLocalPlayerResolvable(); }
int IVEngineClient::GetSplitScreenPlayer( int nSlot )
{ return static_cast<CEngineClient*>(this)->GetSplitScreenPlayer( nSlot ); }
bool IVEngineClient::IsSplitScreenActive()
{ return static_cast<CEngineClient*>(this)->IsSplitScreenActive(); }
bool IVEngineClient::IsValidSplitScreenSlot( int nSlot )
{ return static_cast<CEngineClient*>(this)->IsValidSplitScreenSlot( nSlot ); }
int IVEngineClient::FirstValidSplitScreenSlot()
{ return static_cast<CEngineClient*>(this)->FirstValidSplitScreenSlot(); }
int IVEngineClient::NextValidSplitScreenSlot( int nPreviousSlot )
{ return static_cast<CEngineClient*>(this)->NextValidSplitScreenSlot( nPreviousSlot ); }
ISPSharedMemory * IVEngineClient::GetSinglePlayerSharedMemorySpace( const char *szName, int ent_num )
{ return static_cast<CEngineClient*>(this)->GetSinglePlayerSharedMemorySpace( szName, ent_num ); }
void IVEngineClient::ComputeLightingCube( const Vector& pt, bool bClamp, Vector *pBoxColors )
{ static_cast<CEngineClient*>(this)->ComputeLightingCube( pt, bClamp, pBoxColors ); }
void IVEngineClient::RegisterDemoCustomDataCallback( string_t szCallbackSaveID, pfnDemoCustomDataCallback pCallback )
{ static_cast<CEngineClient*>(this)->RegisterDemoCustomDataCallback( szCallbackSaveID, pCallback ); }
void IVEngineClient::RecordDemoCustomData( pfnDemoCustomDataCallback pCallback, const void *pData, std::size_t iDataLength )
{ static_cast<CEngineClient*>(this)->RecordDemoCustomData( pCallback, pData, iDataLength ); }
void IVEngineClient::SetPitchScale( float flPitchScale )
{ static_cast<CEngineClient*>(this)->SetPitchScale( flPitchScale ); }
float IVEngineClient::GetPitchScale( void )
{ return static_cast<CEngineClient*>(this)->GetPitchScale(); }
bool IVEngineClient::LoadFilmmaker()
{ return static_cast<CEngineClient*>(this)->LoadFilmmaker(); }
void IVEngineClient::UnloadFilmmaker()
{ static_cast<CEngineClient*>(this)->UnloadFilmmaker(); }
void IVEngineClient::SetLeafFlag( int nLeafIndex, int nFlagBits )
{ static_cast<CEngineClient*>(this)->SetLeafFlag( nLeafIndex, nFlagBits ); }
void IVEngineClient::RecalculateBSPLeafFlags( void )
{ static_cast<CEngineClient*>(this)->RecalculateBSPLeafFlags(); }
bool IVEngineClient::DSPGetCurrentDASRoomNew( void )
{ return static_cast<CEngineClient*>(this)->DSPGetCurrentDASRoomNew(); }
bool IVEngineClient::DSPGetCurrentDASRoomChanged( void )
{ return static_cast<CEngineClient*>(this)->DSPGetCurrentDASRoomChanged(); }
bool IVEngineClient::DSPGetCurrentDASRoomSkyAbove( void )
{ return static_cast<CEngineClient*>(this)->DSPGetCurrentDASRoomSkyAbove(); }
float IVEngineClient::DSPGetCurrentDASRoomSkyPercent( void )
{ return static_cast<CEngineClient*>(this)->DSPGetCurrentDASRoomSkyPercent(); }
void IVEngineClient::SetMixGroupOfCurrentMixer( const char *szgroupname, const char *szparam, float val, int setMixerType )
{ static_cast<CEngineClient*>(this)->SetMixGroupOfCurrentMixer( szgroupname, szparam, val, setMixerType ); }
int IVEngineClient::GetMixLayerIndex( const char *szmixlayername )
{ return static_cast<CEngineClient*>(this)->GetMixLayerIndex( szmixlayername ); }
void IVEngineClient::SetMixLayerLevel( int index, float level )
{ static_cast<CEngineClient*>(this)->SetMixLayerLevel( index, level ); }
int IVEngineClient::GetMixGroupIndex( const char *pMixGroupName )
{ return static_cast<CEngineClient*>(this)->GetMixGroupIndex( pMixGroupName ); }
void IVEngineClient::SetMixLayerTriggerFactor( int nMixLayerIndex, int nMixGroupIndex, float flFactor )
{ static_cast<CEngineClient*>(this)->SetMixLayerTriggerFactor( nMixLayerIndex, nMixGroupIndex, flFactor ); }
void IVEngineClient::SetMixLayerTriggerFactor( const char *pMixLayerIndex, const char *pMixGroupIndex, float flFactor )
{ static_cast<CEngineClient*>(this)->SetMixLayerTriggerFactor( pMixLayerIndex, pMixGroupIndex, flFactor ); }
bool IVEngineClient::IsCreatingReslist()
{ return static_cast<CEngineClient*>(this)->IsCreatingReslist(); }
void IVEngineClient::SetTimescale( float flTimescale )
{ static_cast<CEngineClient*>(this)->SetTimescale( flTimescale ); }
void IVEngineClient::SetGamestatsData( void *pGamestatsData )
{ static_cast<CEngineClient*>(this)->SetGamestatsData( pGamestatsData ); }
void * IVEngineClient::GetGamestatsData()
{ return static_cast<CEngineClient*>(this)->GetGamestatsData(); }
void IVEngineClient::GetMouseDelta( float &x, float &y, bool bIgnoreNextMouseDelta )
{ static_cast<CEngineClient*>(this)->GetMouseDelta( x, y, bIgnoreNextMouseDelta ); }
const char * IVEngineClient::Key_LookupBindingEx( const char *pBinding, int iUserId, int iStartCount, BindingLookupOption_t nFlags )
{ return static_cast<CEngineClient*>(this)->Key_LookupBindingEx( pBinding, iUserId, iStartCount, nFlags ); }
int IVEngineClient::Key_CodeForBinding( const char *pBinding, int iUserId, int iStartCount, BindingLookupOption_t nFlags )
{ return static_cast<CEngineClient*>(this)->Key_CodeForBinding( pBinding, iUserId, iStartCount, nFlags ); }
void IVEngineClient::UpdateDAndELights( void )
{ static_cast<CEngineClient*>(this)->UpdateDAndELights(); }
int IVEngineClient::GetBugSubmissionCount() const
{ return static_cast<const CEngineClient*>(this)->GetBugSubmissionCount(); }
void IVEngineClient::ClearBugSubmissionCount()
{ static_cast<CEngineClient*>(this)->ClearBugSubmissionCount(); }
bool IVEngineClient::DoesLevelContainWater() const
{ return static_cast<const CEngineClient*>(this)->DoesLevelContainWater(); }
float IVEngineClient::GetServerSimulationFrameTime() const
{ return static_cast<const CEngineClient*>(this)->GetServerSimulationFrameTime(); }
void IVEngineClient::SolidMoved( class IClientEntity *pSolidEnt, class ICollideable *pSolidCollide, const Vector* pPrevAbsOrigin, bool accurateBboxTriggerChecks )
{ static_cast<CEngineClient*>(this)->SolidMoved( pSolidEnt, pSolidCollide, pPrevAbsOrigin, accurateBboxTriggerChecks ); }
void IVEngineClient::TriggerMoved( class IClientEntity *pTriggerEnt, bool accurateBboxTriggerChecks )
{ static_cast<CEngineClient*>(this)->TriggerMoved( pTriggerEnt, accurateBboxTriggerChecks ); }
void IVEngineClient::ComputeLeavesConnected( const Vector &vecOrigin, int nCount, const int *pLeafIndices, bool *pIsConnected )
{ static_cast<CEngineClient*>(this)->ComputeLeavesConnected( vecOrigin, nCount, pLeafIndices, pIsConnected ); }
void IVEngineClient::SetBlurFade( float amount )
{ static_cast<CEngineClient*>(this)->SetBlurFade( amount ); }
bool IVEngineClient::IsTransitioningToLoad()
{ return static_cast<CEngineClient*>(this)->IsTransitioningToLoad(); }
void IVEngineClient::SearchPathsChangedAfterInstall()
{ static_cast<CEngineClient*>(this)->SearchPathsChangedAfterInstall(); }
void IVEngineClient::ConfigureSystemLevel( int nCPULevel, int nGPULevel )
{ static_cast<CEngineClient*>(this)->ConfigureSystemLevel( nCPULevel, nGPULevel ); }
void IVEngineClient::SetConnectionPassword( char const *pchCurrentPW )
{ static_cast<CEngineClient*>(this)->SetConnectionPassword( pchCurrentPW ); }
void IVEngineClient::ServerCmdKeyValues( KeyValues *pKeyValues )
{ static_cast<CEngineClient*>(this)->ServerCmdKeyValues( pKeyValues ); }
void IVEngineClient::SendMessageToServer( INetMessage *pMessage, bool bForceReliable, bool bVoice )
{ static_cast<CEngineClient*>(this)->SendMessageToServer( pMessage, bForceReliable, bVoice ); }
bool IVEngineClient::SpherePaintSurface( const model_t *pModel, const Vector& vPosition, BYTE color, float flSphereRadius, float flPaintCoatPercent )
{ return static_cast<CEngineClient*>(this)->SpherePaintSurface( pModel, vPosition, color, flSphereRadius, flPaintCoatPercent ); }
bool IVEngineClient::HasPaintmap()
{ return static_cast<CEngineClient*>(this)->HasPaintmap(); }
void IVEngineClient::EnablePaintmapRender()
{ static_cast<CEngineClient*>(this)->EnablePaintmapRender(); }
void IVEngineClient::SphereTracePaintSurface( const model_t *pModel, const Vector& vPosition, const Vector& vContactNormal, float flSphereRadius, std::vector< BYTE >& surfColors )
{ static_cast<CEngineClient*>(this)->SphereTracePaintSurface( pModel, vPosition, vContactNormal, flSphereRadius, surfColors ); }
void IVEngineClient::RemoveAllPaint()
{ static_cast<CEngineClient*>(this)->RemoveAllPaint(); }
void IVEngineClient::PaintAllSurfaces( BYTE color )
{ static_cast<CEngineClient*>(this)->PaintAllSurfaces( color ); }
void IVEngineClient::RemovePaint( const model_t *pModel )
{ static_cast<CEngineClient*>(this)->RemovePaint( pModel ); }
bool IVEngineClient::IsActiveApp()
{ return static_cast<CEngineClient*>(this)->IsActiveApp(); }
bool IVEngineClient::IsClientLocalToActiveServer()
{ return static_cast<CEngineClient*>(this)->IsClientLocalToActiveServer(); }
void IVEngineClient::TickProgressBar()
{ static_cast<CEngineClient*>(this)->TickProgressBar(); }
InputContextHandle_t IVEngineClient::GetInputContext( EngineInputContextId_t id )
{ return static_cast<CEngineClient*>(this)->GetInputContext( id ); }
void IVEngineClient::GetStartupImage( char *pOutBuff, int nOutBuffSize )
{ static_cast<CEngineClient*>(this)->GetStartupImage( pOutBuff, nOutBuffSize ); }
bool IVEngineClient::IsUsingLocalNetworkBackdoor()
{ return static_cast<CEngineClient*>(this)->IsUsingLocalNetworkBackdoor(); }
int IVEngineClient::GetGenericMemoryStats( GenericMemoryStat_t **ppMemoryStats )
{ return static_cast<CEngineClient*>(this)->GetGenericMemoryStats( ppMemoryStats ); }
bool IVEngineClient::GameHasShutdownAndFlushedMemory()
{ return static_cast<CEngineClient*>(this)->GameHasShutdownAndFlushedMemory(); }
int IVEngineClient::GetLastAcknowledgedCommand( void )
{ return static_cast<CEngineClient*>(this)->GetLastAcknowledgedCommand(); }
void IVEngineClient::FinishContainerWrites( int iController )
{ static_cast<CEngineClient*>(this)->FinishContainerWrites( iController ); }
int IVEngineClient::GetServerTick( void )
{ return static_cast<CEngineClient*>(this)->GetServerTick(); }
const char * IVEngineClient::GetModDirectory( void )
{ return static_cast<CEngineClient*>(this)->GetModDirectory(); }
void IVEngineClient::AudioLanguageChanged()
{ static_cast<CEngineClient*>(this)->AudioLanguageChanged(); }
void IVEngineClient::StartLoadingScreenForCommand( const char* command )
{ static_cast<CEngineClient*>(this)->StartLoadingScreenForCommand( command ); }
void IVEngineClient::StartLoadingScreenForKeyValues( KeyValues* keyValues )
{ static_cast<CEngineClient*>(this)->StartLoadingScreenForKeyValues( keyValues ); }
bool IVEngineClient::SOSSetOpvarFloat( const char *pOpVarName, float flValue )
{ return static_cast<CEngineClient*>(this)->SOSSetOpvarFloat( pOpVarName, flValue ); }
bool IVEngineClient::SOSGetOpvarFloat( const char *pOpVarName, float &flValue )
{ return static_cast<CEngineClient*>(this)->SOSGetOpvarFloat( pOpVarName, flValue ); }
CDemoPlaybackParameters_t const * IVEngineClient::GetDemoPlaybackParameters()
{ return static_cast<CEngineClient*>(this)->GetDemoPlaybackParameters(); }
int IVEngineClient::GetClientVersion() const
{ return static_cast<const CEngineClient*>(this)->GetClientVersion(); }
bool IVEngineClient::IsDemoSkipping()
{ return static_cast<CEngineClient*>(this)->IsDemoSkipping(); }
void IVEngineClient::SetDemoImportantEventData( const KeyValues *pData )
{ static_cast<CEngineClient*>(this)->SetDemoImportantEventData( pData ); }
void IVEngineClient::ClearEvents()
{ static_cast<CEngineClient*>(this)->ClearEvents(); }
float IVEngineClient::GetSafeZoneXMin( void ) const
{ return static_cast<const CEngineClient*>(this)->GetSafeZoneXMin(); }
bool IVEngineClient::IsVoiceRecording() const
{ return static_cast<const CEngineClient*>(this)->IsVoiceRecording(); }
void IVEngineClient::ForceVoiceRecordOn() const
{ static_cast<const CEngineClient*>(this)->ForceVoiceRecordOn(); }
int IVEngineClient::GetConnectionDataProtocol() const
{ return static_cast<const CEngineClient*>(this)->GetConnectionDataProtocol(); }
bool IVEngineClient::EngineGotvSyncPacket( const ks::net::CEngineGotvSyncPacket *pPkt )
{ return static_cast<CEngineClient*>(this)->EngineGotvSyncPacket( pPkt ); }
const char* IVEngineClient::AliasToCommandString( const char* szAliasName )
{ return static_cast<CEngineClient*>(this)->AliasToCommandString( szAliasName ); }

