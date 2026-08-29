//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: steam state machine that handles authenticating steam users
//
//===========================================================================//

#ifdef _WIN32
#include "winlite.h"
#include <winsock2.h> // INADDR_ANY defn
#else
#include <netinet/in.h>
#endif

#include "sv_steamauth.h"
#include "sv_filter.h"
#include "inetchannel.h"
#include "netadr.h"
#include "server.h"
#include "proto_oob.h"
#ifndef DEDICATED
#include "Steam.h"
#include "client.h"
#endif
#include "host.h"
#include "sv_plugin.h"
#include "sv_log.h"
#include "filesystem_engine.h"
#include "filesystem_init.h"
#include "tier0/icommandline.h"
#include "steam/steam_gameserver.h"
#include "hltvserver.h"
#if defined( REPLAY_ENABLED )
#include "replayserver.h"
#endif
#include "pr_edict.h"
#include "steam/steamclientpublic.h"
#include "mathlib/expressioncalculator.h"
#include "sys_dll.h"
#include "host_cmd.h"
#include "tier1/fmtstr.h"

extern EUniverse GetSteamUniverse( void );

extern ConVar sv_lan;
extern ConVar sv_region;
extern ConVar cl_hideserverip;



// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#pragma warning( disable: 4355 ) // disables ' 'this' : used in base member initializer list'

ConVar sv_master_share_game_socket( "sv_master_share_game_socket", "1", 0, 
	"Use the game's socket to communicate to the master server. "
	"If this is 0, then it will create a socket on -steamport + 1 "
	"to communicate to the master server on." );

ConVar sv_steamauth_enforce( "sv_steamauth_enforce", "2", FCVAR_RELEASE,
	"By default, player must maintain a reliable connection to Steam servers. When player Steam session drops, enforce it: "
	"2 = instantly kick, 1 = kick at next spawn, 0 = do not kick." );


