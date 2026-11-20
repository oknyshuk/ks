#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import ks.core.basetypes;
import ks.core.types;
import ks.core.vprof;
import ks.core.wchartypes;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.math.vplane;
import ks.platform;
import ks.simd;
import ks.tier1.bitbuf;
import ks.tier1.convar;
import ks.tier1.cvar;
import ks.tier1.icvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;
import ks.tier1.playerid;
#include "edict.h"
#include "iserverentity.h"
#include "engine/ivmodelinfo.h"
#include "soundflags.h"
import std;
#include "engine/iserverplugin.h"
#include "iclient.h"

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class	SendTable;
class	ServerClass;
struct  Ray_t;
class	CGameTrace;
typedef	CGameTrace trace_t;
struct	typedescription_t;
struct	datamap_t;
class	SendTable;
class	ServerClass;
struct  Ray_t;
struct	studiohdr_t;
struct	vcollide_t;
class	IRecipientFilter;
class	ITraceFilter;
struct	client_textmessage_t;
class	INetChannelInfo;
class	ISpatialPartition;
class CStandardSendProxies;
class IAchievementMgr;
class ISPSharedMemory;
namespace ks::net { struct CEngineGotvSyncPacket; }

typedef struct player_info_s player_info_t;

//-----------------------------------------------------------------------------
// defines
//-----------------------------------------------------------------------------

inline constexpr const char *INTERFACEVERSION_VENGINESERVER = "VEngineServer023";

struct bbox_t
{
	Vector mins;
	Vector maxs;
};

enum EncryptedMessageKeyType_t
{
	// Warning: do not renumber the key types as they get baked in demos
	kEncryptedMessageKeyType_None = 0,
	kEncryptedMessageKeyType_Private = 1,
	kEncryptedMessageKeyType_Public = 2,
};

struct CEngineHltvInfo_t
{
	bool m_bBroadcastActive;		// Whether TV broadcast is active
	bool m_bMasterProxy;			// Whether this is master proxy
	float m_flDelay;				// TV delay
	float m_flTime;					// TV match time
	int m_nTvPort;					// TV port
	int m_numSlots;					// Total number of TV slots
	int m_numClients;				// Clients (spectators+proxies) count
	int m_numProxies;				// Number of proxies
	int m_numLocalSlots;			// Local number of TV slots
	int m_numLocalClients;			// Local clients (spectators+proxies) count
	int m_numLocalProxies;			// Local number of proxies
	int m_numRelaySlots;			// Relay number of TV slots
	int m_numRelayClients;			// Relay clients (spectators+proxies) count
	int m_numRelayProxies;			// Relay number of proxies
	int m_numExternalTotalViewers;	// External total viewers
	int m_numExternalLinkedViewers;	// External linked viewers
	uint32 m_relayAddress;			// Relay address
	uint32 m_relayPort;				// Relay port
};

struct ClientReplayEventParams_t
{
	ClientReplayEventParams_t( int nEventType )
	{
		m_nEventType = nEventType;
		m_flSlowdownLength = 0;
		m_flSlowdownRate = 1.0f;
		m_nPrimaryTargetEntIndex = -1; // unknown by default
		m_flEventTime = -1.0; // unknown by default
	}
	int m_nEventType;

	float m_flSlowdownLength;
	float m_flSlowdownRate;

	int m_nPrimaryTargetEntIndex;
	float m_flEventTime;
};

//-----------------------------------------------------------------------------
// Purpose: Interface the engine exposes to the game DLL
//-----------------------------------------------------------------------------
class IVEngineServer
{
public:
	// Tell engine to change level ( "changelevel s1\n" or "changelevel2 s1 s2\n" )
	void		ChangeLevel( const char *s1, const char *s2 );
	
	// Ask engine whether the specified map is a valid map file (exists and has valid version number).
	int			IsMapValid( const char *filename );
	
	// Is this a dedicated server?
	bool		IsDedicatedServer( void );
	
	// Is in Hammer editing mode?
	int			IsInEditMode( void );

	// get arbitrary launch options
	KeyValues* GetLaunchOptions( void );
	
	// Add to the server/client lookup/precache table, the specified string is given a unique index
	// NOTE: The indices for PrecacheModel are 1 based
	//  a 0 returned from those methods indicates the model or sound was not correctly precached
	// However, generic and decal are 0 based
	// If preload is specified, the file is loaded into the server/client's cache memory before level startup, otherwise
	//  it'll only load when actually used (which can cause a disk i/o hitch if it occurs during play of a level).
	int			PrecacheModel( const char *s, bool preload = false );
	int			PrecacheSentenceFile( const char *s, bool preload = false );
	int			PrecacheDecal( const char *name, bool preload = false );
	int			PrecacheGeneric( const char *s, bool preload = false );

