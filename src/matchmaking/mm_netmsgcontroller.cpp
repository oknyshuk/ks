// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

#include "mm_netmsgcontroller.h"

#include "matchmakingqos.h"


//
// Implementation
//

CMatchNetworkMsgControllerBase::CMatchNetworkMsgControllerBase()
{
}

CMatchNetworkMsgControllerBase::~CMatchNetworkMsgControllerBase()
{
}

static CMatchNetworkMsgControllerBase g_MatchNetMsgControllerBase;
CMatchNetworkMsgControllerBase *g_pMatchNetMsgControllerBase = &g_MatchNetMsgControllerBase;

//
// A way to copy just value
//

static void CopyValue( KeyValues *pTo, KeyValues *pFrom )
{
	switch ( pFrom->GetDataType() )
	{
	case KeyValues::TYPE_INT:
		pTo->SetInt( "", pFrom->GetInt() );
		break;
	case KeyValues::TYPE_UINT64:
		pTo->SetUint64( "", pFrom->GetUint64() );
		break;
	case KeyValues::TYPE_STRING:
		pTo->SetString( "", pFrom->GetString() );
		break;
	default:
		DevWarning( "NetMsgCtrlr::CopyValue using unknown type!\n" );
		contract_assert( 0 );
		break;
	}
}

//
// Implementation
//

MM_QOS_t CMatchNetworkMsgControllerBase::GetQOS()
{
	return MM_GetQos();
}

KeyValues * CMatchNetworkMsgControllerBase::GetActiveServerGameDetails( KeyValues *pRequest )
{
	// Query server info
	INetSupport::ServerInfo_t si;
	g_pMatchExtensions->GetINetSupport()->GetServerInfo( &si );

	KeyValues *pDetails = nullptr;
	
	if ( si.m_bActive )
	{
		//
		// Parse the game details from the values
		//
		pDetails = KeyValues::FromString(
			"GameDetailsServer",
			" system { "
				" network LIVE "
				" access public "
			" } "
			" server { "
				" name = "
				" server = "
				" adronline = "
				" adrlocal = "
			" } "
			" members { "
				" numSlots #int#0 "
				" numPlayers #int#0 "
			" } "
			);

		//
		// For a listen server and other MM session overlay the session settings
		//
		if ( !si.m_bDedicated && g_pMatchFramework->GetMatchSession() )
		{
			pDetails->MergeFrom( g_pMatchFramework->GetMatchSession()->GetSessionSettings(), KeyValues::MERGE_KV_BORROW );
		}

		//
		// Get server information
		//

		pDetails->SetString( "server/name", si.m_szServerName );
		pDetails->SetString( "server/server", si.m_bDedicated ? "dedicated" : "listen" );
		pDetails->SetString( "server/adronline", si.m_netAdrOnline.ToString() );
		pDetails->SetString( "server/adrlocal", si.m_netAdr.ToString() );

		if ( si.m_bDedicated && si.m_bLobbyExclusive && si.m_bGroupExclusive )
			pDetails->SetString( "system/access", "friends" );

		si.m_numMaxHumanPlayers = ClampArrayBounds( si.m_numMaxHumanPlayers, g_pMMF->GetMatchTitle()->GetTotalNumPlayersSupported() );
		pDetails->SetInt( "members/numSlots", si.m_numMaxHumanPlayers );

		si.m_numHumanPlayers = ClampArrayBounds( si.m_numHumanPlayers, si.m_numMaxHumanPlayers );
		pDetails->SetInt( "members/numPlayers", si.m_numHumanPlayers );

		static ConVarRef host_info_show( "host_info_show" );
		if ( host_info_show.GetInt() < 2 )
			pDetails->SetString( "options/action", "crypt" );
	}
	else if ( IVEngineClient *pIVEngineClient = g_pMatchExtensions->GetIVEngineClient() )
	{
		if ( pIVEngineClient->IsLevelMainMenuBackground() )
			return nullptr;

		char const *szLevelName = pIVEngineClient->GetLevelNameShort();
		if ( !szLevelName || !*szLevelName )
			return nullptr;

		pDetails = new KeyValues( "GameDetailsClient" );
	}

	if ( !pDetails )
		return nullptr;

	// Allow title to add game-specific settings
	g_pMMF->GetMatchTitleGameSettingsMgr()->ExtendServerDetails( pDetails, pRequest );

	return pDetails;
}

static KeyValues * GetLobbyDetailsTemplate( char const *szReason = "", KeyValues *pSettings = nullptr )
{
	KeyValues *pDetails = KeyValues::FromString(
		"settings",
		" system { "
			" network #empty# "
			" access #empty# "
			" netflag #empty# "
			" lock #empty# "
		" } "
		" options { "
			" server #empty# "
		" } "
		" members { "
			" numSlots #int#0 "
			" numPlayers #int#0 "
		" } "
		);
	
	g_pMMF->GetMatchTitleGameSettingsMgr()->ExtendLobbyDetailsTemplate( pDetails, szReason, pSettings );
	
	return pDetails;
}

KeyValues * CMatchNetworkMsgControllerBase::UnpackGameDetailsFromQOS( MM_GameDetails_QOS_t const *pvQosReply )
{
	//
	// Check if we have correct header
	//
	ks::byte_reader bufQos( pvQosReply->m_pvData, pvQosReply->m_numDataBytes );
	int iProtocol = bufQos.read<int>();
	int iVersion = bufQos.read<int>();

	if ( iProtocol != g_pMatchExtensions->GetINetSupport()->GetEngineBuildNumber() )
		return nullptr;
	if ( 0 != iVersion )
		return nullptr;

	//
	// Read the game details that we have received
	//
	KeyValues *pDetails = new KeyValues( "" );
	if ( !pDetails->ReadAsBinary( bufQos ) )
	{
		pDetails->deleteThis();
		return nullptr;
	}

	// Read the terminator
	int iTerm = bufQos.read<int>();
	if ( iTerm != 0 )
	{
		DevWarning( "UnpackGameDetailsFromQOS found bad QOS block terminator!\n" );
	}

	return pDetails;
}

void CMatchNetworkMsgControllerBase::PackageGameDetailsForQOS( KeyValues *pSettings, ks::byte_writer &buf )
{
	KeyValues *pDetails = GetLobbyDetailsTemplate( "qos", pSettings );
	KeyValues::AutoDelete autodelete( pDetails );

	// Keep only keys specified in the template
	pDetails->MergeFrom( pSettings, KeyValues::MERGE_KV_BORROW );

	// Write the details as binary
	buf.write<int>( g_pMatchExtensions->GetINetSupport()->GetEngineBuildNumber() );
	buf.write<int>( 0 );
	pDetails->WriteAsBinary( buf );
	buf.write<int>( 0 );
}

KeyValues * CMatchNetworkMsgControllerBase::PackageGameDetailsForReservation( KeyValues *pSettings )
{
	KeyValues *res = GetLobbyDetailsTemplate( "reserve", pSettings );
	res->SetName( COM_GetModDirectory() );

	// Keep only keys specified in the template
	res->MergeFrom( pSettings, KeyValues::MERGE_KV_BORROW );

	return res;
}

//-----------------------------------------------------------------------------
#include "facade/imatchnetworkmsgcontroller.inl"