//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
static CSteam3Server s_Steam3Server;
CSteam3Server  &Steam3Server()
{
	return s_Steam3Server;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSteam3Server::CSteam3Server() 
{
	m_bHasActivePlayers = false;
	m_bLogOnResult = false;
	m_eServerMode = eServerModeInvalid;
    m_bWantsSecure = false;		// default to insecure currently, this may change
    m_bInitialized = false;
	m_bLogOnFinished = false;
	m_bMasterServerUpdaterSharingGameSocket = false;
	m_steamIDLanOnly.InstancedSet( 0,0, k_EUniversePublic, k_EAccountTypeInvalid );
	m_SteamIDGS.InstancedSet( 1, 0, k_EUniverseInvalid, k_EAccountTypeInvalid );
	m_QueryPort = 0;
}


//-----------------------------------------------------------------------------
// Purpose: detect current server mode based on cvars & settings
//-----------------------------------------------------------------------------
EServerMode CSteam3Server::GetCurrentServerMode()
{
	if ( sv_lan.GetBool() )
	{
		return eServerModeNoAuthentication;
	}
	else if ( !Host_IsSecureServerAllowed() || CommandLine()->FindParm( "-insecure" ) )
	{
		return eServerModeAuthentication;
	}
	else 
	{
		return eServerModeAuthenticationAndSecure;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSteam3Server::~CSteam3Server()
{
	Shutdown();
}


void CSteam3Server::DeactivateAndLogoff()
{
	if ( serverGameDLL )
		serverGameDLL->GameServerSteamAPIActivated( false );

	if ( SteamGameServer() )
		SteamGameServer()->LogOff();

	Shutdown();
}


void CSteam3Server::Activate()
{
	// we are active, check if sv_lan changed
	if ( GetCurrentServerMode() == m_eServerMode )
	{
		// we are active and LANmode didnt change. done.
		return;
	}

	if ( BIsActive() )
	{
		// shut down before we change server mode
		Shutdown();
	}

	m_unIP = INADDR_ANY;
	m_usPort = 26900;

	int nPort = CommandLine()->FindParm( "-steamport" );
	if ( nPort )
	{
		char const *pPortNum = CommandLine()->GetParm( nPort + 1 );
		m_usPort = EvaluateExpression( pPortNum, 26900 );
	}

	static ConVarRef ipname( "ip" );
	if ( ipname.IsValid() && ipname.GetString()[0] )
	{
		netadr_t ipaddr;
		NET_StringToAdr( ipname.GetString(), &ipaddr );
		if ( !ipaddr.IsLoopback() && !ipaddr.IsLocalhost() )
		{
			m_unIP = ipaddr.GetIPHostByteOrder();
		}
	}
	static ConVarRef ipname_steam( "ip_steam" );	// Override Steam network interface if needed
	if ( ipname_steam.IsValid() && ipname_steam.GetString()[0] )
	{
		netadr_t ipaddr;
		NET_StringToAdr( ipname_steam.GetString(), &ipaddr );
		if ( !ipaddr.IsLoopback() && !ipaddr.IsLocalhost() )
		{
			m_unIP = ipaddr.GetIPHostByteOrder();
		}
	}

	m_eServerMode = GetCurrentServerMode();

	char gamedir[MAX_OSPATH];
	Q_FileBase( com_gamedir, gamedir, sizeof( gamedir ) );

	// Figure out the game port. If we're doing a SrcTV relay, then ignore the NS_SERVER port and don't tell Steam that we have a game server.
	uint16 usGamePort = NET_GetUDPPort( NS_SERVER );
	uint16 usSpectatorPort = NET_GetUDPPort( NS_HLTV );	// ???TODO: what about GOTV[1]?

	uint16 usMasterServerUpdaterPort;
	if ( sv_master_share_game_socket.GetBool() )
	{
		m_bMasterServerUpdaterSharingGameSocket = true;
		usMasterServerUpdaterPort = MASTERSERVERUPDATERPORT_USEGAMESOCKETSHARE;
		if ( sv.IsActive() )
			m_QueryPort = usGamePort;
		else
			m_QueryPort = usSpectatorPort;
	}
	else
	{
		m_bMasterServerUpdaterSharingGameSocket = false;
		usMasterServerUpdaterPort = m_usPort;
		m_QueryPort = m_usPort;
	}
	
	
	m_eServerMode = eServerModeNoAuthentication;
	sv_lan.SetValue( true );
	return;
}


//-----------------------------------------------------------------------------
// Purpose: game server stopped, shutdown Steam game server session
//-----------------------------------------------------------------------------
void CSteam3Server::Shutdown()
{
	if ( !BIsActive() )
		return;
	m_bHasActivePlayers = false;
	m_bLogOnResult = false;
	m_SteamIDGS = k_steamIDNotInitYetGS;
	m_eServerMode = eServerModeInvalid;

}


//-----------------------------------------------------------------------------
// Purpose: returns true if the userid's are the same
//-----------------------------------------------------------------------------
bool CSteam3Server::CompareUserID( const USERID_t & id1, const USERID_t & id2 )
{
	if ( id1.idtype != id2.idtype )
		return false;

	switch ( id1.idtype )
	{
	case IDTYPE_STEAM:
	case IDTYPE_VALVE:
		{
			return ( id1.uid.steamid.m_SteamInstanceID==id2.uid.steamid.m_SteamInstanceID && 
					 id1.uid.steamid.m_SteamLocalUserID.As64bits == id2.uid.steamid.m_SteamLocalUserID.As64bits);
		}
	default:
		break;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: returns true if this userid is already on this server
//-----------------------------------------------------------------------------
bool CSteam3Server::CheckForDuplicateSteamID( const CBaseClient *client )
{
	// in LAN mode we allow reuse of SteamIDs
	if ( BLanOnly() ) 
		return false;

	// Compare connecting client's ID to other IDs on the server
	for ( int i=0 ; i< sv.GetClientCount() ; i++ )
	{
		const IClient *cl = sv.GetClient( i );

		// Not connected, no SteamID yet
		if ( !cl->IsConnected() || cl->IsFakeClient() )
			continue;

		if ( cl->GetNetworkID().idtype != IDTYPE_STEAM )
			continue;

		// don't compare this client against himself in the list
		if ( client == cl )
			continue;

		if ( !CompareUserID( client->GetNetworkID(), cl->GetNetworkID() ) )
			continue;

		// SteamID is reused
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Called when secure policy is set
//-----------------------------------------------------------------------------
const CSteamID &CSteam3Server::GetGSSteamID() const
{
	if ( BLanOnly() )
	{
		// return special LAN mode SteamID
		static CSteamID s_LAN = k_steamIDLanModeGS;
		return s_LAN; 
	}
	else
	{
		return m_SteamIDGS;
	}
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : steamIDFind - 
// Output : IClient
//-----------------------------------------------------------------------------
CBaseClient *CSteam3Server::ClientFindFromSteamID( CSteamID & steamIDFind )
{
	for ( int i=0 ; i< sv.GetClientCount() ; i++ )
	{
		CBaseClient *cl = (CBaseClient *)sv.GetClient( i );

		// Not connected, no SteamID yet
		if ( !cl->IsConnected() || cl->IsFakeClient() )
			continue;

		if ( cl->GetNetworkID().idtype != IDTYPE_STEAM )
			continue;

		USERID_t id = cl->GetNetworkID();
		CSteamID steamIDClient;
		steamIDClient.SetFromSteam2( &id.uid.steamid, steamIDFind.GetEUniverse() );
		if (steamIDClient == steamIDFind )
		{
			return cl;
		}
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: tell Steam that a new user connected
//-----------------------------------------------------------------------------
bool CSteam3Server::NotifyClientConnect( CBaseClient *client, uint32 unUserID, const ns_address & adr, const void *pvCookie, uint32 ucbCookie )
{
	if ( !BIsActive() ) 
		return true;

	if ( !client || client->IsFakeClient() )
		return false;

	// steamID is prepended to the ticket
	CUtlBuffer buffer( pvCookie, ucbCookie, CUtlBuffer::READ_ONLY );
	uint64 ulSteamID = LittleQWord( buffer.GetInt64() );

	CSteamID steamID( ulSteamID );
	// skip the steamID
	pvCookie = (uint8 *)pvCookie + sizeof( uint64 );
	ucbCookie -= sizeof( uint64 );
	EBeginAuthSessionResult eResult = SteamGameServer()->BeginAuthSession( pvCookie, ucbCookie, steamID );
	switch ( eResult )
	{
	case k_EBeginAuthSessionResultOK:
		//Msg("S3: BeginAuthSession request for %x was good.\n", steamID.ConvertToUint64( ) );
		break;
	case k_EBeginAuthSessionResultInvalidTicket:
		Msg("S3: Client connected with invalid ticket: UserID: %x\n", unUserID );
		return false;
	case k_EBeginAuthSessionResultDuplicateRequest:
		Msg("S3: Duplicate client connection: UserID: %x SteamID %llx\n", unUserID, steamID.ConvertToUint64( ) );
		return false;
	case k_EBeginAuthSessionResultInvalidVersion:
		Msg("S3: Client connected with invalid ticket ( old version ): UserID: %x\n", unUserID );
		return false;
	case k_EBeginAuthSessionResultGameMismatch:
		// This error would be very useful to present to the client.
		Msg("S3: Client connected with ticket for the wrong game: UserID: %x\n", unUserID );
		return false;
	case k_EBeginAuthSessionResultExpiredTicket:
		Msg("S3: Client connected with expired ticket: UserID: %x\n", unUserID );
		return false;
	}

	extern ConVar sv_mmqueue_reservation;
	if ( steamID.IsValid() && sv_mmqueue_reservation.GetString()[0] == 'Q' && ( client->GetServer() == &sv ) )
	{
		// For queue reservation client ID must match
		static char const * s_pchTournamentServer = CommandLine()->ParmValue( "-tournament", ( char const * ) NULL );
		bool bQMMplayer = !!strstr( sv_mmqueue_reservation.GetString(), CFmtStr( "[%x]", steamID.GetAccountID() ) );
		bool bQMMcaster = ( !bQMMplayer && s_pchTournamentServer && !!strstr( sv_mmqueue_reservation.GetString(), CFmtStr( "{%x}", steamID.GetAccountID() ) ) );
		if ( !bQMMplayer && !bQMMcaster )
		{
			Msg( "Q: Client connected with forged reservation ticket: userid %x, steamid: %llx\n", unUserID, steamID.ConvertToUint64() );
			return false;
		}
	}
	if ( steamID.IsValid() && steamID.BIndividualAccount() )
	{
		uint32 uiAccountIdSupplied = Q_atoi( client->GetUserSetting( "accountid" ) );
		if ( uiAccountIdSupplied && uiAccountIdSupplied != steamID.GetAccountID() )
		{
			Msg( "SteamID.AccountID: Client connected with forged accountid: userid %x, steamid: %llx, accountid: %u\n", unUserID, steamID.ConvertToUint64(), uiAccountIdSupplied );
			return false;
		}
		if ( adr.GetAddressType() == NSAT_PROXIED_CLIENT )
		{
			if ( adr.m_steamID.GetSteamID().GetAccountID() != steamID.GetAccountID() )
			{
				Msg( "SteamID.AccountID: Client connected with forged accountid: userid %x, steamid: %llx, SDR: %llx\n", unUserID, steamID.ConvertToUint64(), adr.m_steamID.GetSteamID().ConvertToUint64() );
				return false;
			}
		}
	}

	// first checks ok, we know now the SteamID
	client->SetSteamID( steamID );
	if ( steamID.IsValid() && steamID.BIndividualAccount() && ( adr.GetAddressType() == NSAT_PROXIED_CLIENT ) )
		client->SetFullyAuthenticated(); // always flag SDR clients as fully authenticated

	SendUpdatedServerDetails();

	return true;
}

bool CSteam3Server::NotifyLocalClientConnect( CBaseClient *client )
{
	CSteamID steamID;

	extern bool CanShowHostTvStatus();
	bool bClientIsTV = client->IsHLTV() || ( client->IsFakeClient() && !Q_strcmp( client->GetClientName(), "GOTV" ) );
	if ( SteamGameServer() && ( CanShowHostTvStatus() || !bClientIsTV ) )
	{
		steamID = SteamGameServer()->CreateUnauthenticatedUserConnection();
	}
	
	client->SetSteamID( steamID );

	SendUpdatedServerDetails();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *client - 
//-----------------------------------------------------------------------------
void CSteam3Server::NotifyClientDisconnect( CBaseClient *client )
{
	if ( !client || !BIsActive() || !client->IsConnected() || !client->m_SteamID.IsValid() )
		return;

	// Check if the client has a local (anonymous) steam account.  This is the
	// case for bots.  Currently it's also the case for people who connect
	// directly to the SourceTV port.
	if ( client->m_SteamID.GetEAccountType() == k_EAccountTypeAnonGameServer )
	{
		extern bool CanShowHostTvStatus();
		if ( !client->IsHLTV() || CanShowHostTvStatus() )
		{
			SteamGameServer()->SendUserDisconnect( client->m_SteamID );
		}

		// Clear the steam ID, as it was a dummy one that should not be used again
		client->m_SteamID = CSteamID();
	}
	else
	{

		// All bots should have an anonymous account ID
		Assert( !client->IsFakeClient() );

		USERID_t id = client->GetNetworkID();
		if ( id.idtype != IDTYPE_STEAM )
			return;

		// !FIXME! Use ticket auth here and call EndAuthSession

		// Msg("S3: Sending client disconnect for %x\n", steamIDClient.ConvertToUint64( ) );
		SteamGameServer()->EndAuthSession( client->m_SteamID );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSteam3Server::NotifyOfLevelChange()
{
	// we're changing levels, so we may not respond for a while
	if ( m_bHasActivePlayers )
	{
		m_bHasActivePlayers = false;
		SendUpdatedServerDetails();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSteam3Server::NotifyOfServerNameChange()
{
	SendUpdatedServerDetails();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSteam3Server::RunFrame()
{
	bool bHasPlayers = ( sv.GetNumClients() > 0 );

	if ( m_bHasActivePlayers != bHasPlayers )
	{
		m_bHasActivePlayers = bHasPlayers;
		SendUpdatedServerDetails();
	}

	static double s_fLastRunCallback = 0.0f;
	double fCurtime = Plat_FloatTime();
	if ( fCurtime - s_fLastRunCallback > 0.1f )
	{
		s_fLastRunCallback = fCurtime;
	}
}


//-----------------------------------------------------------------------------
// Purpose: lets the steam3 servers know our full details
// Input  : bChangingLevels - true if we're going to heartbeat slowly for a while
//-----------------------------------------------------------------------------
void CSteam3Server::SendUpdatedServerDetails()
{
	if ( !BIsActive() || SteamGameServer() == NULL )
		return;

	// Check if we are running with a special flag and not advertise to Steam
	KeyValues* nextNextSubkey = g_pLaunchOptions->GetFirstSubKey()->GetNextKey()->GetNextKey();
	if ( nextNextSubkey && !V_strcmp( nextNextSubkey->GetString(), "server_is_unavailable" ) )
		return;

	int nHumans;
	int nMaxHumans;
	int nBots;

	sv.GetMasterServerPlayerCounts( nHumans, nMaxHumans, nBots );

	SteamGameServer()->SetBotPlayerCount( nBots );
	SteamGameServer()->SetMaxPlayerCount( nMaxHumans );
	SteamGameServer()->SetPasswordProtected( sv.GetPassword() != NULL );
	SteamGameServer()->SetRegion( sv_region.GetString() );
	SteamGameServer()->SetServerName( sv.GetName() );

	char const *pszMapName = NULL;
	CHltvServerIterator hltv;
	if ( hltv && hltv->IsTVRelay() )
	{
		pszMapName = hltv->GetMapName();
	}
	else
	{
		pszMapName = sv.GetMapName();
	}

	// Drop the workshop prefix and fixup our official maps for server browser
	if ( char const *szStringAfterPrefix = StringAfterPrefix( pszMapName, "workshop" ) )
	{
		if ( ( szStringAfterPrefix[0] == '/' ) || ( szStringAfterPrefix[0] == '\\' ) )
		{
			pszMapName = szStringAfterPrefix;

			if ( ( Q_strlen( szStringAfterPrefix ) > 11 ) &&
				( ( szStringAfterPrefix[10] == '/' ) || ( szStringAfterPrefix[10] == '\\' ) ) )
			{
				bool bOfficialMap = false;
				/** Removed for partner depot **/
				if ( bOfficialMap )
				{
					size_t nOfficalLen = Q_strlen( pszMapName ) + 1;
					char *pchOfficialName = ( char * ) stackalloc( nOfficalLen );
					Q_snprintf( pchOfficialName, nOfficalLen, "//official/%s", szStringAfterPrefix + 11 );
					pszMapName = pchOfficialName;
				}
			}
		}
	}
	SteamGameServer()->SetMapName( pszMapName );

	if ( hltv != NULL )
	{
		SteamGameServer()->SetSpectatorPort( NET_GetUDPPort( NS_HLTV + hltv->GetInstanceIndex() ) );
		SteamGameServer()->SetSpectatorServerName( hltv->GetName() );
	}
	else
	{
		SteamGameServer()->SetSpectatorPort( 0 );
	}


//	Msg( "CSteam3Server::SendUpdatedServerDetails: nNumClients=%d, nMaxClients=%d, nFakeClients=%d:\n", nNumClients, nMaxClients, nFakeClients );
//	for ( int i = 0 ; i < sv.GetClientCount() ; ++i )
//	{
//		IClient	*c = sv.GetClient( i );
//		Msg("  %d: %s, connected=%d, replay=%d, fake=%d\n", i, c->GetClientName(), c->IsConnected() ? 1 : 0, c->IsReplay() ? 1 : 0, c->IsFakeClient() ? 1 : 0 );
//	}

	if ( serverGameDLL && GetGSSteamID().IsValid() )
		serverGameDLL->UpdateGCInformation();
}

bool CSteam3Server::IsMasterServerUpdaterSharingGameSocket()
{
	return m_bMasterServerUpdaterSharingGameSocket;
}

//-----------------------------------------------------------------------------
// Purpose: Select Steam gameserver account to login to
//-----------------------------------------------------------------------------
void sv_setsteamaccount_f( const CCommand &args )
{
	if ( Steam3Server( ).SteamGameServer( ) && Steam3Server( ).SteamGameServer( )->BLoggedOn( ) )
	{
		Warning( "Warning: Game server already logged into steam.  You need to use the sv_setsteamaccount command earlier.\n" );
		return;
	}

	if ( args.ArgC( ) != 2 )
	{
		Warning( "Usage: sv_setsteamaccount <login_token>\n" );
		return;
	}

	Steam3Server( ).SetAccount( args[ 1 ] );
}

static ConCommand sv_setsteamaccount( "sv_setsteamaccount", sv_setsteamaccount_f, "token\nSet game server account token to use for logging in to a persistent game server account", 0 );
