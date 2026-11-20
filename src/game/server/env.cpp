// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2010 Valve Corporation

module;
#include <string.h>
#include <stdlib.h>
#include "eiface.h"
#include "networkvar.h"
#include "reflect.h"
#include "responserules/response_types.h"
#include "server_class.h"
#include "studio.h"
#include "vphysics_interface.h"
#include "baseentity.h"
#include "entity_dispatch.h"
#include "entity_dispatch_list.h"
#include "entity_kinds_dump.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "shareddefs.h"
#include "util.h"
#include "declare_class.h"
#include "predictable_entity.h"
#include "vscript/ivscript.h"
#include "vscript_game.h"
#include "vscript_shared.h"
#include "igameevents.h"
#include "enginecallback.h"
#include "reflect_annotations.h"
#include "string_t.h"
#include "renderparm.h"
#include "engine/IEngineTrace.h"
#include "shake.h"
#include "usermessages_schema.h"
module ks.server;

import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import :fish;
import :vscript;
import :sharedvar;
import :baseentity;
import :player;
import :entity_dispatch;
import :entity_kind;
import :entityutil;
import :entity_2;
import ks.game.shareddefs;
import :cstrike15.bot.bot;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import :nav_4;
import std;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.types;
import ks.math.mathlib;
import ks.tier1.convar;
import ks.tier1.createinterface;
import ks.tier1.registry;
import ks.tier1.strtools;
import ks.tier1.iconvar;
import ks.core.threadtools;
import ks.core.vprof;
import ks.tier1.keyvalues;
import ks.game.ai_activity;
import ks.math.ssemath;
import ks.tier1.vector_ops;
import ks.game.triggers_shared;
import ks.core.random;
import ks.game.beam_flags;
import ks.core.numeric;

struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Ambient light controller entity.

inline float AMBIENT_LIGHT_ENT_THINK_RATE() { return TICK_INTERVAL; }

LINK_ENTITY_TO_CLASS(env_ambient_light, CEnvAmbientLight);

IMPLEMENT_REFLECT_DATAMAP( CEnvAmbientLight )

IMPLEMENT_REFLECT_SERVERCLASS( CEnvAmbientLight, DT_EnvAmbientLight )

void CEnvAmbientLight::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();

	m_vecColor.SetX( static_cast<float>( m_Color.r ) / 255.0f );
	m_vecColor.SetY( static_cast<float>( m_Color.g ) / 255.0f );
	m_vecColor.SetZ( static_cast<float>( m_Color.b ) / 255.0f );
}

void CEnvAmbientLight::InputSetColor(inputdata_t &inputdata)
{
	m_Color = inputdata.value.Color32();
	m_vecColor.SetX( static_cast<float>( m_Color.r ) / 255.0f );
	m_vecColor.SetY( static_cast<float>( m_Color.g ) / 255.0f );
	m_vecColor.SetZ( static_cast<float>( m_Color.b ) / 255.0f );
}

void CEnvAmbientLight::SetColor( const Vector &vecColor )
{
	m_vecColor = vecColor;
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: global dynamic light with cascaded shadow mapping

//#define CsmDbgMsg Msg
#define CsmDbgMsg(x)

ConVar cl_csm_auto_entity( side_registry(), "cl_csm_auto_entity", "1", 0, "" );

CCascadeLight *g_pCascadeLight;

LINK_ENTITY_TO_CLASS(env_cascade_light, CCascadeLight);

IMPLEMENT_REFLECT_DATAMAP( CCascadeLight )

IMPLEMENT_REFLECT_SERVERCLASS( CCascadeLight, DT_CascadeLight )

float CCascadeLight::m_flEnvLightShadowPitch;
QAngle CCascadeLight::m_EnvLightShadowAngles;
bool CCascadeLight::m_bEnvLightShadowValid;
color32 CCascadeLight::m_EnvLightColor;
int CCascadeLight::m_EnvLightColorScale;

CCascadeLight::CCascadeLight() : 
	CBaseEntity()
{
	CsmDbgMsg( "CCascadeLight::CCascadeLight\n" );

	m_bEnabled = true;

	color32 tmp = { 255, 255, 255, 1 };
	m_LightColor = tmp;

	m_LightColorScale = 255;
		
	QAngle angles;
	angles.Init( 50, 43, 0 );
	Vector vForward;
	AngleVectors( angles, &vForward );
	m_shadowDirection = vForward;
	//m_shadowDirection.Init( 0.0f, 0.0f, -1.0f );
		
	m_envLightShadowDirection = m_shadowDirection;
	m_bUseLightEnvAngles = true;
	
	m_flMaxShadowDist = 400.0f;

	g_pCascadeLight = this;
}

CCascadeLight::~CCascadeLight()
{
	g_pCascadeLight = nullptr;

	CsmDbgMsg( "CCascadeLight::~CCascadeLight\n" );
}

//------------------------------------------------------------------------------
// Purpose : Send even though we don't have a model
//------------------------------------------------------------------------------
int CCascadeLight::UpdateTransmitState_Impl()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}

bool CCascadeLight::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "color" ) )
	{
		/* unused?
		float tmp[4];
		UTIL_StringToFloatArray( tmp, 4, szValue );

		m_LightColor.SetR( tmp[0] );
		m_LightColor.SetG( tmp[1] );
		m_LightColor.SetB( tmp[2] );
		m_LightColor.SetA( tmp[3] );*/
	}
	else if ( FStrEq( szKeyName, "angles" ) )
	{
		QAngle angles;
		UTIL_StringToVector( angles.Base(), szValue );
		if (angles == vec3_angle)
		{
			angles.Init( 50, 43, 0 );
		}
		Vector vForward;
		AngleVectors( angles, &vForward );
		m_shadowDirection = vForward;
		return true;
	}
	else if ( FStrEq( szKeyName, "uselightenvangles" ) )
	{
		m_bUseLightEnvAngles = ( std::atoi( szValue ) != 0 );
		return true;
	}
	else if ( FStrEq( szKeyName, "maxshadowdistance" ) )
	{
		m_flMaxShadowDist = std::atof( szValue );
		return true;
	}
	
	return BaseClass::KeyValue_Impl( szKeyName, szValue );
}

