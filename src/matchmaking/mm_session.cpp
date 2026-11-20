// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

#include "mm_framework.h"
#include "filesystem.h"
import ks.core.dbg;
import ks.core.types;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.interfaces;
import ks.tier1.keyvalues;

void MatchSession_BroadcastSessionSettingsUpdate( KeyValues *pUpdateDeletePackage )
{
	KeyValues *notify = new KeyValues( "OnMatchSessionUpdate" );
	notify->SetString( "state", "updated" );

	if ( KeyValues *kvUpdate = pUpdateDeletePackage->FindKey( "update" ) )
		notify->AddSubKey( kvUpdate->MakeCopy() );
	if ( KeyValues *kvDelete = pUpdateDeletePackage->FindKey( "delete" ) )
		notify->AddSubKey( kvDelete->MakeCopy() );

	g_pMatchEventsSubscription->BroadcastEvent( notify );
}


ConVar cl_session( "cl_session", "", FCVAR_USERINFO | FCVAR_HIDDEN | FCVAR_SERVER_CAN_EXECUTE | FCVAR_DEVELOPMENTONLY );

void MatchSession_PrepareClientForConnect( KeyValues *pSettings, uint64 uiReservationCookieOverride )
{
	char chSession[64];
	std::sprintf( chSession, "$%llx", uiReservationCookieOverride ? uiReservationCookieOverride :
		g_pMatchFramework->GetMatchSession()->GetSessionSystemData()->
		GetUint64( "xuidReserve", 0ull ) );
	cl_session.SetValue( chSession );

	g_pMatchFramework->GetMatchTitle()->PrepareClientForConnect( pSettings );
}

ConVar mm_tu_string( "mm_tu_string", "00000000" );

uint64 MatchSession_GetMachineFlags()
{
	uint64 uiFlags = 0;
	return uiFlags;
}

char const * MatchSession_GetTuInstalledString()
{
	return mm_tu_string.GetString();
}

static void mm_debugprint( const CCommand &args );
static ConCommand mm_debugprint_command( "mm_debugprint", mm_debugprint, "Show debug information about current matchmaking session" );
static void mm_debugprint( const CCommand &args )
{
	if ( IMatchSession *pIMatchSession = g_pMMF->GetMatchSession() )
	{
		( ( IMatchSessionInternal * ) pIMatchSession )->DebugPrint();
	}
	else
	{
		DevMsg( "No match session.\n" );
	}
}
