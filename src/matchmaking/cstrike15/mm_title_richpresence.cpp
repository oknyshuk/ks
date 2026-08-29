//===== Copyright � 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_title_richpresence.h"
#include "mm_title_contextvalues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static void SetAllUsersContext( DWORD dwContextId, DWORD dwValue, bool bAsync = true )
{
}

static void SetAllUsersProperty( DWORD dwPropertyId, DWORD cbValue, void const *pvValue )
{
}

KeyValues * MM_Title_RichPresence_PrepareForSessionCreate( KeyValues *pSettings )
{

	return NULL;
}

void MM_Title_RichPresence_Update( KeyValues *pFullSettings, KeyValues *pUpdatedSettings )
{
	( void ) g_pMatchExtensions->GetIBaseClientDLL()->GetRichPresenceStatusString();
}

void MM_Title_RichPresence_PlayersChanged( KeyValues *pFullSettings )
{
//#ifdef _X360
//	// Set the installed DLCs masks
//	static int val[10]; // must be valid for the async call
//	uint64 uiDlcInstalled = g_pMatchFramework->GetMatchSystem()->GetDlcManager()->GetDataInfo()->GetUint64( "@info/installed" );
//	extern ConVar mm_matchmaking_dlcsquery;
//	for ( int k = 1; k <= mm_matchmaking_dlcsquery.GetInt(); ++ k )
//	{
//		val[k] = !!( uiDlcInstalled & ( 1ull << k ) );
//		DevMsg( "DLC%d installed: %d\n", k, val[k] );
//		SetAllUsersProperty( PROPERTY_INSTALLED_DLC1 - 1 + k, sizeof( val[k] ), &val[k] );
//	}
//#endif
}

// Called by the client to notify matchmaking that it should update matchmaking properties based
// on player distribution among the teams.
void MM_Title_RichPresence_UpdateTeamPropertiesCSGO( KeyValues *pCurrentSettings, KeyValues *pTeamProperties )
{
}