	// Check's if the name is precached, but doesn't actually precache the name if not...
	bool		IsModelPrecached( char const *s ) const;
	bool		IsDecalPrecached( char const *s ) const;
	bool		IsGenericPrecached( char const *s ) const;

	// Note that sounds are precached using the IEngineSound interface

	// Special purpose PVS checking
	// Get the cluster # for the specified position
	int			GetClusterForOrigin( const Vector &org );
	// Get the PVS bits for a specified cluster and copy the bits into outputpvs.  Returns the number of bytes needed to pack the PVS
	int			GetPVSForCluster( int cluster, int outputpvslength, unsigned char *outputpvs );
	// Check whether the specified origin is inside the specified PVS
	bool		CheckOriginInPVS( const Vector &org, const unsigned char *checkpvs, int checkpvssize );
	// Check whether the specified worldspace bounding box is inside the specified PVS
	bool		CheckBoxInPVS( const Vector &mins, const Vector &maxs, const unsigned char *checkpvs, int checkpvssize );

	// Returns the server assigned userid for this player.  Useful for logging frags, etc.  
	//  returns -1 if the edict couldn't be found in the list of players.
	int			GetPlayerUserId( const edict_t *e ); 
	const char	*GetPlayerNetworkIDString( const edict_t *e );
	bool		IsUserIDInUse( int userID );	// TERROR: used for transitioning
	int			GetLoadingProgressForUserID( int userID );	// TERROR: used for transitioning

	// Return the current number of used edict slots
	int			GetEntityCount( void );
	
	// Get stats info interface for a client netchannel
	INetChannelInfo* GetPlayerNetInfo( int playerIndex );
	
	// Allocate space for string and return index/offset of string in global string list
	// If iForceEdictIndex is not -1, then it will return the edict with that index. If that edict index
	// is already used, it'll return null.
	edict_t		*CreateEdict( int iForceEdictIndex = -1 );
	// Remove the specified edict and place back into the free edict list
	void		RemoveEdict( edict_t *e );
	
	// Memory allocation for entity class data
	void		*PvAllocEntPrivateData( long cb );
	void		FreeEntPrivateData( void *pEntity );

	// Save/restore uses a special memory allocator (which zeroes newly allocated memory, etc.)
	
	// Emit an ambient sound associated with the specified entity
	void		EmitAmbientSound( int entindex, const Vector &pos, const char *samp, float vol, soundlevel_t soundlevel, int fFlags, int pitch, float delay = 0.0f );

	// Fade out the client's volume level toward silence (or fadePercent)
	void        FadeClientVolume( const edict_t *pEdict, float fadePercent, float fadeOutSeconds, float holdTime, float fadeInSeconds );
	
	// Sentences / sentence groups
	int			SentenceGroupPick( int groupIndex, char *name, int nameBufLen );
	int			SentenceGroupPickSequential( int groupIndex, char *name, int nameBufLen, int sentenceIndex, int reset );
	int			SentenceIndexFromName( const char *pSentenceName );
	const char *SentenceNameFromIndex( int sentenceIndex );
	int			SentenceGroupIndexFromName( const char *pGroupName );
	const char *SentenceGroupNameFromIndex( int groupIndex );
	float		SentenceLength( int sentenceIndex );

	// Issue a command to the command parser as if it was typed at the server console.	
	void		ServerCommand( const char *str );
	// Execute any commands currently in the command parser immediately (instead of once per frame)
	void		ServerExecute( void );
	// Issue the specified command to the specified client (mimics that client typing the command at the console).
	[[gnu::format(printf, 3, 4)]] void		ClientCommand( edict_t *pEdict, const char *szFmt, ... );

	// Set the lightstyle to the specified value and network the change to any connected clients.  Note that val must not 
	//  change place in memory (use MAKE_STRING) for anything that's not compiled into your mod.
	void		LightStyle( int style, const char *val );

	// Project a static decal onto the specified entity / model (for level placed decals in the .bsp)
	void		StaticDecal( const Vector &originInEntitySpace, int decalIndex, int entityIndex, int modelIndex, bool lowpriority );
	
	// Given the current PVS(or PAS) and origin, determine which players should hear/receive the message
	void		Message_DetermineMulticastRecipients( bool usepas, const Vector& origin, CPlayerBitVec& playerbits );

	// Begin a message from a server side entity to its client side counterpart (func_breakable glass, e.g.)
	bf_write	*EntityMessageBegin( int ent_index, ServerClass * ent_class, bool reliable );
	// Finish the Entity or UserMessage and dispatch to network layer
	void		MessageEnd( void );

	// Send a serialized user message; see game/server/enginecallback.h for the wrapper.
	void		SendUserMessage( IRecipientFilter& filter, int message, const void *data, int size );

