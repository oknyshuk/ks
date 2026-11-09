// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "predictableid.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "soundflags.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "player.h"
#include "ndebugoverlay.h"
#include "env_debughistory.h"

// Number of characters worth of debug to use per history category
constexpr auto DEBUG_HISTORY_VERSION = 6;
constexpr auto DEBUG_HISTORY_FIRST_VERSIONED = 5;
constexpr auto MAX_DEBUG_HISTORY_LINE_LENGTH = 256;
constexpr auto MAX_DEBUG_HISTORY_LENGTH = (1000 * MAX_DEBUG_HISTORY_LINE_LENGTH);

//-----------------------------------------------------------------------------
// Purpose: Stores debug history in savegame files for debugging reference
//-----------------------------------------------------------------------------
class CDebugHistory : public CBaseEntity 
{
	DECLARE_CLASS( CDebugHistory, CBaseEntity );
public:

	void	Spawn_Impl();
	void	AddDebugHistoryLine( int iCategory, const char *szLine );
	void	ClearHistories( void );
	void	DumpDebugHistory( int iCategory );


private:
	char m_DebugLines[MAX_HISTORY_CATEGORIES][MAX_DEBUG_HISTORY_LENGTH];
	char *m_DebugLineEnd[MAX_HISTORY_CATEGORIES];
};


LINK_ENTITY_TO_CLASS( env_debughistory, CDebugHistory );

// The handle to the debug history singleton. Created on first access via GetDebugHistory.
static CHandle< CDebugHistory >	s_DebugHistory;


//-----------------------------------------------------------------------------
// Spawn
//-----------------------------------------------------------------------------
void CDebugHistory::Spawn_Impl()
{
	BaseClass::Spawn_Impl();

#ifdef DISABLE_DEBUG_HISTORY
	UTIL_Remove( this );
#else
	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		UTIL_Remove( this );
	}
	else
	{
		Warning( "DEBUG HISTORY IS ENABLED. Disable before release (in env_debughistory.h).\n" );
	}
