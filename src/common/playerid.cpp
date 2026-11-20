// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) Valve Corporation

import std;
import ks.core.platform;
import ks.core.types;
import ks.platform;
import ks.simd;
import ks.tier1.playerid;
import ks.tier1.strtools;

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  : pchPlayerID -		text representation of a player ID
//-----------------------------------------------------------------------------
CPlayerId::CPlayerId( const char *pchPlayerID, EUniverse eDefaultUniverse /* = k_EUniverseInvalid */  )
{
	SetFromString( pchPlayerID, eDefaultUniverse );
}


//-----------------------------------------------------------------------------
// Purpose: Initializes a player ID from a string
// Input  : pchPlayerID -		text representation of a player ID
//-----------------------------------------------------------------------------
void CPlayerId::SetFromString( const char *pchPlayerID, EUniverse eDefaultUniverse )
{
	uint nAccountID = 0;
	uint nInstance = 1;
	EUniverse eUniverse = eDefaultUniverse;
	// Since we are using sscanf with %d to read this value
	// we need eUniverse to be the size of an int.
	static_assert( sizeof(eUniverse) == sizeof(int) );
	EAccountType eAccountType = k_EAccountTypeIndividual;
	if ( *pchPlayerID == '[' )
		pchPlayerID++;

	// BUGBUG Rich use the Q_ functions
	if (*pchPlayerID == 'A')
	{
		// This is test only
		pchPlayerID++; // skip the A
		eAccountType = k_EAccountTypeAnonGameServer;
		if (*pchPlayerID == '-' || *pchPlayerID == ':')
			pchPlayerID++; // skip the optional - or :

		if ( std::strchr( pchPlayerID, '(' ) )
			std::sscanf( std::strchr( pchPlayerID, '(' ), "(%u)", &nInstance );
		const char *pchColon = std::strchr( pchPlayerID, ':' );
		if ( pchColon && *pchColon != 0 && std::strchr( pchColon+1, ':' ))
		{
			std::sscanf( pchPlayerID, "%u:%u:%u", (uint*)&eUniverse, &nAccountID, &nInstance );
		}
		else if ( pchColon )
		{
			std::sscanf( pchPlayerID, "%u:%u", (uint*)&eUniverse, &nAccountID );
		}
		else
		{
			std::sscanf( pchPlayerID, "%u", &nAccountID );
		}

		if ( nAccountID == 0 )
		{
			// i dont care what number you entered
			CreateBlankAnonLogon(eUniverse);
		}
		else
		{
			InstancedSet( nAccountID, nInstance, eUniverse, eAccountType );
		}
		return;
	}
	else if (*pchPlayerID == 'G')
	{
		pchPlayerID++; // skip the G
		eAccountType = k_EAccountTypeGameServer;
		if (*pchPlayerID == '-' || *pchPlayerID == ':')
			pchPlayerID++; // skip the optional - or :
	}
	else if (*pchPlayerID == 'C')
	{
		pchPlayerID++; // skip the C
		eAccountType = k_EAccountTypeContentServer;
		if (*pchPlayerID == '-' || *pchPlayerID == ':')
			pchPlayerID++; // skip the optional - or :
	}
	else if (*pchPlayerID == 'g')
	{
		pchPlayerID++; // skip the g
		eAccountType = k_EAccountTypeClan;
		nInstance = 0;
		if (*pchPlayerID == '-' || *pchPlayerID == ':')
			pchPlayerID++; // skip the optional - or :
	}
	else if (*pchPlayerID == 'c')
	{
		pchPlayerID++; // skip the c
		eAccountType = k_EAccountTypeChat;
		nInstance = k_EChatInstanceFlagClan;
		if (*pchPlayerID == '-' || *pchPlayerID == ':')
			pchPlayerID++; // skip the optional - or :
	}
	else if (*pchPlayerID == 'L')
	{
		pchPlayerID++; // skip the c
		eAccountType = k_EAccountTypeChat;
		nInstance = k_EChatInstanceFlagLobby;
		if (*pchPlayerID == '-' || *pchPlayerID == ':')
			pchPlayerID++; // skip the optional - or :
	}
	else if (*pchPlayerID == 'T')
	{
		pchPlayerID++; // skip the T
		eAccountType = k_EAccountTypeChat;
		nInstance = 0;	// Anon chat
		if (*pchPlayerID == '-' || *pchPlayerID == ':')
			pchPlayerID++; // skip the optional - or :
	}
	else if (*pchPlayerID == 'U')
	{
		pchPlayerID++; // skip the U
		eAccountType = k_EAccountTypeIndividual;
		nInstance = 1;
		if (*pchPlayerID == '-' || *pchPlayerID == ':')
			pchPlayerID++; // skip the optional - or :
	}
	else if (*pchPlayerID == 'i')
	{
		pchPlayerID++; // skip the i
		eAccountType = k_EAccountTypeInvalid;
		nInstance = 1;
		if (*pchPlayerID == '-' || *pchPlayerID == ':')
			pchPlayerID++; // skip the optional - or :
	}
	else
	{
		// Check for a 64-bit player ID, unadorned.
		uint64 unPlayerID64 = V_atoui64( pchPlayerID );
		if ( unPlayerID64 > 0xffffffffU )
		{
			SetFromUint64( unPlayerID64 );
			return;
		}
	}

	if ( std::strchr( pchPlayerID, ':' ) )
	{
		if (*pchPlayerID == '[')
			pchPlayerID++; // skip the optional [
		std::sscanf( pchPlayerID, "%u:%u", (uint*)&eUniverse, &nAccountID );
		if ( eUniverse == k_EUniverseInvalid )
			eUniverse = eDefaultUniverse; 
	}
	else
	{
		std::sscanf( pchPlayerID, "%u", &nAccountID );
	}	
	
	contract_assert( (eUniverse > k_EUniverseInvalid) && (eUniverse < k_EUniverseMax) );

	InstancedSet( nAccountID, nInstance, eUniverse, eAccountType );
}