	// Print szMsg to the client console.
	void		ClientPrintf( edict_t *pEdict, const char *szMsg );

	// SINGLE PLAYER/LISTEN SERVER ONLY (just matching the client .dll api for this)
	// Prints the formatted string to the notification area of the screen ( down the right hand edge
	//  numbered lines starting at position 0
	void		Con_NPrintf( int pos, const char *fmt, ... );
	// SINGLE PLAYER/LISTEN SERVER ONLY(just matching the client .dll api for this)
	// Similar to Con_NPrintf, but allows specifying custom text color and duration information
	void		Con_NXPrintf( const struct con_nprint_s *info, const char *fmt, ... );

	// Change a specified player's "view entity" (i.e., use the view entity position/orientation for rendering the client view)
	void		SetView( const edict_t *pClient, const edict_t *pViewent );

	// Set the player's crosshair angle
	void		CrosshairAngle( const edict_t *pClient, float pitch, float yaw );

	// Get the current game directory (hl2, tf2, hl1, cstrike, etc.)
	void        GetGameDir( char *szGetGameDir, int maxlength );

	// Used by AI node graph code to determine if .bsp and .ain files are out of date
	int 		CompareFileTime( const char *filename1, const char *filename2, int *iCompare );

	// Locks/unlocks the network string tables (.e.g, when adding bots to server, this needs to happen).
	// Be sure to reset the lock after executing your code!!!
	bool		LockNetworkStringTables( bool lock );

	// Create a bot with the given name.  Returns NULL if fake client can't be created
	edict_t		*CreateFakeClient( const char *netname );	

	// Get a convar keyvalue for s specified client
	const char	*GetClientConVarValue( int clientIndex, const char *name );
	
	// Parse a token from a file
	const char	*ParseFile( const char *data, char *token, int maxlen );
	// Copies a file
	bool		CopyFile( const char *source, const char *destination );

	// Reset the pvs, pvssize is the size in bytes of the buffer pointed to by pvs.
	// This should be called right before any calls to AddOriginToPVS
	void		ResetPVS( byte *pvs, int pvssize );
	// Merge the pvs bits into the current accumulated pvs based on the specified origin ( not that each pvs origin has an 8 world unit fudge factor )
	void		AddOriginToPVS( const Vector &origin );
	
	// Mark a specified area portal as open/closed.
	// Use SetAreaPortalStates if you want to set a bunch of them at a time.
	void		SetAreaPortalState( int portalNumber, int isOpen );
	
	// Queue a temp entity for transmission
	void		PlaybackTempEntity( IRecipientFilter& filter, float delay, const void *pSender, const SendTable *pST, int classID  );
	// Given a node number and the specified PVS, return with the node is in the PVS
	int			CheckHeadnodeVisible( int nodenum, const byte *pvs, int vissize );
	// Using area bits, cheeck whether area1 flows into area2 and vice versa (depends on area portal state)
	int			CheckAreasConnected( int area1, int area2 );
	// Given an origin, determine which area index the origin is within
	int			GetArea( const Vector &origin );
	// Get area portal bit set
	void		GetAreaBits( int area, unsigned char *bits, int buflen );
	// Given a view origin (which tells us the area to start looking in) and a portal key,
	// fill in the plane that leads out of this area (it points into whatever area it leads to).
	bool		GetAreaPortalPlane( Vector const &vViewOrigin, int portalKey, VPlane *pPlane );

	// Save/restore wrapper - FIXME:  At some point we should move this to it's own interface
	bool		LoadGameState( char const *pMapName, bool createPlayers );
	void		LoadAdjacentEnts( const char *pOldLevel, const char *pLandmarkName );
	void		ClearSaveDir();

	// Get the pristine map entity lump string.  (e.g., used by CS to reload the map entities when restarting a round.)
	const char*	GetMapEntitiesString();

	// Text message system -- lookup the text message of the specified name
	client_textmessage_t *TextMessageGet( const char *pName );

	// Print a message to the server log file
	void		LogPrint( const char *msg );
	bool		IsLogEnabled();
	// Builds PVS information for an entity
	void		BuildEntityClusterList( edict_t *pEdict, PVSInfo_t *pPVSInfo );

	// A solid entity moved, update spatial partition
	void SolidMoved( edict_t *pSolidEnt, ICollideable *pSolidCollide, const Vector* pPrevAbsOrigin, bool testSurroundingBoundsOnly );
	// A trigger entity moved, update spatial partition
	void TriggerMoved( edict_t *pTriggerEnt, bool testSurroundingBoundsOnly );
	
	// Create/destroy a custom spatial partition
	ISpatialPartition *CreateSpatialPartition( const Vector& worldmin, const Vector& worldmax );
	void 		DestroySpatialPartition( ISpatialPartition * );

