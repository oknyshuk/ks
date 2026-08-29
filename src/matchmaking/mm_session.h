//===== Copyright � 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose:
//
//===========================================================================//

#ifndef MM_SESSION_H
#define MM_SESSION_H
#ifdef _WIN32
#pragma once
#endif


class IMatchSessionInternal : public IMatchSession, public IMatchEventsSink
{
public:
	// Run a frame update
	virtual void Update() = 0;

	// Destroy the session object
	virtual void Destroy() = 0;

	// Debug print a session object
	virtual void DebugPrint() = 0;

	// Check if another session is joinable
	virtual bool IsAnotherSessionJoinable( char const *pszAnotherSessionInfo ) = 0;
};

#include "protocol.h"


#include "sys_session.h"

#include "mm_session_offline_custom.h"

void MatchSession_BroadcastSessionSettingsUpdate( KeyValues *pUpdateDeletePackage );
void MatchSession_PrepareClientForConnect( KeyValues *pSettings, uint64 uiReservationCookieOverride = 0ull );

struct MatchSessionServerInfo_t
{
	struct
	{
		char m_szPublicConnectionString[256];
		char m_szPrivateConnectionString[256];
	} m_dsResult;

	char m_szConnectCmd[256];

	char const *m_szSecureServerAddress;

	XUID m_xuidJingle;
	uint64 m_uiReservationCookie;

	enum ResolveFlags_t
	{
		RESOLVE_DSRESULT		= 0x01,
		RESOLVE_CONNECTSTRING	= 0x02,
		RESOLVE_ALLOW_EXTPEER	= 0x04,
		RESOLVE_QOS_RATE_PROBE	= 0x08,
	};

	enum ResolveMasks_t
	{
		RESOLVE_DEFAULT = RESOLVE_DSRESULT | RESOLVE_CONNECTSTRING | RESOLVE_QOS_RATE_PROBE,
	};
};
bool MatchSession_ResolveServerInfo( KeyValues *pSettings, CSysSessionBase *pSysSession,
									MatchSessionServerInfo_t &info, uint uiResolveFlags = MatchSessionServerInfo_t::RESOLVE_DEFAULT,
									uint64 ullCrypt = 0ull );

uint64 MatchSession_GetMachineFlags();
char const * MatchSession_GetTuInstalledString();

enum MatchSessionMachineFlags_t
{
	MACHINE_PLATFORM_PS3	=	( 1 << 0 ),			// Machine is PS3
};

char const * MatchSession_EncryptAddressString( char const *szAddress, uint64 ullCrypt );
char const * MatchSession_DecryptAddressString( char const *szAddress, uint64 ullCrypt );



#endif // MM_SESSION_H