//-----------------------------------------------------------------------------
// Purpose: Renders the player ID to a buffer.  NOTE: for convenience of calling
//			code, this code returns a pointer to a static buffer and is NOT thread-safe.
// Output:  buffer with rendered player ID
//-----------------------------------------------------------------------------
const char * CPlayerId::Render() const
{
	// longest length of returned string is k_cBufLen
	//	[A:%u:%u:%u]
	//	 %u == 10 * 3 + 6 == 36, plus terminator == 37
	const int k_cBufLen = 37;

	const int k_cBufs = 4;	// # of static bufs to use (so people can compose output with multiple calls to Render() )
	static char rgchBuf[k_cBufs][k_cBufLen];
	static int nBuf = 0;
	char * pchBuf = rgchBuf[nBuf];	// get pointer to current static buf
	nBuf ++;	// use next buffer for next call to this method
	nBuf %= k_cBufs;

	if ( k_EAccountTypeAnonGameServer == m_playerid.m_comp.m_EAccountType )
	{
		V_snprintf( pchBuf, k_cBufLen, "[A:%u:%u:%u]", m_playerid.m_comp.m_EUniverse, m_playerid.m_comp.m_unAccountID, m_playerid.m_comp.m_unAccountInstance );
	}
	else if ( k_EAccountTypeGameServer == m_playerid.m_comp.m_EAccountType )
	{
		V_snprintf( pchBuf, k_cBufLen, "[G:%u:%u]", m_playerid.m_comp.m_EUniverse, m_playerid.m_comp.m_unAccountID );
	}
	else if ( k_EAccountTypeMultiseat == m_playerid.m_comp.m_EAccountType )
	{
		V_snprintf( pchBuf, k_cBufLen, "[M:%u:%u:%u]", m_playerid.m_comp.m_EUniverse, m_playerid.m_comp.m_unAccountID, m_playerid.m_comp.m_unAccountInstance );
	} 
	else if ( k_EAccountTypePending == m_playerid.m_comp.m_EAccountType )
	{
		V_snprintf( pchBuf, k_cBufLen, "[P:%u:%u]", m_playerid.m_comp.m_EUniverse, m_playerid.m_comp.m_unAccountID );
	} 
	else if ( k_EAccountTypeContentServer == m_playerid.m_comp.m_EAccountType )
	{
		V_snprintf( pchBuf, k_cBufLen, "[C:%u:%u]", m_playerid.m_comp.m_EUniverse, m_playerid.m_comp.m_unAccountID );
	}
	else if ( k_EAccountTypeClan == m_playerid.m_comp.m_EAccountType )
	{
		// 'g' for "group"
		V_snprintf( pchBuf, k_cBufLen, "[g:%u:%u]", m_playerid.m_comp.m_EUniverse, m_playerid.m_comp.m_unAccountID );
	}
	else if ( k_EAccountTypeChat == m_playerid.m_comp.m_EAccountType )
	{
		if ( m_playerid.m_comp.m_unAccountInstance & k_EChatInstanceFlagClan )
		{
			V_snprintf( pchBuf, k_cBufLen, "[c:%u:%u]", m_playerid.m_comp.m_EUniverse, m_playerid.m_comp.m_unAccountID );
		}
		else if ( m_playerid.m_comp.m_unAccountInstance & k_EChatInstanceFlagLobby )
		{
			V_snprintf( pchBuf, k_cBufLen, "[L:%u:%u]", m_playerid.m_comp.m_EUniverse, m_playerid.m_comp.m_unAccountID );
		}
		else // Anon chat
		{
			V_snprintf( pchBuf, k_cBufLen, "[T:%u:%u]", m_playerid.m_comp.m_EUniverse, m_playerid.m_comp.m_unAccountID );
		}
	}
	else if ( k_EAccountTypeInvalid == m_playerid.m_comp.m_EAccountType )
	{
		V_snprintf( pchBuf, k_cBufLen, "[I:%u:%u]", m_playerid.m_comp.m_EUniverse, m_playerid.m_comp.m_unAccountID );
	}
	else if ( k_EAccountTypeIndividual == m_playerid.m_comp.m_EAccountType )
	{
		if ( m_playerid.m_comp.m_unAccountInstance != k_unUserDesktopInstance )
			V_snprintf( pchBuf, k_cBufLen, "[U:%u:%u:%u]", m_playerid.m_comp.m_EUniverse, m_playerid.m_comp.m_unAccountID, m_playerid.m_comp.m_unAccountInstance );
		else
			V_snprintf( pchBuf, k_cBufLen, "[U:%u:%u]", m_playerid.m_comp.m_EUniverse, m_playerid.m_comp.m_unAccountID );
	}
	else if ( k_EAccountTypeAnonUser == m_playerid.m_comp.m_EAccountType )
	{
		V_snprintf( pchBuf, k_cBufLen, "[a:%u:%u]", m_playerid.m_comp.m_EUniverse, m_playerid.m_comp.m_unAccountID );
	}
	else
	{
		V_snprintf( pchBuf, k_cBufLen, "[i:%u:%u]", m_playerid.m_comp.m_EUniverse, m_playerid.m_comp.m_unAccountID );
	}
	return pchBuf;
}