	// This returns which entities, to the best of the server's knowledge, the client currently knows about.
	// This is really which entities were in the snapshot that this client last acked.
	// This returns a bit vector with one bit for each entity.
	//
	// USE WITH CARE. Whatever tick the client is really currently on is subject to timing and
	// ordering differences, so you should account for about a quarter-second discrepancy in here.
	// Also, this will return NULL if the client doesn't exist or if this client hasn't acked any frames yet.
	// 
	// iClientIndex is the CLIENT index, so if you use pPlayer->entindex(), subtract 1.
	const std::bitset<MAX_EDICTS>* GetEntityTransmitBitsForClient( int iClientIndex );
	
	// Is the game paused?
	bool		IsPaused();

	// What is the game timescale multiplied with the host_timescale?
	float GetTimescale( void ) const;
	
	// Marks the filename for consistency checking.  This should be called after precaching the file.
	void		ForceExactFile( const char *s );
	void		ForceModelBounds( const char *s, const Vector &mins, const Vector &maxs );
	void		ClearSaveDirAfterClientLoad();

	// Sets a USERINFO client ConVar for a fakeclient
	void		SetFakeClientConVarValue( edict_t *pEntity, const char *cvar, const char *value );
	
	// Marks the material (vmt file) for consistency checking.  If the client and server have different
	// contents for the file, the client's vmt can only use the VertexLitGeneric shader, and can only
	// contain $baseTexture and $bumpmap vars.
	void		ForceSimpleMaterial( const char *s );

	// Is the engine running a background map?
	bool		IsLevelMainMenuBackground( void );

	// Mark some area portals as open/closed. It's more efficient to use this
	// than a bunch of individual SetAreaPortalState calls.
	void		SetAreaPortalStates( const int *portalNumbers, const int *isOpen, int nPortals );

	// Called when relevant edict state flags change.
	void		NotifyEdictFlagsChange( int iEdict );
	
	// Only valid during CheckTransmit. Also, only the PVS, networked areas, and
	// m_pTransmitInfo are valid in the returned strucutre.
	const CCheckTransmitInfo* GetPrevCheckTransmitInfo( edict_t *pPlayerEdict );
	
	CSharedEdictChangeInfo* GetSharedEdictChangeInfo();

	// Tells the engine we can immdiately re-use all edict indices
	// even though we may not have waited enough time
	void			AllowImmediateEdictReuse( );

	// Returns true if the engine is an internal build. i.e. is using the internal bugreporter.
	bool		IsInternalBuild( void );

	IChangeInfoAccessor *GetChangeAccessor( const edict_t *pEdict );	

	// Name of most recently load .sav file
	char const *GetMostRecentlyLoadedFileName();
	char const *GetSaveFileName();

	// Cleans up the cluster list
	void CleanUpEntityClusterList( PVSInfo_t *pPVSInfo );

	void SetAchievementMgr( IAchievementMgr *pAchievementMgr );
	IAchievementMgr *GetAchievementMgr();

	int	GetAppID();
	
	bool IsLowViolence();

	bool IsAnyClientLowViolence();

	// Call this to find out the value of a cvar on the client.
	//
	// It is an asynchronous query, and it will call IServerGameDLL::OnQueryCvarValueFinished when
	// the value comes in from the client.
	//
	// Store the return value if you want to match this specific query to the OnQueryCvarValueFinished call.
	// Returns InvalidQueryCvarCookie if the entity is invalid.
	QueryCvarCookie_t StartQueryCvarValue( edict_t *pPlayerEntity, const char *pName );

	void InsertServerCommand( const char *str );

	// Fill in the player info structure for the specified player index (name, model, etc.)
	bool GetPlayerInfo( int ent_num, player_info_t *pinfo );

	// Returns true if this client has been fully authenticated
	bool IsClientFullyAuthenticated( edict_t *pEdict );

	// This makes the host run 1 tick per frame instead of checking the system timer to see how many ticks to run in a certain frame.
	// i.e. it does the same thing timedemo does.
	void SetDedicatedServerBenchmarkMode( bool bBenchmarkMode );

	bool IsSplitScreenPlayer( int ent_num );
	edict_t *GetSplitScreenPlayerAttachToEdict( int ent_num );
	int	GetNumSplitScreenUsersAttachedToEdict( int ent_num );
	edict_t *GetSplitScreenPlayerForEdict( int ent_num, int nSlot );

	// Used by Foundry to hook into the loadgame process and override the entities that are getting loaded.
	bool IsOverrideLoadGameEntsOn();

	// Used by Foundry when it changes an entity (and possibly its class) but preserves its serial number.
	void ForceFlushEntity( int iEntity );

