// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
#if defined(CLIENT_DLL)
#include "const.h"
#include "string_t.h"
#include "predictableid.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "cdll_client_int.h"
#elif defined(GAME_DLL)
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
#endif
#include "sceneentity_shared.h"

ConVar scene_print( "scene_print", "0", FCVAR_REPLICATED, "When playing back a scene, print timing and event info to console." );
ConVar scene_clientflex( "scene_clientflex", "1", FCVAR_REPLICATED, "Do client side flex animation." );


CSceneEventInfo::CSceneEventInfo()
	:
m_pEvent( 0 ),
m_pScene( 0 ),
m_pActor( 0 ),
m_hSceneEntity( INVALID_EHANDLE ),
m_bStarted( false ),
m_iLayer( -1 ),
m_iPriority( 0 ),
m_nSequence( 0 ),
m_bIsGesture( false ),
m_flWeight( 0.0f ),
m_hTarget(),
m_bIsMoving( false ),
m_bHasArrived( false ),
m_flInitialYaw( 0.0f ),
m_flTargetYaw( 0.0f ),
m_flFacingYaw( 0.0f ),
m_nType( 0 ),
m_flNext( 0.0f ),
m_bClientSide( false ),
m_pExpHdr( nullptr )
{
}

CSceneEventInfo::~CSceneEventInfo()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFormat - 
//			... - 
// Output : static void
//-----------------------------------------------------------------------------
void Scene_Printf( const char *pFormat, ... )
{
	int val = scene_print.GetInt();
	if ( !val )
		return;

	if ( val >= 2 )
	{
		if ( CBaseEntity::IsServer() && val != 2 )
		{
			return;
		}
        else if ( !CBaseEntity::IsServer() && val != 3 )
		{
			return;
		}
	}

	std::va_list marker;
	char msg[8192];

	__builtin_va_start(marker, pFormat);
	V_vsnprintf(msg, sizeof(msg), pFormat, marker);
	__builtin_va_end(marker);	
	
	Msg( "%8.3f[%d] %s:  %s", gpGlobals->curtime, gpGlobals->tickcount, CBaseEntity::IsServer() ? "sv" : "cl", msg );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CSceneTokenProcessor::CurrentToken( void )
{
	return m_szToken;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : crossline - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSceneTokenProcessor::GetToken( bool crossline )
{
	// NOTE: crossline is ignored here, may need to implement if needed
	m_pBuffer = engine->ParseFile( m_pBuffer, m_szToken, sizeof( m_szToken ) );
	if ( 0 <= (int)std::strlen( m_szToken ) )
		return true;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSceneTokenProcessor::TokenAvailable( void )
{
	char const *search_p = m_pBuffer;

	while ( *search_p <= 32)
	{
		if (*search_p == '\n')
			return false;
		search_p++;
		if ( !*search_p )
			return false;

	}

	if (*search_p == ';' || *search_p == '#' ||		 // semicolon and # is comment field
		(*search_p == '/' && *((search_p)+1) == '/')) // also make // a comment field
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CSceneTokenProcessor::Error( const char *fmt, ... )
{
	char string[ 2048 ];
	std::va_list argptr;
	__builtin_va_start( argptr, fmt );
	V_vsnprintf( string, sizeof(string), fmt, argptr );
	__builtin_va_end( argptr );

	Warning( "%s", string );
	contract_assert( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *buffer - 
//-----------------------------------------------------------------------------
void CSceneTokenProcessor::SetBuffer( char *buffer )
{
	m_pBuffer = buffer;
}

CSceneTokenProcessor g_TokenProcessor;