bool CCascadeLight::GetKeyValue_Impl( const char *szKeyName, char *szValue, int iMaxLen )
{
	if ( FStrEq( szKeyName, "color" ) )
	{
		// path unused?
		V_snprintf( szValue, iMaxLen, "%d %d %d %d", m_LightColor.GetR(), m_LightColor.GetG(), m_LightColor.GetB(), m_LightColor.GetA() );
		return true;
	}
	return BaseClass::GetKeyValue_Impl( szKeyName, szValue, iMaxLen );
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CCascadeLight::Spawn_Impl( void )
{
	Precache();
	SetSolid( SOLID_NONE );

	if( m_bStartDisabled )
	{
		m_bEnabled = false;
	}
	else
	{
		m_bEnabled = true;
	}

	if ( m_bEnvLightShadowValid )
	{
		UpdateEnvLight();
	}

	//SetClassname( "cascadelight" );

	BaseClass::Spawn_Impl();
}

void CCascadeLight::Release( void )
{
	g_pCascadeLight = nullptr;
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CCascadeLight::OnActivate()
{
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CCascadeLight::OnDeactivate()
{
}

//------------------------------------------------------------------------------
// Input values
//------------------------------------------------------------------------------
void CCascadeLight::InputSetAngles( inputdata_t &inputdata )
{
	const char *pAngles = inputdata.value.String();

	QAngle angles;
	UTIL_StringToVector( angles.Base(), pAngles );

	Vector vTemp;
	AngleVectors( angles, &vTemp );
	m_shadowDirection = vTemp;
}

//------------------------------------------------------------------------------
// Purpose : Input handlers
//------------------------------------------------------------------------------
void CCascadeLight::InputEnable( inputdata_t &inputdata )
{
	m_bEnabled = true;
	if ( g_pCascadeLight )
	{
		g_pCascadeLight->UpdateEnvLight();
	}
}

void CCascadeLight::InputDisable( inputdata_t &inputdata )
{
	m_bEnabled = false;
	if ( g_pCascadeLight )
	{
		g_pCascadeLight->UpdateEnvLight();
	}
}

void CCascadeLight::InputSetLightColor( inputdata_t &inputdata )
{
	m_LightColor = inputdata.value.Color32();
}

void CCascadeLight::InputSetLightColorScale( inputdata_t &inputdata )
{
	m_LightColorScale = inputdata.value.Int();
	if ( g_pCascadeLight )
	{
		g_pCascadeLight->UpdateEnvLight();
	}
}

void CCascadeLight::SetLightColor( int r, int g, int b, int a )
{
	m_EnvLightColor.r = r;
	m_EnvLightColor.g = g;
	m_EnvLightColor.b = b;

	m_EnvLightColor.a = 0; // use light scale as potentially > 255

	m_EnvLightColorScale = a;

	if ( g_pCascadeLight )
	{
		g_pCascadeLight->UpdateEnvLight();
	}
}

void CCascadeLight::SetEnabled( bool bEnable )
{
	m_bEnabled = bEnable;
}

void CCascadeLight::UpdateEnvLight()
{
	QAngle angles;
	angles.x = -m_flEnvLightShadowPitch;
	angles.y = m_EnvLightShadowAngles.y;
	angles.z = 0;
		
	Vector vForward;
	AngleVectors( angles, &vForward );
	m_envLightShadowDirection = vForward;

	m_LightColor		= m_EnvLightColor;
	m_LightColorScale	= m_EnvLightColorScale;
}

void CCascadeLight::SetEnvLightShadowPitch( float flPitch ) 
{ 
	m_flEnvLightShadowPitch = flPitch; 
	m_bEnvLightShadowValid = true;

	if ( g_pCascadeLight )
	{
		g_pCascadeLight->UpdateEnvLight();
	}
}

void CCascadeLight::SetEnvLightShadowAngles( const QAngle &angles ) 
{ 
	m_EnvLightShadowAngles = angles; 
	m_bEnvLightShadowValid = true;

	if ( g_pCascadeLight )
	{
		g_pCascadeLight->UpdateEnvLight();
	}
}

class CCSMLightManager : public CAutoGameSystemPerFrame
{
public:
	CCSMLightManager()
	{
	}
	
	virtual ~CCSMLightManager()
	{
	}

	virtual void LevelInitPreEntity()
	{
		CsmDbgMsg( "**** LevelInitPreEntity\n" );
	}

	virtual void LevelInitPostEntity()
	{
		CsmDbgMsg( "**** LevelInitPostEntity\n" );

		if ( !cl_csm_auto_entity.GetBool() )
			return;
		if ( g_pCascadeLight ) 
			return;
				
		// Create the env_cascade_light automatically for cs:go - this is a hack that will hopefully go away as we add the entity to all of our maps.
		CBaseEntity *entity = dynamic_cast< CBaseEntity * >( CreateEntityByName( "env_cascade_light" ) );
		if (entity)
		{
			entity->Precache();
			entity->KeyValue( "targetname", "cascadelight" );
			DispatchSpawn(entity);
		}
	}

	virtual void LevelShutdownPreEntity()
	{
		CsmDbgMsg( "**** LevelShutdownPreEntity\n" );
	}
	
	virtual void LevelShutdownPostEntity()
	{
		CsmDbgMsg( "**** LevelShutdownPostEntity\n" );
	}

	virtual void Shutdown()
	{
		CsmDbgMsg( "**** Shutdown\n" );
	}
};

CCSMLightManager g_CSMLightManager;

void C_CSM_Server_Status( const CCommand& args )
{
	Msg( "Entity exists: %u\n", g_pCascadeLight != nullptr );
}

static ConCommand cl_csm_server_status(side_registry(), "cl_csm_server_status", C_CSM_Server_Status, "Usage:\n cl_csm_server_status\n", 0);

#undef CsmDbgMsg

class CBaseEntity;

using namespace ::ks::reflect;

// Number of characters worth of debug to use per history category
static constexpr auto DEBUG_HISTORY_VERSION = 6;
static constexpr auto DEBUG_HISTORY_FIRST_VERSIONED = 5;
static constexpr auto MAX_DEBUG_HISTORY_LINE_LENGTH = 256;
static constexpr auto MAX_DEBUG_HISTORY_LENGTH = (1000 * MAX_DEBUG_HISTORY_LINE_LENGTH);

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
static ConCommand dbghist_addline( side_registry(), "dbghist_addline", CC_DebugHistory_AddLine, "Add a line to the debug history. Format: <category id> <line>", FCVAR_NONE );

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

static ConCommand dbghist_dump(side_registry(), "dbghist_dump", CC_DebugHistory_Dump, 
							   "Dump the debug history to the console. Format: <category id>\n"
							   "    Categories:\n"
							   "     0: Entity I/O\n"
							   "     1: AI Decisions\n"
							   "     2: Scene Print\n"
							   "     3: Alyx Blind\n"
							   "     4: Log of damage done to player",
							   FCVAR_NONE );

using namespace ::ks::reflect;

IMPLEMENT_NETWORKCLASS_ALIASED( EnvDetailController, DT_DetailController )
LINK_ENTITY_TO_CLASS_ALIASED(env_detail_controller,	EnvDetailController);

IMPLEMENT_REFLECT_TABLE( CEnvDetailController, DT_DetailController );

static CEnvDetailController *s_detailController = nullptr;
CEnvDetailController * GetDetailController()
{
	return s_detailController;
}

CEnvDetailController::CEnvDetailController()
{
	s_detailController = this;
}

CEnvDetailController::~CEnvDetailController()
{
	if ( s_detailController == this )
	{
		s_detailController = nullptr;
	}
}

//--------------------------------------------------------------------------------------------------------------
int CEnvDetailController::UpdateTransmitState_Impl()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}

	bool CEnvDetailController::KeyValue_Impl( const char *szKeyName, const char *szValue )
	{
		if (FStrEq(szKeyName, "fademindist"))
		{
			m_flFadeStartDist = std::atof(szValue);
		}
		else if (FStrEq(szKeyName, "fademaxdist"))
		{
			m_flFadeEndDist = std::atof(szValue);
		}

		return true;
	}

class CBasePlayer;
struct inputdata_t;

using namespace ::ks::reflect;

//#include "convar.h"

LINK_ENTITY_TO_CLASS( env_dof_controller, CEnvDOFController );

IMPLEMENT_REFLECT_DATAMAP( CEnvDOFController )

IMPLEMENT_REFLECT_SERVERCLASS( CEnvDOFController, DT_EnvDOFController )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvDOFController::Spawn_Impl( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvDOFController::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	// Find our target entity and hold on to it
	m_hFocusTarget = gEntList.FindEntityByName( nullptr, m_strFocusTargetName );

	// Update if we have a focal target
	if ( m_hFocusTarget )
	{
		SetThink( &CEnvDOFController::UpdateParamBlend );
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CEnvDOFController::UpdateTransmitState_Impl()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvDOFController::InputSetNearBlurDepth( inputdata_t &inputdata )
{
	m_flNearBlurDepth = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvDOFController::InputSetNearFocusDepth( inputdata_t &inputdata )
{
	m_flNearFocusDepth = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvDOFController::InputSetFarFocusDepth( inputdata_t &inputdata )
{
	m_flFarFocusDepth = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvDOFController::InputSetFarBlurDepth( inputdata_t &inputdata )
{
	m_flFarBlurDepth = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvDOFController::InputSetNearBlurRadius( inputdata_t &inputdata )
{
	m_flNearBlurRadius = inputdata.value.Float();
	m_bDOFEnabled = ( m_flNearBlurRadius > 0.0f ) || ( m_flFarBlurRadius > 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvDOFController::InputSetFarBlurRadius( inputdata_t &inputdata )
{
	m_flFarBlurRadius = inputdata.value.Float();
	m_bDOFEnabled = ( m_flNearBlurRadius > 0.0f ) || ( m_flFarBlurRadius > 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvDOFController::SetControllerState( DOFControlSettings_t setting )
{
	m_flNearBlurDepth = setting.flNearBlurDepth;
	m_flNearBlurRadius = setting.flNearBlurRadius;
	m_flNearFocusDepth = setting.flNearFocusDistance;
	
	m_flFarBlurDepth = setting.flFarBlurDepth;
	m_flFarBlurRadius = setting.flFarBlurRadius;
	m_flFarFocusDepth = setting.flFarFocusDistance;

	m_bDOFEnabled = ( m_flNearBlurRadius > 0.0f ) || ( m_flFarBlurRadius > 0.0f );
}

static const float BLUR_DEPTH = 500.0f;

//-----------------------------------------------------------------------------
// Purpose: Blend the parameters to the specified value
//-----------------------------------------------------------------------------
void CEnvDOFController::UpdateParamBlend( void )
{ 
	// Update our focal target if we have one
	if ( m_hFocusTarget )
	{
		CBasePlayer *pPlayer = UTIL_GetSinglePlayer();
		float flDistToFocus = ( m_hFocusTarget->GetAbsOrigin() - pPlayer->GetAbsOrigin() ).Length();
		m_flFarFocusDepth.GetForModify() = flDistToFocus + m_flFocusTargetRange;
		m_flFarBlurDepth.GetForModify() = m_flFarFocusDepth + BLUR_DEPTH;
	}

	SetThink( &CEnvDOFController::UpdateParamBlend );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: Set the "focus" target entity
//-----------------------------------------------------------------------------
void CEnvDOFController::InputSetFocusTarget( inputdata_t &inputdata )
{
	m_hFocusTarget = gEntList.FindEntityByName( nullptr, inputdata.value.String() );
	
	// Update if we have a focal target
	if ( m_hFocusTarget )
	{
		SetThink( &CEnvDOFController::UpdateParamBlend );
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the range behind the focus entity that we'll blur (in units)
//-----------------------------------------------------------------------------
void CEnvDOFController::InputSetFocusTargetRange( inputdata_t &inputdata )
{
	m_flFocusTargetRange = inputdata.value.Float();
}

class CBaseAnimating;
class CSprite;
struct animevent_t;
struct inputdata_t;

using namespace ::ks::reflect;

enum EffectType
{
	EFFECT_TYPE_TRAIL = 1,
	EFFECT_TYPE_SPRITE
};

bool			g_bUnget = false;
unsigned char	*buffer;
char			name[ 256 ];
const char		*currenttoken;
int				tokencount;
char			token[ 1204 ];

class CEffectScriptElement
{
public:

	CEffectScriptElement();

	char m_szEffectName[128];
	CHandle<CSpriteTrail> m_pTrail;
	CHandle<CSprite> m_pSprite;
	int m_iType;
	int m_iRenderType;

	int m_iR;
	int m_iG;
	int m_iB;
	int m_iA;

	char m_szAttachment[128];
	char m_szMaterial[128];

	float m_flScale;
	float m_flFadeTime;
	float m_flTextureRes;

	bool  m_bStopFollowOnKill;

	bool IsActive( void ) { return m_bActive; }
	void Activate( void ) { m_bActive = true; }
	void Deactivate( void ) { m_bActive = false; }
private:

	bool m_bActive;
};

CEffectScriptElement::CEffectScriptElement()
{
	m_pTrail = nullptr;
	m_pSprite = nullptr;
	m_iType = 0;

	Deactivate();
	m_iRenderType = kRenderTransAdd;

	m_iR = 255;
	m_iG = 0;
	m_iB = 0;
	m_iA = 255;

	m_flScale = 1.0f;
	m_flFadeTime = 1.0f;
	m_flTextureRes = -1.0f;
	m_bStopFollowOnKill = false;
}

//-----------------------------------------------------------------------------
// An entity which emits other entities at points 
//-----------------------------------------------------------------------------
class CEnvEffectsScript : public CBaseAnimating
{
public:
	DECLARE_CLASS( CEnvEffectsScript, CBaseAnimating );
	DECLARE_DATADESC();

	void Precache_Impl();
	void Spawn_Impl();
	int  UpdateTransmitState_Impl();

	[[= ks::reflect::Input{ .name = "SetSequence", .type = FIELD_STRING } ]] void InputSetSequence( inputdata_t &inputdata );
	void ParseScriptFile( void );
	void LoadFromBuffer( const char *scriptfile, const char *buffer );

	void Think_Impl( void );

	void ParseNewEffect( void );

	const char *GetScriptFile( void ) 
	{
		return STRING( m_iszScriptName );
	}

	void HandleAnimEvent ( animevent_t *pEvent );
	void TrailEffectEvent( CEffectScriptElement *pEffect );
	void SpriteEffectEvent( CEffectScriptElement *pEffect );

	CEffectScriptElement *GetScriptElementByName( const char *pName );

private:
	
	[[= ks::reflect::Key{ .name = "scriptfile" } ]] string_t m_iszScriptName;
		
	std::vector< CEffectScriptElement > m_ScriptElements;

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
	bool IsRootCommand( void )
	{
		if ( !V_stricmp( token, "effect" ) )
			return true;

		return false;
	}
};

inline bool ParseToken( void )
{
	if ( g_bUnget )
	{
		g_bUnget = false;
		return true;
	}

	currenttoken = engine->ParseFile( currenttoken, token, sizeof( token ) );
	tokencount++;
	return currenttoken != nullptr ? true : false;
}

inline void Unget()
{
	g_bUnget = true;
}

inline bool TokenWaiting( void )
{
	
	const char *p = currenttoken;
	while ( *p && *p!='\n')
	{
		// Special handler for // comment blocks
		if ( *p == '/' && *(p+1) == '/' )
			return false;

		if ( !V_isspace( *p ) || V_isalnum( *p ) )
			return true;

		p++;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CEnvEffectsScript )

LINK_ENTITY_TO_CLASS( env_effectscript, CEnvEffectsScript );

//-----------------------------------------------------------------------------
// Should we transmit it to the client?
//-----------------------------------------------------------------------------
int CEnvEffectsScript::UpdateTransmitState_Impl()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Precache
//-----------------------------------------------------------------------------
void CEnvEffectsScript::Precache_Impl()
{
	BaseClass::Precache_Impl();
	PrecacheModel( STRING( GetModelName() ) );

	if ( m_iszScriptName != NULL_STRING )
		 ParseScriptFile();
	else
		 Warning( "CEnvEffectsScript with no script!\n" );
}

//-----------------------------------------------------------------------------
// Spawn
//-----------------------------------------------------------------------------
void CEnvEffectsScript::Spawn_Impl()
{
	Precache();
	BaseClass::Spawn_Impl();

	// We need a model for its animation sequences even though we don't render it
	SetModel( STRING( GetModelName() ) );

	AddEffects( EF_NODRAW );

	SetThink( &CEnvEffectsScript::Think );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CEnvEffectsScript::Think_Impl( void )
{
	StudioFrameAdvance();
	DispatchAnimEvents( this );

	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CEnvEffectsScript::TrailEffectEvent( CEffectScriptElement *pEffect )
{
	if ( pEffect->IsActive() == false )
	{
		//Only one type of this effect active at a time.
		if ( pEffect->m_pTrail == nullptr )
		{
			pEffect->m_pTrail = CSpriteTrail::SpriteTrailCreate( pEffect->m_szMaterial, GetAbsOrigin(), true );
			pEffect->m_pTrail->FollowEntity( this );
			pEffect->m_pTrail->SetTransparency( pEffect->m_iRenderType, pEffect->m_iR, pEffect->m_iG, pEffect->m_iB, pEffect->m_iA, kRenderFxNone );
			pEffect->m_pTrail->SetStartWidth( pEffect->m_flScale );
			if ( pEffect->m_flTextureRes < 0.0f )
			{
				pEffect->m_pTrail->SetTextureResolution( 1.0f / ( 16.0f * pEffect->m_flScale ) );
			}
			else
			{
				pEffect->m_pTrail->SetTextureResolution( pEffect->m_flTextureRes );
			}
			pEffect->m_pTrail->SetLifeTime( pEffect->m_flFadeTime );
			pEffect->m_pTrail->TurnOn();
			pEffect->m_pTrail->SetAttachment( this, LookupAttachment( pEffect->m_szAttachment ) );

			pEffect->Activate();
		}
	}
}

void CEnvEffectsScript::SpriteEffectEvent( CEffectScriptElement *pEffect )
{
	if ( pEffect->IsActive() == false )
	{
		//Only one type of this effect active at a time.
		if ( pEffect->m_pSprite == nullptr )
		{
			pEffect->m_pSprite = CSprite::SpriteCreate( pEffect->m_szMaterial, GetAbsOrigin(), true );
			pEffect->m_pSprite->FollowEntity( this );
			pEffect->m_pSprite->SetTransparency( pEffect->m_iRenderType, pEffect->m_iR, pEffect->m_iG, pEffect->m_iB, pEffect->m_iA, kRenderFxNone );
			pEffect->m_pSprite->SetScale( pEffect->m_flScale );
			pEffect->m_pSprite->TurnOn();
			pEffect->m_pSprite->SetAttachment( this, LookupAttachment( pEffect->m_szAttachment ) );

			pEffect->Activate();
		}
	}
}

void CEnvEffectsScript::HandleAnimEvent ( animevent_t *pEvent ) 
{
	int nEvent = pEvent->Event();

	if ( nEvent == AE_START_SCRIPTED_EFFECT )
	{
		CEffectScriptElement *pCurrent = GetScriptElementByName( pEvent->options );

		if ( pCurrent )
		{
			if ( pCurrent->m_iType == EFFECT_TYPE_TRAIL )
				 TrailEffectEvent( pCurrent );
			else if ( pCurrent->m_iType == EFFECT_TYPE_SPRITE )
				 SpriteEffectEvent( pCurrent );
		}

		return;
	}

	if ( nEvent == AE_STOP_SCRIPTED_EFFECT )
	{
		CEffectScriptElement *pCurrent = GetScriptElementByName( pEvent->options );

		if ( pCurrent && pCurrent->IsActive() )
		{
			pCurrent->Deactivate();

			if ( pCurrent->m_iType == EFFECT_TYPE_TRAIL )
			{
				if ( pCurrent->m_bStopFollowOnKill == true )
				{
					Vector vOrigin;
					GetAttachment( pCurrent->m_pTrail->m_nAttachment, vOrigin );

					pCurrent->m_pTrail->StopFollowingEntity();

					pCurrent->m_pTrail->m_hAttachedToEntity = nullptr;
					pCurrent->m_pTrail->m_nAttachment = 0;

					pCurrent->m_pTrail->SetAbsOrigin( vOrigin);
				}

				pCurrent->m_pTrail->FadeAndDie( pCurrent->m_flFadeTime );
				pCurrent->m_pTrail = nullptr;
			}

			else if ( pCurrent->m_iType == EFFECT_TYPE_SPRITE )
			{
				if ( pCurrent->m_bStopFollowOnKill == true )
				{
					Vector vOrigin;
					GetAttachment( pCurrent->m_pSprite->m_nAttachment, vOrigin );

					pCurrent->m_pSprite->StopFollowingEntity();

					pCurrent->m_pSprite->m_hAttachedToEntity = nullptr;
					pCurrent->m_pSprite->m_nAttachment = 0;

					pCurrent->m_pSprite->SetAbsOrigin( vOrigin);
				}

				pCurrent->m_pSprite->FadeAndDie( pCurrent->m_flFadeTime );
				pCurrent->m_pSprite = nullptr;
			}
		}
		return;
	}

	BaseClass::HandleAnimEvent( pEvent );
}
//-----------------------------------------------------------------------------
// Purpose: Input that sets the sequence of the entity
//-----------------------------------------------------------------------------
void CEnvEffectsScript::InputSetSequence( inputdata_t &inputdata )
{
	if ( inputdata.value.StringID() != NULL_STRING )
	{
		int nSequence = LookupSequence( STRING( inputdata.value.StringID() ) );
		if ( nSequence != ACT_INVALID )
		{
			SetSequence( nSequence );
			ResetSequenceInfo();
			SetCycle( 0.0f );
			m_flPlaybackRate = 1.0f;
		}
	}
}

void CEnvEffectsScript::ParseScriptFile( void )
{
	int length = 0;
	m_ScriptElements.clear();
	const char *pScriptName = GetScriptFile();

	//Reset everything.
	g_bUnget = false;
	currenttoken = nullptr;
	tokencount = 0;
	std::memset( token, 0, 1204 );
	memset( name, 0, 256 );

	unsigned char *buffer = (unsigned char *)UTIL_LoadFileForMe( pScriptName, &length );
	if ( length <= 0 || !buffer )
	{
		DevMsg( 1, "CEnvEffectsScript:  failed to load %s\n", pScriptName );
		return;
	}

	currenttoken = (const char *)buffer;
	LoadFromBuffer( pScriptName, (const char *)buffer );

	UTIL_FreeFile( buffer );
}

void CEnvEffectsScript::LoadFromBuffer( const char *scriptfile, const char *buffer )
{
	while ( 1 )
	{
		ParseToken();
		
		if ( !token[0] )
		{
			break;
		}

		if ( !V_stricmp( token, "effect" ) )
		{
			ParseNewEffect();
		}
		else
		{
			Warning( "CEnvEffectsScript: Unknown entry type '%s'\n", token );
			break;
		}
	}
}

void CEnvEffectsScript::ParseNewEffect( void )
{
	//Add a new effect to the list.
	CEffectScriptElement NewElement;
	
	// Effect Group Name
	ParseToken();
	V_strncpy( NewElement.m_szEffectName, token, sizeof( NewElement.m_szEffectName ) );

	while ( 1 )
	{
		ParseToken();

		// Oops, part of next definition
		if( IsRootCommand() )
		{
			Unget();
			break;
		}

		if ( !V_stricmp( token, "{" ) )
		{
			while ( 1 )
			{
				ParseToken();
				if ( !V_stricmp( token, "}" ) )
					break;

				if ( !V_stricmp( token, "type" ) )
				{
					ParseToken();

					if ( !V_stricmp( token, "trail" ) )
						NewElement.m_iType = EFFECT_TYPE_TRAIL;
					else if ( !V_stricmp( token, "sprite" ) )
						NewElement.m_iType = EFFECT_TYPE_SPRITE;

					continue;
				}

				if ( !V_stricmp( token, "material" ) )
				{
					ParseToken();
					V_strncpy( NewElement.m_szMaterial, token, sizeof( NewElement.m_szMaterial ) );
					PrecacheModel( NewElement.m_szMaterial );

					continue;
				}

				if ( !V_stricmp( token, "attachment" ) )
				{
					ParseToken();
					V_strncpy( NewElement.m_szAttachment, token, sizeof( NewElement.m_szAttachment ) );

					continue;
				}

				if ( !V_stricmp( token, "color" ) )
				{
					ParseToken();
					std::sscanf( token, "%i %i %i %i", &NewElement.m_iR, &NewElement.m_iG, &NewElement.m_iB, &NewElement.m_iA );

					continue;
				}

				if ( !V_stricmp( token, "scale" ) )
				{
					ParseToken();

					NewElement.m_flScale = std::atof( token );
					continue;
				}

				if ( !V_stricmp( token, "texturescale" ) )
				{
					ParseToken();

					float flTextureScale = std::atof( token );
					NewElement.m_flTextureRes = (flTextureScale > 0.0f) ? 1.0f / flTextureScale : 0.0f;
					continue;
				}

				if ( !V_stricmp( token, "fadetime" ) )
				{
					ParseToken();

					NewElement.m_flFadeTime = std::atof( token );
					continue;
				}

				if ( !V_stricmp( token, "stopfollowonkill" ) )
				{
					ParseToken();

					NewElement.m_bStopFollowOnKill = !!std::atoi( token );
					continue;
				}

			}
			break;
		}
	}

	m_ScriptElements.push_back( NewElement );
}

CEffectScriptElement *CEnvEffectsScript::GetScriptElementByName( const char *pName )
{
	for ( int i = 0; i < std::ssize( m_ScriptElements ); i++ )
	{
		CEffectScriptElement *pCurrent = &m_ScriptElements[ i ];

		if ( pCurrent && !V_stricmp( pCurrent->m_szEffectName, pName ) ) 
		{
			return pCurrent;
		}
	}

	return nullptr;
}

class CBaseEntity;
class CBasePlayer;
class CPointTemplate;
struct inputdata_t;

using namespace ::ks::reflect;

static constexpr auto SF_ENTMAKER_AUTOSPAWN = 0x0001;
static constexpr auto SF_ENTMAKER_WAITFORDESTRUCTION = 0x0002;
static constexpr auto SF_ENTMAKER_IGNOREFACING = 0x0004;
static constexpr auto SF_ENTMAKER_CHECK_FOR_SPACE = 0x0008;
static constexpr auto SF_ENTMAKER_CHECK_PLAYER_LOOKING = 0x0010;

//extern ScriptClassDesc_t * GetScriptDesc( CBaseEntity * );

//-----------------------------------------------------------------------------
// Purpose: An entity that mapmakers can use to ensure there's a required entity never runs out.
//			i.e. physics cannisters that need to be used.
//-----------------------------------------------------------------------------
class CEnvEntityMaker : public CPointEntity
{
	DECLARE_CLASS( CEnvEntityMaker, CPointEntity );
public:
	DECLARE_DATADESC();
	DECLARE_ENT_SCRIPTDESC();

	void Spawn_Impl( void );
	void Activate_Impl( void );

	void		 SpawnEntity( Vector vecAlternateOrigin = vec3_invalid, QAngle vecAlternateAngles = vec3_angle );
	void		 CheckSpawnThink( void );
	[[= ks::reflect::Input{ .name = "ForceSpawn", .type = FIELD_VOID } ]] void		 InputForceSpawn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ForceSpawnAtEntityOrigin", .type = FIELD_STRING } ]] void		 InputForceSpawnAtEntityOrigin( inputdata_t &inputdata );

	void		 SpawnEntityFromScript();
	void		 SpawnEntityAtEntityOriginFromScript( HSCRIPT hEntity );
	void		 SpawnEntityAtNamedEntityOriginFromScript( const char *pszName );
	void		 SpawnEntityAtLocationFromScript( const Vector &vecAlternateOrigin, const Vector &vecAlternateAngles );

private:

	CPointTemplate *FindTemplate();

	bool HasRoomToSpawn();
	bool IsPlayerLooking();

	Vector			m_vecEntityMins;
	Vector			m_vecEntityMaxs;
	EHANDLE			m_hCurrentInstance;
	EHANDLE			m_hCurrentBlocker;	// Last entity that blocked us spawning something
	Vector			m_vecBlockerOrigin;

	// Movement after spawn
	[[= ks::reflect::Key{ .name = "PostSpawnDirection" } ]] QAngle			m_angPostSpawnDirection;
	[[= ks::reflect::Key{ .name = "PostSpawnDirectionVariance" } ]] float			m_flPostSpawnDirectionVariance;
	[[= ks::reflect::Key{ .name = "PostSpawnSpeed" } ]] float			m_flPostSpawnSpeed;
	[[= ks::reflect::Key{ .name = "PostSpawnInheritAngles" } ]] bool			m_bPostSpawnUseAngles;

	[[= ks::reflect::Key{ .name = "EntityTemplate" } ]] string_t		m_iszTemplate;

	[[= ks::reflect::Key{ .name = "OnEntitySpawned" } ]] COutputEvent	m_pOutputOnSpawned;
	[[= ks::reflect::Key{ .name = "OnEntityFailedSpawn" } ]] COutputEvent	m_pOutputOnFailedSpawn;
};

IMPLEMENT_REFLECT_DATAMAP( CEnvEntityMaker )

BEGIN_ENT_SCRIPTDESC( CEnvEntityMaker, CBaseEntity, "env_entity_maker" )
	DEFINE_SCRIPTFUNC_NAMED( SpawnEntityFromScript, "SpawnEntity", "Create an entity at the location of the maker" )
	DEFINE_SCRIPTFUNC_NAMED( SpawnEntityAtEntityOriginFromScript, "SpawnEntityAtEntityOrigin", "Create an entity at the location of a specified entity instance" )
	DEFINE_SCRIPTFUNC_NAMED( SpawnEntityAtNamedEntityOriginFromScript, "SpawnEntityAtNamedEntityOrigin", "Create an entity at the location of a named entity" )
	DEFINE_SCRIPTFUNC_NAMED( SpawnEntityAtLocationFromScript, "SpawnEntityAtLocation", "Create an entity at a specified location and orientaton, orientation is Euler angle in degrees (pitch, yaw, roll)" )
END_SCRIPTDESC()

LINK_ENTITY_TO_CLASS( env_entity_maker, CEnvEntityMaker );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvEntityMaker::Spawn_Impl( void )
{
	m_vecEntityMins = vec3_origin;
	m_vecEntityMaxs = vec3_origin;
	m_hCurrentInstance = nullptr;
	m_hCurrentBlocker = nullptr;
	m_vecBlockerOrigin = vec3_origin;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvEntityMaker::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	// check for valid template
	if ( m_iszTemplate == NULL_STRING )
	{
		Warning( "env_entity_maker %s has no template entity!\n", GetEntityName().ToCStr() );
		UTIL_Remove( this );
		return;
	}

	// Spawn an instance
	if ( m_spawnflags & SF_ENTMAKER_AUTOSPAWN )
	{
		SpawnEntity();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPointTemplate *CEnvEntityMaker::FindTemplate()
{
	// Find our point_template
	CPointTemplate *pTemplate = dynamic_cast<CPointTemplate *>(gEntList.FindEntityByName( nullptr, STRING(m_iszTemplate) ));
	if ( !pTemplate )
	{
		Warning( "env_entity_maker %s failed to find template %s.\n", GetEntityName().ToCStr(), STRING(m_iszTemplate) );
	}

	return pTemplate;
}

//-----------------------------------------------------------------------------
// Purpose: Spawn an instance of the entity
//-----------------------------------------------------------------------------
void CEnvEntityMaker::SpawnEntity( Vector vecAlternateOrigin, QAngle vecAlternateAngles )
{
	CPointTemplate *pTemplate = FindTemplate();
	if (!pTemplate)
		return;

	// Spawn our template
	Vector vecSpawnOrigin = GetAbsOrigin();
	QAngle vecSpawnAngles = GetAbsAngles();

	if( vecAlternateOrigin != vec3_invalid )
	{
		// We have a valid alternate origin and angles. Use those instead
		// of spawning the items at my own origin and angles.
		vecSpawnOrigin = vecAlternateOrigin;
		vecSpawnAngles = vecAlternateAngles;
	}

	std::vector< CBaseEntity* > hNewEntities;
	if ( !pTemplate->CreateInstance( vecSpawnOrigin, vecSpawnAngles, &hNewEntities, this ) )
		return;
	
	//Adrian: oops we couldn't spawn the entity (or entities) for some reason!
	if ( hNewEntities.empty() )
		 return;
	
	m_hCurrentInstance = hNewEntities[0];

	// Assume it'll block us
	m_hCurrentBlocker = m_hCurrentInstance;
	m_vecBlockerOrigin = m_hCurrentBlocker->GetAbsOrigin();

	// Store off the mins & maxs the first time we spawn
	if ( m_vecEntityMins == vec3_origin )
	{
		m_hCurrentInstance->CollisionProp()->WorldSpaceAABB( &m_vecEntityMins, &m_vecEntityMaxs );
		m_vecEntityMins -= m_hCurrentInstance->GetAbsOrigin();
		m_vecEntityMaxs -= m_hCurrentInstance->GetAbsOrigin();
	}

	// Fire our output
	m_pOutputOnSpawned.FireOutput( this, this );

	// Start thinking
	if ( m_spawnflags & SF_ENTMAKER_AUTOSPAWN )
	{
		SetThink( &CEnvEntityMaker::CheckSpawnThink );
		SetNextThink( gpGlobals->curtime + 0.5f );
	}

	// If we have a specified post spawn speed, apply it to all spawned entities
	if ( m_flPostSpawnSpeed )
	{
		for ( int i = 0; i < std::ssize( hNewEntities ); i++ )
		{
			CBaseEntity *pEntity = hNewEntities[i];
			if ( pEntity->GetMoveType() == MOVETYPE_NONE )
				continue;

			// Calculate a velocity for this entity
			Vector vForward,vRight,vUp;
			QAngle angSpawnDir( m_angPostSpawnDirection );
			if ( m_bPostSpawnUseAngles )
			{
				if ( GetParent() )
				{
					angSpawnDir += GetParent()->GetAbsAngles();
				}
				else
				{
					angSpawnDir += GetAbsAngles();
				}
			}
			AngleVectors( angSpawnDir, &vForward, &vRight, &vUp );
			Vector vecShootDir = vForward;
			vecShootDir += vRight * random_valve->RandomFloat(-1, 1) * m_flPostSpawnDirectionVariance;
			vecShootDir += vForward * random_valve->RandomFloat(-1, 1) * m_flPostSpawnDirectionVariance;
			vecShootDir += vUp * random_valve->RandomFloat(-1, 1) * m_flPostSpawnDirectionVariance;
			VectorNormalize( vecShootDir );
			vecShootDir *= m_flPostSpawnSpeed;

			// Apply it to the entity
			IPhysicsObject *pPhysicsObject = pEntity->VPhysicsGetObject();
			if ( pPhysicsObject )
			{
				pPhysicsObject->AddVelocity(&vecShootDir, nullptr);
			}
			else
			{
				pEntity->SetAbsVelocity( vecShootDir );
			}
		}
	}

	pTemplate->CreationComplete( hNewEntities );
}

//-----------------------------------------------------------------------------
// Purpose: Spawn an instance of the entity
//-----------------------------------------------------------------------------
void CEnvEntityMaker::SpawnEntityFromScript()
{
	SpawnEntity();
}

//-----------------------------------------------------------------------------
// Purpose: Spawn an instance of the entity
//-----------------------------------------------------------------------------
void CEnvEntityMaker::SpawnEntityAtEntityOriginFromScript( HSCRIPT hEntity )
{
	CBaseEntity *pTargetEntity = ToEnt( hEntity );
	if ( pTargetEntity )
	{
		SpawnEntity( pTargetEntity->GetAbsOrigin(), pTargetEntity->GetAbsAngles() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Spawn an instance of the entity
//-----------------------------------------------------------------------------
void CEnvEntityMaker::SpawnEntityAtNamedEntityOriginFromScript( const char *pszName )
{
	CBaseEntity *pTargetEntity = gEntList.FindEntityByName( nullptr, pszName, this, nullptr, nullptr );

	if( pTargetEntity )
	{
		SpawnEntity( pTargetEntity->GetAbsOrigin(), pTargetEntity->GetAbsAngles() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Spawn an instance of the entity
//-----------------------------------------------------------------------------
void CEnvEntityMaker::SpawnEntityAtLocationFromScript( const Vector &vecAlternateOrigin, const Vector &vecAlternateAngles )
{
	SpawnEntity( vecAlternateOrigin, *((QAngle *)&vecAlternateAngles) );
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not the template entities can fit if spawned.
// Input  : pBlocker - Returns blocker unless NULL.
//-----------------------------------------------------------------------------
bool CEnvEntityMaker::HasRoomToSpawn()
{
	// Do we have a blocker from last time?
	if ( m_hCurrentBlocker )
	{
		// If it hasn't moved, abort immediately
		if ( m_vecBlockerOrigin == m_hCurrentBlocker->GetAbsOrigin() )
		{
			return false;
		}
	}

	// Check to see if there's enough room to spawn
	trace_t tr;
	UTIL_TraceHull( GetAbsOrigin(), GetAbsOrigin(), m_vecEntityMins, m_vecEntityMaxs, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
	if ( tr.Ent<CBaseEntity>() || tr.startsolid )
	{
		// Store off our blocker to check later
		m_hCurrentBlocker = tr.Ent<CBaseEntity>();
		if ( m_hCurrentBlocker )
		{
			m_vecBlockerOrigin = m_hCurrentBlocker->GetAbsOrigin();
		}

		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the player is looking towards us.
//-----------------------------------------------------------------------------
bool CEnvEntityMaker::IsPlayerLooking()
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer )
		{
			// Only spawn if the player's looking away from me
			Vector vLookDir = pPlayer->EyeDirection3D();
			Vector vTargetDir = GetAbsOrigin() - pPlayer->EyePosition();
			VectorNormalize( vTargetDir );

			float fDotPr = DotProduct( vLookDir,vTargetDir );
			if ( fDotPr > 0 )
				return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if we should spawn another instance
//-----------------------------------------------------------------------------
void CEnvEntityMaker::CheckSpawnThink( void )
{
	SetNextThink( gpGlobals->curtime + 0.5f );

	// Do we have an instance?
	if ( m_hCurrentInstance )
	{
		// If Wait-For-Destruction is set, abort immediately
		if ( m_spawnflags & SF_ENTMAKER_WAITFORDESTRUCTION )
			return;
	}

	// Check to see if there's enough room to spawn
	if ( !HasRoomToSpawn() )
		return;

	// We're clear, now check to see if the player's looking
	if ( !( HasSpawnFlags( SF_ENTMAKER_IGNOREFACING ) ) && IsPlayerLooking() )
		return;

	// Clear, no player watching, so spawn!
	SpawnEntity();
}

//-----------------------------------------------------------------------------
// Purpose: Spawns the entities, checking for space if flagged to do so.
//-----------------------------------------------------------------------------
void CEnvEntityMaker::InputForceSpawn( inputdata_t &inputdata )
{
	CPointTemplate *pTemplate = FindTemplate();
	if (!pTemplate)
		return;

	if ( HasSpawnFlags( SF_ENTMAKER_CHECK_FOR_SPACE ) && !HasRoomToSpawn() )
	{
		m_pOutputOnFailedSpawn.FireOutput( this, this );
		return;
	}

	if ( HasSpawnFlags( SF_ENTMAKER_CHECK_PLAYER_LOOKING ) && IsPlayerLooking() )
	{
		m_pOutputOnFailedSpawn.FireOutput( this, this );
		return;
	}

	SpawnEntity();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvEntityMaker::InputForceSpawnAtEntityOrigin( inputdata_t &inputdata )
{
	CBaseEntity *pTargetEntity = gEntList.FindEntityByName( nullptr, inputdata.value.String(), this, inputdata.pActivator, inputdata.pCaller );
		
	if( pTargetEntity )
	{
		SpawnEntity( pTargetEntity->GetAbsOrigin(), pTargetEntity->GetAbsAngles() );
	}
}

class CBaseEntity;
class CBasePlayer;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: An entity for creating instructor hints entirely with map logic

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEnvInstructorHint : public CPointEntity
{
public:
	DECLARE_CLASS( CEnvInstructorHint, CPointEntity );
	DECLARE_DATADESC();

private:
	[[= ks::reflect::Input{ .name = "ShowHint", .type = FIELD_STRING } ]] void InputShowHint( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EndHint", .type = FIELD_VOID } ]] void InputEndHint( inputdata_t &inputdata );
	
	[[= ks::reflect::Key{ .name = "hint_replace_key" } ]] string_t	m_iszReplace_Key;
	[[= ks::reflect::Key{ .name = "hint_target" } ]] string_t	m_iszHintTargetEntity;
	[[= ks::reflect::Key{ .name = "hint_timeout" } ]] int			m_iTimeout;
	[[= ks::reflect::Key{ .name = "hint_icon_onscreen" } ]] string_t	m_iszIcon_Onscreen;
	[[= ks::reflect::Key{ .name = "hint_icon_offscreen" } ]] string_t	m_iszIcon_Offscreen;
	[[= ks::reflect::Key{ .name = "hint_caption" } ]] string_t	m_iszCaption;
	[[= ks::reflect::Key{ .name = "hint_activator_caption" } ]] string_t	m_iszActivatorCaption;
	[[= ks::reflect::Key{ .name = "hint_color" } ]] color32		m_Color;
	[[= ks::reflect::Key{ .name = "hint_icon_offset" } ]] float		m_fIconOffset;
	[[= ks::reflect::Key{ .name = "hint_range" } ]] float		m_fRange;
	[[= ks::reflect::Key{ .name = "hint_pulseoption" } ]] uint8		m_iPulseOption;
	[[= ks::reflect::Key{ .name = "hint_alphaoption" } ]] uint8		m_iAlphaOption;
	[[= ks::reflect::Key{ .name = "hint_shakeoption" } ]] uint8		m_iShakeOption;
	[[= ks::reflect::Key{ .name = "hint_static" } ]] bool		m_bStatic;
	[[= ks::reflect::Key{ .name = "hint_nooffscreen" } ]] bool		m_bNoOffscreen;
	[[= ks::reflect::Key{ .name = "hint_forcecaption" } ]] bool		m_bForceCaption;
	[[= ks::reflect::Key{ .name = "hint_binding" } ]] string_t	m_iszBinding;
	[[= ks::reflect::Key{ .name = "hint_gamepad_binding" } ]] string_t	m_iszGamepadBinding;
	[[= ks::reflect::Key{ .name = "hint_allow_nodraw_target" } ]] bool		m_bAllowNoDrawTarget;
	[[= ks::reflect::Key{ .name = "hint_local_player_only" } ]] bool		m_bLocalPlayerOnly;
};

LINK_ENTITY_TO_CLASS( env_instructor_hint, CEnvInstructorHint );

IMPLEMENT_REFLECT_DATAMAP( CEnvInstructorHint )

#define LOCATOR_ICON_FX_PULSE_SLOW		0x00000001
#define LOCATOR_ICON_FX_ALPHA_SLOW		0x00000008
#define LOCATOR_ICON_FX_SHAKE_NARROW	0x00000040
#define LOCATOR_ICON_FX_STATIC			0x00000100	// This icon draws at a fixed location on the HUD.

//-----------------------------------------------------------------------------
// Purpose: Input handler for showing the message and/or playing the sound.
//-----------------------------------------------------------------------------
void CEnvInstructorHint::InputShowHint( inputdata_t &inputdata )
{
	static int s_InstructorServerHintEventCreate = 0;
	IGameEvent * event = gameeventmanager->CreateEvent( "instructor_server_hint_create", false, &s_InstructorServerHintEventCreate );
	if ( event )
	{
		CBaseEntity *pTargetEntity = gEntList.FindEntityByName( nullptr, m_iszHintTargetEntity );
		if( pTargetEntity == nullptr )
			pTargetEntity = inputdata.pActivator;

		if( pTargetEntity == nullptr )
			pTargetEntity = GetWorldEntity();

		char szColorString[128];
		V_snprintf( szColorString, sizeof( szColorString ), "%.3d,%.3d,%.3d", m_Color.r, m_Color.g, m_Color.b );

		int iFlags = 0;
		
		iFlags |= (m_iPulseOption == 0) ? 0 : (LOCATOR_ICON_FX_PULSE_SLOW << (m_iPulseOption - 1));
		iFlags |= (m_iAlphaOption == 0) ? 0 : (LOCATOR_ICON_FX_ALPHA_SLOW << (m_iAlphaOption - 1));
		iFlags |= (m_iShakeOption == 0) ? 0 : (LOCATOR_ICON_FX_SHAKE_NARROW << (m_iShakeOption - 1));
		iFlags |= m_bStatic ? LOCATOR_ICON_FX_STATIC : 0;

		CBasePlayer *pActivator = nullptr;
		bool bFilterByActivator = m_bLocalPlayerOnly;
		if ( bFilterByActivator )
			pActivator = entity_cast<CBasePlayer>( inputdata.pActivator );

		if ( inputdata.value.StringID() != NULL_STRING )
		{
			CBaseEntity *pTarget = gEntList.FindEntityByName( nullptr, inputdata.value.String() );
			pActivator = entity_cast<CBasePlayer>( pTarget );
			if ( pActivator )
			{
				bFilterByActivator = true;
			}
		}
		else
		{
			if ( GameRules()->IsMultiplayer() == false )
			{
				pActivator = UTIL_GetLocalPlayer(); 
			}
			else
			{
				if ( !pTargetEntity )
				{
					Warning( "Failed to play server side instructor hint: no player specified for hint\n" );
					contract_assert( 0 );
				}
			}
		}

		const char *pActivatorCaption = m_iszActivatorCaption.ToCStr();
		if ( !pActivatorCaption || pActivatorCaption[ 0 ] == '\0' )
		{
			pActivatorCaption = m_iszCaption.ToCStr();
		}

		event->SetString( "hint_name", GetEntityName().ToCStr() );
		event->SetString( "hint_replace_key", m_iszReplace_Key.ToCStr() );
		event->SetInt( "hint_target", pTargetEntity->entindex() );
		event->SetInt( "hint_activator_userid", ( pActivator ? pActivator->GetUserID() : 0 ) );
		event->SetInt( "hint_timeout", m_iTimeout );
		event->SetString( "hint_icon_onscreen", m_iszIcon_Onscreen.ToCStr() );
		event->SetString( "hint_icon_offscreen", m_iszIcon_Offscreen.ToCStr() );
		event->SetString( "hint_caption", m_iszCaption.ToCStr() );
		event->SetString( "hint_activator_caption", pActivatorCaption );
		event->SetString( "hint_color", szColorString );
		event->SetFloat( "hint_icon_offset", m_fIconOffset );
		event->SetFloat( "hint_range", m_fRange );
		event->SetInt( "hint_flags", iFlags );
		event->SetString( "hint_binding", m_iszBinding.ToCStr() );
		event->SetString( "hint_gamepad_binding", m_iszGamepadBinding.ToCStr() );
		event->SetBool( "hint_allow_nodraw_target", m_bAllowNoDrawTarget );
		event->SetBool( "hint_nooffscreen", m_bNoOffscreen );
		event->SetBool( "hint_forcecaption", m_bForceCaption );
		event->SetBool( "hint_local_player_only", bFilterByActivator );

		gameeventmanager->FireEvent( event );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvInstructorHint::InputEndHint( inputdata_t &inputdata )
{
	static int s_InstructorServerHintEventStop = 0;
	IGameEvent * event = gameeventmanager->CreateEvent( "instructor_server_hint_stop", false, &s_InstructorServerHintEventStop );
	if ( event )
	{
		event->SetString( "hint_name", GetEntityName().ToCStr() );

		gameeventmanager->FireEvent( event );
	}
}

//-----------------------------------------------------------------------------
// Purpose: A generic target entity that gets replicated to the client for instructor hint targetting
//-----------------------------------------------------------------------------
class CInfoInstructorHintTarget : public CPointEntity
{
public:
	DECLARE_CLASS( CInfoInstructorHintTarget, CPointEntity );

	int UpdateTransmitState_Impl( void )	// set transmit filter to transmit always
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

};

LINK_ENTITY_TO_CLASS( info_target_instructor_hint, CInfoInstructorHintTarget );

#undef LOCATOR_ICON_FX_PULSE_SLOW
#undef LOCATOR_ICON_FX_ALPHA_SLOW
#undef LOCATOR_ICON_FX_SHAKE_NARROW
#undef LOCATOR_ICON_FX_STATIC

class CBaseAnimating;
class CSkyCamera;
struct inputdata_t;

using namespace ::ks::reflect;

// HACK HACK:  Must match cl_dll/cl_animevent.h!!!!
static constexpr auto CL_EVENT_SPRITEGROUP_CREATE = 6002;

//-----------------------------------------------------------------------------
// An entity which emits other entities at points 
//-----------------------------------------------------------------------------
class CEnvParticleScript : public CBaseAnimating
{
	[[= ks::reflect::NetTable{ .name = "DT_EnvParticleScript" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CEnvParticleScript, CBaseAnimating );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CEnvParticleScript();

	void Precache_Impl();
	void Spawn_Impl();
	void Activate_Impl();
	int  UpdateTransmitState_Impl();

	[[= ks::reflect::Input{ .name = "SetSequence", .type = FIELD_STRING } ]] void InputSetSequence( inputdata_t &inputdata );

private:

	void	PrecacheAnimationEventMaterials();

	CNetworkVar( float, m_flSequenceScale, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
};

//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CEnvParticleScript )

LINK_ENTITY_TO_CLASS( env_particlescript, CEnvParticleScript );

//-----------------------------------------------------------------------------
// Datatable
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_SERVERCLASS( CEnvParticleScript, DT_EnvParticleScript )

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CEnvParticleScript::CEnvParticleScript()
{
	UseClientSideAnimation();
}

void CEnvParticleScript::PrecacheAnimationEventMaterials()
{
	CStudioHdr *hdr = GetModelPtr();
	if ( hdr )
	{
		int numseq = hdr->GetNumSeq();
		for ( int i = 0; i < numseq; ++i )
		{
			mstudioseqdesc_t& seqdesc = hdr->pSeqdesc( i );
			int ecount = seqdesc.numevents;
			for ( int j = 0 ; j < ecount; ++j )
			{
				const mstudioevent_for_client_server_t* event = (const mstudioevent_for_client_server_t*)seqdesc.pEvent( j );
				if ( event->Event() == CL_EVENT_SPRITEGROUP_CREATE )
				{
					char pAttachmentName[256];
					char pSpriteName[256];
					int nArgs = std::sscanf( event->pszOptions(), "%255s %255s", pAttachmentName, pSpriteName );
					if ( nArgs == 2 )
					{
						PrecacheMaterial( pSpriteName );
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Precache
//-----------------------------------------------------------------------------
void CEnvParticleScript::Precache_Impl()
{
	BaseClass::Precache_Impl();
	PrecacheModel( STRING( GetModelName() ) );
	
	// We need a model for its animation sequences even though we don't render it
	SetModel( STRING( GetModelName() ) );

	PrecacheAnimationEventMaterials();
}

//-----------------------------------------------------------------------------
// Spawn
//-----------------------------------------------------------------------------
void CEnvParticleScript::Spawn_Impl()
{
	Precache();
	BaseClass::Spawn_Impl();
	AddEffects( EF_NOSHADOW );
	// We need a model for its animation sequences even though we don't render it
	SetModel( STRING( GetModelName() ) );
}

//-----------------------------------------------------------------------------
// Activate
//-----------------------------------------------------------------------------
void CEnvParticleScript::Activate_Impl()
{
	BaseClass::Activate_Impl();

	DetectInSkybox();
	CSkyCamera *pCamera = GetEntitySkybox();
	if ( pCamera )
	{
		float flSkyboxScale = pCamera->m_skyboxData.scale;
		if ( flSkyboxScale == 0.0f )
		{
			flSkyboxScale = 1.0f;
		}

		m_flSequenceScale = flSkyboxScale;
	}
	else
	{
		m_flSequenceScale = 1.0f;
	}

	m_flPlaybackRate = 1.0f;
}

//-----------------------------------------------------------------------------
// Should we transmit it to the client?
//-----------------------------------------------------------------------------
int CEnvParticleScript::UpdateTransmitState_Impl()
{
	if ( IsEffectActive( EF_NODRAW ) )
	{
		return SetTransmitState( FL_EDICT_DONTSEND );
	}

	if ( IsEFlagSet( EFL_IN_SKYBOX ) )
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	return SetTransmitState( FL_EDICT_PVSCHECK );
}

//-----------------------------------------------------------------------------
// Purpose: Input that sets the sequence of the entity
//-----------------------------------------------------------------------------
void CEnvParticleScript::InputSetSequence( inputdata_t &inputdata )
{
	if ( inputdata.value.StringID() != NULL_STRING )
	{
		int nSequence = LookupSequence( STRING( inputdata.value.StringID() ) );
		if ( nSequence != ACT_INVALID )
		{
			SetSequence( nSequence );
		}
	}
}

class CBasePlayer;
struct inputdata_t;

using namespace ::ks::reflect;

LINK_ENTITY_TO_CLASS( env_player_surface_trigger, CEnvPlayerSurfaceTrigger );

IMPLEMENT_REFLECT_DATAMAP( CEnvPlayerSurfaceTrigger )

// Global list of surface triggers
std::vector< CHandle<CEnvPlayerSurfaceTrigger> >	g_PlayerSurfaceTriggers;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEnvPlayerSurfaceTrigger::~CEnvPlayerSurfaceTrigger( void )
{
	ks::erase_first( g_PlayerSurfaceTriggers, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvPlayerSurfaceTrigger::Spawn_Impl( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );

	m_iCurrentGameMaterial = 0;
	m_bDisabled = false;

	g_PlayerSurfaceTriggers.push_back( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvPlayerSurfaceTrigger::OnRestore_Impl( void )
{
	BaseClass::OnRestore_Impl();

	g_PlayerSurfaceTriggers.push_back( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvPlayerSurfaceTrigger::SetPlayerSurface( CBasePlayer *pPlayer, char gameMaterial )
{
	// Ignore players in the air (stops bunny hoppers escaping triggers)
	if ( gameMaterial == 0 )
		return;

	// Loop through the surface triggers and tell them all about the change
	int iCount = std::ssize( g_PlayerSurfaceTriggers );
	for ( int i = 0; i < iCount; i++ )
	{
		g_PlayerSurfaceTriggers[i]->PlayerSurfaceChanged( pPlayer, gameMaterial );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvPlayerSurfaceTrigger::PlayerSurfaceChanged( CBasePlayer *pPlayer, char gameMaterial )
{
	if ( m_bDisabled )
		return;

	// Fire the output if we've changed, but only if it involves the target material
	if ( gameMaterial != (char)m_iCurrentGameMaterial &&
	     ( gameMaterial == m_iTargetGameMaterial || m_iCurrentGameMaterial == m_iTargetGameMaterial ) )
	{
		DevMsg( 2, "Player changed material to %d (was %d)\n", gameMaterial, m_iCurrentGameMaterial );

		m_iCurrentGameMaterial = (int)gameMaterial;

		SetThink( &CEnvPlayerSurfaceTrigger::UpdateMaterialThink );
		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Think function to fire outputs. Done this way so that sv_alternate ticks
//			doesn't allow multiple surface changes in the same tick to fire outputs.
//-----------------------------------------------------------------------------
void CEnvPlayerSurfaceTrigger::UpdateMaterialThink( void )
{
	if ( m_iCurrentGameMaterial == m_iTargetGameMaterial )
	{
		m_OnSurfaceChangedToTarget.FireOutput( nullptr, this );
	}
	else 
	{
		m_OnSurfaceChangedFromTarget.FireOutput( nullptr, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvPlayerSurfaceTrigger::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvPlayerSurfaceTrigger::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
}

class CBaseEntity;
class CBasePlayer;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Projects a texture into the world (like the flashlight)

LINK_ENTITY_TO_CLASS( env_projectedtexture, CEnvProjectedTexture );

IMPLEMENT_REFLECT_DATAMAP( CEnvProjectedTexture )

IMPLEMENT_REFLECT_SERVERCLASS( CEnvProjectedTexture, DT_EnvProjectedTexture )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEnvProjectedTexture::CEnvProjectedTexture( void )
{
	m_bState = true;
	m_bAlwaysUpdate = false;
	m_flLightFOV = 45.0f;
	m_bEnableShadows = false;
	m_bSimpleProjection = false;
	m_bLightOnlyTarget = false;
	m_bLightWorld = true;
	m_bCameraSpace = false;

	V_strcpy( m_SpotlightTextureName.GetForModify(), "effects/flashlight_border" );
	V_strcpy( m_SpotlightTextureName.GetForModify(), "effects/flashlight001" );

	m_nSpotlightTextureFrame = 0;
	m_flBrightnessScale = 1.0f;
	m_LightColor.Init( 255, 255, 255, 255 );
	m_flColorTransitionTime = 0.5f;
	m_flAmbient = 0.0f;
	m_flNearZ = 4.0f;
	m_flFarZ = 750.0f;
	m_nShadowQuality = 0;
	m_flProjectionSize = 500.0f;
	m_flRotation = 0.0f;
}

void UTIL_ColorStringToLinearFloatColor( Vector &color, const char *pString )
{
	float tmp[4];
	UTIL_StringToFloatArray( tmp, 4, pString );
	if( tmp[3] <= 0.0f )
	{
		tmp[3] = 255.0f;
	}
	tmp[3] *= ( 1.0f / 255.0f );
	color.x = tmp[0] * ( 1.0f / 255.0f ) * tmp[3];
	color.y = tmp[1] * ( 1.0f / 255.0f ) * tmp[3];
	color.z = tmp[2] * ( 1.0f / 255.0f ) * tmp[3];
}

bool CEnvProjectedTexture::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "lightcolor" ) )
	{
		float tmp[4];
		UTIL_StringToFloatArray( tmp, 4, szValue );

		m_LightColor.SetR( tmp[0] );
		m_LightColor.SetG( tmp[1] );
		m_LightColor.SetB( tmp[2] );
		m_LightColor.SetA( tmp[3] );
	}
	else if ( FStrEq( szKeyName, "texturename" ) )
	{
		V_strcpy( m_SpotlightTextureName.GetForModify(), szValue );
	}
	else
	{
		return BaseClass::KeyValue_Impl( szKeyName, szValue );
	}

	return true;
}

bool CEnvProjectedTexture::GetKeyValue_Impl( const char *szKeyName, char *szValue, int iMaxLen )
{
	if ( FStrEq( szKeyName, "lightcolor" ) )
	{
		V_snprintf( szValue, iMaxLen, "%d %d %d %d", m_LightColor.GetR(), m_LightColor.GetG(), m_LightColor.GetB(), m_LightColor.GetA() );
		return true;
	}
	else if ( FStrEq( szKeyName, "texturename" ) )
	{
		V_snprintf( szValue, iMaxLen, "%s", m_SpotlightTextureName.Get() );
		return true;
	}
	return BaseClass::GetKeyValue_Impl( szKeyName, szValue, iMaxLen );
}

void CEnvProjectedTexture::InputTurnOn( inputdata_t &inputdata )
{
	// Force all other projected textures off
	EnforceSingleProjectionRules();

	m_bState = true;
}

void CEnvProjectedTexture::InputTurnOff( inputdata_t &inputdata )
{
	m_bState = false;
}

void CEnvProjectedTexture::InputAlwaysUpdateOn( inputdata_t &inputdata )
{
	m_bAlwaysUpdate = true;
}

void CEnvProjectedTexture::InputAlwaysUpdateOff( inputdata_t &inputdata )
{
	m_bAlwaysUpdate = false;
}

void CEnvProjectedTexture::InputSetFOV( inputdata_t &inputdata )
{
	m_flLightFOV = inputdata.value.Float();
}

void CEnvProjectedTexture::InputSetTarget( inputdata_t &inputdata )
{
	m_hTargetEntity = inputdata.value.Entity();
}

void CEnvProjectedTexture::InputSetCameraSpace( inputdata_t &inputdata )
{
	m_bCameraSpace = inputdata.value.Bool();
}

void CEnvProjectedTexture::InputSetLightOnlyTarget( inputdata_t &inputdata )
{
	m_bLightOnlyTarget = inputdata.value.Bool();
}

void CEnvProjectedTexture::InputSetLightWorld( inputdata_t &inputdata )
{
	m_bLightWorld = inputdata.value.Bool();
}

void CEnvProjectedTexture::InputSetEnableShadows( inputdata_t &inputdata )
{
	m_bEnableShadows = inputdata.value.Bool();
}

void CEnvProjectedTexture::InputSetLightColor( inputdata_t &inputdata )
{
	m_LightColor = inputdata.value.Color32();
}

void CEnvProjectedTexture::InputSetAmbient( inputdata_t &inputdata )
{
	m_flAmbient = inputdata.value.Float();
}

void CEnvProjectedTexture::InputSetLightStyle( inputdata_t &inputdata )
{
	m_iStyle = inputdata.value.Int();
}

void CEnvProjectedTexture::InputSetPattern( inputdata_t &inputdata )
{
	m_iszPattern = inputdata.value.StringID();
	engine->LightStyle( m_iStyle, (char *) STRING( m_iszPattern ) );
}

void CEnvProjectedTexture::InputSetNearZ( inputdata_t &inputdata )
{
	m_flNearZ = inputdata.value.Float();
}

void CEnvProjectedTexture::InputSetFarZ( inputdata_t &inputdata )
{
	m_flFarZ = inputdata.value.Float();
}

void CEnvProjectedTexture::InputSetSpotlightTexture( inputdata_t &inputdata )
{
	contract_assert( 0 );
	Warning( "SetSpotlightTexture is disabled. If you need this feature reimplemented, tell a programmer.\n" );
	//V_strcpy( m_SpotlightTextureName.GetForModify(), inputdata.value.String() );
}

void CEnvProjectedTexture::Spawn_Impl( void )
{
	m_bState = ( ( GetSpawnFlags() & ENV_PROJECTEDTEXTURE_STARTON ) != 0 );
	m_bAlwaysUpdate = ( ( GetSpawnFlags() & ENV_PROJECTEDTEXTURE_ALWAYSUPDATE ) != 0 );

	// Update light styles
	if ( m_iStyle >= 32 )
	{
		if ( m_iszPattern == NULL_STRING && m_iDefaultStyle > 0 )
		{
			m_iszPattern = MAKE_STRING( GetDefaultLightstyleString( m_iDefaultStyle ) );
		}

		if ( m_bState == false )
			engine->LightStyle( m_iStyle, "a" );
		else if ( m_iszPattern != NULL_STRING )
			engine->LightStyle( m_iStyle, (char *) STRING( m_iszPattern ) );
		else
			engine->LightStyle( m_iStyle, "m" );
	}

	BaseClass::Spawn_Impl();
}

void CEnvProjectedTexture::EnforceSingleProjectionRules( bool bWarnOnEnforcement )
{
	// Once a light is turned on, turn off all other possible lights in the level
	CBaseEntity *pFlashlight = nullptr;
	while ( ( pFlashlight = gEntList.FindEntityByClassname( pFlashlight, "env_projectedtexture" ) ) != nullptr )
	{
		// Obviously, don't turn yourself off
		if ( pFlashlight == this )
			continue;

		if ( bWarnOnEnforcement )
		{
			CEnvProjectedTexture *pProjTex = static_cast<CEnvProjectedTexture *>(pFlashlight);
			if ( pProjTex && pProjTex->m_bState )
			{
				Warning( "Warning: env_projected_texture (%s) forced off by (%s)\n", pProjTex->GetEntityNameAsCStr(), GetEntityNameAsCStr() );
			}
		}

		variant_t emptyVariant;
		pFlashlight->AcceptInput( "TurnOff", this, this, emptyVariant, 0 );
	}
}

void CEnvProjectedTexture::Activate_Impl( void )
{
	SetThink( &CEnvProjectedTexture::InitialThink );
	SetNextThink( gpGlobals->curtime + 0.1f );

	BaseClass::Activate_Impl();

	if ( m_bState )
	{
		// Make sure that we stomp any other active projected texture off when we activate
		EnforceSingleProjectionRules( true );
	}
}

void CEnvProjectedTexture::InitialThink( void )
{
	m_hTargetEntity = gEntList.FindEntityByName( nullptr, m_target );
}

int CEnvProjectedTexture::UpdateTransmitState_Impl()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

// Console command for creating env_projectedtexture entities
void CC_CreateFlashlight( const CCommand &args )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if( !pPlayer )
		return;

	QAngle angles = pPlayer->EyeAngles();
	Vector origin = pPlayer->EyePosition();		

	CEnvProjectedTexture *pFlashlight = dynamic_cast< CEnvProjectedTexture * >( CreateEntityByName("env_projectedtexture") );
	if( args.ArgC() > 1 )
	{
		pFlashlight->SetName( AllocPooledString( args[1] ) );
	}

	pFlashlight->Teleport( &origin, &angles, nullptr );

}
static ConCommand create_flashlight(side_registry(), "create_flashlight", CC_CreateFlashlight, 0, FCVAR_CHEAT);

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Entity to control screen overlays on a player

// named by the Proxy<> annotation below, so it must be declared before the class.
void SendProxy_String_tToString( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEnvScreenOverlay : public CPointEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_EnvScreenOverlay" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CEnvScreenOverlay, CPointEntity );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CEnvScreenOverlay();

	// Always transmit to clients
	int UpdateTransmitState_Impl();
	void Spawn_Impl( void );
	void Precache_Impl( void );

	[[= ks::reflect::Input{ .name = "StartOverlays", .type = FIELD_VOID } ]] void	InputStartOverlay( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "StopOverlays", .type = FIELD_VOID } ]] void	InputStopOverlay( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SwitchOverlay", .type = FIELD_INTEGER } ]] void	InputSwitchOverlay( inputdata_t &inputdata );

	void	SetActive( bool bActive ) { m_bIsActive = bActive; }
	
protected:
	CNetworkArray( string_t, m_iszOverlayNames, MAX_SCREEN_OVERLAYS, [[= ks::reflect::Key{ .name = "OverlayName10", .index = 9 } ]]  [[= ks::reflect::Key{ .name = "OverlayName9", .index = 8 } ]]  [[= ks::reflect::Key{ .name = "OverlayName8", .index = 7 } ]]  [[= ks::reflect::Key{ .name = "OverlayName7", .index = 6 } ]]  [[= ks::reflect::Key{ .name = "OverlayName6", .index = 5 } ]]  [[= ks::reflect::Key{ .name = "OverlayName5", .index = 4 } ]]  [[= ks::reflect::Key{ .name = "OverlayName4", .index = 3 } ]]  [[= ks::reflect::Key{ .name = "OverlayName3", .index = 2 } ]]  [[= ks::reflect::Key{ .name = "OverlayName2", .index = 1 } ]]  [[= ks::reflect::Key{ .name = "OverlayName1", .index = 0 } ]]
	               [[= ks::reflect::Net{ .enc = ks::reflect::WireEnc::String, .varlen = true } ]]
	               [[= ks::reflect::Proxy<SendProxy_String_tToString, ks::reflect::WireSide::Send>{} ]] );
	CNetworkArray( float, m_flOverlayTimes, MAX_SCREEN_OVERLAYS, [[= ks::reflect::Key{ .name = "OverlayTime10", .index = 9 } ]]  [[= ks::reflect::Key{ .name = "OverlayTime9", .index = 8 } ]]  [[= ks::reflect::Key{ .name = "OverlayTime8", .index = 7 } ]]  [[= ks::reflect::Key{ .name = "OverlayTime7", .index = 6 } ]]  [[= ks::reflect::Key{ .name = "OverlayTime6", .index = 5 } ]]  [[= ks::reflect::Key{ .name = "OverlayTime5", .index = 4 } ]]  [[= ks::reflect::Key{ .name = "OverlayTime4", .index = 3 } ]]  [[= ks::reflect::Key{ .name = "OverlayTime3", .index = 2 } ]]  [[= ks::reflect::Key{ .name = "OverlayTime2", .index = 1 } ]]  [[= ks::reflect::Key{ .name = "OverlayTime1", .index = 0 } ]]
	               [[= ks::reflect::Net{ .bits = 11, .low = -1.0f, .high = 63.0f, .flags = SPROP_ROUNDDOWN, .varlen = true } ]] );
	CNetworkVar( float, m_flStartTime, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( int, m_iDesiredOverlay, [[= ks::reflect::Net{ .bits = 5 } ]] );
	CNetworkVar( bool, m_bIsActive, [[= ks::reflect::Net{} ]] );
};

LINK_ENTITY_TO_CLASS( env_screenoverlay, CEnvScreenOverlay );

IMPLEMENT_REFLECT_DATAMAP( CEnvScreenOverlay )

void SendProxy_String_tToString( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	string_t *pString = (string_t*)pData;
	pOut->m_pString = (char*)STRING( *pString );
}

IMPLEMENT_REFLECT_SERVERCLASS( CEnvScreenOverlay, DT_EnvScreenOverlay )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEnvScreenOverlay::CEnvScreenOverlay( void )
{
	m_flStartTime = 0;
	m_iDesiredOverlay = 0;
	m_bIsActive = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvScreenOverlay::Spawn_Impl( void )
{
	Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvScreenOverlay::Precache_Impl( void )
{
	for ( int i = 0; i < 10; i++ )
	{
		if ( m_iszOverlayNames[i] == NULL_STRING )
			continue;

		PrecacheMaterial( STRING( m_iszOverlayNames[i] ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CEnvScreenOverlay::InputStartOverlay( inputdata_t &inputdata )
{
	if ( m_iszOverlayNames[0] == NULL_STRING )
	{
		Warning("env_screenoverlay %s has no overlays to display.\n", STRING(GetEntityName()) );
		return;
	}

	m_flStartTime = gpGlobals->curtime;
	m_bIsActive = true;

	// Turn off any other screen overlays out there
	CBaseEntity *pEnt = nullptr;
	while ( (pEnt = gEntList.FindEntityByClassname( pEnt, "env_screenoverlay" )) != nullptr )
	{
		if ( pEnt != this )
		{
			CEnvScreenOverlay *pOverlay = static_cast<CEnvScreenOverlay*>(pEnt);
			pOverlay->SetActive( false );
		}
	}
}

int CEnvScreenOverlay::UpdateTransmitState_Impl()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

void CEnvScreenOverlay::InputSwitchOverlay( inputdata_t &inputdata )
{
	int iNewOverlay = inputdata.value.Int() - 1;
	iNewOverlay = abs( iNewOverlay );

	if ( m_iszOverlayNames[iNewOverlay] == NULL_STRING )
	{
		Warning("env_screenoverlay %s has no overlays to display.\n", STRING(GetEntityName()) );
		return;
	}

	m_iDesiredOverlay = iNewOverlay;
	m_flStartTime = gpGlobals->curtime;
}

void CEnvScreenOverlay::InputStopOverlay( inputdata_t &inputdata )
{
	if ( m_iszOverlayNames[0] == NULL_STRING )
	{
		Warning("env_screenoverlay %s has no overlays to display.\n", STRING(GetEntityName()) );
		return;
	}

	m_flStartTime = -1;
	m_bIsActive = false; 
}

// ====================================================================================
//
//  Screen-space effects
//
// ====================================================================================

class CEnvScreenEffect : public CPointEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_EnvScreenEffect" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CEnvScreenEffect, CPointEntity );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	// We always want to be sent to the client
	CEnvScreenEffect( void ) { 	AddEFlags( EFL_FORCE_CHECK_TRANSMIT ); }
	int UpdateTransmitState_Impl( void )	{ return SetTransmitState( FL_EDICT_ALWAYS ); }
	void Spawn_Impl( void );
	void Precache_Impl( void );

private:

	[[= ks::reflect::Input{ .name = "StartEffect", .type = FIELD_FLOAT } ]] void InputStartEffect( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "StopEffect", .type = FIELD_FLOAT } ]] void InputStopEffect( inputdata_t &inputdata );

	CNetworkVar( float, m_flDuration, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( int, m_nType, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "type" } ]] );
};

LINK_ENTITY_TO_CLASS( env_screeneffect, CEnvScreenEffect );

// CEnvScreenEffect
IMPLEMENT_REFLECT_DATAMAP( CEnvScreenEffect )

IMPLEMENT_REFLECT_SERVERCLASS( CEnvScreenEffect, DT_EnvScreenEffect )

void CEnvScreenEffect::Spawn_Impl( void )
{
	Precache();
}

void CEnvScreenEffect::Precache_Impl( void )
{
	PrecacheMaterial( "effects/stun" );
	PrecacheMaterial( "effects/introblur" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvScreenEffect::InputStartEffect( inputdata_t &inputdata )
{
	// Take the duration as our value
	m_flDuration = inputdata.value.Float();

	EntityMessageBegin( this );
		WRITE_BYTE( 0 );
	MessageEnd();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvScreenEffect::InputStopEffect( inputdata_t &inputdata )
{
	m_flDuration = inputdata.value.Float();

	// Send the stop notification
	EntityMessageBegin( this );
		WRITE_BYTE( 1 );
	MessageEnd();
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

class CTextureToggle : public CPointEntity
{
public:
	DECLARE_CLASS( CTextureToggle, CPointEntity );

	[[= ks::reflect::Input{ .name = "IncrementTextureIndex", .type = FIELD_VOID } ]] void	InputIncrementBrushTexIndex( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetTextureIndex", .type = FIELD_INTEGER } ]] void	InputSetBrushTexIndex( inputdata_t &inputdata );

private:
	
	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( env_texturetoggle, CTextureToggle );

IMPLEMENT_REFLECT_DATAMAP( CTextureToggle )

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CTextureToggle::InputIncrementBrushTexIndex( inputdata_t& inputdata )
{
	CBaseEntity *pEntity = gEntList.FindEntityByName( nullptr, m_target );
		
	while( pEntity ) 
	{
		int iCurrentIndex =  pEntity->GetTextureFrameIndex() + 1;
		pEntity->SetTextureFrameIndex( iCurrentIndex );

		pEntity = gEntList.FindEntityByName( pEntity, m_target ); 
	}
}

void CTextureToggle::InputSetBrushTexIndex( inputdata_t& inputdata )
{
	CBaseEntity *pEntity = gEntList.FindEntityByName( nullptr, m_target );
		
	while( pEntity ) 
	{
		int iData = inputdata.value.Int();

		pEntity->SetTextureFrameIndex( iData );
		pEntity = gEntList.FindEntityByName( pEntity, m_target ); 
	}
}

class CBaseEntity;
class CBasePlayer;
class CTonemapTrigger;
struct inputdata_t;

using namespace ::ks::reflect;

// Spawn Flags
static constexpr auto SF_TONEMAP_MASTER = 0x0001;

// 0 - eyes fully closed / fully black
// 1 - nominal 
// 16 - eyes wide open / fully white

//-----------------------------------------------------------------------------
// Purpose: Entity that controls player's tonemap
//-----------------------------------------------------------------------------
class CEnvTonemapController : public CPointEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_EnvTonemapController" } ]]
	static constexpr bool ks_class = true;

	DECLARE_CLASS( CEnvTonemapController, CPointEntity );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CEnvTonemapController();

	void	Spawn_Impl( void );
	int		UpdateTransmitState_Impl( void );

	bool	IsMaster( void ) const					{ return HasSpawnFlags( SF_TONEMAP_MASTER ); }

	// Inputs
	[[= ks::reflect::Input{ .name = "SetTonemapRate", .type = FIELD_FLOAT } ]] void	InputSetTonemapRate( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetAutoExposureMin", .type = FIELD_FLOAT } ]] void	InputSetAutoExposureMin( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetAutoExposureMax", .type = FIELD_FLOAT } ]] void	InputSetAutoExposureMax( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "UseDefaultAutoExposure", .type = FIELD_VOID } ]] void	InputUseDefaultAutoExposure( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetBloomScale", .type = FIELD_FLOAT } ]] void	InputSetBloomScale( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "UseDefaultBloomScale", .type = FIELD_VOID } ]] void	InputUseDefaultBloomScale( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetBloomScaleRange", .type = FIELD_FLOAT } ]] void	InputSetBloomScaleRange( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "SetBloomExponent", .type = FIELD_FLOAT } ]] void	InputSetBloomExponent( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetBloomSaturation", .type = FIELD_FLOAT } ]] void	InputSetBloomSaturation( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetTonemapPercentTarget", .type = FIELD_FLOAT } ]] void	InputSetTonemapPercentTarget( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetTonemapPercentBrightPixels", .type = FIELD_FLOAT } ]] void	InputSetTonemapPercentBrightPixels( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetTonemapMinAvgLum", .type = FIELD_FLOAT } ]] void	InputSetTonemapMinAvgLum( inputdata_t &inputdata );

public:
	CNetworkVar( bool, m_bUseCustomAutoExposureMin, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( bool, m_bUseCustomAutoExposureMax, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( bool, m_bUseCustomBloomScale, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );
	CNetworkVar( float, m_flCustomAutoExposureMin, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flCustomAutoExposureMax, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flCustomBloomScale, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flCustomBloomScaleMinimum, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flBloomExponent, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flBloomSaturation, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flTonemapPercentTarget, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flTonemapPercentBrightPixels, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flTonemapMinAvgLum, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_flTonemapRate, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
};

LINK_ENTITY_TO_CLASS( env_tonemap_controller, CEnvTonemapController );

IMPLEMENT_REFLECT_DATAMAP( CEnvTonemapController )

IMPLEMENT_REFLECT_SERVERCLASS( CEnvTonemapController, DT_EnvTonemapController )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEnvTonemapController::CEnvTonemapController()
{
	m_flBloomExponent = 2.5f;
	m_flBloomSaturation = 1.0f;
	m_flTonemapPercentTarget = 65.0f;
	m_flTonemapPercentBrightPixels = 2.0f;
	m_flTonemapMinAvgLum = 3.0f;
	m_flTonemapRate = 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::Spawn_Impl( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CEnvTonemapController::UpdateTransmitState_Impl()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: set a base and minimum bloom scale
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetBloomScaleRange( inputdata_t &inputdata )
{
	float bloom_max=1, bloom_min=1;
	int nargs=std::sscanf("%f %f",inputdata.value.String(), bloom_max, bloom_min );
	if (nargs != 2)
	{
		Warning("%s (%s) received SetBloomScaleRange input without 2 arguments. Syntax: <max bloom> <min bloom>\n", GetClassname(), GetDebugName() );
		return;
	}
	m_flCustomBloomScale=bloom_max;
	m_flCustomBloomScale=bloom_min;
}

//-----------------------------------------------------------------------------
// Purpose: Set the auto exposure min to the specified value
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetAutoExposureMin( inputdata_t &inputdata )
{
	m_flCustomAutoExposureMin = inputdata.value.Float();
	m_bUseCustomAutoExposureMin = true;
}

//-----------------------------------------------------------------------------
// Purpose: Set the auto exposure max to the specified value
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetAutoExposureMax( inputdata_t &inputdata )
{
	m_flCustomAutoExposureMax = inputdata.value.Float();
	m_bUseCustomAutoExposureMax = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputUseDefaultAutoExposure( inputdata_t &inputdata )
{
	m_bUseCustomAutoExposureMin = false;
	m_bUseCustomAutoExposureMax = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetBloomScale( inputdata_t &inputdata )
{
	m_flCustomBloomScale = inputdata.value.Float();
	m_flCustomBloomScaleMinimum = m_flCustomBloomScale;
	m_bUseCustomBloomScale = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputUseDefaultBloomScale( inputdata_t &inputdata )
{
	m_bUseCustomBloomScale = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetBloomExponent( inputdata_t &inputdata )
{
	m_flBloomExponent = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetBloomSaturation( inputdata_t &inputdata )
{
	m_flBloomSaturation = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetTonemapPercentTarget( inputdata_t &inputdata )
{
	m_flTonemapPercentTarget = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetTonemapPercentBrightPixels( inputdata_t &inputdata )
{
	m_flTonemapPercentBrightPixels= inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetTonemapMinAvgLum( inputdata_t &inputdata )
{
	m_flTonemapMinAvgLum = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetTonemapRate( inputdata_t &inputdata )
{
	m_flTonemapRate = inputdata.value.Float();
}

//--------------------------------------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( trigger_tonemap, CTonemapTrigger );

IMPLEMENT_REFLECT_DATAMAP( CTonemapTrigger )

//--------------------------------------------------------------------------------------------------------
void CTonemapTrigger::Spawn_Impl( void )
{
	AddSpawnFlags( SF_TRIGGER_ALLOW_CLIENTS );

	BaseClass::Spawn_Impl();
	InitTrigger();

	m_hTonemapController = gEntList.FindEntityByName( nullptr, m_tonemapControllerName );
}

//--------------------------------------------------------------------------------------------------------
void CTonemapTrigger::StartTouch_Impl( CBaseEntity *other )
{
	if ( !PassesTriggerFilters( other ) )
		return;

	BaseClass::StartTouch_Impl( other );

	CBasePlayer *player = ToBasePlayer( other );
	if ( !player )
		return;

	player->OnTonemapTriggerStartTouch( this );
}

//--------------------------------------------------------------------------------------------------------
void CTonemapTrigger::EndTouch_Impl( CBaseEntity *other )
{
	if ( !PassesTriggerFilters( other ) )
		return;

	BaseClass::EndTouch_Impl( other );

	CBasePlayer *player = ToBasePlayer( other );
	if ( !player )
		return;

	player->OnTonemapTriggerEndTouch( this );
}

//-----------------------------------------------------------------------------
// Purpose: Clear out the tonemap controller.
//-----------------------------------------------------------------------------
void CTonemapSystem::LevelInitPreEntity( void )
{
	m_hMasterController = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: On level load find the master tonemap controller.  If no controller is 
//			set as Master, use the first tonemap controller found.
//-----------------------------------------------------------------------------
void CTonemapSystem::LevelInitPostEntity( void )
{
	// Overall master controller
	CEnvTonemapController *pTonemapController = nullptr;
	do
	{
		pTonemapController = static_cast<CEnvTonemapController*>( gEntList.FindEntityByClassname( pTonemapController, "env_tonemap_controller" ) );
		if ( pTonemapController )
		{
			if ( m_hMasterController == nullptr )
			{
				m_hMasterController = pTonemapController;
			}
			else
			{
				if ( pTonemapController->IsMaster() )
				{
					m_hMasterController = pTonemapController;
				}
			}
		}
	} while ( pTonemapController );

}

//--------------------------------------------------------------------------------------------------------
CTonemapSystem s_TonemapSystem( "TonemapSystem" );

//--------------------------------------------------------------------------------------------------------
CTonemapSystem *TheTonemapSystem( void )
{
	return &s_TonemapSystem;
}

//--------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------
CBaseEntity *CTonemapTrigger::GetTonemapController( void ) const
{
	return m_hTonemapController.Get();
}

//--------------------------------------------------------------------------------------------------------
CBaseEntity *CTonemapSystem::GetMasterTonemapController( void ) const
{
	return m_hMasterController.Get();
}

using namespace ::ks::reflect;

// Purpose: Information about algorithmic stuff that can occur on both client + server
// In order to reduce network traffic, it's possible to create a algorithms
// that will work on both the client and the server and be totally repeatable.
// All we need do is to send down initial conditions and let the algorithm
// compute the values at various times. Note that this algorithm will be called
// at different times with different frequencies on the client and server.
// The trick here is that in order for it to be repeatable, the algorithm either
// cannot depend on random numbers, or, if it does, we need to make sure that
// the random numbers generated are effectively done at the beginning of time,
// so that differences in frame rate on client and server won't matter. It also
// is important that the initial state sent across the network is identical
// bitwise so that we produce the exact same results. Therefore no compression
// should be used in the datatables.
// Note also that each algorithm must have its own random number stream so that
// it cannot possibly interact with other code using random numbers that will
// be called at various different intervals on the client + server. Use the
// CUniformRandomStream class for this.
// There are two types of client-server neutral code: Code that doesn't interact
// with player prediction, and code that does. The code that doesn't interact
// with player prediction simply has to be able to produce the result f(time)
// where time is monotonically increasing. For prediction, we have to produce
// the result f(time) where time does *not* monotonically increase (time can be
// anywhere between the "current" time and the prior 10 seconds).
// Code that is not used by player prediction can maintain state because later
// calls will always compute the value at some future time. This computation can
// use random number generation, but with the following restriction: Your code
// must generate exactly the same number of random numbers regardless of how
// frequently the code is called.
// In specific, this means that all random numbers used must either be computed
// at init time, or must be used in an 'event-based form'. Namely, use random
// numbers to compute the time at which events occur and the random inputs for
// those events.  When simulating forward, you must simulate all intervening
// time and generate the same number of random numbers.
// For functions planned to be used by player prediction, one method is to use
// some sort of stateless computation (where the only states are the initial
// state and time). Note that random number generators have state implicit in
// the number of calls made to that random number generator, and therefore you
// cannot call a random number generator unless you are able to
// 1) Use a random number generator that can return the ith random number, namely:
//	float r = random( i );	// i == the ith number in the random sequence
// 2) Be able to accurately know at any given time t how many random numbers
//		have already been generated (namely, compute the i in part 1 above).
// There is another alternative for code meant to be used by player prediction:
// you could just store a history of 'events' from which you could completely
// determine the value of f(time). That history would need to be at least 10
// seconds long, which is guaranteed to be longer than the amount of time that
// prediction would need. I've written a class which I haven't tested yet (but
// will be using soon) called CTimedEventQueue (currently located in
// env_wind_shared.h) which I plan to use to solve my problem (getting wind to
// blow players).

//-----------------------------------------------------------------------------
// globals
//-----------------------------------------------------------------------------

static std::vector< CEnvWindShared * > s_windControllers;

CEnvWindShared::CEnvWindShared()
{
	m_pWindSound = nullptr;
	s_windControllers.push_back( this );
}

CEnvWindShared::~CEnvWindShared()
{
	if (m_pWindSound)
	{
		CSoundEnvelopeController::GetController().Shutdown( m_pWindSound );
	}
	ks::erase_first( s_windControllers, this );
}

void CEnvWindShared::Init( int nEntIndex, int iRandomSeed, float flTime, 
						  int iInitialWindYaw, float flInitialWindSpeed )
{
	m_iEntIndex = nEntIndex;
	m_flWindAngleVariation = m_flWindSpeedVariation = 1.0f;
	m_flStartTime = m_flSimTime = m_flSwitchTime = m_flVariationTime = flTime;
	m_iWindSeed = iRandomSeed;
	m_Stream.SetSeed( iRandomSeed );
	m_WindVariationStream.SetSeed( iRandomSeed );
	m_iWindDir = m_iInitialWindDir = iInitialWindYaw;
	// Bound it for networking as a postive integer
	m_iInitialWindDir = (int)( anglemod( m_iInitialWindDir ) );

	m_flAveWindSpeed = m_flWindSpeed = m_flInitialWindSpeed = flInitialWindSpeed;

	/*
	// Cache in the wind sound...
	if (!g_pEffects->IsServer())
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		m_pWindSound = controller.SoundCreate( -1, CHAN_STATIC, 
			"EnvWind.Loop", ATTN_NONE );
		controller.Play( m_pWindSound, 0.0f, 100 );
	}
	*/

	// Next time a change happens (which will happen immediately), it'll stop gusting
	m_bGusting = true;
}

//-----------------------------------------------------------------------------
// Computes wind variation
//-----------------------------------------------------------------------------

static constexpr auto WIND_VARIATION_UPDATE_TIME = 0.1f;

void CEnvWindShared::ComputeWindVariation( float flTime )
{
	// The wind variation is updated every 10th of a second..
	while( flTime >= m_flVariationTime )
	{
		m_flWindAngleVariation = m_WindVariationStream.RandomFloat( -10, 10 );
		m_flWindSpeedVariation = 1.0 + m_WindVariationStream.RandomFloat( -0.2, 0.2 );
		m_flVariationTime += WIND_VARIATION_UPDATE_TIME;
	}
}

//-----------------------------------------------------------------------------
// Updates the wind sound
//-----------------------------------------------------------------------------
void CEnvWindShared::UpdateWindSound( float flTotalWindSpeed )
{
	if (!g_pEffects->IsServer())
	{
		float flDuration = random_valve->RandomFloat( 1.0f, 2.0f );
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		// FIXME: Tweak with these numbers
		float flNormalizedWindSpeed = flTotalWindSpeed / 150.0f;
		if (flNormalizedWindSpeed > 1.0f)
			flNormalizedWindSpeed = 1.0f;
		float flPitch = 120 * Bias( flNormalizedWindSpeed, 0.3f ) + 100;
		float flVolume = 0.3f * Bias( flNormalizedWindSpeed, 0.3f ) + 0.7f;
		controller.SoundChangePitch( m_pWindSound, flPitch, flDuration );
		controller.SoundChangeVolume( m_pWindSound, flVolume, flDuration );
	}
}

//-----------------------------------------------------------------------------
// Updates the swaying of trees
//-----------------------------------------------------------------------------
static constexpr auto TREE_SWAY_UPDATE_TIME = 2.0f;

void CEnvWindShared::UpdateTreeSway( float flTime )
{
}

//-----------------------------------------------------------------------------
// Updates the wind speed
//-----------------------------------------------------------------------------

static constexpr auto WIND_ACCELERATION = 150.0f; // wind speed can accelerate this many units per second
static constexpr auto WIND_DECELERATION = 15.0f; // wind speed can decelerate this many units per second

float CEnvWindShared::WindThink( float flTime )
{
	// NOTE: This algorithm can be client-server neutal because we're using 
	// the random number generator to generate *time* at which the wind changes.
	// We therefore need to structure the algorithm so that no matter the
	// frequency of calls to this function we produce the same wind speeds...

	ComputeWindVariation( flTime );

	// Update Tree Sway
	UpdateTreeSway( flTime );

	while (true)
	{
		// First, simulate up to the next switch time...
		float flTimeToSwitch = m_flSwitchTime - m_flSimTime;
		float flMaxDeltaTime = flTime - m_flSimTime;

		bool bGotToSwitchTime = (flMaxDeltaTime > flTimeToSwitch);

		float flSimDeltaTime = bGotToSwitchTime ? flTimeToSwitch : flMaxDeltaTime;

		// Now that we've chosen 
		// either ramp up, or sleep till change
		bool bReachedSteadyState = true;
		if ( m_flAveWindSpeed > m_flWindSpeed )
		{
			m_flWindSpeed += WIND_ACCELERATION * flSimDeltaTime;
			if (m_flWindSpeed > m_flAveWindSpeed)
				m_flWindSpeed = m_flAveWindSpeed;
			else
				bReachedSteadyState = false;
		}
		else if ( m_flAveWindSpeed < m_flWindSpeed )
		{
			m_flWindSpeed -= WIND_DECELERATION * flSimDeltaTime;
			if (m_flWindSpeed < m_flAveWindSpeed)
				m_flWindSpeed = m_flAveWindSpeed;
			else
				bReachedSteadyState = false;
		}

		// Update the sim time

		// If we didn't get to a switch point, then we're done simulating for now 
		if (!bGotToSwitchTime)
		{
			m_flSimTime = flTime;

			// We're about to exit, let's set the wind velocity...
			QAngle vecWindAngle( 0, m_iWindDir + m_flWindAngleVariation, 0 );
			AngleVectors( vecWindAngle, &m_currentWindVector );
			float flTotalWindSpeed = m_flWindSpeed * m_flWindSpeedVariation;
			m_currentWindVector *= flTotalWindSpeed;

			// If we reached a steady state, we don't need to be called until the switch time
			// Otherwise, we should be called immediately

			// FIXME: If we ever call this from prediction, we'll need
			// to only update the sound if it's a new time
			// Or, we'll need to update the sound elsewhere.
			// Update the sound....
//			UpdateWindSound( flTotalWindSpeed );

			// Always immediately call, the wind is forever varying
			return ( flTime + 0.01f );
		}

		m_flSimTime = m_flSwitchTime;

		// Switch gusting state..
		if( m_bGusting )
		{
			// wind is gusting, so return to normal wind
			m_flAveWindSpeed = m_Stream.RandomInt( m_iMinWind, m_iMaxWind );

			// set up for another gust later
			m_bGusting = false;
			m_flSwitchTime += m_flMinGustDelay + m_Stream.RandomFloat( 0, m_flMaxGustDelay );

			m_OnGustEnd.FireOutput( nullptr, nullptr );
		}
		else
		{
			// time for a gust.
			m_flAveWindSpeed = m_Stream.RandomInt( m_iMinGust, m_iMaxGust );

			// change wind direction, maybe a lot
			m_iWindDir = anglemod( m_iWindDir + m_Stream.RandomInt(-m_iGustDirChange, m_iGustDirChange) );

			// set up to stop the gust in a short while
			m_bGusting = true;

			m_OnGustStart.FireOutput( nullptr, nullptr );

			// !!!HACKHACK - gust duration tied to the length of a particular wave file
			m_flSwitchTime += m_flGustDuration;
		}
	}
}

//-----------------------------------------------------------------------------
// Method to reset wind speed..
//-----------------------------------------------------------------------------
void ResetWindspeed()
{
	for ( CEnvWindShared *pController : s_windControllers )
	{
		pController->m_currentWindVector.Init( 0, 0, 0 );
	}

}

//-----------------------------------------------------------------------------
// GetWindspeedAtTime was never finished to actually take time in to consideration.  We don't need 
// features that aren't written, but we do need to have multiple wind controllers on a map, so
// we need to find the one that is affecting the given location and return its speed.
//-----------------------------------------------------------------------------
Vector GetWindspeedAtLocation( const Vector &location )
{
	for ( CEnvWindShared *thisWindController : s_windControllers )
	{
		float distance = (thisWindController->m_location - location).Length();

		if( distance < thisWindController->m_windRadius )
		{
			// This location is within our area of influence, so return our computer wind vector
			return thisWindController->m_currentWindVector;
		}
	}

	for ( CEnvWindShared *thisWindController : s_windControllers )
	{

		if( thisWindController->m_windRadius == -1.0f )
		{
			// We do a second search for a global controller so you don't have to worry about order in the list.  
			return thisWindController->m_currentWindVector;
		}
	}

	return Vector(0,0,0);// No wind
}

//-----------------------------------------------------------------------------
// Method to sample the windspeed at a particular time
//-----------------------------------------------------------------------------
void GetWindspeedAtTime( float flTime, Vector &vecVelocity )
{
	// For now, ignore history and time.. fix later when we use wind to affect
	// client-side prediction
	if ( s_windControllers.empty() )
	{
		vecVelocity.Init( 0, 0, 0 );
	}
	else
	{
		VectorCopy( s_windControllers.front()->m_currentWindVector, vecVelocity );
	}
}

//-----------------------------------------------------------------------------
void CEnvWindShared::SetLocation( const Vector &location )
{
	m_location = location;
}

class CBaseEntity;
class CBasePlayer;
struct inputdata_t;

using namespace ::ks::reflect;

static constexpr auto ENV_ZOOM_OVERRIDE = (1<<0);

class CEnvZoom : public CPointEntity
{
public:
	DECLARE_CLASS( CEnvZoom, CPointEntity );

	[[= ks::reflect::Input{ .name = "Zoom", .type = FIELD_VOID } ]] void	InputZoom( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "UnZoom", .type = FIELD_VOID } ]] void	InputUnZoom( inputdata_t &inputdata );

	int	GetFOV( void ) { return m_nFOV;	}
	float GetSpeed( void ) { return m_flSpeed;	}
private:

	[[= ks::reflect::Key{ .name = "Rate" } ]] float	m_flSpeed;
	[[= ks::reflect::Key{ .name = "FOV" } ]] int		m_nFOV;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( env_zoom, CEnvZoom );

IMPLEMENT_REFLECT_DATAMAP( CEnvZoom )

bool CanOverrideEnvZoomOwner( CBaseEntity *pZoomOwner )
{
	CEnvZoom *pZoom = dynamic_cast<CEnvZoom*>(pZoomOwner );

	if ( pZoom == nullptr || pZoom && pZoom->HasSpawnFlags( ENV_ZOOM_OVERRIDE ) == false )
		 return false;

	return true;
}

float GetZoomOwnerDesiredFOV( CBaseEntity *pZoomOwner )
{
	if ( CanOverrideEnvZoomOwner( pZoomOwner ) )
	{
		CEnvZoom *pZoom = dynamic_cast<CEnvZoom*>( pZoomOwner );

		return pZoom->GetFOV();
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CEnvZoom::InputZoom( inputdata_t &inputdata )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();

	if ( pPlayer )
	{

		// If the player's already holding a fov from another env_zoom, we're allowed to overwrite it
		if ( pPlayer->GetFOVOwner() && FClassnameIs( pPlayer->GetFOVOwner(), "env_zoom" ) )
		{
			pPlayer->ClearZoomOwner();
		}

		//Stuff the values
		pPlayer->SetFOV( this, m_nFOV, m_flSpeed );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CEnvZoom::InputUnZoom( inputdata_t &inputdata )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();

	if ( pPlayer )
	{
		// Stuff the values
		pPlayer->SetFOV( this, 0 );
	}
}

class CBaseEntity;
class CBaseFilter;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Implements visual effects entities: sprites, beams, bubbles, etc.

// Keeps us from doing strcmps in the tracefilter.
string_t g_iszPhysicsPropClassname;

enum Touch_t
{
	touch_none = 0,
	touch_player_only,
	touch_npc_only,
	touch_player_or_npc,
	touch_player_or_npc_or_physicsprop,
};

class CEnvBeam : public CBeam
{
	[[= ks::reflect::KeyFrom<"m_nClipStyle", ks::reflect::Key{ .name = "ClipStyle" } >{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CEnvBeam, CBeam );

	void	Spawn_Impl( void );
	void	Precache_Impl( void );
	void	Activate_Impl( void );

	void	StrikeThink( void );
	void	UpdateThink( void );
	void	RandomArea( void );
	void	RandomPoint( const Vector &vecSrc );
	void	Zap( const Vector &vecSrc, const Vector &vecDest );

	void	Strike( void );

	bool	PassesTouchFilters(CBaseEntity *pOther);

	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "StrikeOnce", .type = FIELD_VOID } ]] void InputStrikeOnce( inputdata_t &inputdata );

	void TurnOn( void );
	void TurnOff( void );
	void Toggle( void );
	
	const char *GetDecalName( void ){ return STRING( m_iszDecal );}

	inline bool ServerSide( void )
	{
		if ( m_life == 0 && !HasSpawnFlags(SF_BEAM_RING) )
			return true;

		return false;
	}

	DECLARE_DATADESC();

	void	BeamUpdateVars( void );

protected:
	// true if the end point vecline was specified in hammer
	inline bool HasEndPointHandle() { return !m_vEndPointRelative.IsZero();  }

	int		m_active;
	int		m_spriteTexture;

	[[= ks::reflect::Key{ .name = "LightningStart" } ]] string_t m_iszStartEntity;
	[[= ks::reflect::Key{ .name = "LightningEnd" } ]] string_t m_iszEndEntity;
	[[= ks::reflect::Key{ .name = "life" } ]] float	m_life;
	[[= ks::reflect::Key{ .name = "BoltWidth" } ]] float	m_boltWidth;
	[[= ks::reflect::Key{ .name = "NoiseAmplitude" } ]] float	m_noiseAmplitude;
	[[= ks::reflect::Key{ .name = "TextureScroll" } ]] int		m_speed;
	[[= ks::reflect::Key{ .name = "StrikeTime" } ]] float	m_restrike;
	[[= ks::reflect::Key{ .name = "texture" } ]] string_t m_iszSpriteName;
	[[= ks::reflect::Key{ .name = "framestart" } ]] int		m_frameStart;

	// endpoint may be optionally specified as a vecline instead of a target entity. 
	// note: this mechanism seems rather roundabout, because the parent CBeam has
	// the m_vecEndPos data member; however, the CEnvBeam is programmed to overwrite it 
	// each frame with the position of a target entity, so the easiest way to 
	// implement this behavior was to put this bit of redundant data in the child
	// class and have it get written back every frame. If this bothers you, 
	// please fix it.
	[[= ks::reflect::Key{ .name = "targetpoint" } ]] Vector m_vEndPointWorld;  // this is the point as read from the level spec; however it's not used, because
	Vector m_vEndPointRelative; // on spawn, endpoint is transformed into local space here.

	[[= ks::reflect::Key{ .name = "Radius" } ]] float	m_radius;

	[[= ks::reflect::Key{ .name = "TouchType" } ]] Touch_t		m_TouchType;
	[[= ks::reflect::Key{ .name = "filtername" } ]] string_t	m_iFilterName;
	EHANDLE		m_hFilter;

	[[= ks::reflect::Key{ .name = "decalname" } ]] string_t		m_iszDecal;

	[[= ks::reflect::Key{ .name = "OnTouchedByEntity" } ]] COutputEvent	m_OnTouchedByEntity;
};

LINK_ENTITY_TO_CLASS( env_beam, CEnvBeam );

IMPLEMENT_REFLECT_DATAMAP( CEnvBeam )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvBeam::Spawn_Impl( void )
{
	if ( !m_iszSpriteName )
	{
		SetThink( &CEnvBeam::SUB_Remove );
		return;
	}

	BaseClass::Spawn_Impl();

	m_noiseAmplitude = std::min<std::common_type_t<int, float>>(MAX_BEAM_NOISEAMPLITUDE, m_noiseAmplitude);

	// Check for tapering
	if ( HasSpawnFlags( SF_BEAM_TAPEROUT ) )
	{
		SetWidth( m_boltWidth );
		SetEndWidth( 0 );
	}
	else
	{
		SetWidth( m_boltWidth );
		SetEndWidth( GetWidth() );	// Note: EndWidth is not scaled
	}

	// if a non-targetentity endpoint was specified, transform it into local relative space
	// so it can move along with the base
	if (!m_vEndPointWorld.IsZero())
	{
		WorldToEntitySpace( m_vEndPointWorld, &m_vEndPointRelative );
	}
	else
	{
		m_vEndPointRelative.Zero();
	}

	if ( ServerSide() )
	{
		SetThink( &CEnvBeam::UpdateThink );
		SetNextThink( gpGlobals->curtime );
		SetFireTime( gpGlobals->curtime );

		if ( GetEntityName() != NULL_STRING )
		{
			if ( !(m_spawnflags & SF_BEAM_STARTON) )
			{
				AddEffects( EF_NODRAW );
				m_active = 0;
				SetNextThink( TICK_NEVER_THINK );
			}
			else
			{
				m_active = 1;
			}
		}
	}
	else
	{
		m_active = 0;
		if ( !GetEntityName() || FBitSet(m_spawnflags, SF_BEAM_STARTON) )
		{
			SetThink( &CEnvBeam::StrikeThink );
			SetNextThink( gpGlobals->curtime + 1.0f );
		}
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvBeam::Precache_Impl( void )
{
	if ( !V_stristr( STRING(m_iszSpriteName), ".vmt" ) )
	{
		// HACK/YWB:  This was almost always the laserbeam.spr, so alloc'ing the name a second time with the proper extension isn't going to
		//  kill us on memrory.
		//Warning( "Level Design Error:  %s (%i:%s) Sprite name (%s) missing .vmt extension!\n",
		//	STRING( m_iClassname ), entindex(), GetEntityName(), STRING(m_iszSpriteName) );

		char fixedname[ 512 ];
		V_strncpy( fixedname, STRING( m_iszSpriteName ), sizeof( fixedname ) );

		V_SetExtension( fixedname, ".vmt", sizeof( fixedname ) );
		
		m_iszSpriteName = AllocPooledString( fixedname );
	}

	g_iszPhysicsPropClassname = AllocPooledString( "prop_physics" );

	m_spriteTexture = PrecacheModel( STRING(m_iszSpriteName) );
	BaseClass::Precache_Impl();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvBeam::Activate_Impl( void )
{
	// Get a handle to my filter entity if there is one
	if (m_iFilterName != NULL_STRING)
	{
		m_hFilter = dynamic_cast<CBaseFilter *>(gEntList.FindEntityByName( nullptr, m_iFilterName ));
	}

	BaseClass::Activate_Impl();

	if ( ServerSide() )
		BeamUpdateVars();
}

//-----------------------------------------------------------------------------
// Purpose: Input handler to turn the lightning on either continually or for
//			interval refiring.
//-----------------------------------------------------------------------------
void CEnvBeam::InputTurnOn( inputdata_t &inputdata )
{
	if ( !m_active )
	{
		TurnOn();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler to turn the lightning off.
//-----------------------------------------------------------------------------
void CEnvBeam::InputTurnOff( inputdata_t &inputdata )
{
	if ( m_active )
	{
		TurnOff();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler to toggle the lightning on/off.
//-----------------------------------------------------------------------------
void CEnvBeam::InputToggle( inputdata_t &inputdata )
{
	if ( m_active )
	{
		TurnOff();
	}
	else
	{
		TurnOn();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for making the beam strike once. This will not affect
//			any interval refiring that might be going on. If the lifetime is set
//			to zero (infinite) it will turn on and stay on.
//-----------------------------------------------------------------------------
void CEnvBeam::InputStrikeOnce( inputdata_t &inputdata )
{
	Strike();
}

//-----------------------------------------------------------------------------
// Purpose: Turns the lightning on. If it is set for interval refiring, it will
//			begin doing so. If it is set to be continually on, it will do so.
//-----------------------------------------------------------------------------
void CEnvBeam::TurnOn( void )
{
	m_active = 1;

	if ( ServerSide() )
	{
		RemoveEffects( EF_NODRAW );
		DoSparks( GetAbsStartPos(), GetAbsEndPos() );

		SetThink( &CEnvBeam::UpdateThink );
		SetNextThink( gpGlobals->curtime );
		SetFireTime( gpGlobals->curtime );
	}
	else
	{
		SetThink( &CEnvBeam::StrikeThink );
		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvBeam::TurnOff( void )
{
	m_active = 0;

	if ( ServerSide() )
	{
		AddEffects( EF_NODRAW );
	}

	SetNextThink( TICK_NEVER_THINK );
	SetThink( nullptr );
}

//-----------------------------------------------------------------------------
// Purpose: Think function for striking at intervals.
//-----------------------------------------------------------------------------
void CEnvBeam::StrikeThink( void )
{
	if ( m_life != 0 )
	{
		if ( m_spawnflags & SF_BEAM_RANDOM )
			SetNextThink( gpGlobals->curtime + m_life + random_valve->RandomFloat( 0, m_restrike ) );
		else
			SetNextThink( gpGlobals->curtime + m_life + m_restrike );
	}
	m_active = 1;

	if (!m_iszEndEntity && !HasEndPointHandle())
	{
		if (!m_iszStartEntity)
		{
			RandomArea( );
		}
		else
		{
			CBaseEntity *pStart = RandomTargetname( STRING(m_iszStartEntity) );
			if (pStart != nullptr)
			{
				RandomPoint( pStart->GetAbsOrigin() );
			}
			else
			{
				Msg( "env_beam: unknown entity \"%s\"\n", STRING(m_iszStartEntity) );
			}
		}
		return;
	}

	Strike();
}

//-----------------------------------------------------------------------------
// Purpose: Strikes once for its configured lifetime.
//-----------------------------------------------------------------------------
void CEnvBeam::Strike( void )
{
	CBroadcastRecipientFilter filter;

	CBaseEntity *pStart = RandomTargetname( STRING(m_iszStartEntity) );
	CBaseEntity *pEnd = RandomTargetname( STRING(m_iszEndEntity) );

	// if the end entity is missing, we use the Hammer-specified vector offset instead.
	bool bEndPointFromEntity = pEnd != nullptr;

	if ( pStart == nullptr || ( !bEndPointFromEntity && !HasEndPointHandle() ) )
		return;

	Vector vEndPointLocation;
	if ( bEndPointFromEntity )
	{
		 vEndPointLocation = pEnd->GetAbsOrigin() ;
	}
	else
	{
		EntityToWorldSpace( m_vEndPointRelative, &vEndPointLocation );
	}

	m_speed = clamp( m_speed, 0, MAX_BEAM_SCROLLSPEED );
	
	bool pointStart = IsStaticPointEntity( pStart );
	bool pointEnd = !bEndPointFromEntity || IsStaticPointEntity( pEnd );

	if ( pointStart || pointEnd )
	{
		if ( m_spawnflags & SF_BEAM_RING )
		{
			// don't work
			return;
		}

		te->BeamEntPoint( filter, 0.0,
			pointStart ? 0 : pStart->entindex(),
			pointStart ? &pStart->GetAbsOrigin() : nullptr,
			pointEnd ? 0 : pEnd->entindex(),
			pointEnd ? &vEndPointLocation : nullptr,
			m_spriteTexture,
			0,	// No halo
			m_frameStart,
			(int)m_flFrameRate,
			m_life,
			m_boltWidth,
			m_boltWidth,	// End width
			0,				// No fade
			m_noiseAmplitude,
			m_clrRender->r,	m_clrRender->g,	m_clrRender->b,	m_clrRender->a,
			m_speed );
	}
	else
	{
		if ( m_spawnflags & SF_BEAM_RING)
		{
			te->BeamRing( filter, 0.0,
				pStart->entindex(), 
				pEnd->entindex(), 
				m_spriteTexture, 
				0,	// No halo
				m_frameStart,
				(int)m_flFrameRate,
				m_life,
				m_boltWidth,
				0,	// No spread
				m_noiseAmplitude,
				m_clrRender->r,
				m_clrRender->g,
				m_clrRender->b,
				m_clrRender->a,
				m_speed );
		}
		else
		{
			te->BeamEnts( filter, 0.0,
				pStart->entindex(), 
				pEnd->entindex(), 
				m_spriteTexture,
				0,	// No halo
				m_frameStart,
				(int)m_flFrameRate,
				m_life,
				m_boltWidth,
				m_boltWidth,	// End width
				0,				// No fade
				m_noiseAmplitude,
				m_clrRender->r,
				m_clrRender->g,
				m_clrRender->b,
				m_clrRender->a,
				m_speed );

		}
	}

	DoSparks( pStart->GetAbsOrigin(), pEnd->GetAbsOrigin() );
	if ( m_flDamage > 0 )
	{
		trace_t tr;
		UTIL_TraceLine( pStart->GetAbsOrigin(), pEnd->GetAbsOrigin(), MASK_SOLID, nullptr, COLLISION_GROUP_NONE, &tr );
		BeamDamageInstant( &tr, m_flDamage );
	}
	
}

class CTraceFilterPlayersNPCs : public ITraceFilter
{
public:
	bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pServerEntity );
		if ( pEntity )
		{
			if ( pEntity->IsPlayer() || nullptr )
				return true;
		}

		return false;
	}
	virtual TraceType_t	GetTraceType() const
	{
		return TRACE_ENTITIES_ONLY;
	}
};

class CTraceFilterPlayersNPCsPhysicsProps : public ITraceFilter
{
public:
	bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pServerEntity );
		if ( pEntity )
		{
			if ( pEntity->IsPlayer() || nullptr || pEntity->m_iClassname == g_iszPhysicsPropClassname )
				return true;
		}

		return false;
	}
	virtual TraceType_t	GetTraceType() const
	{
		return TRACE_ENTITIES_ONLY;
	}
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CEnvBeam::PassesTouchFilters(CBaseEntity *pOther)
{
	bool fPassedSoFar = false;

	// Touched some player or NPC!
	if( m_TouchType != touch_npc_only )
	{
		if( pOther->IsPlayer() )
		{
			fPassedSoFar = true;
		}
	}

	if( m_TouchType != touch_player_only )
	{
		
	}

	if( m_TouchType == touch_player_or_npc_or_physicsprop )
	{
		if( pOther->m_iClassname == g_iszPhysicsPropClassname )
		{
			fPassedSoFar = true;
		}
	}

	if( fPassedSoFar )
	{
		CBaseFilter* pFilter = (CBaseFilter*)(m_hFilter.Get());
		return (!pFilter) ? true : pFilter->PassesFilter( this, pOther );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvBeam::UpdateThink( void )
{
	// Apply damage every 1/10th of a second.
	if ( ( m_flDamage > 0 ) && ( gpGlobals->curtime >= m_flFireTime + 0.1 ) )
	{
		trace_t tr;
		UTIL_TraceLine( GetAbsStartPos(), GetAbsEndPos(), MASK_SOLID, nullptr, COLLISION_GROUP_NONE, &tr );
		BeamDamage( &tr );
		// BeamDamage calls RelinkBeam, so no need to call it again.
	}
	else
	{
		RelinkBeam();
	}

	if( m_TouchType != touch_none )
	{
		trace_t tr;
		Ray_t ray;
		ray.Init( GetAbsStartPos(), GetAbsEndPos() );

		if( m_TouchType == touch_player_or_npc_or_physicsprop )
		{
			CTraceFilterPlayersNPCsPhysicsProps traceFilter;
			enginetrace->TraceRay( ray, MASK_SHOT, &traceFilter, &tr );
		}
		else
		{
			CTraceFilterPlayersNPCs traceFilter;
			enginetrace->TraceRay( ray, MASK_SHOT, &traceFilter, &tr );
		}

		if( tr.fraction != 1.0 && PassesTouchFilters( tr.Ent<CBaseEntity>() ) )
		{
			m_OnTouchedByEntity.FireOutput( tr.Ent<CBaseEntity>(), this, 0 );
			return;
		}
	}

	SetNextThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vecSrc - 
//			&vecDest - 
//-----------------------------------------------------------------------------
void CEnvBeam::Zap( const Vector &vecSrc, const Vector &vecDest )
{
	CBroadcastRecipientFilter filter;

	te->BeamPoints( filter, 0.0,
		&vecSrc, 
		&vecDest, 
		m_spriteTexture, 
		0,	// No halo
		m_frameStart,
		(int)m_flFrameRate,
		m_life,
		m_boltWidth,
		m_boltWidth,	// End width
		0,				// No fade
		m_noiseAmplitude,
		m_clrRender->r,
		m_clrRender->g,
		m_clrRender->b,
		m_clrRender->a,
		m_speed );

	DoSparks( vecSrc, vecDest );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvBeam::RandomArea( void )
{
	int iLoops = 0;

	for (iLoops = 0; iLoops < 10; iLoops++)
	{
		Vector vecSrc = GetAbsOrigin();

		Vector vecDir1 = Vector( random_valve->RandomFloat( -1.0, 1.0 ), random_valve->RandomFloat( -1.0, 1.0 ),random_valve->RandomFloat( -1.0, 1.0 ) );
		VectorNormalize( vecDir1 );
		trace_t	tr1;
		UTIL_TraceLine( vecSrc, vecSrc + vecDir1 * m_radius, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr1 );

		if (tr1.fraction == 1.0)
			continue;

		Vector vecDir2;
		do {
			vecDir2 = Vector( random_valve->RandomFloat( -1.0, 1.0 ), random_valve->RandomFloat( -1.0, 1.0 ),random_valve->RandomFloat( -1.0, 1.0 ) );
		} while (DotProduct(vecDir1, vecDir2 ) > 0);
		VectorNormalize( vecDir2 );
		trace_t	tr2;
		UTIL_TraceLine( vecSrc, vecSrc + vecDir2 * m_radius, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr2 );

		if (tr2.fraction == 1.0)
			continue;

		if ((tr1.endpos - tr2.endpos).Length() < m_radius * 0.1)
			continue;

		UTIL_TraceLine( tr1.endpos, tr2.endpos, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr2 );

		if (tr2.fraction != 1.0)
			continue;

 		Zap( tr1.endpos, tr2.endpos );

		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : vecSrc - 
//-----------------------------------------------------------------------------
void CEnvBeam::RandomPoint( const Vector &vecSrc )
{
	int iLoops = 0;

	for (iLoops = 0; iLoops < 10; iLoops++)
	{
		Vector vecDir1 = Vector( random_valve->RandomFloat( -1.0, 1.0 ), random_valve->RandomFloat( -1.0, 1.0 ),random_valve->RandomFloat( -1.0, 1.0 ) );
		VectorNormalize( vecDir1 );
		trace_t	tr1;
		UTIL_TraceLine( vecSrc, vecSrc + vecDir1 * m_radius, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr1 );

		if ((tr1.endpos - vecSrc).Length() < m_radius * 0.1)
			continue;

		if (tr1.fraction == 1.0)
			continue;

		Zap( vecSrc, tr1.endpos );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvBeam::BeamUpdateVars( void )
{
	CBaseEntity *pStart = gEntList.FindEntityByName( nullptr, m_iszStartEntity );
	CBaseEntity *pEnd = gEntList.FindEntityByName( nullptr, m_iszEndEntity );

	// if the end entity is missing, we use the Hammer-specified vector offset instead.
	bool bEndPointFromEntity = pEnd != nullptr;

	if (( pStart == nullptr ) || ( !bEndPointFromEntity && !HasEndPointHandle() ))
	{
		return;
	}

	Vector vEndPointPos;
	if ( bEndPointFromEntity )
	{
		vEndPointPos = pEnd->GetAbsOrigin();
	}
	else
	{
		EntityToWorldSpace( m_vEndPointRelative, &vEndPointPos );
	}

	m_nNumBeamEnts = 2;

	m_speed = clamp( m_speed, 0, MAX_BEAM_SCROLLSPEED );

	// NOTE: If the end entity is the beam itself (and the start entity
	// isn't *also* the beam itself, we've got problems. This is a problem
	// because SetAbsStartPos actually sets the entity's origin.
	if ( ( pEnd == this ) && ( pStart != this ) )
	{
		DevMsg("env_beams cannot have the end entity be the beam itself\n"
			"unless the start entity is also the beam itself!\n" );
		contract_assert( 0 );
	}

	SetModelName( m_iszSpriteName );
	SetTexture( m_spriteTexture );

	SetType( BEAM_ENTPOINT );

	if ( IsStaticPointEntity( pStart ) )
	{
		SetAbsStartPos( pStart->GetAbsOrigin() );
	}
	else
	{
		SetStartEntity( pStart );
	}

	if ( !bEndPointFromEntity || IsStaticPointEntity( pEnd ) )
	{
		SetAbsEndPos( vEndPointPos );
	}
	else
	{
		SetEndEntity( pEnd );
	}

	RelinkBeam();

	SetWidth( std::min(MAX_BEAM_WIDTH, m_boltWidth) );
	SetNoise( std::min<std::common_type_t<int, float>>(MAX_BEAM_NOISEAMPLITUDE, m_noiseAmplitude) );
	SetFrame( m_frameStart );
	SetScrollRate( m_speed );
	if ( m_spawnflags & SF_BEAM_SHADEIN )
	{
		SetBeamFlags( FBEAM_SHADEIN );
	}
	else if ( m_spawnflags & SF_BEAM_SHADEOUT )
	{
		SetBeamFlags( FBEAM_SHADEOUT );
	}
}

class CBasePlayer;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Implements visual effects entities: sprites, beams, bubbles, etc.

class CEnvFade : public CLogicalEntity
{
private:

	[[= ks::reflect::Key{ .name = "duration" } ]] float m_Duration;
	[[= ks::reflect::Key{ .name = "holdtime" } ]] float m_HoldTime;

	[[= ks::reflect::Key{ .name = "OnBeginFade" } ]] COutputEvent m_OnBeginFade;

	DECLARE_DATADESC();

	float m_flFadeStartTime;
	float m_flReverseFadeStartTime;
	[[= ks::reflect::Key{ .name = "ReverseFadeDuration" } ]] float m_flReverseFadeDuration;

public:
	DECLARE_CLASS( CEnvFade, CLogicalEntity );

	CEnvFade();

	void Spawn_Impl( void );

	inline float Duration( void ) { return m_Duration; }
	inline float HoldTime( void ) { return m_HoldTime; }

	inline void SetDuration( float duration ) { m_Duration = duration; }
	inline void SetHoldTime( float hold ) { m_HoldTime = hold; }

	int DrawDebugTextOverlays_Impl(void);

	// Inputs
	[[= ks::reflect::Input{ .name = "Fade", .type = FIELD_VOID } ]] void InputFade( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "FadeReverse", .type = FIELD_VOID } ]] void InputReverseFade( inputdata_t &inputdata );
};

LINK_ENTITY_TO_CLASS( env_fade, CEnvFade );

IMPLEMENT_REFLECT_DATAMAP( CEnvFade )

static constexpr auto SF_FADE_IN = 0x0001; // Fade in, not out
static constexpr auto SF_FADE_MODULATE = 0x0002; // Modulate, don't blend
static constexpr auto SF_FADE_ONLYONE = 0x0004;
static constexpr auto SF_FADE_STAYOUT = 0x0008;

CEnvFade::CEnvFade()
		: m_flFadeStartTime(0.0f),
		  m_flReverseFadeStartTime(0.0f)
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvFade::Spawn_Impl( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that does the screen fade.
//-----------------------------------------------------------------------------
void CEnvFade::InputFade( inputdata_t &inputdata )
{
	int fadeFlags = 0;

	if ( m_spawnflags & SF_FADE_IN )
	{
		fadeFlags |= FFADE_IN;
	}
	else
	{
		fadeFlags |= FFADE_OUT;
	}

	if ( m_spawnflags & SF_FADE_MODULATE )
	{
		fadeFlags |= FFADE_MODULATE;
	}

	if ( m_spawnflags & SF_FADE_STAYOUT )
	{
		fadeFlags |= FFADE_STAYOUT;
	}

	//color32 fadeColor = m_clrRender;

	//if( m_flReverseFadeStartTime != 0.0f )
	//{
	//	float flCurrentFadeTime = gpGlobals->curtime - m_flReverseFadeStartTime;

	//	//Change the fade alpha to match the alpha of the current fade to prevent a pop
	//	if( flCurrentFadeTime < m_Duration )
	//	{
	//		fadeColor.a = m_clrRender.GetA() * ( flCurrentFadeTime )/ m_flReverseFadeDuration;
	//	}
	//}

	if ( m_spawnflags & SF_FADE_ONLYONE )
	{
		if ( inputdata.pActivator->IsNetClient() )
		{
			UTIL_ScreenFade( inputdata.pActivator, m_clrRender, Duration(), HoldTime(), fadeFlags );
		}
	}
	else
	{
		UTIL_ScreenFadeAll( m_clrRender, Duration(), HoldTime(), fadeFlags|FFADE_PURGE );
	}

	m_flFadeStartTime = gpGlobals->curtime;

	m_OnBeginFade.FireOutput( inputdata.pActivator, this );
}

void CEnvFade::InputReverseFade( inputdata_t &inputdata )
{
	int fadeFlags = 0;

	if ( m_spawnflags & SF_FADE_IN )
	{
		fadeFlags |= FFADE_OUT;
	}
	else
	{
		fadeFlags |= FFADE_IN;
	}

	if ( m_spawnflags & SF_FADE_MODULATE )
	{
		fadeFlags |= FFADE_MODULATE;
	}

	if ( m_spawnflags & SF_FADE_STAYOUT )
	{
		fadeFlags |= FFADE_STAYOUT;
	}

	color32 fadeColor = m_clrRender;

	if( m_flFadeStartTime != 0.0f )
	{
		float flCurrentFadeTime = gpGlobals->curtime - m_flFadeStartTime;

		//Change the fade alpha to match the alpha of the current fade to prevent a pop
		if( flCurrentFadeTime < m_Duration )
		{
			fadeColor.a = m_clrRender.GetA() * ( flCurrentFadeTime )/ m_Duration;
		}
	}

	if ( m_spawnflags & SF_FADE_ONLYONE )
	{
		if ( inputdata.pActivator->IsNetClient() )
		{
			UTIL_ScreenFade( inputdata.pActivator, fadeColor, m_flReverseFadeDuration, HoldTime(), fadeFlags );
		}
	}
	else
	{
		UTIL_ScreenFadeAll( fadeColor, m_flReverseFadeDuration, HoldTime(), fadeFlags|FFADE_PURGE );
	}

	m_flReverseFadeStartTime = gpGlobals->curtime;

	m_OnBeginFade.FireOutput( inputdata.pActivator, this );
}

//-----------------------------------------------------------------------------
// Purpose: Fetches the arguments from the command line for the fadein and fadeout
//			console commands.
// Input  : flTime - Returns the fade time in seconds (the time to fade in or out)
//			clrFade - Returns the color to fade to or from.
//-----------------------------------------------------------------------------
static void GetFadeParms( const CCommand &args, float &flTime, color32 &clrFade)
{
	flTime = 2.0f;

	if ( args.ArgC() > 1 )
	{
		flTime = std::atof( args[1] );
	}
	
	clrFade.r = 0;
	clrFade.g = 0;
	clrFade.b = 0;
	clrFade.a = 255;

	if ( args.ArgC() > 4 )
	{
		clrFade.r = std::atoi( args[2] );
		clrFade.g = atoi( args[3] );
		clrFade.b = atoi( args[4] );

		if ( args.ArgC() == 5 )
		{
			clrFade.a = atoi( args[5] );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Console command to fade out to a given color.
//-----------------------------------------------------------------------------
static void CC_FadeOut( const CCommand &args )
{
	float flTime;
	color32 clrFade;
	GetFadeParms( args, flTime, clrFade );

	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	UTIL_ScreenFade( pPlayer, clrFade, flTime, 0, FFADE_OUT | FFADE_PURGE | FFADE_STAYOUT );
}
static ConCommand fadeout(side_registry(), "fadeout", CC_FadeOut, "fadeout {time r g b}: Fades the screen to black or to the specified color over the given number of seconds.", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Purpose: Console command to fade in from a given color.
//-----------------------------------------------------------------------------
static void CC_FadeIn( const CCommand &args )
{
	float flTime;
	color32 clrFade;
	GetFadeParms( args, flTime, clrFade );

	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	UTIL_ScreenFade( pPlayer, clrFade, flTime, 0, FFADE_IN | FFADE_PURGE );
}

static ConCommand fadein(side_registry(), "fadein", CC_FadeIn, "fadein {time r g b}: Fades the screen in from black or from the specified color over the given number of seconds.", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CEnvFade::DrawDebugTextOverlays_Impl( void ) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		// print duration
		V_snprintf(tempstr,sizeof(tempstr),"    duration: %f", m_Duration);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		// print hold time
		V_snprintf(tempstr,sizeof(tempstr),"    hold time: %f", m_HoldTime);
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}

class CBaseEntity;
class CBasePlayer;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Implements visual effects entities: sprites, beams, bubbles, etc.

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEnvHudHint : public CPointEntity
{
public:
	DECLARE_CLASS( CEnvHudHint, CPointEntity );

	void	Spawn_Impl( void );
	void	Precache_Impl( void );

private:
	[[= ks::reflect::Input{ .name = "ShowHudHint", .type = FIELD_VOID } ]] void InputShowHudHint( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "HideHudHint", .type = FIELD_VOID } ]] void InputHideHudHint( inputdata_t &inputdata );
	[[= ks::reflect::Key{ .name = "message" } ]] string_t m_iszMessage;
	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( env_hudhint, CEnvHudHint );

IMPLEMENT_REFLECT_DATAMAP( CEnvHudHint )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvHudHint::Spawn_Impl( void )
{
	Precache();

	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvHudHint::Precache_Impl( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for showing the message and/or playing the sound.
//-----------------------------------------------------------------------------
void CEnvHudHint::InputShowHudHint( inputdata_t &inputdata )
{
	CBaseEntity *pPlayer = nullptr;

	if ( inputdata.pActivator && inputdata.pActivator->IsPlayer() )
	{
		pPlayer = inputdata.pActivator;
	}
	else
	{
		pPlayer = UTIL_GetLocalPlayer();
	}

	if ( pPlayer )
	{
		if ( !pPlayer || !pPlayer->IsNetClient() )
			return;

		CSingleUserRecipientFilter user( (CBasePlayer *)pPlayer );
		user.MakeReliable();

		ks::net::CCSUsrMsg_KeyHintText msg;
		msg.hints.emplace_back( STRING(m_iszMessage) );
		SendUserMessage( user, ks::net::CS_UM_KeyHintText, msg );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvHudHint::InputHideHudHint( inputdata_t &inputdata )
{
	CBaseEntity *pPlayer = nullptr;

	if ( inputdata.pActivator && inputdata.pActivator->IsPlayer() )
	{
		pPlayer = inputdata.pActivator;
	}
	else
	{
		pPlayer = UTIL_GetLocalPlayer();
	}

	if ( pPlayer )
	{
		if ( !pPlayer || !pPlayer->IsNetClient() )
			return;

		CSingleUserRecipientFilter user( (CBasePlayer *)pPlayer );
		user.MakeReliable();

		ks::net::CCSUsrMsg_KeyHintText msg;
		msg.hints.emplace_back( STRING(NULL_STRING) );
		SendUserMessage( user, ks::net::CS_UM_KeyHintText, msg );
	}
}

class CBaseEntity;
class CSprite;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: A special kind of beam effect that traces from its start position to
//			its end position and stops if it hits anything.

LINK_ENTITY_TO_CLASS( env_laser, CEnvLaser );

IMPLEMENT_REFLECT_DATAMAP( CEnvLaser )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::Spawn_Impl( void )
{
	if ( !GetModelName() )
	{
		SetThink( &CEnvLaser::SUB_Remove );
		return;
	}

	SetSolid( SOLID_NONE );							// Remove model & collisions
	SetThink( &CEnvLaser::StrikeThink );

	SetEndWidth( GetWidth() );				// Note: EndWidth is not scaled

	PointsInit( GetLocalOrigin(), GetLocalOrigin() );

	Precache( );

	if ( !m_pSprite && m_iszSpriteName != NULL_STRING )
	{
		m_pSprite = CSprite::SpriteCreate( STRING(m_iszSpriteName), GetAbsOrigin(), true );
	}
	else
	{
		m_pSprite = nullptr;
	}

	if ( m_pSprite )
	{
		m_pSprite->SetParent( GetMoveParent() );
		m_pSprite->SetTransparency( kRenderGlow, m_clrRender->r, m_clrRender->g, m_clrRender->b, m_clrRender->a, m_nRenderFX );
	}

	if ( GetEntityName() != NULL_STRING && !(m_spawnflags & SF_BEAM_STARTON) )
	{
		TurnOff();
	}
	else
	{
		TurnOn();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::Precache_Impl( void )
{
	SetModelIndex( PrecacheModel( STRING( GetModelName() ) ) );
	if ( m_iszSpriteName != NULL_STRING )
		PrecacheModel( STRING(m_iszSpriteName) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEnvLaser::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "width"))
	{
		SetWidth( std::atof(szValue) );
	}
	else if (FStrEq(szKeyName, "NoiseAmplitude"))
	{
		SetNoise( std::atoi(szValue) );
	}
	else if (FStrEq(szKeyName, "TextureScroll"))
	{
		SetScrollRate( std::atoi(szValue) );
	}
	else if (FStrEq(szKeyName, "texture"))
	{
		SetModelName( AllocPooledString(szValue) );
	}
	else
	{
		BaseClass::KeyValue_Impl( szKeyName, szValue );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether the laser is currently active.
//-----------------------------------------------------------------------------
int CEnvLaser::IsOn( void )
{
	if ( IsEffectActive( EF_NODRAW ) )
		return 0;
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::InputTurnOn( inputdata_t &inputdata )
{
	if (!IsOn())
	{
		TurnOn();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::InputTurnOff( inputdata_t &inputdata )
{
	if (IsOn())
	{
		TurnOff();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::InputToggle( inputdata_t &inputdata )
{
	if ( IsOn() )
	{
		TurnOff();
	}
	else
	{
		TurnOn();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::TurnOff( void )
{
	AddEffects( EF_NODRAW );
	if ( m_pSprite )
		m_pSprite->TurnOff();

	SetNextThink( TICK_NEVER_THINK );
	SetThink( nullptr );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::TurnOn( void )
{
	RemoveEffects( EF_NODRAW );
	if ( m_pSprite )
		m_pSprite->TurnOn();

	m_flFireTime = gpGlobals->curtime;

	SetThink( &CEnvLaser::StrikeThink );

	//
	// Call StrikeThink here to update the end position, otherwise we will see
	// the beam in the wrong place for one frame since we cleared the nodraw flag.
	//
	StrikeThink();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::FireAtPoint( trace_t &tr )
{
	SetAbsEndPos( tr.endpos );
	if ( m_pSprite )
	{
		UTIL_SetOrigin( m_pSprite, tr.endpos );
	}

	// Apply damage and do sparks every 1/10th of a second.
	if ( gpGlobals->curtime >= m_flFireTime + 0.1 )
	{
		BeamDamage( &tr );
		DoSparks( GetAbsStartPos(), tr.endpos );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvLaser::StrikeThink( void )
{
	CBaseEntity *pEnd = RandomTargetname( STRING( m_iszLaserTarget ) );

	Vector vecFireAt = GetAbsEndPos();
	if ( pEnd )
	{
		vecFireAt = pEnd->GetAbsOrigin();
	}

	trace_t tr;

	UTIL_TraceLine( GetAbsOrigin(), vecFireAt, MASK_SOLID, nullptr, COLLISION_GROUP_NONE, &tr );
	FireAtPoint( tr );
	SetNextThink( gpGlobals->curtime );
}

class CBaseEntity;
class CRecipientFilter;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Implements visual effects entities: sprites, beams, bubbles, etc.

LINK_ENTITY_TO_CLASS( env_message, CMessage );

IMPLEMENT_REFLECT_DATAMAP( CMessage )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMessage::Spawn_Impl( void )
{
	Precache();

	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );

	switch( m_MessageAttenuation )
	{
	case 1: // Medium radius
		m_Radius = ATTN_STATIC;
		break;
	
	case 2:	// Large radius
		m_Radius = ATTN_NORM;
		break;

	case 3:	//EVERYWHERE
		m_Radius = ATTN_NONE;
		break;
	
	default:
	case 0: // Small radius
		m_Radius = SNDLVL_IDLE;
		break;
	}
	m_MessageAttenuation = 0;

	// Remap volume from [0,10] to [0,1].
	m_MessageVolume *= 0.1;

	// No volume, use normal
	if ( m_MessageVolume <= 0 )
	{
		m_MessageVolume = 1.0;
	}
}

void CMessage::Precache_Impl( void )
{
	if ( m_sNoise != NULL_STRING )
	{
		PrecacheScriptSound( STRING(m_sNoise) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for showing the message and/or playing the sound.
//-----------------------------------------------------------------------------
void CMessage::InputShowMessage( inputdata_t &inputdata )
{
	CBaseEntity *pPlayer = nullptr;

	if ( m_spawnflags & SF_MESSAGE_ALL )
	{
		UTIL_ClientPrintAll( HUD_PRINTCENTER, STRING( m_iszMessage ) );
	}
	else
	{
		if ( inputdata.pActivator && inputdata.pActivator->IsPlayer() )
		{
			pPlayer = inputdata.pActivator;
		}
		else
		{
			pPlayer = (gpGlobals->maxClients > 1) ? nullptr : UTIL_GetLocalPlayer();
		}

		if ( pPlayer && pPlayer->IsPlayer() )
		{
			CRecipientFilter filterPlayer;
			filterPlayer.MakeReliable();
			filterPlayer.AddRecipient( ToBasePlayer( pPlayer ) );
			UTIL_ClientPrintFilter( filterPlayer, HUD_PRINTCENTER, STRING( m_iszMessage ) );
			filterPlayer.RemoveAllRecipients();
		}
	}

	if ( m_sNoise != NULL_STRING )
	{
		CPASAttenuationFilter filter( this );
		
		EmitSound_t ep;
		ep.m_nChannel = CHAN_BODY;
		ep.m_pSoundName = (char*)STRING(m_sNoise);
		ep.m_flVolume = m_MessageVolume;
		ep.m_SoundLevel = ATTN_TO_SNDLVL( m_Radius );

		EmitSound( filter, entindex(), ep );
	}

	if ( m_spawnflags & SF_MESSAGE_ONCE )
	{
		UTIL_Remove( this );
	}

	m_OnShowMessage.FireOutput( inputdata.pActivator, this );
}

void CMessage::Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	inputdata_t inputdata;

	inputdata.pActivator	= nullptr;
	inputdata.pCaller		= nullptr;

	InputShowMessage( inputdata );
}

class CCredits : public CPointEntity
{
public:
	DECLARE_CLASS( CMessage, CPointEntity );
	DECLARE_DATADESC();

	void	Spawn_Impl( void );
	[[= ks::reflect::Input{ .name = "RollCredits", .type = FIELD_VOID } ]] void	InputRollCredits( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "RollOutroCredits", .type = FIELD_VOID } ]] void	InputRollOutroCredits( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ShowLogo", .type = FIELD_VOID } ]] void	InputShowLogo( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetLogoLength", .type = FIELD_FLOAT } ]] void	InputSetLogoLength( inputdata_t &inputdata );

	[[= ks::reflect::Key{ .name = "OnCreditsDone" } ]] COutputEvent m_OnCreditsDone;

	void OnRestore_Impl();
private:

	void		RollOutroCredits();

	bool		m_bRolledOutroCredits;
	float		m_flLogoLength;
};

LINK_ENTITY_TO_CLASS( env_credits, CCredits );

IMPLEMENT_REFLECT_DATAMAP( CCredits )

void CCredits::Spawn_Impl( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
}

static void CreditsDone_f( void )
{
	CCredits *pCredits = (CCredits*)gEntList.FindEntityByClassname( nullptr, "env_credits" );

	if ( pCredits )
	{
		pCredits->m_OnCreditsDone.FireOutput( pCredits, pCredits );
	}
}

static ConCommand creditsdone(side_registry(), "creditsdone", CreditsDone_f );

extern ConVar sv_unlockedchapters;

void CCredits::OnRestore_Impl()
{
	BaseClass::OnRestore_Impl();

	if ( m_bRolledOutroCredits )
	{
		// Roll them again so that the client .dll will send the "creditsdone" message and we'll
		//  actually get back to the main menu
		RollOutroCredits();
	}
}

void CCredits::RollOutroCredits()
{
	sv_unlockedchapters.SetValue( "15" );

	// Gurjeets - Not used in CSGO
	//CBasePlayer *pPlayer = UTIL_GetLocalPlayer();

	//CSingleUserRecipientFilter user( pPlayer );
	//user.MakeReliable();

	//UserMessageBegin( user, "CreditsMsg" );
	//	WRITE_BYTE( 3 );
	//MessageEnd();
}

void CCredits::InputRollOutroCredits( inputdata_t &inputdata )
{
	RollOutroCredits();

	// In case we save restore
	m_bRolledOutroCredits = true;

	gamestats->Event_Credits();
}

void CCredits::InputShowLogo( inputdata_t &inputdata )
{
	ASSERT(0);

	// Gurjeets - Not used in CSGO
	//CBasePlayer *pPlayer = UTIL_GetLocalPlayer();

	//CSingleUserRecipientFilter user( pPlayer );
	//user.MakeReliable();

	//if ( m_flLogoLength )
	//{
	//	UserMessageBegin( user, "LogoTimeMsg" );
	//		WRITE_FLOAT( m_flLogoLength );
	//	MessageEnd();
	//}
	//else
	//{
	//	UserMessageBegin( user, "CreditsMsg" );
	//		WRITE_BYTE( 1 );
	//	MessageEnd();
	//}
}

void CCredits::InputSetLogoLength( inputdata_t &inputdata )
{
	m_flLogoLength = inputdata.value.Float();
}

void CCredits::InputRollCredits( inputdata_t &inputdata )
{
	// Gurjeets - Not used in CSGO
	//CBasePlayer *pPlayer = UTIL_GetLocalPlayer();

	//CSingleUserRecipientFilter user( pPlayer );
	//user.MakeReliable();

	//UserMessageBegin( user, "CreditsMsg" );
	//	WRITE_BYTE( 2 );
	//MessageEnd();
}

//-----------------------------------------------------------------------------
// Purpose: Play the outtro stats at the end of the campaign
//-----------------------------------------------------------------------------
class COuttroStats : public CPointEntity
{
public:
	DECLARE_CLASS( COuttroStats, CPointEntity );
	DECLARE_DATADESC();

	void Spawn_Impl( void );
	[[= ks::reflect::Input{ .name = "RollCredits", .type = FIELD_VOID } ]] void InputRollCredits( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "RollStatsCrawl", .type = FIELD_VOID } ]] void InputRollStatsCrawl( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SkipStateChanged", .type = FIELD_VOID } ]] void InputSkipStateChanged( inputdata_t &inputdata );

	void SkipThink( void );
	void CalcSkipState( int &skippingPlayers, int &totalPlayers );

	[[= ks::reflect::Key{ .name = "OnOuttroStatsDone" } ]] COutputEvent m_OnOuttroStatsDone;
};

//-----------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( env_outtro_stats, COuttroStats );

IMPLEMENT_REFLECT_DATAMAP( COuttroStats )

//-----------------------------------------------------------------------------
void COuttroStats::Spawn_Impl( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
}

//-----------------------------------------------------------------------------
void COuttroStats::InputRollStatsCrawl( inputdata_t &inputdata )
{
	// Gurjeets - Not used in CSGO
	//CReliableBroadcastRecipientFilter players;
	//UserMessageBegin( players, "StatsCrawlMsg" );
	//MessageEnd();

	SetThink( &COuttroStats::SkipThink );
	SetNextThink( gpGlobals->curtime + 1.0 );
}

//-----------------------------------------------------------------------------
void COuttroStats::InputRollCredits( inputdata_t &inputdata )
{
	ASSERT(0);

	// Gurjeets - Not used in CSGO

	//CReliableBroadcastRecipientFilter players;
	//UserMessageBegin( players, "creditsMsg" );
	//MessageEnd();
}

void COuttroStats::SkipThink( void )
{
	// if all valid players are skipping, then end
	int iNumSkips = 0;
	int iNumPlayers = 0;
	CalcSkipState( iNumSkips, iNumPlayers );

	if ( iNumSkips >= iNumPlayers )
	{
//		TheDirector->StartScenarioExit();
	}
	else
		SetNextThink( gpGlobals->curtime + 1.0 );
}

void COuttroStats::CalcSkipState( int &skippingPlayers, int &totalPlayers )
{
	// calc skip state
	skippingPlayers = 0;
	totalPlayers = 0;
}

void COuttroStats::InputSkipStateChanged( inputdata_t &inputdata )
{
	int iNumSkips = 0;
	int iNumPlayers = 0;
	CalcSkipState( iNumSkips, iNumPlayers );

	DevMsg( "COuttroStats: Skip state changed. %d players, %d skips\n", iNumPlayers, iNumSkips );
	
	// Gurjeets - Not used in CSGO
	//// Don't send to players in singleplayer
	//if ( iNumPlayers > 1 )
	//{
	//	CReliableBroadcastRecipientFilter players;
	//	UserMessageBegin( players, "StatsSkipState" );
	//		WRITE_BYTE( iNumSkips );
	//		WRITE_BYTE( iNumPlayers );
	//	MessageEnd();
	//}
}

void CC_Test_Outtro_Stats( const CCommand& args )
{
	CBaseEntity *pOuttro = gEntList.FindEntityByClassname( nullptr, "env_outtro_stats" );
	if ( pOuttro )
	{
		variant_t emptyVariant;
		pOuttro->AcceptInput( "RollStatsCrawl", nullptr, nullptr, emptyVariant, 0 );
	}
}
static ConCommand test_outtro_stats(side_registry(), "test_outtro_stats", CC_Test_Outtro_Stats, 0, FCVAR_CHEAT);

class CBaseEntity;
class CBasePlayer;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: Implements a screen shake effect that can also shake physics objects.
// NOTE: UTIL_ScreenShake() will only shake players who are on the ground

class CPhysicsShake : public IMotionEvent
{
	DECLARE_SIMPLE_DATADESC();

public:
	virtual simresult_e	Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular )
	{
		Vector contact;
		if ( !pObject->GetContactPoint( &contact, nullptr ) )
			return SIM_NOTHING;

		// fudge the force a bit to make it more dramatic
		pObject->CalculateForceOffset( m_force * (1.0f + pObject->GetMass()*0.4f), contact, &linear, &angular );

		return SIM_LOCAL_FORCE;
	}

	Vector  m_force;
};

IMPLEMENT_REFLECT_DATAMAP_SIMPLE( CPhysicsShake )

class CEnvShake : public CPointEntity
{
private:
	[[= ks::reflect::Key{ .name = "amplitude" } ]] float m_Amplitude;
	[[= ks::reflect::Key{ .name = "frequency" } ]] float m_Frequency;
	[[= ks::reflect::Key{ .name = "duration" } ]] float m_Duration;
	[[= ks::reflect::Key{ .name = "radius" } ]] float m_Radius;			// radius of 0 means all players
	float m_stopTime;
	float m_nextShake;
	float m_currentAmp;

	Vector	m_maxForce;

	IPhysicsMotionController	*m_pShakeController;
	CPhysicsShake				m_shakeCallback;

	DECLARE_DATADESC();

public:
	DECLARE_CLASS( CEnvShake, CPointEntity );

	virtual ~CEnvShake( void );
	void	Precache_Impl( void );
	void	Spawn_Impl( void );
	void	OnRestore_Impl( void );

	inline	float	Amplitude( void ) { return m_Amplitude; }
	inline	float	Frequency( void ) { return m_Frequency; }
	inline	float	Duration( void ) { return m_Duration; }
	float			Radius( bool bPlayers = true );
	inline	void	SetAmplitude( float amplitude ) { m_Amplitude = amplitude; }
	inline	void	SetFrequency( float frequency ) { m_Frequency = frequency; }
	inline	void	SetDuration( float duration ) { m_Duration = duration; }
	inline	void	SetRadius( float radius ) { m_Radius = radius; }

	int DrawDebugTextOverlays_Impl(void);

	// Input handlers
	[[= ks::reflect::Input{ .name = "StartShake", .type = FIELD_VOID } ]] void InputStartShake( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "StopShake", .type = FIELD_VOID } ]] void InputStopShake( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Amplitude", .type = FIELD_FLOAT } ]] void InputAmplitude( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Frequency", .type = FIELD_FLOAT } ]] void InputFrequency( inputdata_t &inputdata );

	// Causes the camera/physics shakes to happen:
	void ApplyShake( ShakeCommand_t command ); 
	void Think_Impl( void );
};

LINK_ENTITY_TO_CLASS( env_shake, CEnvShake );

IMPLEMENT_REFLECT_DATAMAP( CEnvShake )

static constexpr auto SF_SHAKE_EVERYONE = 0x0001; // Don't check radius
static constexpr auto SF_SHAKE_INAIR = 0x0004; // Shake players in air
static constexpr auto SF_SHAKE_PHYSICS = 0x0008; // Shake physically (not just camera)
static constexpr auto SF_SHAKE_ROPES = 0x0010; // Shake ropes too.
static constexpr auto SF_SHAKE_NO_VIEW = 0x0020; // DON'T shake the view (only ropes and/or physics objects)
static constexpr auto SF_SHAKE_NO_RUMBLE = 0x0040; // DON'T Rumble the XBox Controller
static constexpr auto SF_TILT_EASE_INOUT = 0x0080; // Ease in and out of the tilt

//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CEnvShake::~CEnvShake( void )
{
	if ( m_pShakeController )
	{
		physenv->DestroyMotionController( m_pShakeController );
	}
}

float CEnvShake::Radius(bool bPlayers)
{
	// The radius for players is zero if SF_SHAKE_EVERYONE is set
	if ( bPlayers && HasSpawnFlags(SF_SHAKE_EVERYONE))
		return 0;
	return m_Radius;
}

//-----------------------------------------------------------------------------
// Purpose: Sets default member values when spawning.
//-----------------------------------------------------------------------------
void CEnvShake::Precache_Impl()
{
	BaseClass::Precache_Impl();
	CRopeKeyframe::PrecacheShakeRopes();
}

void CEnvShake::Spawn_Impl( void )
{
	Precache();
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
	
	if ( GetSpawnFlags() & SF_SHAKE_EVERYONE )
	{
		m_Radius = 0;
	}
	
	if ( HasSpawnFlags( SF_SHAKE_NO_VIEW ) && !HasSpawnFlags( SF_SHAKE_PHYSICS ) && !HasSpawnFlags( SF_SHAKE_ROPES ) )
	{
		DevWarning( "env_shake %s with \"Don't shake view\" spawnflag set without \"Shake physics\" or \"Shake ropes\" spawnflags set.", GetDebugName() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Restore the motion controller
//-----------------------------------------------------------------------------
void CEnvShake::OnRestore_Impl( void )
{
	BaseClass::OnRestore_Impl();

	if ( m_pShakeController )
	{
		m_pShakeController->SetEventHandler( &m_shakeCallback );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvShake::ApplyShake( ShakeCommand_t command )
{
	if ( !HasSpawnFlags( SF_SHAKE_NO_VIEW ) || !HasSpawnFlags( SF_SHAKE_NO_RUMBLE ) )
	{
		bool air = (GetSpawnFlags() & SF_SHAKE_INAIR) ? true : false;
		UTIL_ScreenShake( GetAbsOrigin(), Amplitude(), Frequency(), Duration(), Radius(), command, air );
	}
		
	if ( GetSpawnFlags() & SF_SHAKE_ROPES )
	{
		CRopeKeyframe::ShakeRopes( GetAbsOrigin(), Radius(false), Frequency() );
	}

	if ( GetSpawnFlags() & SF_SHAKE_PHYSICS )
	{
		if ( !m_pShakeController )
		{
			m_pShakeController = physenv->CreateMotionController( &m_shakeCallback );
		}
		// do physics shake
		switch( command )
		{
		case SHAKE_START:
		case SHAKE_START_NORUMBLE:
		case SHAKE_START_RUMBLEONLY:
			{
				m_stopTime = gpGlobals->curtime + Duration();
				m_nextShake = 0;
				m_pShakeController->ClearObjects();
				SetNextThink( gpGlobals->curtime );
				m_currentAmp = Amplitude();
				CBaseEntity *list[1024];
				float radius = Radius(false);
				
				// probably checked "Shake Everywhere" do a big radius
				if ( !radius )
				{
					radius = 512;
				}
				Vector extents = Vector(radius, radius, radius);
				extents.z = std::max<std::common_type_t<vec_t, int>>(extents.z, 100);
				Vector mins = GetAbsOrigin() - extents;
				Vector maxs = GetAbsOrigin() + extents;
				int count = UTIL_EntitiesInBox( list, 1024, mins, maxs, 0 );

				for ( int i = 0; i < count; i++ )
				{
					//
					// Only shake physics entities that players can see. This is one frame out of date
					// so it's possible that we could miss objects if a player changed PVS this frame.
					//
					if ( ( list[i]->GetMoveType() == MOVETYPE_VPHYSICS ) )
					{
						IPhysicsObject *pPhys = list[i]->VPhysicsGetObject();
						if ( pPhys && pPhys->IsMoveable() )
						{
							m_pShakeController->AttachObject( pPhys, false );
							pPhys->Wake();
						}
					}
				}
			}
			break;
		case SHAKE_STOP:
			m_pShakeController->ClearObjects();
			break;
		case SHAKE_AMPLITUDE:
			m_currentAmp = Amplitude();
		case SHAKE_FREQUENCY:
			m_pShakeController->WakeObjects();
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that starts the screen shake.
//-----------------------------------------------------------------------------
void CEnvShake::InputStartShake( inputdata_t &inputdata )
{
	if ( HasSpawnFlags( SF_SHAKE_NO_RUMBLE ) )
	{
		ApplyShake( SHAKE_START_NORUMBLE );
	}
	else if ( HasSpawnFlags( SF_SHAKE_NO_VIEW ) )
	{
		ApplyShake( SHAKE_START_RUMBLEONLY );
	}
	else
	{
		ApplyShake( SHAKE_START );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that stops the screen shake.
//-----------------------------------------------------------------------------
void CEnvShake::InputStopShake( inputdata_t &inputdata )
{
	ApplyShake( SHAKE_STOP );
}

//-----------------------------------------------------------------------------
// Purpose: Handles changes to the shake amplitude from an external source.
//-----------------------------------------------------------------------------
void CEnvShake::InputAmplitude( inputdata_t &inputdata )
{
	SetAmplitude( inputdata.value.Float() );
	ApplyShake( SHAKE_AMPLITUDE );
}

//-----------------------------------------------------------------------------
// Purpose: Handles changes to the shake frequency from an external source.
//-----------------------------------------------------------------------------
void CEnvShake::InputFrequency( inputdata_t &inputdata )
{
	SetFrequency( inputdata.value.Float() );
	ApplyShake( SHAKE_FREQUENCY );
}

//-----------------------------------------------------------------------------
// Purpose: Calculates the physics shake values
//-----------------------------------------------------------------------------
void CEnvShake::Think_Impl( void )
{
	int i;

	if ( gpGlobals->curtime > m_nextShake )
	{
		// Higher frequency means we recalc the extents more often and perturb the display again
		m_nextShake = gpGlobals->curtime + (1.0f / Frequency());

		// Compute random shake extents (the shake will settle down from this)
		for (i = 0; i < 2; i++ )
		{
			m_maxForce[i] = random_valve->RandomFloat( -1, 1 );
		}
		// make the force it point mostly up
		m_maxForce.z = 4;
		VectorNormalize( m_maxForce );
		m_maxForce *= m_currentAmp * 400;	// amplitude is the acceleration of a 100kg object
	}

	float fraction = ( m_stopTime - gpGlobals->curtime ) / Duration();

	if ( fraction < 0 )
	{
		m_pShakeController->ClearObjects();
		return;
	}

	float freq = 0;
	// Ramp up frequency over duration
	if ( fraction )
	{
		freq = (Frequency() / fraction);
	}

	// square fraction to approach zero more quickly
	fraction *= fraction;

	// Sine wave that slowly settles to zero
	fraction = fraction * std::sin( gpGlobals->curtime * freq );

	// Add to view origin
	for ( i = 0; i < 3; i++ )
	{
		// store the force in the controller callback
		m_shakeCallback.m_force[i] = m_maxForce[i] * fraction;
	}

	// Drop amplitude a bit, less for higher frequency shakes
	m_currentAmp -= m_currentAmp * ( gpGlobals->frametime / (Duration() * Frequency()) );
	SetNextThink( gpGlobals->curtime );
}

//------------------------------------------------------------------------------
// Purpose: Console command to cause a screen shake.
//------------------------------------------------------------------------------
void CC_Shake( void )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if (pPlayer)
	{
		UTIL_ScreenShake( pPlayer->WorldSpaceCenter(), 25.0, 150.0, 1.0, 750, SHAKE_START );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Returns current text offset from the top
//-----------------------------------------------------------------------------
int CEnvShake::DrawDebugTextOverlays_Impl( void ) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];

		// print amplitude
		V_snprintf(tempstr,sizeof(tempstr),"    magnitude: %f", m_Amplitude);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		// print frequency
		V_snprintf(tempstr,sizeof(tempstr),"    frequency: %f", m_Frequency);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		// print duration
		V_snprintf(tempstr,sizeof(tempstr),"    duration: %f", m_Duration);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		// print radius
		V_snprintf(tempstr,sizeof(tempstr),"    radius: %f", m_Radius);
		EntityText(text_offset,tempstr,0);
		text_offset++;

	}
	return text_offset;
}

static ConCommand shake(side_registry(), "shake", CC_Shake, "Shake the screen.", FCVAR_CHEAT );

// Tilt effect

class CEnvTilt : public CPointEntity
{
private:
	[[= ks::reflect::Key{ .name = "duration" } ]] float m_Duration;
	[[= ks::reflect::Key{ .name = "radius" } ]] float m_Radius;			// radius of 0 means all players
	[[= ks::reflect::Key{ .name = "tilttime" } ]] [[= ks::reflect::As{ FIELD_TIME } ]] float m_TiltTime;
	float m_stopTime;

	DECLARE_DATADESC();

public:
	DECLARE_CLASS( CEnvShake, CPointEntity );

	void	Precache_Impl( void );
	void	Spawn_Impl( void );

	QAngle			TiltAngle( void );
	inline	float	Duration( void ) { return m_Duration; }
	float			Radius( bool bPlayers = true );
	inline	float	TiltTime( void ) { return m_TiltTime; }
	inline	void	SetDuration( float duration ) { m_Duration = duration; }
	inline	void	SetRadius( float radius ) { m_Radius = radius; }

	int DrawDebugTextOverlays_Impl(void);

	// Input handlers
	[[= ks::reflect::Input{ .name = "StartTilt", .type = FIELD_VOID } ]] void InputStartTilt( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "StopTilt", .type = FIELD_VOID } ]] void InputStopTilt( inputdata_t &inputdata );

	// Causes the camera/physics shakes to happen:
	void ApplyTilt( ShakeCommand_t command ); 
};

LINK_ENTITY_TO_CLASS( env_tilt, CEnvTilt );

IMPLEMENT_REFLECT_DATAMAP( CEnvTilt )

QAngle CEnvTilt::TiltAngle( void )
{
	QAngle qTiltAngles = GetAbsAngles();
	qTiltAngles.y = 0.0f;	// Tilting around the up axis makes no sense
	return qTiltAngles;
}

float CEnvTilt::Radius(bool bPlayers)
{
	// The radius for players is zero if SF_SHAKE_EVERYONE is set
	if ( bPlayers && HasSpawnFlags(SF_SHAKE_EVERYONE))
		return 0;
	return m_Radius;
}

//-----------------------------------------------------------------------------
// Purpose: Sets default member values when spawning.
//-----------------------------------------------------------------------------
void CEnvTilt::Precache_Impl()
{
	BaseClass::Precache_Impl();
	CRopeKeyframe::PrecacheShakeRopes();
}

void CEnvTilt::Spawn_Impl( void )
{
	Precache();
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );

	if ( GetSpawnFlags() & SF_SHAKE_EVERYONE )
	{
		m_Radius = 0;
	}

	if ( HasSpawnFlags( SF_SHAKE_NO_VIEW ) && !HasSpawnFlags( SF_SHAKE_PHYSICS ) && !HasSpawnFlags( SF_SHAKE_ROPES ) )
	{
		DevWarning( "env_shake %s with \"Don't shake view\" spawnflag set without \"Shake physics\" or \"Shake ropes\" spawnflags set.", GetDebugName() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTilt::ApplyTilt( ShakeCommand_t command )
{
	if ( !HasSpawnFlags( SF_SHAKE_NO_VIEW ) || !HasSpawnFlags( SF_SHAKE_NO_RUMBLE ) )
	{
		UTIL_ScreenTilt( GetAbsOrigin(), TiltAngle(), Duration(), Radius(), TiltTime(), command, (GetSpawnFlags() & SF_TILT_EASE_INOUT) != 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that starts the screen shake.
//-----------------------------------------------------------------------------
void CEnvTilt::InputStartTilt( inputdata_t &inputdata )
{
	if ( HasSpawnFlags( SF_SHAKE_NO_RUMBLE ) )
	{
		ApplyTilt( SHAKE_START_NORUMBLE );
	}
	else if ( HasSpawnFlags( SF_SHAKE_NO_VIEW ) )
	{
		ApplyTilt( SHAKE_START_RUMBLEONLY );
	}
	else
	{
		ApplyTilt( SHAKE_START );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that stops the screen shake.
//-----------------------------------------------------------------------------
void CEnvTilt::InputStopTilt( inputdata_t &inputdata )
{
	ApplyTilt( SHAKE_STOP );
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Returns current text offset from the top
//-----------------------------------------------------------------------------
int CEnvTilt::DrawDebugTextOverlays_Impl( void ) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT)
	{
		char tempstr[512];

		// print duration
		V_snprintf(tempstr,sizeof(tempstr),"    duration: %f", m_Duration);
		EntityText(text_offset,tempstr,0);
		text_offset++;

		// print radius
		V_snprintf(tempstr,sizeof(tempstr),"    radius: %f", m_Radius);
		EntityText(text_offset,tempstr,0);
		text_offset++;

	}
	return text_offset;
}

class CBaseEntity;
struct inputdata_t;

using namespace ::ks::reflect;

// Purpose: A point entity that periodically emits sparks and "bzzt" sounds.

ConVar fx_new_sparks ( side_registry(), "fx_new_sparks", "1", FCVAR_CHEAT, "Use new style sparks.\n" );

//-----------------------------------------------------------------------------
// Purpose: Emits sparks from the given location and plays a random spark sound.
// Input  : pev - 
//			location - 
//-----------------------------------------------------------------------------
void DoSpark( CBaseEntity *ent, const Vector &location, int nMagnitude, int nTrailLength, bool bPlaySound, const Vector &vecDir )
{
	g_pEffects->Sparks( location, nMagnitude, nTrailLength, &vecDir );
}

static const int SF_SPARK_START_ON			= 64;
static const int SF_SPARK_GLOW				= 128;
static const int SF_SPARK_SILENT			= 256;
static const int SF_SPARK_DIRECTIONAL		= 512;

class CEnvSpark : public CPointEntity
{
	DECLARE_CLASS( CEnvSpark, CPointEntity );

public:
	CEnvSpark( void );

	void	Spawn_Impl(void);
	void	Precache_Impl(void);
	void	SparkThink(void);

	// Input handlers
	[[= ks::reflect::Input{ .name = "StartSpark", .type = FIELD_VOID } ]] void InputStartSpark( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "StopSpark", .type = FIELD_VOID } ]] void InputStopSpark( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ToggleSpark", .type = FIELD_VOID } ]] void InputToggleSpark( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SparkOnce", .type = FIELD_VOID } ]] void InputSparkOnce( inputdata_t &inputdata );
	
	DECLARE_DATADESC();

	[[= ks::reflect::Key{ .name = "MaxDelay" } ]] float			m_flDelay;
	int				m_nGlowSpriteIndex;
	[[= ks::reflect::Key{ .name = "Magnitude" } ]] int				m_nMagnitude;
	[[= ks::reflect::Key{ .name = "TrailLength" } ]] int				m_nTrailLength;

	[[= ks::reflect::Key{ .name = "OnSpark" } ]] COutputEvent	m_OnSpark;
};

IMPLEMENT_REFLECT_DATAMAP( CEnvSpark )

LINK_ENTITY_TO_CLASS(env_spark, CEnvSpark);

//-----------------------------------------------------------------------------
// Purpose: Constructor! Exciting, isn't it?
//-----------------------------------------------------------------------------
CEnvSpark::CEnvSpark( void )
{
	m_nMagnitude = 1;
	m_nTrailLength = 1;
}

//-----------------------------------------------------------------------------
// Purpose: Called when spawning, after keyvalues have been handled.
//-----------------------------------------------------------------------------
void CEnvSpark::Spawn_Impl(void)
{
	SetThink( nullptr );
	SetUse( nullptr );

	if (FBitSet(m_spawnflags, SF_SPARK_START_ON))
	{
		SetThink(&CEnvSpark::SparkThink);	// start sparking
	}

	SetNextThink( gpGlobals->curtime + 0.1 + random_valve->RandomFloat( 0, 1.5 ) );

	// Negative delays are not allowed
	if (m_flDelay < 0)
	{
		m_flDelay = 0;
	}

	Precache( );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvSpark::Precache_Impl(void)
{
	// Unlock string tables to avoid an engine warning
	bool oldLock = engine->LockNetworkStringTables( false );
	m_nGlowSpriteIndex = PrecacheModel("sprites/glow01.vmt");
	engine->LockNetworkStringTables( oldLock );

	if ( IsPrecacheAllowed() )
	{
		PrecacheScriptSound( "DoSpark" );
		PrecacheParticleSystem( "env_sparks_omni" );
		PrecacheParticleSystem( "env_sparks_directional" );
	}
}

extern ConVar phys_pushscale;

//-----------------------------------------------------------------------------
// Purpose: Emits sparks at random intervals.
//-----------------------------------------------------------------------------
void CEnvSpark::SparkThink(void)
{
	SetNextThink( gpGlobals->curtime + 0.1 + random_valve->RandomFloat(0, m_flDelay) );

	m_OnSpark.FireOutput( this, this );

	if ( !( m_spawnflags & SF_SPARK_SILENT ) )
	{
		EmitSound( "DoSpark" );
	}

	if ( fx_new_sparks.GetBool() )
	{
		if ( FBitSet( m_spawnflags, SF_SPARK_DIRECTIONAL ) )
			DispatchParticleEffect( "env_sparks_directional", WorldSpaceCenter(), Vector ( m_nMagnitude, m_nTrailLength, FBitSet(m_spawnflags, SF_SPARK_GLOW) ), GetAbsAngles() );
		else
			DispatchParticleEffect( "env_sparks_omni", WorldSpaceCenter(), Vector ( m_nMagnitude, m_nTrailLength, FBitSet(m_spawnflags, SF_SPARK_GLOW) ), GetAbsAngles() );
	}
	else
	{
		Vector vecDir = vec3_origin;
		if ( FBitSet( m_spawnflags, SF_SPARK_DIRECTIONAL ) )
		{
			AngleVectors( GetAbsAngles(), &vecDir );
		}

		DoSpark( this, WorldSpaceCenter(), m_nMagnitude, m_nTrailLength, !( m_spawnflags & SF_SPARK_SILENT ), vecDir );

		if (FBitSet(m_spawnflags, SF_SPARK_GLOW))
		{
			CPVSFilter filter( GetAbsOrigin() );
			te->GlowSprite( filter, 0.0, &GetAbsOrigin(), m_nGlowSpriteIndex, 0.2, 1.5, 25 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for starting the sparks.
//-----------------------------------------------------------------------------
void CEnvSpark::InputStartSpark( inputdata_t &inputdata )
{
	SetThink(&CEnvSpark::SparkThink);
	SetNextThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: Shoot one spark.
//-----------------------------------------------------------------------------
void CEnvSpark::InputSparkOnce( inputdata_t &inputdata )
{
	SparkThink();
	SetNextThink( TICK_NEVER_THINK );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for starting the sparks.
//-----------------------------------------------------------------------------
void CEnvSpark::InputStopSpark( inputdata_t &inputdata )
{
	SetThink(nullptr);
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for toggling the on/off state of the sparks.
//-----------------------------------------------------------------------------
void CEnvSpark::InputToggleSpark( inputdata_t &inputdata )
{
	if (GetNextThink() == TICK_NEVER_THINK)
	{
		InputStartSpark( inputdata );
	}
	else
	{
		InputStopSpark( inputdata );
	}
}