	//Finds or Creates a shared memory space, the returned pointer will automatically be AddRef()ed
	ISPSharedMemory *GetSinglePlayerSharedMemorySpace( const char *szName, int ent_num = MAX_EDICTS );

	// Allocate hunk memory
	void *AllocLevelStaticData( std::size_t bytes );

	// Gets a list of all clusters' bounds.  Returns total number of clusters.
	int GetClusterCount();
	int GetAllClusterBounds( bbox_t *pBBoxList, int maxBBox );

	bool IsCreatingReslist();

	void Pause( bool bPause, bool bForce = false );

	void SetTimescale( float flTimescale );

	// Methods to set/get a gamestats data container so client & server running in same process can send combined data
	// The game's gamestats container, opaque to the engine
	void SetGamestatsData( void *pGamestatsData );
	void *GetGamestatsData();

	// Returns the player id of the specified player. It'll be NULL if the player hasn't authenticated yet.
	const CPlayerId	*GetClientPlayerID( const edict_t *pPlayerEdict, bool bRequireFullyAuthenticated = false );

	// Validate session
	void HostValidateSession();

	// Update the 360 pacifier/spinner
	void RefreshScreenIfNecessary();

	// Tells the engine to allocate paint surfaces
	bool HasPaintmap();

	// Returns true if the surface paint colors changed
	bool SpherePaintSurface( const model_t *pModel, const Vector& vPosition, BYTE color, float flSphereRadius, float flPaintCoatPercent );

	void SphereTracePaintSurface( const model_t *pModel, const Vector& vPosition, const Vector& vContactNormal, float flSphereRadius, std::vector< BYTE >& surfColor );
	void RemoveAllPaint();
	void PaintAllSurfaces( BYTE color );
	void RemovePaint( const model_t* pModel );

	// Send a client command keyvalues
	// keyvalues are deleted inside the function
	void ClientCommandKeyValues( edict_t *pEdict, KeyValues *pCommand );

	// Returns the XUID of the specified player. It'll be NULL if the player hasn't connected yet.
	uint64 GetClientXUID( edict_t *pPlayerEdict );
	bool IsActiveApp();

	void SetNoClipEnabled( bool bEnabled );

	void GetPaintmapDataRLE( std::vector< uint32 > &data );
	void LoadPaintmapDataRLE( const std::vector< uint32 > &data );
	void SendPaintmapDataToClient( edict_t *pPlayerEdict );

	// Gets the accumulated latency for the sounds related to choreos.
	float GetLatencyForChoreoSounds();

	CrossPlayPlatform_t GetClientCrossPlayPlatform( int ent_num );

	// Create the instance baseline for a serverclass if it doesn't exist in the engine's list already
	void EnsureInstanceBaseline( int ent_num );

	// Sets server reservation payload
	bool ReserveServerForQueuedGame( char const *szReservationPayload );

	// Get the TV information
	bool GetEngineHltvInfo( CEngineHltvInfo_t &info );

	// Add HLTV proxy whitelist to bypass password and auth checks upon connection, as CIDR a.b.c.d/numbits
	void AddHltvRelayProxyWhitelist( uint32 a, uint32 b, uint32 c, uint32 d, uint32 numbits );

	// Server version from the version info file
	int GetServerVersion() const;

	// On master HLTV this call updates number of external viewers
	void UpdateHltvExternalViewers( uint32 numTotalViewers, uint32 numLinkedViewers );

	// Check whether sv_shutdown was requested
	bool WasShutDownRequested() const;

	bool StartClientHltvReplay( int nClientIndex, const HltvReplayParams_t &params );
	void StopClientHltvReplay( int nClientIndex );
	int GetClientHltvReplayDelay( int nClientIndex );
	bool HasHltvReplay();
	bool ClientCanStartHltvReplay( int nClientIndex );
	void ClientResetReplayRequestTime( int nClientIndex );
	bool AnyClientsInHltvReplayMode();
	int GetLocalClientIndex( void );
};

inline constexpr const char *INTERFACEVERSION_SERVERGAMEDLL = "ServerGameDLL005";

//-----------------------------------------------------------------------------
// Purpose: These are the interfaces that the game .dll exposes to the engine
//-----------------------------------------------------------------------------
class IServerGameDLL
{
public:
	// Initialize the game (one-time call when the DLL is first loaded )
	// Return false if there is an error during startup.
	virtual bool			DLLInit(	CreateInterfaceFn engineFactory, 
										CreateInterfaceFn physicsFactory, 
										CreateInterfaceFn fileSystemFactory, 
										CGlobalVars *pGlobals) = 0;

	// This is called when a new game is started. (restart, map)
	virtual bool			GameInit( void ) = 0;

	// Called any time a new level is started (after GameInit() also on level transitions within a game)
	virtual bool			LevelInit( char const *pMapName, 
									char const *pMapEntities, char const *pOldLevel, 
									char const *pLandmarkName, bool loadGame, bool background ) = 0;