#endif

	ClearHistories();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDebugHistory::AddDebugHistoryLine( int iCategory, const char *szLine )
{
	if ( iCategory < 0 || iCategory >= MAX_HISTORY_CATEGORIES )
	{
		Warning("Attempted to add a debughistory line to category %d. Valid categories are %d to %d.\n", iCategory, 0, (MAX_HISTORY_CATEGORIES-1) );
		return;
	}
	
	// Don't do debug history before the singleton is properly set up.
	if ( !m_DebugLineEnd[iCategory] )
		return;

	const char *pszRemaining = szLine;
	int iCharsToWrite = std::strlen( pszRemaining ) + 1;	// Add 1 so that we copy the null terminator

	// Clip the line if it's too long. Wasteful doing it this way, but keeps code below nice & simple.
	char szTmpBuffer[MAX_DEBUG_HISTORY_LINE_LENGTH];
	if ( iCharsToWrite > MAX_DEBUG_HISTORY_LINE_LENGTH)
	{
		std::memcpy( szTmpBuffer, szLine, sizeof(szTmpBuffer) );
		szTmpBuffer[MAX_DEBUG_HISTORY_LINE_LENGTH-1] = '\0';
		pszRemaining = szTmpBuffer;
		iCharsToWrite = MAX_DEBUG_HISTORY_LINE_LENGTH;
	}

	while ( iCharsToWrite )
	{
		int iCharsLeftBeforeLoop = sizeof(m_DebugLines[iCategory]) - (m_DebugLineEnd[iCategory] - m_DebugLines[iCategory]);

		// Write into the buffer
		int iWrote = std::min( iCharsToWrite, iCharsLeftBeforeLoop );
		std::memcpy( m_DebugLineEnd[iCategory], pszRemaining, iWrote );	
		m_DebugLineEnd[iCategory] += iWrote;
		pszRemaining += iWrote;

		// Did we loop?
		if ( iWrote == iCharsLeftBeforeLoop )
		{
			m_DebugLineEnd[iCategory] = m_DebugLines[iCategory];
		}

		iCharsToWrite -= iWrote;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDebugHistory::DumpDebugHistory( int iCategory )
{
	if ( iCategory < 0 || iCategory >= MAX_HISTORY_CATEGORIES )
	{
		Warning("Attempted to dump a history for category %d. Valid categories are %d to %d.\n", iCategory, 0, (MAX_HISTORY_CATEGORIES-1) );
		return;
	}

	// Find the start of the oldest whole debug line.
	const char *pszLine = m_DebugLineEnd[iCategory] + 1;
	if ( (pszLine - m_DebugLines[iCategory]) >= sizeof(m_DebugLines[iCategory]) )
	{
		pszLine = m_DebugLines[iCategory];
	}

	// Are we at the start of a line? If there's a null terminator before us, then we're good to go.
	while ( (!( pszLine == m_DebugLines[iCategory] && *(m_DebugLines[iCategory]+sizeof(m_DebugLines[iCategory])-1) == '\0' ) &&
			!( pszLine != m_DebugLines[iCategory] && *(pszLine-1) == '\0' )) 
			|| *pszLine == '\0' )
	{
		pszLine++;

		// Have we looped?
		if ( (pszLine - m_DebugLines[iCategory]) >= sizeof(m_DebugLines[iCategory]) )
		{
			pszLine = m_DebugLines[iCategory];
		}

		if ( pszLine == m_DebugLineEnd[iCategory] )
		{
			// We looped through the entire history, and found nothing.
			Msg( "Debug History of Category %d is EMPTY\n", iCategory );
			return;
		}
	}

	// Now print everything up till the end
	char szMsgBuffer[MAX_DEBUG_HISTORY_LINE_LENGTH];
	char *pszMsg = szMsgBuffer;
	Msg( "Starting Debug History Dump of Category %d\n", iCategory );
	while ( pszLine != m_DebugLineEnd[iCategory] )
	{
		*pszMsg = *pszLine;
		if ( *pszLine == '\0' )
		{
			if ( szMsgBuffer[0] != '\0' )
			{
				// Found a full line, so print it

				Msg( "%s", szMsgBuffer );
			}

			// Clear the buffer
			pszMsg = szMsgBuffer;
			*pszMsg = '\0';
		}
		else
		{
			pszMsg++;
		}

		pszLine++;

		// Have we looped?
		if ( (pszLine - m_DebugLines[iCategory]) >= sizeof(m_DebugLines[iCategory]) )
		{
			pszLine = m_DebugLines[iCategory];
		}
	}
	Msg("Ended Debug History Dump of Category %d\n", iCategory );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDebugHistory::ClearHistories( void )
{
	for ( int i = 0; i < MAX_HISTORY_CATEGORIES; i++ )
	{
		std::memset( m_DebugLines[i], 0, sizeof(m_DebugLines[i]) );
		m_DebugLineEnd[i] = m_DebugLines[i];
	}
}


//-----------------------------------------------------------------------------
// Purpose: Singleton debug history.  Created by first usage.
//-----------------------------------------------------------------------------
CDebugHistory *GetDebugHistory()
{
#ifdef DISABLE_DEBUG_HISTORY
	return nullptr;
#endif

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		return nullptr;

	if ( s_DebugHistory == nullptr )
	{
		CBaseEntity *pEnt = gEntList.FindEntityByClassname( nullptr, "env_debughistory" );
		if ( pEnt )
		{
			s_DebugHistory = dynamic_cast<CDebugHistory*>(pEnt);
		}
		else
		{
			s_DebugHistory = ( CDebugHistory * )CreateEntityByName( "env_debughistory" );
			if ( s_DebugHistory )
			{
				s_DebugHistory->Spawn();
			}
		}
	}

	contract_assert( s_DebugHistory );
	return s_DebugHistory;
}

void ClearDebugHistory()
{
	s_DebugHistory = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void AddDebugHistoryLine( int iCategory, const char *pszLine )
{
#ifdef DISABLE_DEBUG_HISTORY
	return;
#else
	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		return;

	if ( !GetDebugHistory() )
	{
		Warning("Failed to find or create an env_debughistory.\n" );
		return;
	}

	GetDebugHistory()->AddDebugHistoryLine( iCategory, pszLine );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CC_DebugHistory_AddLine( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( args.ArgC() < 3 )
	{
		Warning("Incorrect parameters. Format: <category id> <line>\n");
		return;
	}

	int iCategory = std::atoi(args[ 1 ]);
	const char *pszLine = args[ 2 ];
	AddDebugHistoryLine( iCategory, pszLine );
}
static ConCommand dbghist_addline( "dbghist_addline", CC_DebugHistory_AddLine, "Add a line to the debug history. Format: <category id> <line>", FCVAR_NONE );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CC_DebugHistory_Dump( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( args.ArgC() < 2 )
	{
		Warning("Incorrect parameters. Format: <category id>\n");
		return;
	}

	if ( GetDebugHistory() )
	{
		int iCategory = std::atoi(args[ 1 ]);
		GetDebugHistory()->DumpDebugHistory( iCategory );
	}
}

static ConCommand dbghist_dump("dbghist_dump", CC_DebugHistory_Dump, 
							   "Dump the debug history to the console. Format: <category id>\n"
							   "    Categories:\n"
							   "     0: Entity I/O\n"
							   "     1: AI Decisions\n"
							   "     2: Scene Print\n"
							   "     3: Alyx Blind\n"
							   "     4: Log of damage done to player",
							   FCVAR_NONE );