//-----------------------------------------------------------------------------
// Purpose: Renders the passed-in player ID to a buffer.  NOTE: for convenience of calling
//			code, this code returns a pointer to a static buffer and is NOT thread-safe.
// Input:	64-bit representation of player ID to render
// Output:  buffer with rendered player ID
//-----------------------------------------------------------------------------
const char * CPlayerId::Render( uint64 ulPlayerID )
{
	CPlayerId playerUID( ulPlayerID );
	return playerUID.Render();
}


//-----------------------------------------------------------------------------
// Purpose: some player IDs are for internal use only
// This is really debug code, but we run with asserts on in retail, so ...
//-----------------------------------------------------------------------------
bool CPlayerId::BValidExternalPlayerID() const
{
	if ( m_playerid.m_comp.m_EAccountType == k_EAccountTypePending )
		return false;
	if ( m_playerid.m_comp.m_EAccountType != k_EAccountTypeAnonGameServer && m_playerid.m_comp.m_EAccountType != k_EAccountTypeContentServer && m_playerid.m_comp.m_EAccountType != k_EAccountTypeAnonUser )
	{
		if ( m_playerid.m_comp.m_unAccountID == 0 && m_playerid.m_comp.m_unAccountInstance == 0 )
			return false;
	}
	return true;
}