	// The server is about to activate
	virtual void			ServerActivate( edict_t *pEdictList, int edictCount, int clientMax ) = 0;

	// The server should run physics/think on all edicts
	virtual void			GameFrame( bool simulating ) = 0;

	// Called once per simulation frame on the final tick
	virtual void			PreClientUpdate( bool simulating ) = 0;

	// Called when a level is shutdown (including changing levels)
	virtual void			LevelShutdown( void ) = 0;
	// This is called when a game ends (server disconnect, death, restart, load)
	// NOT on level transitions within a game
	virtual void			GameShutdown( void ) = 0;

	// Called once during DLL shutdown
	virtual void			DLLShutdown( void ) = 0;

	// Get the simulation interval (must be compiled with identical values into both client and game .dll for MOD!!!)
	// Right now this is only requested at server startup time so it can't be changed on the fly, etc.
	virtual float			GetTickInterval( void ) const = 0;

	// Give the list of datatable classes to the engine.  The engine matches class names from here with
	//  edict_t::classname to figure out how to encode a class's data for networking
	virtual ServerClass*	GetAllServerClasses( void ) = 0;

	// Returns string describing current .dll.  e.g., TeamFortress 2, Half-Life 2.  
	//  Hey, it's more descriptive than just the name of the game directory
	virtual const char     *GetGameDescription( void ) = 0;      
	
	// Let the game .dll allocate it's own network/shared string tables
	virtual void			CreateNetworkStringTables( void ) = 0;
	
	// Save/restore system hooks
	virtual bool			IsRestoring() = 0;

	// Returns the number of entities moved across the transition
	// Build the list of maps adjacent to the current map

	// Hand over the StandardSendProxies in the game DLL's module.
	virtual CStandardSendProxies*	GetStandardSendProxies() = 0;

	// Called once during startup, after the game .dll has been loaded and after the client .dll has also been loaded
	virtual void			PostInit() = 0;
	// Called once per frame even when no level is loaded...
	virtual void			Think( bool finalTick ) = 0;


	// Returns true if the game DLL wants the server not to be made public.
	// Used by commentary system to hide multiplayer commentary servers from the master.
	virtual bool			ShouldHideServer( void ) = 0;

	virtual void			InvalidateMdlCache() = 0;

	// * This function is new with version 6 of the interface.
	//
	// This is called when a query from IServerPluginHelpers::StartQueryCvarValue is finished.
	// iCookie is the value returned by IServerPluginHelpers::StartQueryCvarValue.
	// Added with version 2 of the interface.
	virtual void			OnQueryCvarValueFinished( QueryCvarCookie_t iCookie, edict_t *pPlayerEntity, EQueryCvarValueStatus eStatus, const char *pCvarName, const char *pCvarValue ) = 0;

	// Called to apply lobby settings to a dedicated server
	virtual void			ApplyGameSettings( KeyValues *pKV ) = 0;

	// 
	virtual void			GetMatchmakingTags( char *buf, std::size_t bufSize ) = 0;

	virtual void			ServerHibernationUpdate( bool bHibernating ) = 0;

	// Added for CS:GO - 
	// In Competetive mode we do not want to allow direct-connect to Valve servers
	virtual bool			ShouldAllowDirectConnect() = 0;
	virtual bool			FriendsReqdForDirectConnect() = 0;
	virtual bool			IsLoadTestServer() = 0;

	// Is this an Official dedicated server for ranked matchmaking?
	virtual bool			IsValveDS() = 0;

	// Builds extended server info for new connecting client
	virtual KeyValues*		GetExtendedServerInfoForNewClient() = 0;

	// Marks the queue matchmaking game as starting
	virtual void ReportGCQueuedMatchStart( int32 iReservationStage, uint32 *puiConfirmedAccounts, int numConfirmedAccounts ) = 0;

	// Matchmaking game data buffer
	virtual void			GetMatchmakingGameData( char *buf, std::size_t bufSize ) = 0;

	// Returns which encryption key to use for messages to be encrypted for TV
	virtual EncryptedMessageKeyType_t GetMessageEncryptionKey( INetMessage *pMessage ) = 0;

	// If server game dll needs more time before server process quits then
	// it should return true to hold game server reservation from this interface method.
	// If this method returns false then the server process will clear the reservation
	// and might shutdown to meet uptime or memory limit requirements.
	virtual bool ShouldHoldGameServerReservation( float flTimeElapsedWithoutClients ) = 0;

	// Pure server validation failed for the given client, client supplied
	// data is included in the payload
	virtual void OnPureServerFileValidationFailure( edict_t *edictClient, const char *path, const char *fileName, uint32 crc, int32 hashType, int32 len, int packNumber, int packFileID ) = 0;

	// Precaches particle systems defined in the specific file
	virtual void PrecacheParticleSystemFile( const char *pParticleSystemFile ) = 0;

	// Last chance validation on connect packet for the client, non-NULL return value
	// causes the client connect to be aborted with the provided error
	virtual char const * ClientConnectionValidatePreNetChan( bool bGameServer, char const *adr, int nAuthProtocol, uint64 ullPlayerID ) = 0;

	// Network channel notification from engine to game server code
	virtual void OnEngineClientNetworkEvent( edict_t *edictClient, uint64 ullPlayerID, int nEventType, void *pvParam ) = 0;

	// Engine notifying GC with a message
	virtual void EngineGotvSyncPacket( const ks::net::CEngineGotvSyncPacket *pPkt ) = 0;

	// GOTV client attempt redirect over SDR
	virtual bool OnEngineClientProxiedRedirect( uint64 ullClient, const char *adrProxiedRedirect, const char *adrRegular ) = 0;
};

//-----------------------------------------------------------------------------
// Just an interface version name for the random number interface
// See vstdlib/random.h for the interface definition
// NOTE: If you change this, also change VENGINE_CLIENT_RANDOM_INTERFACE_VERSION in cdll_int.h
//-----------------------------------------------------------------------------
inline constexpr const char *VENGINE_SERVER_RANDOM_INTERFACE_VERSION = "VEngineRandom001";

inline constexpr const char *INTERFACEVERSION_SERVERGAMEENTS = "ServerGameEnts001";
//-----------------------------------------------------------------------------
// Purpose: Interface to get at server entities
//-----------------------------------------------------------------------------
class IServerGameEnts
{
public:
	virtual					~IServerGameEnts()	{}

	// The engine wants to mark two entities as touching
	virtual void			MarkEntitiesAsTouching( edict_t *e1, edict_t *e2 ) = 0;

	// Frees the entity attached to this edict
	virtual void			FreeContainingEntity( edict_t * ) = 0; 

	// This allows the engine to get at edicts in a CGameTrace.
	virtual edict_t*		BaseEntityToEdict( IServerUnknown *pEnt ) = 0;
	virtual IServerUnknown*	EdictToBaseEntity( edict_t *pEdict ) = 0;

	// This sets a bit in pInfo for each edict in the list that wants to be transmitted to the 
	// client specified in pInfo.
	//
	// This is also where an entity can force other entities to be transmitted if it refers to them
	// with ehandles.
	virtual void			CheckTransmit( CCheckTransmitInfo *pInfo, const unsigned short *pEdictIndices, int nEdicts ) = 0;

	// TERROR: Perform any PVS cleanup before a full update
	virtual void			PrepareForFullUpdate( edict_t *pEdict ) = 0;
};

inline constexpr const char *INTERFACEVERSION_SERVERGAMECLIENTS = "ServerGameClients004";

//-----------------------------------------------------------------------------
// Purpose: Player / Client related functions
//-----------------------------------------------------------------------------
class IServerGameClients
{
public:
	// Get server maxplayers and lower bound for same
	virtual void			GetPlayerLimits( int& minplayers, int& maxplayers, int &defaultMaxPlayers ) const = 0;

	// Client is connecting to server ( return false to reject the connection )
	//	You can specify a rejection message by writing it into reject
	virtual bool			ClientConnect( edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen ) = 0;

	// Client is going active
	// If bLoadGame is true, don't spawn the player because its state is already setup.
	virtual void			ClientActive( edict_t *pEntity, bool bLoadGame ) = 0;
	
	virtual void			ClientFullyConnect( edict_t *pEntity ) = 0;

	// Client is disconnecting from server
	virtual void			ClientDisconnect( edict_t *pEntity ) = 0;
	
	// Client is connected and should be put in the game
	virtual void			ClientPutInServer( edict_t *pEntity, char const *playername ) = 0;
	
	// The client has typed a command at the console
	virtual void			ClientCommand( edict_t *pEntity, const CCommand &args ) = 0;

	// Sets the client index for the client who typed the command into his/her console
	virtual void			SetCommandClient( int index ) = 0;
	
	// A player changed one/several replicated cvars (name etc)
	virtual void			ClientSettingsChanged( edict_t *pEdict ) = 0;
	
	// Determine PVS origin and set PVS for the player/viewentity
	virtual void			ClientSetupVisibility( edict_t *pViewEntity, edict_t *pClient, unsigned char *pvs, int pvssize ) = 0;
	
	// A block of CUserCmds has arrived from the user, decode them and buffer for execution during player simulation
	virtual float			ProcessUsercmds( edict_t *player, bf_read *buf, int numcmds, int totalcmds,
								int dropped_packets, bool ignore, bool paused ) = 0;
	
	// Let the game .dll do stuff after messages have been sent to all of the clients once the server frame is complete
	virtual void			PostClientMessagesSent( void ) = 0;

	// For players, looks up the CPlayerState structure corresponding to the player (the engine
	// and the game each define it, so it is passed untyped)
	virtual void			*GetPlayerState( edict_t *player ) = 0;

	// Get the ear position for a specified client
	virtual void			ClientEarPosition( edict_t *pEntity, Vector *pEarOrigin ) = 0;

	virtual bool			ClientReplayEvent( edict_t *pEntity, const ClientReplayEventParams_t &params ) = 0;

	// returns number of delay ticks if player is in Replay mode (0 = no delay)
	virtual int				GetReplayDelay( edict_t *player, int& entity ) = 0;

	// Anything this game .dll wants to add to the bug reporter text (e.g., the entity/model under the picker crosshair)
	//  can be added here
	virtual void			GetBugReportInfo( char *buf, int buflen ) = 0;

	// TERROR: A player sent a voice packet
	virtual void			ClientVoice( edict_t *pEdict ) = 0;

	// A user has had their network id setup and validated 
	virtual void			NetworkIDValidated( const char *pszUserName, const char *pszNetworkID, CPlayerId playerUID ) = 0;

	// Returns max splitscreen slot count ( 1 == no splits, 2 for 2-player split screen )
	virtual int				GetMaxSplitscreenPlayers() = 0;

	// Return # of human slots, -1 if can't determine or don't care (engine will assume it's == maxplayers )
	virtual int				GetMaxHumanPlayers() = 0;

	// The client has submitted a keyvalues command
	virtual void			ClientCommandKeyValues( edict_t *pEntity, KeyValues *pKeyValues ) = 0;

	// Server override for supplied client name
	virtual const char *	ClientNameHandler( uint64 xuid, const char *pchName ) = 0;

	// Client submitted a user command
	virtual void			ClientSvcUserMessage( edict_t *pEntity, int nType, int nPassthrough, uint32 cbSize, const void *pvBuffer ) = 0;

};

inline constexpr const char *INTERFACEVERSION_UPLOADGAMESTATS = "ServerUploadGameStats001";

class IUploadGameStats
{
public:
	// Note that this call will block the server until the upload is completed, so use only at levelshutdown if at all.
	bool UploadGameStats( 
		char const *mapname,				// Game map name
		unsigned int blobversion,			// Version of the binary blob data
		unsigned int blobsize,				// Size in bytes of blob data
		const void *pvBlobData );		// Pointer to the blob data.

	// Call when created to init the CSER connection
	void InitConnection( void );

	// Call periodically to poll for a CSER connection
	void UpdateConnection( void );

	// If user has disabled stats tracking, do nothing
	bool IsGameStatsLoggingEnabled();

	// Gets a non-personally identifiable unique ID for this user, used for tracking total gameplay time across
	//  multiple stats sessions, but isn't trackable back to their account or id.
	// Buffer should be 16 bytes, ID will come back as a hexadecimal string version of a GUID
	void GetPseudoUniqueId( char *buf, std::size_t bufsize );

	// For determining general % of users running using cyber cafe accounts...
	bool IsCyberCafeUser( void );

	// Only works in single player
	bool IsHDREnabled( void );
};

inline constexpr const char *INTERFACEVERSION_PLUGINHELPERSCHECK = "PluginHelpersCheck001";

//-----------------------------------------------------------------------------
// Purpose: allows the game dll to control which plugin functions can be run
//-----------------------------------------------------------------------------
class IPluginHelpersCheck
{
public:
	virtual bool CreateMessage( const char *plugin, edict_t *pEntity, DIALOG_TYPE type, KeyValues *data ) = 0;
};

//-----------------------------------------------------------------------------
// Purpose: Interface exposed from the client .dll back to the engine for specifying shared .dll IAppSystems (e.g., ISoundEmitterSystem)
//-----------------------------------------------------------------------------
class IServerDLLSharedAppSystems
{
public:
	virtual int	Count() = 0;
	virtual char const *GetDllName( int idx ) = 0;
	virtual char const *GetInterfaceName( int idx ) = 0;
};

inline constexpr const char *SERVER_DLL_SHARED_APPSYSTEMS = "VServerDllSharedAppSystems001";

inline constexpr const char *INTERFACEVERSION_SERVERGAMETAGS = "ServerGameTags001";

//-----------------------------------------------------------------------------
// Purpose: querying the game dll for Server cvar tags
//-----------------------------------------------------------------------------
class IServerGameTags
{
public:
	// Get the list of cvars that require tags to show differently in the server browser
	virtual void			GetTaggedConVarList( KeyValues *pCvarTagList ) = 0;
};
