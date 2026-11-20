// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation
// Copyright (c) 2026-2026 oknyshuk

module;
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <wchar.h>
#include "cdll_int.h"
#include "client_class.h"
#include "reflect_predmap.h"
#include "reflect_recvtable.h"
#include "declare_class.h"
#include "reflect_annotations.h"
#include "bspflags.h"
#include "const.h"
#include "materialsystem/imesh.h"
#include "vphysics_interface.h"
#include "worldsize.h"
#include "bspfile.h"
#include "client_textmessage.h"
#include "localize/ilocalize.h"
#include "edict.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialproxy.h"
#include "materialsystem/imaterialvar.h"
#include "imaterialproxydict.h"
#include "iclientrenderable.h"
#include "materialsystem/itexture.h"
#include "cmodel.h"
#include "collisionutils.h"
#include "engine/ICollideable.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "networksystem/inetworksystem.h"
#include "dt_recv.h"
#include "engine/IEngineTrace.h"
#include "icliententity.h"
#include "mathlib/IceKey.H"
#include "model_types.h"
#include "vphysics/object_hash.h"
#include "dt_send.h"
#include "networkvar.h"
#include "predictable_entity.h"
module ks.client;

import std;
import ks.core.basetypes;
import ks.platform;
import ks.simd;
import ks.tier1.reflect_wirevar;
import ks.tier1.vector_ops;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :sharedvar;
import :util;
import :particlemgr;
import :cstrike15.cs_shareddefs;
import :precache;
import :noopanimcontroller;
import :achievements_and_stats_interface;
import :baseentity;
import :econ.econ_item_schema_minimal;
import ks.game.gamemovement;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.registry;
import ks.tier1.strtools;
import ks.tier1.bitbuf;
import ks.core.dbg;
import ks.game.mapentities_shared;
import ks.game.shareddefs;
import ks.math.compressed_vector;
import ks.math.mathlib;
import ks.math.vector;
import ks.tier1.exprevaluator;
import ks.tier1.interfaces;
import ks.math.vmatrix;
import ks.tier1.keyvalues;
import ks.game.entity_cast;
import ks.math.ssemath;
import ks.core.platform;
import ks.game.in_buttons;
import ks.game.triggers_shared;
import :triggers;
import ks.core.types;
import ks.core.numeric;
import ks.tier1.checksum_md5;
import ks.core.wchartypes;
import ks.tier1.checksum_crc;
import :cstrike15.cs_player;
import ks.core.cmdline;
import ks.core.random;
import ks.core.scratcharena;
import ks.math.vector2d;
import ks.math.vplane;
import ks.math.ssequaternion;

class CBasePlayer;
class CTeam;

using namespace ::ks::reflect;

// Purpose: Client side CTeam class

//-----------------------------------------------------------------------------
// Purpose: RecvProxy that converts the Team's player UtlVector to entindexes
//-----------------------------------------------------------------------------
void RecvProxy_PlayerList(  const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CTeam *pTeam = (CTeam*)pOut;
	pTeam->m_aPlayers[pData->m_iElement] = pData->m_Value.m_Int;
}

void RecvProxyArrayLength_PlayerArray( void *pStruct, int objectID, int currentArrayLength )
{
	CTeam *pTeam = (CTeam*)pStruct;
	
	if ( std::ssize( pTeam->m_aPlayers ) != currentArrayLength )
		pTeam->m_aPlayers.assign( currentArrayLength, {} );
}

IMPLEMENT_REFLECT_CLIENTCLASS( CTeam, DT_Team, CTeam )

IMPLEMENT_REFLECT_PREDMAP( CTeam );

// Global list of client side team entities
std::vector< CTeam * > g_Teams;

//=================================================================================================
// CTeam functionality

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTeam::CTeam()
{
	m_scoreTotal = 0;
	m_scoreFirstHalf = 0;
	m_scoreSecondHalf = 0;	
	m_scoreOvertime = 0;
	m_iClanID = 0;
	
	memset( m_szTeamname, 0, sizeof(m_szTeamname) );
	memset( m_szClanTeamname, 0, sizeof(m_szClanTeamname) );
	memset( m_szTeamFlagImage, 0, sizeof(m_szTeamFlagImage) );
	memset( m_szTeamLogoImage, 0, sizeof(m_szTeamLogoImage) );
	memset( m_szTeamMatchStat, 0, sizeof( m_szTeamMatchStat ) );
	
	m_iDeaths = 0;
	m_iPing = 0;
	m_iPacketloss = 0;
	m_bSurrendered = 0;
	m_numMapVictories = 0;

	// Add myself to the global list of team entities
	g_Teams.push_back( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTeam::~CTeam()
{
	ks::erase_first( g_Teams, this );
}

void CTeam::RemoveAllPlayers()
{
	m_aPlayers.clear();
}

void CTeam::PreDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PreDataUpdate( updateType );
}

//-----------------------------------------------------------------------------
// Gets the ith player on the team (may return NULL) 
//-----------------------------------------------------------------------------
CBasePlayer* CTeam::GetPlayer( int idx )
{
	return (CBasePlayer*)cl_entitylist->GetEnt(m_aPlayers[idx]);
}

int CTeam::GetTeamNumber() const
{
	return m_iTeamNum;
}

//=================================================================================================
// TEAM HANDLING
//=================================================================================================
// Purpose: 
//-----------------------------------------------------------------------------
const char *CTeam::Get_Name( void )
{
	return m_szTeamname;
}

//=================================================================================================
// Purpose: 
//-----------------------------------------------------------------------------
const char *CTeam::Get_ClanName( void )
{
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return "";
	}

	return m_szClanTeamname;
}

//=================================================================================================
// Purpose: 
//-----------------------------------------------------------------------------
const char *CTeam::Get_FlagImageString( void )
{
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return "";
	}

	return m_szTeamFlagImage;
}

//=================================================================================================
// Purpose: 
//-----------------------------------------------------------------------------
const char *CTeam::Get_LogoImageString( void )
{
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return "";
	}

	return m_szTeamLogoImage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTeam::Get_Deaths( void )
{
	return m_iDeaths;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTeam::Get_Ping( void )
{
	return m_iPing;
}

//-----------------------------------------------------------------------------
// Purpose: Return the number of players in this team
//-----------------------------------------------------------------------------
int CTeam::Get_Number_Players( void )
{
	return std::ssize( m_aPlayers );
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the specified player is on this team
//-----------------------------------------------------------------------------
bool CTeam::ContainsPlayer( int iPlayerIndex )
{
	for (int i = 0; i < std::ssize( m_aPlayers ); i++ )
	{
		if ( m_aPlayers[i] == iPlayerIndex )
			return true;
	}

	return false;
}

void CTeam::ClientThink()
{
}

int CTeam::GetGGLeader( int nTeam )
{
	if ( nTeam == TEAM_CT )
		return m_nGGLeaderEntIndex_CT;
	else if ( nTeam == TEAM_TERRORIST )
		return m_nGGLeaderEntIndex_T;

	return -1;
}

//=================================================================================================
// GLOBAL CLIENT TEAM HANDLING
//=================================================================================================
// Purpose: Get the CTeam for the local player
//-----------------------------------------------------------------------------
CTeam *GetLocalTeam( void )
{
	CBasePlayer *player = CBasePlayer::GetLocalPlayer();

	if ( !player )
		return nullptr;
	
	return GetPlayersTeam( player->index );
}

//-----------------------------------------------------------------------------
// Purpose: Get the CTeam for the specified team number
//-----------------------------------------------------------------------------
CTeam *GetGlobalTeam( int iTeamNumber )
{
	for (int i = 0; i < std::ssize( g_Teams ); i++ )
	{
		if ( g_Teams[i]->GetTeamNumber() == iTeamNumber )
			return g_Teams[i];
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the number of teams you can access via GetGlobalTeam() (hence the +1)
//-----------------------------------------------------------------------------
int GetNumTeams()
{
	return std::ssize( g_Teams ) + 1; 
}

//-----------------------------------------------------------------------------
// Purpose: Get the team of the specified player
//-----------------------------------------------------------------------------
CTeam *GetPlayersTeam( int iPlayerIndex )
{
	for (int i = 0; i < std::ssize( g_Teams ); i++ )
	{
		if ( g_Teams[i]->ContainsPlayer( iPlayerIndex ) )
			return g_Teams[i];
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Get the team of the specified player
//-----------------------------------------------------------------------------
CTeam *GetPlayersTeam( CBasePlayer *pPlayer )
{
	return GetPlayersTeam( pPlayer->entindex() );
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the two specified players are on the same team
//-----------------------------------------------------------------------------
bool ArePlayersOnSameTeam( int iPlayerIndex1, int iPlayerIndex2 )
{
	for (int i = 0; i < std::ssize( g_Teams ); i++ )
	{
		if ( g_Teams[i]->ContainsPlayer( iPlayerIndex1 ) && g_Teams[i]->ContainsPlayer( iPlayerIndex2 ) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of team managers
//-----------------------------------------------------------------------------
int GetNumberOfTeams( void )
{
	return std::ssize( g_Teams );
}

using namespace ::ks::reflect;

using namespace ::ks::reflect;

// Purpose: round based game rule convars shared by the multiplayer rules.

ConVar mp_capstyle( side_registry(), "mp_capstyle", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Sets the style of capture points used. 0 = Fixed players required to cap. 1 = More players cap faster, but longer cap times." );
ConVar mp_blockstyle( side_registry(), "mp_blockstyle", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Sets the style of capture point blocking used. 0 = Blocks break captures completely. 1 = Blocks only pause captures." );
ConVar mp_respawnwavetime( side_registry(), "mp_respawnwavetime", "10.0", FCVAR_NOTIFY | FCVAR_REPLICATED, "Time between respawn waves." );
ConVar mp_capdeteriorate_time( side_registry(), "mp_capdeteriorate_time", "90.0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Time it takes for a full capture point to deteriorate." );
ConVar mp_tournament ( side_registry(), "mp_tournament", "0", FCVAR_REPLICATED | FCVAR_NOTIFY );

ConVar mp_teams_unbalance_limit( side_registry(), "mp_teams_unbalance_limit", "1", FCVAR_REPLICATED | FCVAR_NOTIFY,
					 "Teams are unbalanced when one team has this many more players than the other team. (0 disables check)",
					 true, 0,	// min value
					 true, 30	// max value
					 );

ConVar mp_maxrounds( side_registry(), "mp_maxrounds", "0", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE, "max number of rounds to play before server changes maps", true, 0, false, 0 );
ConVar mp_winlimit( side_registry(), "mp_winlimit", "0", FCVAR_REPLICATED | FCVAR_NOTIFY, "Max score one team can reach before server changes maps", true, 0, false, 0 );
ConVar mp_disable_respawn_times( side_registry(), "mp_disable_respawn_times", "0", FCVAR_NOTIFY | FCVAR_REPLICATED );
ConVar mp_bonusroundtime( side_registry(), "mp_bonusroundtime", "15", FCVAR_REPLICATED, "Time after round win until round restarts", true, 5, true, 15 );

// Utility function
bool FindInList( const char **pStrings, const char *pToFind )
{
	int i = 0;
	while ( pStrings[i][0] != 0 )
	{
		if ( V_stricmp( pStrings[i], pToFind ) == 0 )
			return true;
		i++;
	}

	return false;
}

using namespace ::ks::reflect;

IMPLEMENT_REFLECT_CLIENTCLASS( CTesla, DT_Tesla, CTesla )

CTesla::CTesla()
{
}

void CTesla::ReceiveMessage( int classID, bf_read &msg )
{
	CTeslaInfo teslaInfo;

	msg.ReadBitVec3Coord( teslaInfo.m_vPos );
	teslaInfo.m_vAngles.Init();

	teslaInfo.m_nEntIndex = msg.ReadShort();
	teslaInfo.m_flRadius = msg.ReadFloat();

	teslaInfo.m_vColor.x = ((unsigned char)msg.ReadChar()) / 255.0f;
	teslaInfo.m_vColor.y = ((unsigned char)msg.ReadChar()) / 255.0f;
	teslaInfo.m_vColor.z = ((unsigned char)msg.ReadChar()) / 255.0f;
	
	float flAlpha = 0;
	flAlpha = ((unsigned char)msg.ReadChar()) / 255.0f;

	teslaInfo.m_nBeams = msg.ReadChar();
	
	teslaInfo.m_flBeamWidth = msg.ReadFloat();
	teslaInfo.m_flTimeVisible = msg.ReadFloat();
	teslaInfo.m_pszSpriteName = m_iszSpriteName;

	EmitSound( m_SoundName );

	m_QueuedCommands.push_back( teslaInfo );
	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

void CTesla::ClientThink()
{
	for ( const CTeslaInfo &info : m_QueuedCommands )
	{
		FX_Tesla( info );
	}
	m_QueuedCommands.clear();
	SetNextClientThink( CLIENT_THINK_NEVER );
}

class CBaseEntity;

using namespace ::ks::reflect;

// In this test, the server makes an entity (call it A) that has an EHANDLE to another 
// entity (call it B). Intitially, A is sent to the client but B is not. Later, 
// the server decides to send B to the client too. At that point, without resending A's EHANDLE,
// the client's EHANDLE should access B.

	class CHandleTest : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_HandleTest" } ]]
	static constexpr bool ks_class = true;

	public:
		DECLARE_CLASS( CHandleTest, CBaseEntity );
		DECLARE_CLIENTCLASS();
		
		CHandleTest()
		{
		}

		virtual void OnDataChanged( DataUpdateType_t type )
		{
			Msg( "m_bSendHandle: %d, m_Handle.Get: 0x%p\n", m_bSendHandle, m_Handle.Get() );
		}

		[[= ks::reflect::Net{} ]] EHANDLE m_Handle;
		[[= ks::reflect::Net{} ]] bool m_bSendHandle;
	};

IMPLEMENT_REFLECT_CLIENTCLASS( CHandleTest, DT_HandleTest, CHandleTest )

class CBaseEntity;

using namespace ::ks::reflect;

class CTest_ProxyToggle_Networkable;
static CTest_ProxyToggle_Networkable *g_pTestObj = 0;

// ---------------------------------------------------------------------------------------- //
// CTest_ProxyToggle_Networkable
// ---------------------------------------------------------------------------------------- //

namespace DT_ProxyToggle_ProxiedData { extern RecvTable g_RecvTable; }

class CTest_ProxyToggle_Networkable : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_ProxyToggle_ProxiedData", .base = false } ]]
	[[= ks::reflect::NetTable{ .name = "DT_ProxyToggle" } ]]
	[[= ks::reflect::SubTable<"blah", &DT_ProxyToggle_ProxiedData::g_RecvTable>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTest_ProxyToggle_Networkable, CBaseEntity );
	DECLARE_CLIENTCLASS();

			CTest_ProxyToggle_Networkable()
			{
				g_pTestObj = this;
			}

			~CTest_ProxyToggle_Networkable()
			{
				g_pTestObj = 0;
			}

	[[= ks::reflect::Net{ .table = "DT_ProxyToggle_ProxiedData" } ]] int		m_WithProxy;
};

// ---------------------------------------------------------------------------------------- //
// Datatables.
// ---------------------------------------------------------------------------------------- //

IMPLEMENT_REFLECT_TABLE_IN( CTest_ProxyToggle_Networkable, DT_ProxyToggle_ProxiedData );

IMPLEMENT_REFLECT_CLIENTCLASS( CTest_ProxyToggle_Networkable, DT_ProxyToggle, CTest_ProxyToggle_Networkable )

// ---------------------------------------------------------------------------------------- //
// Console commands.
// ---------------------------------------------------------------------------------------- //

// The engine uses this to get the current value.
static void Test_ProxyToggle_EnsureValue( const CCommand &args );
static ConCommand Test_ProxyToggle_EnsureValue_command( side_registry(), "Test_ProxyToggle_EnsureValue", Test_ProxyToggle_EnsureValue, "Test_ProxyToggle_EnsureValue", FCVAR_CHEAT );
static void Test_ProxyToggle_EnsureValue( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		Error( "Test_ProxyToggle_EnsureValue: requires value parameter." );
	}
	else if ( !g_pTestObj )
	{
		Error( "Test_ProxyToggle_EnsureValue: object doesn't exist on the client." );
	}

	int wantedValue = atoi( args[ 1 ] );
	if ( g_pTestObj->m_WithProxy != wantedValue )
	{
		Error( "Test_ProxyToggle_EnsureValue: value (%d) doesn't match wanted value (%d).", g_pTestObj->m_WithProxy, wantedValue );
	}
}

class CBaseEntity;

using namespace ::ks::reflect;

// -------------------------------------------------------------------------------- //
// An entity used to test traceline
// -------------------------------------------------------------------------------- //
class CTestTraceline : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_TestTraceline", .base = false } ]]
	[[= ks::reflect::From<"m_clrRender", ks::reflect::Net{}, RecvProxy_Int32ToColor32>{} ]]
	[[= ks::reflect::From<"m_vecNetworkOrigin", ks::reflect::Net{ .wire = "m_vecOrigin" }>{} ]]
	[[= ks::reflect::From<"m_angNetworkAngles", ks::reflect::Net{ .wire = "m_angRotation", .index = 0 }>{} ]]
	[[= ks::reflect::From<"m_angNetworkAngles", ks::reflect::Net{ .wire = "m_angRotation", .index = 1 }>{} ]]
	[[= ks::reflect::From<"m_angNetworkAngles", ks::reflect::Net{ .wire = "m_angRotation", .index = 2 }>{} ]]
	[[= ks::reflect::From<"m_hNetworkMoveParent", ks::reflect::Net{ .wire = "moveparent" }, RecvProxy_IntToMoveParent>{} ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTestTraceline, CBaseEntity );
	DECLARE_CLIENTCLASS();

					CTestTraceline();
	virtual			~CTestTraceline();

// IClientEntity overrides.
public:
	virtual int			DrawModel( int flags, const RenderableInstance_t &instance );
	virtual bool		ShouldDraw() { return true; }

private:
	void DrawCube( Vector& center, unsigned char* pColor );
	IMaterial* m_pWireframe;
};

// Expose it to the engine.
IMPLEMENT_CLIENTCLASS(CTestTraceline, DT_TestTraceline, CTestTraceline);

IMPLEMENT_REFLECT_TABLE( CTestTraceline, DT_TestTraceline );

// -------------------------------------------------------------------------------- //
// Functions.
// -------------------------------------------------------------------------------- //

CTestTraceline::CTestTraceline()
{
	m_pWireframe = materials->FindMaterial("debug/debugwireframevertexcolor", TEXTURE_GROUP_OTHER);
}

CTestTraceline::~CTestTraceline()
{
}

enum
{
	CUBE_SIZE = 5
};

void CTestTraceline::DrawCube( Vector& center, unsigned char* pColor )
{
	Vector facePoints[8];
	Vector bmins, bmaxs;

	bmins[0] = center[0] - CUBE_SIZE;
	bmins[1] = center[1] - CUBE_SIZE;
	bmins[2] = center[2] - CUBE_SIZE;

	bmaxs[0] = center[0] + CUBE_SIZE;
	bmaxs[1] = center[1] + CUBE_SIZE;
	bmaxs[2] = center[2] + CUBE_SIZE;

	facePoints[0][0] = bmins[0];
	facePoints[0][1] = bmins[1];
	facePoints[0][2] = bmins[2];

	facePoints[1][0] = bmins[0];
	facePoints[1][1] = bmins[1];
	facePoints[1][2] = bmaxs[2];

	facePoints[2][0] = bmins[0];
	facePoints[2][1] = bmaxs[1];
	facePoints[2][2] = bmins[2];

	facePoints[3][0] = bmins[0];
	facePoints[3][1] = bmaxs[1];
	facePoints[3][2] = bmaxs[2];

	facePoints[4][0] = bmaxs[0];
	facePoints[4][1] = bmins[1];
	facePoints[4][2] = bmins[2];

	facePoints[5][0] = bmaxs[0];
	facePoints[5][1] = bmins[1];
	facePoints[5][2] = bmaxs[2];

	facePoints[6][0] = bmaxs[0];
	facePoints[6][1] = bmaxs[1];
	facePoints[6][2] = bmins[2];

	facePoints[7][0] = bmaxs[0];
	facePoints[7][1] = bmaxs[1];
	facePoints[7][2] = bmaxs[2];

	int nFaces[6][4] =
	{
		{ 0, 2, 3, 1 },
		{ 0, 1, 5, 4 },
		{ 4, 5, 7, 6 },
		{ 2, 6, 7, 3 },
		{ 1, 3, 7, 5 },
		{ 0, 4, 6, 2 }
	};

	for (int nFace = 0; nFace < 6; nFace++)
	{
		int nP1, nP2, nP3, nP4;

		nP1 = nFaces[nFace][0];
		nP2 = nFaces[nFace][1];
		nP3 = nFaces[nFace][2];
		nP4 = nFaces[nFace][3];

		// Draw the face.
		CMeshBuilder meshBuilder;
		CMatRenderContextPtr pRenderContext( materials );
		IMesh* pMesh = pRenderContext->GetDynamicMesh();
		meshBuilder.DrawQuad( pMesh, facePoints[nP1].Base(), facePoints[nP2].Base(), 
			facePoints[nP3].Base(), facePoints[nP4].Base(), pColor, true );
	}
}

int CTestTraceline::DrawModel( int flags, const RenderableInstance_t &instance )
{
	trace_t tr;
	Vector forward, right, up, endpos, hitpos;
	AngleVectors (GetAbsAngles(), &forward, &right, &up);
	endpos = GetAbsOrigin() + forward * MAX_TRACE_LENGTH;

	UTIL_TraceLine( GetAbsOrigin(), endpos, MASK_SOLID_BRUSHONLY, nullptr, COLLISION_GROUP_NONE, &tr );

	CMatRenderContextPtr pRenderContext( materials );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true, nullptr, nullptr, m_pWireframe );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_LINES, 1 );

	meshBuilder.Position3fv( GetAbsOrigin().Base() );
	meshBuilder.Color3ub( 255, 255, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv( tr.endpos.Base() );
	meshBuilder.Color3ub( 255, 255, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	// Didn't hit anything
	if ( tr.fraction != 1.0 )
	{
		unsigned char color[] = { 0, 255, 0 };
		DrawCube( tr.endpos, color );
	}

	if ( (!tr.allsolid) && (tr.fractionleftsolid != 0.0) )
	{
		unsigned char color[] = { 255, 0, 0 };
		DrawCube( tr.startpos, color );
	}

	return 1;
}

//
// text_message.cpp
//
// implementation of CHudTextMessage class
//
// this class routes messages through titles.txt for localisation
//

//-----------------------------------------------------------------------------
// Purpose: Implements IHudTextMessage
//-----------------------------------------------------------------------------
class CHudTextMessage : public IHudTextMessage
{
public:
	virtual char *LocaliseTextString( const char *msg, char *dst_buffer, int buffer_size );
	virtual const char * const BufferedLocaliseTextString( const char *msg );
	virtual const char *LookupString( const char *msg_name, int *msg_dest = nullptr );
};

// Singleton
static CHudTextMessage g_HudTextMessage;
IHudTextMessage *hudtextmessage = &g_HudTextMessage;

//-----------------------------------------------------------------------------
// Purpose:  Searches through the string for any msg names (indicated by a '#')
// any found are looked up in titles.txt and the new message substituted
// the new value is pushed into dst_buffer
// Input  : *msg - 
//			*dst_buffer - 
//			buffer_size - 
// Output : char
//-----------------------------------------------------------------------------
char *CHudTextMessage::LocaliseTextString( const char *msg, char *dst_buffer, int buffer_size )
{
	char *dst = dst_buffer;
	for ( const char *src = msg; *src != 0 && buffer_size > 0; buffer_size-- )
	{
		if ( *src == '#' )
		{
			// cut msg name out of string
            static const int max_word_len = 255;
            static char word_buf[max_word_len];
			const char *word_start = src;
            char *word = word_buf;
            int word_len = 0;
            for ( ++src ; *src >= 'A' && *src <= 'z'; word++, src++ )
			{
				*word = *src;
				if( word_len++ >= ( max_word_len - 1 ) )
                {
				    // word is too long, return empty string.
				    Warning( "#Token too long in LocaliseTextString!\n" );
				    dst_buffer[0] = 0;
				    return dst_buffer;
                }
			}
			*word = 0;

			// lookup msg name in titles.txt
			client_textmessage_t *clmsg = TextMessageGet( word_buf );
			if ( !clmsg || !(clmsg->pMessage) )
			{
				src = word_start;
				*dst = *src;
				dst++, src++;
				continue;
			}

			// Does titles.txt want to lookup into cstrike_<language>.txt?
			wchar_t *pLocalizedStr;
			if ( clmsg->pMessage[0] == '#' && ((pLocalizedStr = g_pLocalize->Find( clmsg->pMessage )) != nullptr ) )
			{
				g_pLocalize->ConvertUnicodeToANSI( pLocalizedStr, dst, buffer_size );
			}
			else
			{
				// copy string into message over the msg name
				for ( char *wsrc = (char*)clmsg->pMessage; *wsrc != 0; wsrc++, dst++ )
				{
					*dst = *wsrc;
				}
				*dst = 0;
			}
		}
		else
		{
			*dst = *src;
			dst++, src++;
			*dst = 0;
		}
	}

    // ensure null termination
	dst_buffer[buffer_size-1] = 0; 
	return dst_buffer;
}

//-----------------------------------------------------------------------------
// Purpose: As above, but with a local static buffer
// Input  : *msg - 
// Output : char pointer to local static buffer that shouldn't be changed.
//-----------------------------------------------------------------------------
const char * const CHudTextMessage::BufferedLocaliseTextString( const char *msg )
{
	static char dst_buffer[1024];
	return LocaliseTextString( msg, dst_buffer, 1024 );
}

//-----------------------------------------------------------------------------
// Purpose:  Simplified version of LocaliseTextString;  assumes string is only one word
// Input  : *msg - 
//			*msg_dest - 
// Output : char
//-----------------------------------------------------------------------------
const char *CHudTextMessage::LookupString( const char *msg, int *msg_dest )
{
	if ( !msg )
		return "";

	// '#' character indicates this is a reference to a string in titles.txt, and not the string itself
	if ( msg[0] == '#' ) 
	{
		// this is a message name, so look up the real message
		client_textmessage_t *clmsg = TextMessageGet( msg+1 );

		if ( !clmsg || !(clmsg->pMessage) )
			return msg; // lookup failed, so return the original string
		
		if ( msg_dest )
		{
			// check to see if titles.txt info overrides msg destination
			// if clmsg->effect is less than 0, then clmsg->effect holds -1 * message_destination
			if ( clmsg->effect < 0 )  // 
				*msg_dest = -clmsg->effect;
		}

		return clmsg->pMessage;
	}
	else
	{  // nothing special about this message, so just return the same string
		return msg;
	}
}

// $textureScrollVar
// $textureScrollRate
// $textureScrollAngle
class CTextureScrollMaterialProxy : public IMaterialProxy
{
public:
	CTextureScrollMaterialProxy();
	virtual ~CTextureScrollMaterialProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
	virtual void Release( void ) { delete this; }
	virtual IMaterial *GetMaterial();

private:
	IMaterialVar *m_pTextureScrollVar;
	CFloatInput m_TextureScrollRate;
	CFloatInput m_TextureScrollAngle;
	CFloatInput m_TextureScale;
};

CTextureScrollMaterialProxy::CTextureScrollMaterialProxy()
{
	m_pTextureScrollVar = nullptr;
}

CTextureScrollMaterialProxy::~CTextureScrollMaterialProxy()
{
}

bool CTextureScrollMaterialProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	char const* pScrollVarName = pKeyValues->GetString( "textureScrollVar" );
	if( !pScrollVarName )
		return false;

	bool foundVar;
	m_pTextureScrollVar = pMaterial->FindVar( pScrollVarName, &foundVar, false );
	if( !foundVar )
		return false;

	m_TextureScrollRate.Init( pMaterial, pKeyValues, "textureScrollRate", 1.0f );
	m_TextureScrollAngle.Init( pMaterial, pKeyValues, "textureScrollAngle", 0.0f );
	m_TextureScale.Init( pMaterial, pKeyValues, "textureScale", 1.0f );

	return true;
}

void CTextureScrollMaterialProxy::OnBind( void *pC_BaseEntity )
{
	if( !m_pTextureScrollVar )
	{
		return;
	}

	float rate, angle, scale;

	// set default values if these variables don't exist.
	rate		= m_TextureScrollRate.GetFloat();
	angle		= m_TextureScrollAngle.GetFloat();
	scale		= m_TextureScale.GetFloat();

	float sOffset, tOffset;
	
	sOffset = gpGlobals->curtime * std::cos( angle * ( std::numbers::pi / 180.0f ) ) * rate;
	tOffset = gpGlobals->curtime * std::sin( angle * ( std::numbers::pi / 180.0f ) ) * rate;

	// make sure that we are positive
	if( sOffset < 0.0f )
	{
		sOffset += 1.0f + -( int )sOffset;
	}
	if( tOffset < 0.0f )
	{
		tOffset += 1.0f + -( int )tOffset;
	}
			    
	// make sure that we are in a [0,1] range
	sOffset = sOffset - ( int )sOffset;
	tOffset = tOffset - ( int )tOffset;
	
	if (m_pTextureScrollVar->GetType() == MATERIAL_VAR_TYPE_MATRIX)
	{
		VMatrix mat( scale, 0.0f, 0.0f, sOffset,
			0.0f, scale, 0.0f, tOffset,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f );
		m_pTextureScrollVar->SetMatrixValue( mat );
	}
	else
	{
		m_pTextureScrollVar->SetVecValue( sOffset, tOffset, 0.0f );
	}
}

IMaterial *CTextureScrollMaterialProxy::GetMaterial()
{
	return m_pTextureScrollVar->GetOwningMaterial();
}

EXPOSE_MATERIAL_PROXY( CTextureScrollMaterialProxy, TextureScroll );

class CTimeMaterialProxy : public CResultProxy
{
public:
	virtual void OnBind( void *pC_BaseEntity );
};					    

void CTimeMaterialProxy::OnBind( void *pC_BaseEntity )
{
	SetFloatResult( gpGlobals->curtime );
}

EXPOSE_MATERIAL_PROXY( CTimeMaterialProxy, CurrentTime );

class CBaseEntity;

using namespace ::ks::reflect;

EXPOSE_MATERIAL_PROXY( CBaseToggleTextureProxy, ToggleTexture );

//-----------------------------------------------------------------------------
// Constructor, destructor: 
//-----------------------------------------------------------------------------

CBaseToggleTextureProxy::CBaseToggleTextureProxy()
{
	Cleanup();
}

CBaseToggleTextureProxy::~CBaseToggleTextureProxy()
{
	Cleanup();
}

CBaseEntity *CBaseToggleTextureProxy::BindArgToEntity( void *pArg )
{
	IClientRenderable *pRend = (IClientRenderable *)pArg;
	return EntityCast<CBaseEntity>( pRend->GetIClientUnknown()->GetBaseEntity() );
}

//-----------------------------------------------------------------------------
// Initialization, shutdown
//-----------------------------------------------------------------------------
bool CBaseToggleTextureProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	char const* pTextureVarName = pKeyValues->GetString( "toggleTextureVar" );
	if( !pTextureVarName )
		return false;

	bool foundVar;
	m_TextureVar = pMaterial->FindVar( pTextureVarName, &foundVar, false );
	if( !foundVar )
		return false;

	char const* pTextureFrameNumVarName = pKeyValues->GetString( "toggleTextureFrameNumVar" );
	if( !pTextureFrameNumVarName )
		return false;

	m_TextureFrameNumVar = pMaterial->FindVar( pTextureFrameNumVarName, &foundVar, false );
	if( !foundVar )
		return false;
	
	m_WrapAnimation = !!pKeyValues->GetInt( "toggleShouldWrap", 1 );
	return true;
}

void CBaseToggleTextureProxy::Cleanup()
{
	m_TextureVar = nullptr;
	m_TextureFrameNumVar = nullptr;
}

//-----------------------------------------------------------------------------
// Does the dirty deed
//-----------------------------------------------------------------------------
void CBaseToggleTextureProxy::OnBind( void *pC_BaseEntity )
{
	contract_assert( m_TextureVar );

	if (!pC_BaseEntity)
		return;

	if( m_TextureVar->GetType() != MATERIAL_VAR_TYPE_TEXTURE )
	{
		return;
	}

	ITexture *pTexture = nullptr;

	pTexture = m_TextureVar->GetTextureValue();

	if ( pTexture == nullptr )
		 return;

	CBaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );

	if ( pEntity == nullptr )
		 return;
	
	int numFrames = pTexture->GetNumAnimationFrames();
	int frame = pEntity->GetTextureFrameIndex();

	int intFrame = ((int)frame) % numFrames; 

	if ( m_WrapAnimation == false )
	{
		if ( frame > numFrames )
			 intFrame = numFrames;
	}
		
	m_TextureFrameNumVar->SetIntValue( intFrame );
}

IMaterial *CBaseToggleTextureProxy::GetMaterial()
{
	return m_TextureFrameNumVar->GetOwningMaterial();
}

class ParticleDraw;

//-----------------------------------------------------------------------------
// Sees if the tracer is behind the camera or should be culled
//-----------------------------------------------------------------------------

static bool ClipTracer( const Vector &start, const Vector &delta, Vector &clippedStart, Vector &clippedDelta )
{
	// dist1 = start dot forward - origin dot forward
	// dist2 = (start + delta ) dot forward - origin dot forward
	// in camera space this is -start[2] since origin = 0 and vecForward = (0, 0, -1)
	float dist1 = -start[2];
	float dist2 = dist1 - delta[2];
	
	// Clipped, skip this tracer
	if ( dist1 <= 0 && dist2 <= 0 )
		return true;

	clippedStart = start;
	clippedDelta = delta;
	
	// Needs to be clipped
	if ( dist1 <= 0 || dist2 <= 0 )
	{
		float fraction = dist2 - dist1;

		// Too close to clipping plane
		if ( fraction < 1e-3 && fraction > -1e-3 )
			return true;

		fraction = -dist1 / fraction;

		if ( dist1 <= 0 )
		{
			VectorMA( start, fraction, delta, clippedStart );
		}
		else
		{
			VectorMultiply( delta, fraction, clippedDelta );
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Computes the four verts to draw the tracer with
//-----------------------------------------------------------------------------
bool Tracer_ComputeVerts( const Vector &start, const Vector &delta, float width, Vector *pVerts )
{
	Vector clippedStart, clippedDelta;

	// Clip the tracer
	if ( ClipTracer( start, delta, clippedStart, clippedDelta ) )
		return false;

	// Figure out direction in camera space of the normal
	Vector normal;
	CrossProduct( clippedDelta, clippedStart, normal );
					  
	// don't draw if they are parallel
	float sqLength = DotProduct( normal, normal );
	if (sqLength < 1e-3)
		return false;

	// Resize the normal to be appropriate based on the width
	VectorScale( normal, 0.5f * width / std::sqrt(sqLength), normal );

	VectorSubtract( clippedStart, normal, pVerts[0] );
	VectorAdd( clippedStart, normal, pVerts[1] );

	VectorAdd( pVerts[0], clippedDelta, pVerts[2] );
	VectorAdd( pVerts[1], clippedDelta, pVerts[3] );

	return true;
}

void Tracer_Draw( CMeshBuilder *pMeshBuilder, Vector const& start, Vector const& delta, float width, float* color, float startV, float endV )
{
	// Clip the tracer
	Vector verts[4];
	if (!Tracer_ComputeVerts( start, delta, width, verts ))
		return;

	// NOTE: Gotta get the winding right so it's not backface culled
	// (we need to turn of backface culling for these bad boys)
	pMeshBuilder->Position3f( verts[0].x, verts[0].y, verts[0].z );
	pMeshBuilder->TexCoord2f( 0, 0.0f, startV );
	if (color)
	{
		pMeshBuilder->Color4fv( color );
	}
	else
	{
		pMeshBuilder->Color4ub( 255, 255, 255, 255 );
	}
	pMeshBuilder->AdvanceVertex();

	pMeshBuilder->Position3f( verts[1].x, verts[1].y, verts[1].z );
	pMeshBuilder->TexCoord2f( 0, 1.0f, startV );
	if (color)
	{
		pMeshBuilder->Color4fv( color );
	}
	else
	{
		pMeshBuilder->Color4ub( 255, 255, 255, 255 );
	}
	pMeshBuilder->AdvanceVertex();

	pMeshBuilder->Position3f( verts[3].x, verts[3].y, verts[3].z );
	pMeshBuilder->TexCoord2f( 0, 1.0f, endV );
	if (color)
		pMeshBuilder->Color4fv( color );
	else
		pMeshBuilder->Color4ub( 255, 255, 255, 255 );
	pMeshBuilder->AdvanceVertex();

	pMeshBuilder->Position3f( verts[2].x, verts[2].y, verts[2].z );
	pMeshBuilder->TexCoord2f( 0, 0.0f, endV );
	if (color)
		pMeshBuilder->Color4fv( color );
	else
		pMeshBuilder->Color4ub( 255, 255, 255, 255 );
	pMeshBuilder->AdvanceVertex();
}

//-----------------------------------------------------------------------------
// draw a tracer.
//-----------------------------------------------------------------------------
void Tracer_Draw( ParticleDraw* pDraw, Vector const& start, Vector const& delta, float width, float* color, float startV, float endV )
{
	if( !pDraw->GetMeshBuilder() )
		return;

	Tracer_Draw( pDraw->GetMeshBuilder(), start, delta, width, color, startV, endV );
}

class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

IMPLEMENT_REFLECT_CLIENTCLASS( CBaseTrigger, DT_BaseTrigger, CBaseTrigger )

//-----------------------------------------------------------------------------
// Purpose: Disables auto movement on players that touch it
//-----------------------------------------------------------------------------
class CTriggerPlayerMovement : public CBaseTrigger
{
	[[= ks::reflect::NetTable{ .name = "DT_TriggerPlayerMovement" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTriggerPlayerMovement, CBaseTrigger );
	DECLARE_CLIENTCLASS();

	CTriggerPlayerMovement();
	~CTriggerPlayerMovement();

	void StartTouch( CBaseEntity *pOther );
	void EndTouch( CBaseEntity *pOther );

protected:

	virtual void UpdatePartitionListEntry();

public:
	CTriggerPlayerMovement	*m_pNext;
};

IMPLEMENT_REFLECT_CLIENTCLASS( CTriggerPlayerMovement, DT_TriggerPlayerMovement, CTriggerPlayerMovement )

CEntityClassList< CTriggerPlayerMovement > g_TriggerPlayerMovementList;
template<> CTriggerPlayerMovement *CEntityClassList<CTriggerPlayerMovement>::m_pClassList = nullptr;

CTriggerPlayerMovement::CTriggerPlayerMovement()
{
	g_TriggerPlayerMovementList.Insert( this );
}

CTriggerPlayerMovement::~CTriggerPlayerMovement()
{
	g_TriggerPlayerMovementList.Remove( this );
}

//-----------------------------------------------------------------------------
// Little enumeration class used to try touching all triggers
//-----------------------------------------------------------------------------
template< class T >
class CFastTouchTriggers
{
public:
	CFastTouchTriggers( CBaseEntity *pEnt, T *pTriggers ) : m_pEnt( pEnt ), m_pTriggers( pTriggers )
	{
		m_pCollide = pEnt->GetCollideable();
		m_nRequiredTriggerFlags = m_pCollide->GetRequiredTriggerFlags();
		contract_assert( m_pCollide );

		Vector vecMins, vecMaxs;
		CM_GetCollideableTriggerTestBox( m_pCollide, &vecMins, &vecMaxs );
		const Vector &vecStart = m_pCollide->GetCollisionOrigin();
		m_Ray.Init( vecStart, vecStart, vecMins, vecMaxs );
	}

	[[gnu::always_inline]] inline void CM_GetCollideableTriggerTestBox( ICollideable *pCollide, Vector *pMins, Vector *pMaxs )
	{
		if ( pCollide->GetSolid() == SOLID_BBOX )
		{
			*pMins = pCollide->OBBMins();
			*pMaxs = pCollide->OBBMaxs();
		}
		else
		{
			const Vector &vecStart = pCollide->GetCollisionOrigin();
			pCollide->WorldSpaceSurroundingBounds( pMins, pMaxs );
			*pMins -= vecStart;
			*pMaxs -= vecStart;
		}
	}

	[[gnu::always_inline]] inline void Check( T *pEntity, bool bIgnoreTriggerSolidFlags )
	{
		// Hmmm.. everything in this list should be a trigger....
		ICollideable *pTriggerCollideable = pEntity->GetCollideable();
		int nTriggerSolidFlags = pTriggerCollideable->GetSolidFlags();
		if ( !bIgnoreTriggerSolidFlags
			&& m_nRequiredTriggerFlags && ( nTriggerSolidFlags & m_nRequiredTriggerFlags ) == m_nRequiredTriggerFlags )
			return;

		if ( nTriggerSolidFlags & FSOLID_USE_TRIGGER_BOUNDS )
		{
			Vector vecTriggerMins, vecTriggerMaxs;
			pTriggerCollideable->WorldSpaceTriggerBounds( &vecTriggerMins, &vecTriggerMaxs ); 
			if ( !IsBoxIntersectingRay( vecTriggerMins, vecTriggerMaxs, m_Ray ) )
			{
				return;
			}
		}
		else
		{
			trace_t tr;
			enginetrace->ClipRayToCollideable( m_Ray, MASK_SOLID, pTriggerCollideable, &tr );
			if ( !(tr.contents & MASK_SOLID) )
				return;
		}

		trace_t tr;
		UTIL_ClearTrace( tr );
		tr.endpos = (m_pEnt->GetAbsOrigin() + pEntity->GetAbsOrigin()) * 0.5;
		m_pEnt->PhysicsMarkEntitiesAsTouching( pEntity, tr );
	}

	[[gnu::always_inline]] inline void Run( bool bIgnoreTriggerSolidFlags = false )
	{
		for ( T *trigger = m_pTriggers; trigger ; trigger = trigger->m_pNext )
		{
			if ( trigger->IsDormant() )
				continue;
			Check( trigger, bIgnoreTriggerSolidFlags );
		}
	}

	Ray_t m_Ray;

private:
	CBaseEntity			*m_pEnt;
	ICollideable			*m_pCollide;
	uint					m_nRequiredTriggerFlags;
	T						*m_pTriggers;
};

void TouchTriggerPlayerMovement( CBaseEntity *pEntity )
{
	CFastTouchTriggers< CTriggerPlayerMovement > helper( pEntity, g_TriggerPlayerMovementList.m_pClassList );
	helper.Run();
}

void CTriggerPlayerMovement::UpdatePartitionListEntry()
{
	if ( !m_bClientSidePredicted )
	{
		BaseClass::UpdatePartitionListEntry();
		return;
	}

	::partition->RemoveAndInsert( 
		PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_NON_STATIC_EDICTS,  // remove
		PARTITION_CLIENT_TRIGGER_ENTITIES,  // add
		CollisionProp()->GetPartitionHandle() );
}

void CTriggerPlayerMovement::StartTouch( CBaseEntity *pOther )
{	
	CBasePlayer *pPlayer = ToBasePlayer( pOther );

	if ( !pPlayer )
		return;

	if ( HasSpawnFlags( SF_TRIGGER_AUTO_DUCK ) )
	{
		pPlayer->ForceButtons( IN_DUCK );
	}

	if ( HasSpawnFlags( SF_TRIGGER_AUTO_WALK ) )
	{
		pPlayer->ForceButtons( IN_SPEED );
	}

	// UNDONE: Currently this is the only operation this trigger can do
	if ( HasSpawnFlags( SF_TRIGGER_MOVE_AUTODISABLE ) )
	{
		pPlayer->m_Local.m_bAllowAutoMovement = false;
	}
}

void CTriggerPlayerMovement::EndTouch( CBaseEntity *pOther )
{
	CBasePlayer *pPlayer = ToBasePlayer( pOther );
	if ( !pPlayer )
		return;

	if ( HasSpawnFlags( SF_TRIGGER_AUTO_DUCK ) )
	{
		pPlayer->UnforceButtons( IN_DUCK );
	}

	if ( HasSpawnFlags( SF_TRIGGER_AUTO_WALK ) )
	{
		pPlayer->UnforceButtons( IN_SPEED );
	}

	if ( HasSpawnFlags( SF_TRIGGER_MOVE_AUTODISABLE ) )
	{
		pPlayer->m_Local.m_bAllowAutoMovement = true;
	}
}

IMPLEMENT_REFLECT_CLIENTCLASS( CBaseVPhysicsTrigger, DT_BaseVPhysicsTrigger, CBaseVPhysicsTrigger )

//////////////////////////////////////////////////////////////////////////
//
// Sound operator trigger
//

class CTriggerSoundOperator : public CBaseTrigger
{
	[[= ks::reflect::NetTable{ .name = "DT_TriggerSoundOperator" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTriggerSoundOperator, CBaseTrigger );
	DECLARE_CLIENTCLASS();

	CTriggerSoundOperator();
	~CTriggerSoundOperator();

	void StartTouch( CBaseEntity *pOther );
	void EndTouch( CBaseEntity *pOther );

protected:
	virtual void UpdatePartitionListEntry();

	[[= ks::reflect::Net{} ]] int m_nSoundOperator;
	void UpdateSosVar( bool bTouchingNow );

public:
	CTriggerSoundOperator	*m_pNext;
};

IMPLEMENT_REFLECT_CLIENTCLASS( CTriggerSoundOperator, DT_TriggerSoundOperator, CTriggerSoundOperator )

CEntityClassList< CTriggerSoundOperator > g_TriggerSoundOperators;
template<> CTriggerSoundOperator *CEntityClassList<CTriggerSoundOperator>::m_pClassList = nullptr;

CTriggerSoundOperator::CTriggerSoundOperator()
{
	g_TriggerSoundOperators.Insert( this );
	m_nSoundOperator = 0;
}

CTriggerSoundOperator::~CTriggerSoundOperator()
{
	g_TriggerSoundOperators.Remove( this );
}

static char const * g_szTriggerSoundOperatorVariables[] = {
	"sosVarCustomVolume1",
	"sosVarCustomVolume2"
};
static bool g_bTriggerSoundOperatorVariablesSet[ std::size( g_szTriggerSoundOperatorVariables ) ] = {
};

void TouchTriggerSoundOperator( CBaseEntity *pEntity )
{
	CFastTouchTriggers< CTriggerSoundOperator > helper( pEntity, g_TriggerSoundOperators.m_pClassList );
	helper.Run( true );
}

void UntouchAllTriggerSoundOperator( CBaseEntity *pEntity )
{
	for ( int j = 0; j < std::size( g_szTriggerSoundOperatorVariables ); ++ j )
	{
		if ( g_bTriggerSoundOperatorVariablesSet[ j ] )
		{
			g_bTriggerSoundOperatorVariablesSet[ j ] = false;
			engine->SOSSetOpvarFloat( g_szTriggerSoundOperatorVariables[ j ], 0.0f );
		}
	}
}

void CTriggerSoundOperator::UpdatePartitionListEntry()
{
	if ( !m_bClientSidePredicted )
	{
		BaseClass::UpdatePartitionListEntry();
		return;
	}

	::partition->RemoveAndInsert(
		PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_NON_STATIC_EDICTS,  // remove
		PARTITION_CLIENT_TRIGGER_ENTITIES,  // add
		CollisionProp()->GetPartitionHandle() );
}

void CTriggerSoundOperator::StartTouch( CBaseEntity *pOther )
{
	CBasePlayer *pLocalPlayer = CBasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer || ( pOther != pLocalPlayer ) )
		return;

	UpdateSosVar( true );
}

void CTriggerSoundOperator::EndTouch( CBaseEntity *pOther )
{
	CBasePlayer *pLocalPlayer = CBasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer || ( pOther != pLocalPlayer ) )
		return;

	UpdateSosVar( false );
}

void CTriggerSoundOperator::UpdateSosVar( bool bTouchingNow )
{
	int nVar = 0;
	if ( ( m_nSoundOperator >= 0 )
		&& ( m_nSoundOperator < std::size( g_szTriggerSoundOperatorVariables ) ) )
		nVar = m_nSoundOperator;
	char const *szVar = g_szTriggerSoundOperatorVariables[ nVar ];
	g_bTriggerSoundOperatorVariablesSet[ nVar ] = bTouchingNow;
	engine->SOSSetOpvarFloat( szVar, bTouchingNow ? 1.0f : 0.0f );
}

using namespace ::ks::reflect;

static ConVar		scr_centertime( side_registry(), "scr_centertime", "4" );

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
CCenterPrint::CCenterPrint( void )
{
}

void CCenterPrint::SetTextColor( int r, int g, int b, int a )
{
}

void CCenterPrint::Print( const char *text )
{
}

void CCenterPrint::Print( const wchar_t *text )
{
}

void CCenterPrint::ColorPrint( int r, int g, int b, int a, char *text )
{
}

void CCenterPrint::ColorPrint( int r, int g, int b, int a, wchar_t *text )
{
}

void CCenterPrint::Clear( void )
{
}

void CCenterPrint::Destroy( void )
{
}

static CCenterPrint g_CenterString[ MAX_SPLITSCREEN_PLAYERS ];
CCenterPrint *GetCenterPrint()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return &g_CenterString[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}

CUserMessageRegisterBase *CUserMessageRegisterBase::s_pHead = nullptr;

CUserMessageRegisterBase::CUserMessageRegisterBase()
{	
	// Link it in.
	m_pNext = s_pHead;
	s_pHead = this;
}

void CUserMessageRegisterBase::RegisterAll()
{
	for ( CUserMessageRegisterBase *pCur=s_pHead; pCur; pCur=pCur->m_pNext )
	{
		pCur->Register();
	}
}

class CUserCmd;

using namespace ::ks::reflect;

// TF2 specific, need enough space for OBJ_LAST items from tf_shareddefs.h
static constexpr auto WEAPON_SUBTYPE_BITS = 6;

//-----------------------------------------------------------------------------
ConVar net_showusercmd( side_registry(), "net_showusercmd", "0", 0, "Show user command encoding" );
#define LogUserCmd( msg, ... ) if ( net_showusercmd.GetInt() ) { ConDMsg( msg, __VA_ARGS__ ); }

//-----------------------------------------------------------------------------
static bool WriteUserCmdDeltaInt( bf_write *buf, const char *what, int from, int to, int bits = 32 )
{
	if ( from != to )
	{
		LogUserCmd( "\t%s %d -> %d\n", what, from, to );

		buf->WriteOneBit( 1 );
		buf->WriteUBitLong( to, bits );
		return true;
	}

	buf->WriteOneBit( 0 );
	return false;
}

static bool WriteUserCmdDeltaShort( bf_write *buf, const char *what, int from, int to )
{
	if ( from != to )
	{
		LogUserCmd( "\t%s %d -> %d\n", what, from, to );

		buf->WriteOneBit( 1 );
		buf->WriteShort( to );
		return true;
	}

	buf->WriteOneBit( 0 );
	return false;
}

static bool WriteUserCmdDeltaFloat( bf_write *buf, const char *what, float from, float to )
{
	if ( from != to )
	{
		LogUserCmd( "\t%s %2.2f -> %2.2f\n", what, from, to );

		buf->WriteOneBit( 1 );
		buf->WriteFloat( to );
		return true;
	}

	buf->WriteOneBit( 0 );
	return false;
}

static bool WriteUserCmdDeltaCoord( bf_write *buf, const char *what, float from, float to )
{
	if ( from != to )
	{
		LogUserCmd( "\t%s %2.2f -> %2.2f\n", what, from, to );

		buf->WriteOneBit( 1 );
		buf->WriteBitCoord( to );
		return true;
	}

	buf->WriteOneBit( 0 );
	return false;
}

static bool WriteUserCmdDeltaAngle( bf_write *buf, const char *what, float from, float to, int bits )
{
	if ( from != to )
	{
		LogUserCmd( "\t%s %2.2f -> %2.2f\n", what, from, to );

		buf->WriteOneBit( 1 );
		buf->WriteBitAngle( to, bits );
		return true;
	}

	buf->WriteOneBit( 0 );
	return false;
}

static bool WriteUserCmdDeltaVec3Coord( bf_write *buf, const char *what, const Vector &from, const Vector &to )
{
	if ( from != to )
	{
		LogUserCmd( "\t%s %2.2f -> %2.2f\n", what, from.x, to.x );

		buf->WriteOneBit( 1 );
		buf->WriteBitVec3Coord( to );
		return true;
	}

	buf->WriteOneBit( 0 );
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Write a delta compressed user command.
// Input  : *buf - 
//			*to - 
//			*from - 
// Output : static
//-----------------------------------------------------------------------------
void WriteUsercmd( bf_write *buf, const CUserCmd *to, const CUserCmd *from )
{
	LogUserCmd("WriteUsercmd: from=%d to=%d\n", from->command_number, to->command_number );

	WriteUserCmdDeltaInt( buf, "command_number", from->command_number + 1, to->command_number, 32 );
	WriteUserCmdDeltaInt( buf, "tick_count", from->tick_count + 1, to->tick_count, 32 );

	WriteUserCmdDeltaFloat( buf, "viewangles[0]", from->viewangles[0], to->viewangles[0] );
	WriteUserCmdDeltaFloat( buf, "viewangles[1]", from->viewangles[1], to->viewangles[1] );
	WriteUserCmdDeltaFloat( buf, "viewangles[2]", from->viewangles[2], to->viewangles[2] );

	// aim direction for motion controllers
	WriteUserCmdDeltaFloat( buf, "aimdirection[0]", from->aimdirection[0], to->aimdirection[0] );
	WriteUserCmdDeltaFloat( buf, "aimdirection[1]", from->aimdirection[1], to->aimdirection[1] );
	WriteUserCmdDeltaFloat( buf, "aimdirection[2]", from->aimdirection[2], to->aimdirection[2] );

	WriteUserCmdDeltaFloat( buf, "forwardmove", from->forwardmove, to->forwardmove );
	WriteUserCmdDeltaFloat( buf, "sidemove", from->sidemove, to->sidemove );
	WriteUserCmdDeltaFloat( buf, "upmove", from->upmove, to->upmove );
	WriteUserCmdDeltaInt( buf, "buttons", from->buttons, to->buttons, 32 );
	WriteUserCmdDeltaInt( buf, "impulse", from->impulse, to->impulse, 8 );

	if ( WriteUserCmdDeltaInt( buf, "weaponselect", from->weaponselect, to->weaponselect, MAX_EDICT_BITS ) )
	{
		WriteUserCmdDeltaInt( buf, "weaponsubtype", from->weaponsubtype, to->weaponsubtype, WEAPON_SUBTYPE_BITS );
	}

	// TODO: Can probably get away with fewer bits.
	WriteUserCmdDeltaShort( buf, "mousedx", from->mousedx, to->mousedx );
	WriteUserCmdDeltaShort( buf, "mousedy", from->mousedy, to->mousedy );

	if ( IsHeadTrackingEnabled() )
	{
		// TrackIR head angles
		WriteUserCmdDeltaAngle( buf, "headangles[0]", from->headangles[ 0 ], to->headangles[ 0 ], 16 );
		WriteUserCmdDeltaAngle( buf, "headangles[1]", from->headangles[ 1 ], to->headangles[ 1 ], 16 );
		WriteUserCmdDeltaAngle( buf, "headangles[2]", from->headangles[ 2 ], to->headangles[ 2 ], 8 );

		// TrackIR head offset
		WriteUserCmdDeltaVec3Coord( buf, "headoffset", from->headoffset, to->headoffset );
		// TrackIR
	}

}

//-----------------------------------------------------------------------------
// Purpose: Read in a delta compressed usercommand.
// Input  : *buf - 
//			*move - 
//			*from - 
// Output : static void ReadUsercmd
//-----------------------------------------------------------------------------
void ReadUsercmd( bf_read *buf, CUserCmd *move, CUserCmd *from )
{
	// Assume no change
	*move = *from;

	if ( buf->ReadOneBit() )
	{
		move->command_number = buf->ReadUBitLong( 32 );
	}
	else
	{
		// Assume steady increment
		move->command_number = from->command_number + 1;
	}

	if ( buf->ReadOneBit() )
	{
		move->tick_count = buf->ReadUBitLong( 32 );
	}
	else
	{
		// Assume steady increment
		move->tick_count = from->tick_count + 1;
	}

	// Read direction
	if ( buf->ReadOneBit() )
	{
		move->viewangles[0] = buf->ReadFloat();
	}

	if ( buf->ReadOneBit() )
	{
		move->viewangles[1] = buf->ReadFloat();
	}

	if ( buf->ReadOneBit() )
	{
		move->viewangles[2] = buf->ReadFloat();
	}

	// Read aim direction
	if ( buf->ReadOneBit() )
	{
		move->aimdirection[0] = buf->ReadFloat();
	}

	if ( buf->ReadOneBit() )
	{
		move->aimdirection[1] = buf->ReadFloat();
	}

	if ( buf->ReadOneBit() )
	{
		move->aimdirection[2] = buf->ReadFloat();
	}

	// Read movement
	if ( buf->ReadOneBit() )
	{
		float flMove = buf->ReadFloat();
		move->forwardmove = IsFinite( flMove ) && IsEntityCoordinateReasonable( flMove ) ? flMove : 0;
	}

	if ( buf->ReadOneBit() )
	{
		float flMove = buf->ReadFloat();
		move->sidemove = IsFinite( flMove ) && IsEntityCoordinateReasonable( flMove ) ? flMove : 0;
	}

	if ( buf->ReadOneBit() )
	{
		float flMove = buf->ReadFloat();
		move->upmove = IsFinite( flMove ) && IsEntityCoordinateReasonable( flMove ) ? flMove : 0;
	}

	// read buttons
	if ( buf->ReadOneBit() )
	{
		move->buttons = buf->ReadUBitLong( 32 );
	}

	if ( buf->ReadOneBit() )
	{
		move->impulse = buf->ReadUBitLong( 8 );
	}

	if ( buf->ReadOneBit() )
	{
		move->weaponselect = buf->ReadUBitLong( MAX_EDICT_BITS );
		if ( buf->ReadOneBit() )
		{
			move->weaponsubtype = buf->ReadUBitLong( WEAPON_SUBTYPE_BITS );
		}
	}

	move->random_seed = MD5_PseudoRandom( move->command_number ) & 0x7fffffff;

	if ( buf->ReadOneBit() )
	{
		move->mousedx = buf->ReadShort();
	}

	if ( buf->ReadOneBit() )
	{
		move->mousedy = buf->ReadShort();
	}

	if ( IsHeadTrackingEnabled() )
	{
		// TrackIR head angles
		if ( buf->ReadOneBit() )
		{
			move->headangles[0] = buf->ReadBitAngle( 16 );
		}

		if ( buf->ReadOneBit() )
		{
			move->headangles[1] = buf->ReadBitAngle( 16 );
		}

		if ( buf->ReadOneBit() )
		{
			move->headangles[2] = buf->ReadBitAngle( 8 );
		}

		// TrackIR head offset
		if ( buf->ReadOneBit() )
		{
			buf->ReadBitVec3Coord(move->headoffset);
		}
		// TrackIR
	}

}

CRC32_t CUserCmd::GetChecksum( void) const
{
	CRC32_t crc;

	CRC32_Init( &crc );
	CRC32_ProcessBuffer( &crc, &command_number, sizeof( command_number ) );
	CRC32_ProcessBuffer( &crc, &tick_count, sizeof( tick_count ) );
	CRC32_ProcessBuffer( &crc, &viewangles, sizeof( viewangles ) );
	CRC32_ProcessBuffer( &crc, &aimdirection, sizeof( aimdirection ) );
	CRC32_ProcessBuffer( &crc, &forwardmove, sizeof( forwardmove ) );   
	CRC32_ProcessBuffer( &crc, &sidemove, sizeof( sidemove ) );      
	CRC32_ProcessBuffer( &crc, &upmove, sizeof( upmove ) );         
	CRC32_ProcessBuffer( &crc, &buttons, sizeof( buttons ) );		
	CRC32_ProcessBuffer( &crc, &impulse, sizeof( impulse ) );        
	CRC32_ProcessBuffer( &crc, &weaponselect, sizeof( weaponselect ) );	
	CRC32_ProcessBuffer( &crc, &weaponsubtype, sizeof( weaponsubtype ) );
	CRC32_ProcessBuffer( &crc, &random_seed, sizeof( random_seed ) );
	CRC32_ProcessBuffer( &crc, &mousedx, sizeof( mousedx ) );
	CRC32_ProcessBuffer( &crc, &mousedy, sizeof( mousedy ) );

	CRC32_Final( &crc );

	return crc;
}

#undef LogUserCmd

class IUserMessageBinder;

static ConVar cl_show_usermessage( side_registry(), "cl_show_usermessage", "0", 0, "Shows the incoming user messages for this client and dumps them out the type and size of the messages to the console. Setting this to 2 will display message contents as well" ); // filter incoming voice data

//-----------------------------------------------------------------------------
extern int CL_GetHltvReplayDelay();
std::shared_ptr< void > IUserMessageBinder::Parse( int32 nPassthroughFlags, const void *msg, int size, bool &bSilentIgnore )
{
	if ( size < 0 || size > NET_MAX_PAYLOAD )
	{
		return nullptr;
	}

	if ( CL_GetHltvReplayDelay() )
	{
		if ( nPassthroughFlags != m_nExpectedPassthroughInReplay )
		{
			// this is a wrong timeline message. Ignore it.
			bSilentIgnore = true;
			return nullptr;
		}
	}

	return m_Parse( msg, size );
}

//-----------------------------------------------------------------------------
CUserMessages::CUserMessages()
{
}

//-----------------------------------------------------------------------------
CUserMessages::~CUserMessages()
{
	m_UserMessageBinderMap.clear();
}

// we might wanna move this definition here::
// template< int msgType, typename PB_OBJECT_TYPE, int32 nExpectedPassthroughInReplay >
// virtual void * CUserMessageBinder::BindParams<msgType, PB_OBJECT_TYPE, nExpectedPassthroughInReplay >::Parse( int32 nPassthroughFlags, const void *msg, int size )

//-----------------------------------------------------------------------------
bool CUserMessages::DispatchUserMessage( int msg_type, int32 nPassthroughFlags, int size, const void *msg )
{
	auto index = m_UserMessageBinderMap.find( msg_type );
	if ( index == m_UserMessageBinderMap.end() )
	{
		DevMsg( "CUserMessages::DispatchUserMessage:  Unknown msg type %i\n", msg_type );
		return true;
	}
		
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	// Handlers may bind/unbind while invoked; map nodes are stable, so the reference stays valid.
	UserMessageBinderVec_t &binders = index->second[ nSlot ];
	if ( std::ssize( binders ) == 0 )
	{
		// not hooking a usermessage is acceptable, pretend we parsed it
		return true;
	}

	IUserMessageBinder *pHandler = binders[0];
	if ( !pHandler )
	{
		// not hooking a usermessage is acceptable, pretend we parsed it
		return true;
	}

	bool bSilentIgnore = false;
	std::shared_ptr< void > pMsg = pHandler->Parse( nPassthroughFlags, msg, size, bSilentIgnore );

	if ( bSilentIgnore )
	{
		//DevMsg( "CUserMessages::DispatchUserMessage: Silently ignoring alt-timeline msg type %i\n", msg_type );
		return true;
	}

	if ( !pMsg )
	{
		DevMsg( "CUserMessages::DispatchUserMessage:  Parse error msg type=%i\n", msg_type );
		contract_assert( 0 );
		return false;
	}

	//handle logging to the console if this is enabled
	if ( cl_show_usermessage.GetBool() )
	{
		Msg("DispatchUserMessage - %s(%d) bytes: %d\n", pHandler->TypeName(), msg_type, int( pHandler->ByteSize( pMsg.get() ) ) );
		//handle message content display if they have it set to a value > 1
		if( cl_show_usermessage.GetInt() > 1 )
			Msg("%s", pHandler->Describe( pMsg.get() ).c_str() );
	}

	bool result = true;
	for ( int i = 0; i < std::ssize( binders ); i++ )
	{
		IUserMessageBinder *h = binders[i];
		if ( h )
		{
			result = h->Invoke( pMsg.get() );
		}

		if ( !result )
		{
			break;
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
void CUserMessages::BindMessage( IUserMessageBinder *pMessageBinder )
{
	if ( !pMessageBinder )
	{
		return;
	}

	int message = pMessageBinder->GetType();

	auto [index, bInserted] = m_UserMessageBinderMap.try_emplace( message );
	if ( bInserted )
	{
		index->second.assign( MAX_SPLITSCREEN_PLAYERS, {} );
	}

	ASSERT_LOCAL_PLAYER_RESOLVABLE();

	index->second[ GET_ACTIVE_SPLITSCREEN_SLOT() ].push_back( pMessageBinder );
}

//-----------------------------------------------------------------------------
bool CUserMessages::UnbindMessage( IUserMessageBinder *pMessageBinder )
{
	if ( !pMessageBinder )
	{
		return true;
	}

	int message = pMessageBinder->GetType();

	auto index = m_UserMessageBinderMap.find( message );
	
	if ( index == m_UserMessageBinderMap.end() )
	{
		return false;
	}

	for ( int split = 0; split < MAX_SPLITSCREEN_PLAYERS; ++split )
	{
		auto &binders = index->second[ split ];
		for ( int i = 0; i < std::ssize( binders ); i++ )
		{
			if ( binders[i] == pMessageBinder )
			{
				binders.erase( binders.begin() + i );
				return true;
			}
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// Singleton
CUserMessages *UserMessages()
{
	static CUserMessages g_UserMessages;
	return &g_UserMessages;
}

class CEffectData;
class CBaseEntity;
class CBasePlayer;
struct FindClosestPassableSpace_TraceAdapter_t;

using namespace ::ks::reflect;

ConVar r_visualizetraces( side_registry(), "r_visualizetraces", "0", FCVAR_CHEAT );
ConVar developer(side_registry(), "developer", "0", FCVAR_RELEASE, "Set developer message level" ); // developer mode

float UTIL_VecToYaw( const Vector &vec )
{
	if (vec.y == 0 && vec.x == 0)
		return 0;
	
	float yaw = std::atan2( vec.y, vec.x );

	yaw = RAD2DEG(yaw);

	if (yaw < 0)
		yaw += 360;

	return yaw;
}

float UTIL_VecToPitch( const Vector &vec )
{
	float pitch = 0;
	Vector tmp = vec;
	if ( VectorNormalize( tmp ) > 0 )
	{
		pitch = RAD2DEG( std::asin( -tmp.z ) );
	}
	return pitch;
}

float UTIL_VecToYaw( const matrix3x4_t &matrix, const Vector &vec )
{
	Vector tmp = vec;
	VectorNormalize( tmp );

	float x = matrix[0][0] * tmp.x + matrix[1][0] * tmp.y + matrix[2][0] * tmp.z;
	float y = matrix[0][1] * tmp.x + matrix[1][1] * tmp.y + matrix[2][1] * tmp.z;

	if (x == 0.0f && y == 0.0f)
		return 0.0f;
	
	float yaw = std::atan2( -y, x );

	yaw = RAD2DEG(yaw);

	if (yaw < 0)
		yaw += 360;

	return yaw;
}

float UTIL_VecToPitch( const matrix3x4_t &matrix, const Vector &vec )
{
	float pitch = 0;
	Vector tmp = vec;
	if ( VectorNormalize( tmp ) > 0 )
	{
		float z = matrix[0][2] * tmp.x + matrix[1][2] * tmp.y + matrix[2][2] * tmp.z;
		pitch = RAD2DEG( std::asin( -z ) );
		if (pitch < 0)
			pitch += 360;
	}
	return pitch;
}

Vector UTIL_YawToVector( float yaw )
{
	Vector ret;
	
	ret.z = 0;
	float angle = DEG2RAD( yaw );
	SinCos( angle, &ret.y, &ret.x );

	return ret;
}

//-----------------------------------------------------------------------------
// Purpose: Helper function get get determinisitc random values for shared/prediction code
// Input  : seedvalue - 
//			*module - 
//			line - 
// Output : static int
//-----------------------------------------------------------------------------
static int SeedFileLineHash( int seedvalue, const char *sharedname, int additionalSeed )
{
	CRC32_t retval;

	CRC32_Init( &retval );

	//ensure cross-platform agreement
	seedvalue = ( seedvalue );
	additionalSeed = ( additionalSeed );

	CRC32_ProcessBuffer( &retval, (void *)&seedvalue, sizeof( int ) );
	CRC32_ProcessBuffer( &retval, (void *)&additionalSeed, sizeof( int ) );
	CRC32_ProcessBuffer( &retval, (void *)sharedname, V_strlen( sharedname ) );
	
	CRC32_Final( &retval );

	return (int)( retval );
}

float SharedRandomFloat( const char *sharedname, float flMinVal, float flMaxVal, int additionalSeed /*=0*/ )
{
	contract_assert( CBaseEntity::GetPredictionRandomSeed() != -1 );

	int seed = SeedFileLineHash( CBaseEntity::GetPredictionRandomSeed(), sharedname, additionalSeed );
	RandomSeed( seed );
	return RandomFloat( flMinVal, flMaxVal );
}

int SharedRandomInt( const char *sharedname, int iMinVal, int iMaxVal, int additionalSeed /*=0*/ )
{
	contract_assert( CBaseEntity::GetPredictionRandomSeed() != -1 );

	int seed = SeedFileLineHash( CBaseEntity::GetPredictionRandomSeed(), sharedname, additionalSeed );
	RandomSeed( seed );
	return RandomInt( iMinVal, iMaxVal );
}

Vector SharedRandomVector( const char *sharedname, float minVal, float maxVal, int additionalSeed /*=0*/ )
{
	contract_assert( CBaseEntity::GetPredictionRandomSeed() != -1 );

	int seed = SeedFileLineHash( CBaseEntity::GetPredictionRandomSeed(), sharedname, additionalSeed );
	RandomSeed( seed );
	// HACK:  Can't call RandomVector/Angle because it uses rand() not vstlib Random*() functions!
	// Get a random vector.
	Vector random_valve;
	random_valve.x = RandomFloat( minVal, maxVal );
	random_valve.y = RandomFloat( minVal, maxVal );
	random_valve.z = RandomFloat( minVal, maxVal );
	return random_valve;
}

QAngle SharedRandomAngle( const char *sharedname, float minVal, float maxVal, int additionalSeed /*=0*/ )
{
	contract_assert( CBaseEntity::GetPredictionRandomSeed() != -1 );

	int seed = SeedFileLineHash( CBaseEntity::GetPredictionRandomSeed(), sharedname, additionalSeed );
	RandomSeed( seed );

	// HACK:  Can't call RandomVector/Angle because it uses rand() not vstlib Random*() functions!
	// Get a random vector.
	Vector random_valve;
	random_valve.x = RandomFloat( minVal, maxVal );
	random_valve.y = RandomFloat( minVal, maxVal );
	random_valve.z = RandomFloat( minVal, maxVal );
	return QAngle( random_valve.x, random_valve.y, random_valve.z );
}

//-----------------------------------------------------------------------------
//
// Shared client/server trace filter code
//
//-----------------------------------------------------------------------------
bool PassServerEntityFilter( const IHandleEntity *pTouch, const IHandleEntity *pPass ) 
{
	if ( !pPass )
		return true;

	if ( pTouch == pPass )
		return false;

	const CBaseEntity *pEntTouch = EntityFromEntityHandle( pTouch );
	const CBaseEntity *pEntPass = EntityFromEntityHandle( pPass );
	if ( !pEntTouch || !pEntPass )
		return true;

	// don't clip against own missiles
	if ( pEntTouch->GetOwnerEntity() == pEntPass )
		return false;
	
	// don't clip against owner
	if ( pEntPass->GetOwnerEntity() == pEntTouch )
		return false;	

	return true;
}

//-----------------------------------------------------------------------------
// A standard filter to be applied to just about everything.
//-----------------------------------------------------------------------------
bool StandardFilterRules( IHandleEntity *pHandleEntity, int fContentsMask )
{
	CBaseEntity *pCollide = EntityFromEntityHandle( pHandleEntity );

	// Static prop case...
	if ( !pCollide )
		return true;

	SolidType_t solid = pCollide->GetSolid();
	const model_t *pModel = pCollide->GetModel();

	if ( ( modelinfo->GetModelType( pModel ) != mod_brush ) || (solid != SOLID_BSP && solid != SOLID_VPHYSICS) )
	{
		if ( (fContentsMask & CONTENTS_MONSTER) == 0 )
			return false;
	}

	// This code is used to cull out tests against see-thru entities
	if ( !(fContentsMask & CONTENTS_WINDOW) )
	{
		ClientRenderHandle_t hRender = pCollide->RenderHandle();
		if ( hRender == INVALID_CLIENT_RENDER_HANDLE )
		{
			// this is to handle invisible owned entities (e.g. bone followers) with transparent owners
			// not being filtered properly.  This is kind of a hack but seems to provide the behavior we 
			// want in all cases - if you are owned by something transparent and you yourself are not visible
			// we filter based on your owner's transparency
			CBaseEntity *pOwner = pCollide->GetOwnerEntity();
			if ( pOwner )
			{
				hRender = pOwner->RenderHandle();
			}
		}
		if ( g_pClientLeafSystem->GetTranslucencyType( hRender ) == RENDERABLE_IS_TRANSLUCENT )
			return false;
	}

	// FIXME: this is to skip BSP models that are entities that can be 
	// potentially moved/deleted, similar to a monster but doors don't seem to 
	// be flagged as monsters
	// FIXME: the FL_WORLDBRUSH looked promising, but it needs to be set on 
	// everything that's actually a worldbrush and it currently isn't
	if ( !(fContentsMask & CONTENTS_MOVEABLE) && (pCollide->GetMoveType() == MOVETYPE_PUSH))// !(touch->flags & FL_WORLDBRUSH) )
		return false;

	return true;
}

bool IsWeaponClassname( const char *pszClassName )
{
	// Look at the name of the class to determine if this is a weapon.
	return ( !V_strnicmp( "weapon_", pszClassName, WEAPON_CLASSNAME_PREFIX_LENGTH ) );
}

//-----------------------------------------------------------------------------
// Simple trace filter
//-----------------------------------------------------------------------------
CTraceFilterSimple::CTraceFilterSimple( const IHandleEntity *passedict, int collisionGroup,
										ShouldHitFunc_t pExtraShouldHitFunc )
{
	m_pPassEnt = passedict;
	m_collisionGroup = collisionGroup;
	m_pExtraShouldHitCheckFunction = pExtraShouldHitFunc;
}

//-----------------------------------------------------------------------------
// The trace filter!
//-----------------------------------------------------------------------------
bool CTraceFilterSimple::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	if ( !StandardFilterRules( pHandleEntity, contentsMask ) )
		return false;

	if ( m_pPassEnt )
	{
		if ( !PassServerEntityFilter( pHandleEntity, m_pPassEnt ) )
		{
			return false;
		}
	}

	// Don't test if the game code tells us we should ignore this collision...
	CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
	if ( !pEntity )
		return false;
	if ( !pEntity->ShouldCollide( m_collisionGroup, contentsMask ) )
		return false;
	if ( pEntity && !g_pGameRules->ShouldCollide( m_collisionGroup, pEntity->GetCollisionGroup() ) )
		return false;
	if ( m_pExtraShouldHitCheckFunction &&
		 (! ( m_pExtraShouldHitCheckFunction( pHandleEntity, contentsMask ) ) ) )
		return false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Trace filter that doesn't hit NPCs or players
//-----------------------------------------------------------------------------
bool CTraceFilterNoNPCsOrPlayer::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	if ( CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask ) )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		if ( !pEntity )
			return false;
		return (!pEntity->IsNPC() && !pEntity->IsPlayer());
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Trace filter that doesn't hit players
//-----------------------------------------------------------------------------
bool CTraceFilterNoPlayers::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	if ( CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask ) )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		if ( !pEntity )
			return false;

		return !pEntity->IsPlayer();
	}
	return false;
}

//-----------------------------------------------------------------------------
// Trace filter that skips two entities
//-----------------------------------------------------------------------------
CTraceFilterSkipTwoEntities::CTraceFilterSkipTwoEntities( const IHandleEntity *passentity, const IHandleEntity *passentity2, int collisionGroup ) :
	BaseClass( passentity, collisionGroup ), m_pPassEnt2(passentity2)
{
}

bool CTraceFilterSkipTwoEntities::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	contract_assert( pHandleEntity );
	if ( !PassServerEntityFilter( pHandleEntity, m_pPassEnt2 ) )
		return false;

	return BaseClass::ShouldHitEntity( pHandleEntity, contentsMask );
}

//-----------------------------------------------------------------------------
// Trace filter that can take a list of entities to ignore
//-----------------------------------------------------------------------------
CTraceFilterSimpleList::CTraceFilterSimpleList( int collisionGroup ) :
	CTraceFilterSimple( nullptr, collisionGroup )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTraceFilterSimpleList::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	if ( ks::find_index( m_PassEntities, pHandleEntity ) != -1 )
		return false;

	return CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );
}

//-----------------------------------------------------------------------------
// Purpose: Add an entity to my list of entities to ignore in the trace
//-----------------------------------------------------------------------------
void CTraceFilterSimpleList::AddEntityToIgnore( IHandleEntity *pEntity )
{
	m_PassEntities.push_back( pEntity );
}

void CTraceFilterSimpleList::AddEntitiesToIgnore( int nCount, IHandleEntity **ppEntities )
{
	int nIndex = std::ssize( m_PassEntities ); m_PassEntities.resize( m_PassEntities.size() + nCount );
	std::memcpy( &m_PassEntities[nIndex], ppEntities, nCount * sizeof( IHandleEntity* ) );
}

//-----------------------------------------------------------------------------
// Sweeps against a particular model, using collision rules 
//-----------------------------------------------------------------------------
void UTIL_TraceModel( const Vector &vecStart, const Vector &vecEnd, const Vector &hullMin, 
					  const Vector &hullMax, CBaseEntity *pentModel, int collisionGroup, trace_t *ptr )
{
	// Cull it....
	if ( pentModel && pentModel->ShouldCollide( collisionGroup, MASK_ALL ) )
	{
		Ray_t ray;
		ray.Init( vecStart, vecEnd, hullMin, hullMax );
		enginetrace->ClipRayToEntity( ray, MASK_ALL, pentModel, ptr ); 
	}
	else
	{
		std::memset( ptr, 0, sizeof(trace_t) );
		ptr->fraction = 1.0f;
	}
}

bool UTIL_EntityHasMatchingRootParent( CBaseEntity *pRootParent, CBaseEntity *pEntity )
{
	if ( pRootParent )
	{
		// NOTE: Don't let siblings/parents collide.
		if ( pRootParent == pEntity->GetRootMoveParent() )
			return true;
		if ( pEntity->GetOwnerEntity() && pRootParent == pEntity->GetOwnerEntity()->GetRootMoveParent() )
			return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Sweep an entity from the starting to the ending position 
//-----------------------------------------------------------------------------
class CTraceFilterEntity : public CTraceFilterSimple
{
	DECLARE_CLASS( CTraceFilterEntity, CTraceFilterSimple );

public:
	CTraceFilterEntity( CBaseEntity *pEntity, int nCollisionGroup ) 
		: CTraceFilterSimple( pEntity, nCollisionGroup )
	{
		m_pRootParent = pEntity->GetRootMoveParent();
		m_pEntity = pEntity;
		m_checkHash = g_EntityCollisionHash->IsObjectInHash(pEntity);
	}

	bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		if ( !pEntity )
			return false;

		// Check parents against each other
		// NOTE: Don't let siblings/parents collide.
		if ( UTIL_EntityHasMatchingRootParent( m_pRootParent, pEntity ) )
			return false;

		if ( m_checkHash )
		{
			if ( g_EntityCollisionHash->IsObjectPairInHash( m_pEntity, pEntity ) )
				return false;
		}

		return BaseClass::ShouldHitEntity( pHandleEntity, contentsMask );
	}

private:

	CBaseEntity *m_pRootParent;
	CBaseEntity *m_pEntity;
	bool		m_checkHash;
};

class CTraceFilterEntityIgnoreOther : public CTraceFilterEntity
{
	DECLARE_CLASS( CTraceFilterEntityIgnoreOther, CTraceFilterEntity );
public:
	CTraceFilterEntityIgnoreOther( CBaseEntity *pEntity, const IHandleEntity *pIgnore, int nCollisionGroup ) : 
		CTraceFilterEntity( pEntity, nCollisionGroup ), m_pIgnoreOther( pIgnore )
	{
	}

	bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		if ( pHandleEntity == m_pIgnoreOther )
			return false;

		return BaseClass::ShouldHitEntity( pHandleEntity, contentsMask );
	}

private:
	const IHandleEntity *m_pIgnoreOther;
};

//-----------------------------------------------------------------------------
// Sweeps a particular entity through the world 
//-----------------------------------------------------------------------------
void UTIL_TraceEntity( CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, unsigned int mask, trace_t *ptr )
{
	ICollideable *pCollision = pEntity->GetCollideable();
	//Vector vec = pCollision->OBBMaxs();
	// Adding this assertion here so game code catches it, but really the assertion belongs in the engine
	// because one day, rotated collideables will work!
	contract_assert( pCollision->GetCollisionAngles() == vec3_angle );

	CTraceFilterEntity traceFilter( pEntity, pCollision->GetCollisionGroup() );

	enginetrace->SweepCollideable( pCollision, vecAbsStart, vecAbsEnd, pCollision->GetCollisionAngles(), mask, &traceFilter, ptr );
}

void UTIL_TraceEntity( CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, 
					  unsigned int mask, const IHandleEntity *pIgnore, int nCollisionGroup, trace_t *ptr )
{
	ICollideable *pCollision;
	pCollision = pEntity->GetCollideable();

	// Adding this assertion here so game code catches it, but really the assertion belongs in the engine
	// because one day, rotated collideables will work!
	contract_assert( pCollision->GetCollisionAngles() == vec3_angle );

	CTraceFilterEntityIgnoreOther traceFilter( pEntity, pIgnore, nCollisionGroup );

	enginetrace->SweepCollideable( pCollision, vecAbsStart, vecAbsEnd, pCollision->GetCollisionAngles(), mask, &traceFilter, ptr );
}

void UTIL_TraceEntity( CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, 
					  unsigned int mask, ITraceFilter *pFilter, trace_t *ptr )
{
	ICollideable *pCollision;
	pCollision = pEntity->GetCollideable();

	// Adding this assertion here so game code catches it, but really the assertion belongs in the engine
	// because one day, rotated collideables will work!
	contract_assert( pCollision->GetCollisionAngles() == vec3_angle );

	enginetrace->SweepCollideable( pCollision, vecAbsStart, vecAbsEnd, pCollision->GetCollisionAngles(), mask, pFilter, ptr );
}

// ----
// This is basically a regular TraceLine that uses the FilterEntity filter.
void UTIL_TraceLineFilterEntity( CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, 
					   unsigned int mask, int nCollisionGroup, trace_t *ptr )
{
	CTraceFilterEntity traceFilter( pEntity, nCollisionGroup );
	UTIL_TraceLine( vecAbsStart, vecAbsEnd, mask, &traceFilter, ptr );
}

void UTIL_ClipTraceToPlayers( const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask, ITraceFilter *filter, trace_t *tr )
{
	trace_t playerTrace;
	Ray_t ray;
	float smallestFraction = tr->fraction;
	const float maxRange = 60.0f;

	ray.Init( vecAbsStart, vecAbsEnd );

	for ( int k = 1; k <= gpGlobals->maxClients; ++k )
	{
		CBasePlayer *player = UTIL_PlayerByIndex( k );

		if ( !player || !player->IsAlive() )
			continue;

		if ( player->IsDormant() )
			continue;

		if ( filter && filter->ShouldHitEntity( player, mask ) == false )
			continue;

		float range = DistanceToRay( player->WorldSpaceCenter(), vecAbsStart, vecAbsEnd );
		if ( range < 0.0f || range > maxRange )
			continue;

		enginetrace->ClipRayToEntity( ray, mask|CONTENTS_HITBOX, player, &playerTrace );
		if ( playerTrace.fraction < smallestFraction )
		{
			// we shortened the ray - save off the trace
			*tr = playerTrace;
			smallestFraction = playerTrace.fraction;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Make a tracer using a particle effect
//-----------------------------------------------------------------------------
void UTIL_ParticleTracer( const char *pszTracerEffectName, const Vector &vecStart, const Vector &vecEnd, 
				 int iEntIndex, int iAttachment, bool bWhiz )
{
	int iParticleIndex = GetParticleSystemIndex( pszTracerEffectName );
	UTIL_Tracer( vecStart, vecEnd, iEntIndex, iAttachment, 0, bWhiz, "ParticleTracer", iParticleIndex );
}

//-----------------------------------------------------------------------------
// Purpose: Make a tracer effect using the old, non-particle system, tracer effects.
//-----------------------------------------------------------------------------
void UTIL_Tracer( const Vector &vecStart, const Vector &vecEnd, int iEntIndex, 
				 int iAttachment, float flVelocity, bool bWhiz, const char *pCustomTracerName, int iParticleID )
{
	CEffectData data;
	data.m_vStart = vecStart;
	data.m_vOrigin = vecEnd;
	data.m_hEntity = ClientEntityList().EntIndexToHandle( iEntIndex );
	data.m_flScale = flVelocity;
	data.m_nHitBox = iParticleID;

	// Flags
	if ( bWhiz )
	{
		data.m_fFlags |= TRACER_FLAG_WHIZ;
	}

	if ( iAttachment != TRACER_DONT_USE_ATTACHMENT )
	{
		data.m_fFlags |= TRACER_FLAG_USEATTACHMENT;
		// Stomp the start, since it's not going to be used anyway
		data.m_nAttachmentIndex = iAttachment;
	}

	// Fire it off
	if ( pCustomTracerName )
	{
		DispatchEffect( pCustomTracerName, data );
	}
	else
	{
		DispatchEffect( "Tracer", data );
	}
}

void UTIL_BloodDrips( const Vector &origin, const Vector &direction, int color, int amount )
{
	if ( !UTIL_ShouldShowBlood( color ) )
		return;

	if ( color == DONT_BLEED || amount == 0 )
		return;

	if ( g_Language.GetInt() == LANGUAGE_GERMAN && color == BLOOD_COLOR_RED )
		color = 0;

	if ( g_pGameRules->IsMultiplayer() )
	{
		// scale up blood effect in multiplayer for better visibility
		amount *= 5;
	}

	if ( amount > 255 )
		amount = 255;

	if (color == BLOOD_COLOR_MECH)
	{
		g_pEffects->Sparks(origin);
		if (random_valve->RandomFloat(0, 2) >= 1)
		{
			UTIL_Smoke(origin, random_valve->RandomInt(10, 15), 10);
		}
	}
	else
	{
		// Normal blood impact
		UTIL_BloodImpact( origin, direction, color, amount );
	}
}	

//-----------------------------------------------------------------------------
// Purpose: Returns low violence settings
//-----------------------------------------------------------------------------
static ConVar	violence_hblood( side_registry(), "violence_hblood","1", 0, "Draw human blood" );
static ConVar	violence_hgibs( side_registry(), "violence_hgibs","1", 0, "Show human gib entities" );
static ConVar	violence_ablood( side_registry(), "violence_ablood","1", 0, "Draw alien blood" );
static ConVar	violence_agibs( side_registry(), "violence_agibs","1", 0, "Show alien gib entities" );

bool UTIL_IsLowViolence( void )
{
	// These convars are no longer necessary -- the engine is the final arbiter of
	// violence settings -- but they're here for legacy support and for testing low
	// violence when the engine is in normal violence mode.
	if ( !violence_hblood.GetBool() || !violence_ablood.GetBool() || !violence_hgibs.GetBool() || !violence_agibs.GetBool() )
		return true;

	return engine->IsLowViolence();
}

bool UTIL_ShouldShowBlood( int color )
{
	if ( color != DONT_BLEED )
	{
		if ( color == BLOOD_COLOR_RED )
		{
			return violence_hblood.GetBool();
		}
		else
		{
			return violence_ablood.GetBool();
		}
	}
	return false;
}

//------------------------------------------------------------------------------
// Purpose : Use trace to pass a specific decal type to the entity being decaled
// Input   :
// Output  :
//------------------------------------------------------------------------------
void UTIL_DecalTrace( trace_t *pTrace, char const *decalName )
{
	if (pTrace->fraction == 1.0)
		return;

	CBaseEntity *pEntity = pTrace->Ent<CBaseEntity>();
	if ( !pEntity )
		return;
	pEntity->DecalTrace( pTrace, decalName );
}

void UTIL_BloodDecalTrace( trace_t *pTrace, int bloodColor )
{
	if ( UTIL_ShouldShowBlood( bloodColor ) )
	{
		if ( bloodColor == BLOOD_COLOR_RED )
		{
			UTIL_DecalTrace( pTrace, "Blood" );
		}
		else if (bloodColor == BLOOD_COLOR_BRIGHTGREEN)
		{				
			UTIL_DecalTrace( pTrace, "GreenBlood" );
		}
		else
		{
			UTIL_DecalTrace( pTrace, "YellowBlood" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &pos - 
//			&dir - 
//			color - 
//			amount - 
//-----------------------------------------------------------------------------
void UTIL_BloodImpact( const Vector &pos, const Vector &dir, int color, int amount )
{
	CEffectData	data;

	data.m_vOrigin = pos;
	data.m_vNormal = dir;
	data.m_flScale = (float)amount;
	data.m_nColor = (unsigned char)color;

	DispatchEffect( "bloodimpact", data );
}

bool UTIL_IsSpaceEmpty( CBaseEntity *pMainEnt, const Vector &vMin, const Vector &vMax )
{
	Vector vHalfDims = ( vMax - vMin ) * 0.5f;
	Vector vCenter = vMin + vHalfDims;

	trace_t trace;
	int mask = (pMainEnt) ? pMainEnt->PhysicsSolidMaskForEntity() : MASK_SOLID;
	UTIL_TraceHull( vCenter, vCenter, -vHalfDims, vHalfDims, mask, pMainEnt, COLLISION_GROUP_NONE, &trace );

	bool bClear = ( trace.fraction == 1 && trace.allsolid != 1 && (trace.startsolid != 1) );
	return bClear;
}

bool UTIL_IsSpaceEmpty( CBaseEntity *pMainEnt, const Vector &vMin, const Vector &vMax, unsigned int mask, ITraceFilter *pFilter )
{
	Vector vHalfDims = ( vMax - vMin ) * 0.5f;
	Vector vCenter = vMin + vHalfDims;

	trace_t trace;
	UTIL_TraceHull( vCenter, vCenter, -vHalfDims, vHalfDims, mask, pFilter, &trace );

	bool bClear = ( trace.fraction == 1 && trace.allsolid != 1 && (trace.startsolid != 1) );
	return bClear;
}

float UTIL_WaterLevel( const Vector &position, float minz, float maxz )
{
	Vector midUp = position;
	midUp.z = minz;

	if ( !(UTIL_PointContents(midUp, MASK_WATER) & MASK_WATER) )
		return minz;

	midUp.z = maxz;
	if ( UTIL_PointContents(midUp, MASK_WATER) & MASK_WATER )
		return maxz;

	float diff = maxz - minz;
	while (diff > 1.0)
	{
		midUp.z = minz + diff/2.0;
		if ( UTIL_PointContents(midUp, MASK_WATER) & MASK_WATER )
		{
			minz = midUp.z;
		}
		else
		{
			maxz = midUp.z;
		}
		diff = maxz - minz;
	}

	return midUp.z;
}

void UTIL_StringToFloatArray( float *pVector, int count, const char *pString )
{
	char *pstr, *pfront, tempString[128];
	int	j;

	V_strncpy( tempString, pString, sizeof(tempString) );
	pstr = pfront = tempString;

	for ( j = 0; j < count; j++ )			// lifted from pr_edict.c
	{
		pVector[j] = std::atof( pfront );

		// skip any leading whitespace
		while ( *pstr && *pstr <= ' ' )
			pstr++;

		// skip to next whitespace
		while ( *pstr && *pstr > ' ' )
			pstr++;

		if (!*pstr)
			break;

		pstr++;
		pfront = pstr;
	}
	for ( j++; j < count; j++ )
	{
		pVector[j] = 0;
	}
}

void UTIL_StringToVector( float *pVector, const char *pString )
{
	UTIL_StringToFloatArray( pVector, 3, pString );
}

void UTIL_DecodeICE( unsigned char * buffer, int size, const unsigned char *key )
{
	ks::ScratchArena<> scratch;
	if ( !key )
		return;

	IceKey ice( 0 ); // level 0 = 64bit key
	ice.set( key ); // set key

	int blockSize = ice.blockSize();

	unsigned char *temp = (unsigned char *) scratch.AllocBytes( PAD_NUMBER( size, blockSize ) );
	unsigned char *p1 = buffer;
	unsigned char *p2 = temp;
				
	// encrypt data in 8 byte blocks
	int bytesLeft = size;
	while ( bytesLeft >= blockSize )
	{
		ice.decrypt( p1, p2 );
		bytesLeft -= blockSize;
		p1+=blockSize;
		p2+=blockSize;
	}

	// copy encrypted data back to original buffer
	V_memcpy( buffer, temp, size-bytesLeft );
}

void UTIL_EncodeICE( unsigned char * buffer, unsigned int size, const unsigned char *key )
{
	if ( !key )
		return;

	IceKey ice( 0 ); // level 0 = 64bit key
	ice.set( key ); // set key

	unsigned char *cipherText = buffer;
	unsigned char *plainText = buffer;
	uint bytesEncrypted = 0;

	while (bytesEncrypted < size)
	{
		ice.encrypt( plainText, cipherText );
		bytesEncrypted += 8;
		cipherText += 8;
		plainText += 8;
	}
}

// work-around since client header doesn't like inlined gpGlobals->curtime
float IntervalTimer::Now( void ) const
{
	return gpGlobals->curtime;
}

// work-around since client header doesn't like inlined gpGlobals->curtime
float CountdownTimer::Now( void ) const
{
	return gpGlobals->curtime;
}

IMPLEMENT_REFLECT_TABLE( IntervalTimer, DT_IntervalTimer );

IMPLEMENT_REFLECT_PREDMAP_NO_BASE( IntervalTimer );

IMPLEMENT_REFLECT_TABLE( CountdownTimer, DT_CountdownTimer );
IMPLEMENT_REFLECT_PREDMAP_NO_BASE( CountdownTimer );

IMPLEMENT_REFLECT_TABLE( CTimeline, DT_Timeline );

void CTimeline::ClearValues( void )
{
	Invalidate();

	std::memset( m_flValues.m_Value, 0, sizeof( m_flValues.m_Value ) );
	memset( m_nValueCounts.m_Value, 0, sizeof( m_nValueCounts.m_Value ) );
	m_nBucketCount = 0;
	m_flInterval = TIMELINE_INTERVAL_START;
	m_flFinalValue = 0.0f;
	m_bStopped = false;
}

void CTimeline::RecordValue( float flValue )
{
	if ( !HasStarted() || m_bStopped )
		return;

	int iBucket = GetCurrentBucket();

	contract_assert( iBucket >= 0 );

	while ( iBucket >= TIMELINE_ARRAY_SIZE )
	{
		Compress();
		iBucket = GetCurrentBucket();
	}

	if ( iBucket >= m_nBucketCount )
	{
		m_nBucketCount = iBucket + 1;
	}

	m_flValues.GetForModify( iBucket ) += flValue;
	m_nValueCounts.GetForModify( iBucket )++;

	if ( m_nCompressionType == TIMELINE_COMPRESSION_SUM || 
		 m_nCompressionType == TIMELINE_COMPRESSION_AVERAGE || 
		 m_nCompressionType == TIMELINE_COMPRESSION_AVERAGE_BLEND )
	{
		// Fill in blank preceding entries
		float flCurrentValue = m_flValues[ iBucket ] / m_nValueCounts[ iBucket ];

		int iPrecedingBucket = iBucket - 1;

		while ( iPrecedingBucket >= 0 && m_nValueCounts[ iPrecedingBucket ] <= 0 )
		{
			iPrecedingBucket--;
		}

		// Get the last logged value (or the current if this is the first)
		float flPrecedingValue = flCurrentValue;

		if ( iPrecedingBucket >= 0 )
		{
			// Last logged value
			if ( m_nCompressionType == TIMELINE_COMPRESSION_SUM )
			{
				flPrecedingValue = m_flValues[ iPrecedingBucket ];

				if ( m_nValueCounts[ iBucket ] == 1 )
				{
					// Sum in the previous bucket if this is the first value in this bucket
					m_flValues.GetForModify( iBucket ) += flPrecedingValue;
				}
			}
			else
			{
				flPrecedingValue = m_flFinalValue;
			}
		}

		// Number of buckets for blending from old to new value
		float flNumBuckets = ( iBucket - iPrecedingBucket ) + 1;

		if ( flNumBuckets >= 3.0f )
		{
			if ( m_nCompressionType == TIMELINE_COMPRESSION_AVERAGE_BLEND )
			{
				// Blend empty values between preceding and current
				float flInterpBucket = 1.0f;

				for ( int i = iPrecedingBucket + 1; i < iBucket; ++i, flInterpBucket += 1.0f )
				{
					float flInterp = flInterpBucket / flNumBuckets;
					m_flValues.Set( i, flPrecedingValue * ( 1.0f - flInterp ) + flCurrentValue * flInterp );
					m_nValueCounts.Set( i, 1 );
				}
			}
			else
			{
				// Set empty values to preceding value
				for ( int i = iPrecedingBucket + 1; i < iBucket; ++i )
				{
					m_flValues.Set( i, flPrecedingValue );
					m_nValueCounts.Set( i, 1 );
				}
			}
		}
	}

	m_flFinalValue = flValue;
}

float CTimeline::GetValue( int i ) const
{
	contract_assert( i >= 0 && i < m_nBucketCount );

	if ( i < 0 || i >= m_nBucketCount )
	{
		return 0.0f;
	}

	if ( m_nValueCounts[ i ] <= 0 )
	{
		return 0.0f;
	}
	else if ( i == 0 && m_nCompressionType == TIMELINE_COMPRESSION_SUM && m_nBucketCount > 1 )
	{
		// Aways start at 0 for sums!
		return 0.0;
	}
	else
	{
		switch ( m_nCompressionType )
		{
		case TIMELINE_COMPRESSION_AVERAGE:
		case TIMELINE_COMPRESSION_AVERAGE_BLEND:
			return m_flValues[ i ] / m_nValueCounts[ i ];

		case TIMELINE_COMPRESSION_SUM:
		case TIMELINE_COMPRESSION_COUNT_PER_INTERVAL:
		default:
			return m_flValues[ i ];
		}
	}
}

float CTimeline::GetValueAtInterp( float fInterp ) const
{
	if ( fInterp <= 0.0f )
	{
		return GetValue( 0 );
	}

	if ( fInterp >= 1.0f )
	{
		if ( m_nCompressionType == TIMELINE_COMPRESSION_SUM || 
			 m_nCompressionType == TIMELINE_COMPRESSION_COUNT_PER_INTERVAL )
		{
			return GetValue( Count() - 1 );
		}
		else
		{
			return m_flFinalValue;
		}
	}

	float fBucket = fInterp * ( Count() - 1 );
	int nBucket = fBucket;
	fBucket -= nBucket;

	float fValue = GetValue( nBucket );
	float fNextValue = GetValue( nBucket + 1 );

	return fValue * ( 1.0f - fBucket ) + fNextValue * fBucket;
}

void CTimeline::Compress( void )
{
	int i, j;

	switch ( m_nCompressionType )
	{
	case TIMELINE_COMPRESSION_SUM:
		for ( i = 0, j = 0; i < TIMELINE_ARRAY_SIZE; i += 2, ++j )
		{
			m_flValues.GetForModify( j ) = std::max( m_flValues[ i ], m_flValues[ i + 1 ] );
			m_nValueCounts.GetForModify( j ) = m_nValueCounts[ i ] + m_nValueCounts[ i + 1 ];
		}
		break;

	default:
		for ( i = 0, j = 0; i < TIMELINE_ARRAY_SIZE; i += 2, ++j )
		{
			m_flValues.GetForModify( j ) = m_flValues[ i ] + m_flValues[ i + 1 ];
			m_nValueCounts.GetForModify( j ) = m_nValueCounts[ i ] + m_nValueCounts[ i + 1 ];
		}
		break;
	}

	int nRemainingBytes = ( TIMELINE_ARRAY_SIZE - j ) * sizeof( m_flValues[0] );

	std::memset( &( m_flValues.GetForModify( j ) ), 0, nRemainingBytes );
	memset( &( m_nValueCounts.GetForModify( j ) ), 0, nRemainingBytes );

	m_flInterval *= 2.0f;

	m_nBucketCount = j;
}

	CBasePlayer *UTIL_PlayerByIndex( int entindex )
	{	
		// Sanity check the index being passed in
		if ( entindex < 1 || entindex > gpGlobals->maxClients )
			return nullptr;

		return ToBasePlayer( ClientEntityList().GetEnt( entindex ) );
	}

unsigned short UTIL_GetAchievementEventMask( void )
{
	CRC32_t mapCRC;
	CRC32_Init( &mapCRC );

	char lowercase[ 256 ];
	V_FileBase( engine->GetLevelName(), lowercase, sizeof( lowercase ) );
	V_strlower( lowercase );

	CRC32_ProcessBuffer( &mapCRC, lowercase, V_strlen( lowercase ) );
	CRC32_Final( &mapCRC );

	return ( mapCRC & 0xFFFF );
}

const char* ReadAndAllocStringValue( KeyValues *pSub, const char *pName, const char *pFilename )
{
	const char *pValue = pSub->GetString( pName, nullptr );
	if ( !pValue )
	{
		if ( pFilename )
		{
			DevWarning( "Can't get key value	'%s' from file '%s'.\n", pName, pFilename );
		}
		return "";
	}

	int len = V_strlen( pValue ) + 1;
	char *pAlloced = new char[ len ];
	contract_assert( pAlloced );
	V_strncpy( pAlloced, pValue, len );
	return pAlloced;
}

int UTIL_StringFieldToInt( const char *szValue, const char **pValueStrings, int iNumStrings )
{
	if ( !szValue || !szValue[0] )
		return -1;

	for ( int i = 0; i < iNumStrings; i++ )
	{
		if ( FStrEq(szValue, pValueStrings[i]) )
			return i;
	}

	contract_assert( 0 );
	return -1;
}

static char s_NumBitsInNibble[ 16 ] = 
{
	0, // 0000 = 0
	1, // 0001 = 1
	1, // 0010 = 2
	2, // 0011 = 3
	1, // 0100 = 4
	2, // 0101 = 5
	2, // 0110 = 6
	3, // 0111 = 7
	1, // 1000 = 8
	2, // 1001 = 9
	2, // 1010 = 10
	3, // 1011 = 11
	2, // 1100 = 12
	3, // 1101 = 13
	3, // 1110 = 14
	4, // 1111 = 15
};

int UTIL_CountNumBitsSet( unsigned int nVar )
{
	int nNumBits = 0;

	while ( nVar > 0 )
	{
		// Look up and add in bits in the bottom nibble
		nNumBits += s_NumBitsInNibble[ nVar & 0x0f ];

		// Shift one nibble to the right
		nVar >>= 4;
	}

	return nNumBits;
}

int UTIL_CountNumBitsSet( uint64 nVar )
{
	int nNumBits = 0;

	while ( nVar > 0 )
	{
		// Look up and add in bits in the bottom nibble
		nNumBits += s_NumBitsInNibble[ nVar & 0x0f ];

		// Shift one nibble to the right
		nVar >>= 4;
	}

	return nNumBits;
}

bool UTIL_FindClosestPassableSpace( const Vector &vOriginalCenter, const Vector &vExtents, const Vector &vIndecisivePush, unsigned int iIterations, Vector &vCenterOut, int nAxisRestrictionFlags, FindClosestPassableSpace_TraceAdapter_t *pTraceAdapter )
{
	contract_assert( vExtents != vec3_origin );

	trace_t traces[2];
	Ray_t entRay;
	entRay.m_Extents = vExtents;
	entRay.m_IsRay = false;
	entRay.m_IsSwept = true;
	entRay.m_StartOffset = vec3_origin;

	Vector vOriginalExtents = vExtents;
	Vector vCenter = vOriginalCenter;
	Vector vGrowSize = vExtents * (1.0f / (float)(iIterations + 1));
	Vector vCurrentExtents = vExtents - vGrowSize;

	int iLargestExtent = 0;
	{
		float fLargestExtent = vOriginalExtents[0];
		for( int i = 1; i != 3; ++i )
		{
			if( vOriginalExtents[i] > fLargestExtent )
			{
				iLargestExtent = i;
				fLargestExtent = vOriginalExtents[i];
			}
		}
	}

	Ray_t testRay;
	testRay.m_Extents = vGrowSize;
	testRay.m_IsRay = false;
	testRay.m_IsSwept = true;
	testRay.m_StartOffset = vec3_origin;

	float fOriginalExtentDists[8]; //distance between extents
	//generate distance lookup. We reference this by XOR'ing the indices of two extents to find the axis of difference
	{
		//Since the ratios of lengths never change, we're going to normalize these distances to a value so we can simply scale on each iteration
		//We've picked the largest extent as the basis simply because it's nonzero
		float fNormalizer = 1.0f / vOriginalExtents[iLargestExtent];
				
		float fXDiff = vOriginalExtents.x * 2.0f * fNormalizer;
		float fXSqr = fXDiff * fXDiff;

		float fYDiff = vOriginalExtents.y * 2.0f * fNormalizer;
		float fYSqr = fYDiff * fYDiff;

		float fZDiff = vOriginalExtents.z * 2.0f * fNormalizer;
		float fZSqr = fZDiff * fZDiff;

		fOriginalExtentDists[0] = 0.0f; //should never get hit
		fOriginalExtentDists[1] = fXDiff; //line along x axis		
		fOriginalExtentDists[2] = fYDiff; //line along y axis
		fOriginalExtentDists[3] = std::sqrt( fXSqr + fYSqr ); //diagonal perpendicular to z-axis
		fOriginalExtentDists[4] = fZDiff; //line along z axis
		fOriginalExtentDists[5] = sqrt( fXSqr + fZSqr ); //diagonal perpendicular to y-axis
		fOriginalExtentDists[6] = sqrt( fYSqr + fZSqr ); //diagonal perpendicular to x-axis
		fOriginalExtentDists[7] = sqrt( fXSqr + fYSqr + fZSqr ); //diagonal on all axes
	}

	Vector ptExtents[8]; //ordering is going to be like 3 bits, where 0 is a min on the related axis, and 1 is a max on the same axis, axis order x y z
	float fExtentsValidation[8]; //some points are more valid than others, and this is our measure

	vCenter.z += 0.001f; //to satisfy m_IsSwept on first pass
	
	unsigned int iFailCount;
	for( iFailCount = 0; iFailCount != iIterations; ++iFailCount )
	{
		//float fXDistribution[2] = { -vCurrentExtents.x, vCurrentExtents.x };
		//float fYDistribution[3] = { -vCurrentExtents.y, 0.0f, vCurrentExtents.y };
		//float fZDistribution[5] = { -vCurrentExtents.z, 0.0f, 0.0f, 0.0f, vCurrentExtents.z };

		//hey look, they can overlap
		float fExtentDistribution[6];
		fExtentDistribution[ 0 ] = vCenter.z + ( ( ( nAxisRestrictionFlags & FL_AXIS_DIRECTION_NZ ) == 0 ) ? ( -vCurrentExtents.z ) : ( 0.0f ) );	// Z-
		fExtentDistribution[ 1 ] = vCenter.x + ( ( ( nAxisRestrictionFlags & FL_AXIS_DIRECTION_NX ) == 0 ) ? ( -vCurrentExtents.x ) : ( 0.0f ) );	// X-
		fExtentDistribution[ 2 ] = vCenter.x + ( ( ( nAxisRestrictionFlags & FL_AXIS_DIRECTION_X ) == 0 ) ? ( vCurrentExtents.x ) : ( 0.0f ) );		// X+
		fExtentDistribution[ 3 ] = vCenter.y + ( ( ( nAxisRestrictionFlags & FL_AXIS_DIRECTION_NY ) == 0 ) ? ( -vCurrentExtents.y ) : ( 0.0f ) );	// Y-
		fExtentDistribution[ 4 ] = vCenter.z + ( ( ( nAxisRestrictionFlags & FL_AXIS_DIRECTION_Z ) == 0 ) ? ( vCurrentExtents.z ) : ( 0.0f ) );		// Z+
		fExtentDistribution[ 5 ] = vCenter.y + ( ( ( nAxisRestrictionFlags & FL_AXIS_DIRECTION_Y ) == 0 ) ? ( vCurrentExtents.y ) : ( 0.0f ) );		// Y+

		float *pXDistribution = &fExtentDistribution[1];
		float *pYDistribution = &fExtentDistribution[3];

		bool bExtentInvalid[8];
		float fExtentDists[8];
		bool bAnyInvalid = false;
		for( int i = 0; i != 8; ++i )
		{
			ptExtents[i].x = pXDistribution[i & (1<<0)]; //fExtentDistribution[(0 or 1) + 1]
			ptExtents[i].y = pYDistribution[i & (1<<1)]; //fExtentDistribution[(0 or 2) + 3]
			ptExtents[i].z = fExtentDistribution[i & (1<<2)]; //fExtentDistribution[(0 or 4)]

			fExtentsValidation[i] = 0.0f;
			bExtentInvalid[i] = pTraceAdapter->pPointOutsideWorldFunc( ptExtents[i], pTraceAdapter );
			bAnyInvalid |= bExtentInvalid[i];
			fExtentDists[i] = fOriginalExtentDists[i] * vExtents[iLargestExtent];
		}

		//trace from all extents to all other extents and rate the validity
		{
			unsigned int counters[2]; //I know it's weird, get over it
			for( counters[0] = 0; counters[0] != 7; ++counters[0] )
			{
				for( counters[1] = counters[0] + 1; counters[1] != 8; ++counters[1] )
				{
					for( int i = 0; i != 2; ++i )
					{
						if( bExtentInvalid[counters[i]] )
						{
							traces[i].startsolid = true;
							traces[i].fraction = 0.0f;
						}
						else
						{
							testRay.m_Start = ptExtents[counters[i]];
							testRay.m_Delta = ptExtents[counters[1-i]] - ptExtents[counters[i]];
							pTraceAdapter->pTraceFunc( testRay, &traces[i], pTraceAdapter );
						}
					}

					float fDistance = fExtentDists[counters[0] ^ counters[1]];

					for( int i = 0; i != 2; ++i )
					{
						if( (traces[i].fraction == 1.0f) && (traces[1-i].fraction != 1.0f) )
						{
							//One sided collision >_<
							traces[i].startsolid = true;
							traces[i].fraction = 0.0f;
							break;
						}
					}

					for( int i = 0; i != 2; ++i )
					{
						if( traces[i].startsolid )
						{
							bExtentInvalid[counters[i]] = true;
							bAnyInvalid = true;
						}
						else
						{
							fExtentsValidation[counters[i]] += traces[i].fraction * fDistance;
						}
					}
				}
			}
		}

		//optimally we should do this check before tracing extents. But one sided collision is a bitch
		if( !bAnyInvalid )
		{
			//try to trace back to the starting position (if we start in valid, the endpoint will be closer to the original center)
			entRay.m_Start = vCenter;
			entRay.m_Delta = vOriginalCenter - vCenter;

			pTraceAdapter->pTraceFunc( entRay, &traces[0], pTraceAdapter );
			if( traces[0].startsolid == false )
			{
				//damned one sided collision
				vCenterOut = traces[0].endpos;
				return !pTraceAdapter->pPointOutsideWorldFunc( vCenterOut, pTraceAdapter );
			}
		}

		//find the direction to move based on the extent validity
		{
			Vector vNewOriginDirection( 0.0f, 0.0f, 0.0f );
			float fTotalValidation = 0.0f;
			for( int i = 0; i != 8; ++i )
			{
				if( !bExtentInvalid[i] )
				{
					vNewOriginDirection += (ptExtents[i] - vCenter) * fExtentsValidation[i];
					fTotalValidation += fExtentsValidation[i];
				}
			}

			if( fTotalValidation != 0.0f )
			{
				vCenter += (vNewOriginDirection / fTotalValidation);

				//increase sizing
				testRay.m_Extents += vGrowSize; //increase the ray size
				vCurrentExtents -= vGrowSize; //while reducing the overall test region size (so outermost ray extents are the same)
			}
			else
			{
				//no point was valid, apply the indecisive vector
				vCenter += vIndecisivePush;

				//reset sizing
				testRay.m_Extents = vGrowSize;
				vCurrentExtents = vOriginalExtents - vGrowSize;
			}
		}
	}

	//Warning( "FindClosestPassableSpace() failure.\n" );

	// X360TBD: Hits in portal devtest
	//AssertMsg( IsGameConsole() || iFailCount != iIterations, "FindClosestPassableSpace() failure." );
	vCenterOut = vOriginalCenter;
	return false;
}

static void EngineTraceFunc( const Ray_t &ray, trace_t *pResult, FindClosestPassableSpace_TraceAdapter_t *pTraceAdapter )
{
	enginetrace->TraceRay( ray, pTraceAdapter->fMask, pTraceAdapter->pTraceFilter, pResult );
}

static bool EnginePointOutsideWorldFunc( const Vector &vTest, FindClosestPassableSpace_TraceAdapter_t *pTraceAdapter )
{
	return enginetrace->PointOutsideWorld( vTest );
}

bool UTIL_FindClosestPassableSpace( const Vector &vOriginalCenter, const Vector &vExtents, const Vector &vIndecisivePush, ITraceFilter *pTraceFilter, unsigned int fMask, unsigned int iIterations, Vector &vCenterOut, int nAxisRestrictionFlags )
{
	FindClosestPassableSpace_TraceAdapter_t adapter;
	adapter.pTraceFunc = EngineTraceFunc;
	adapter.pPointOutsideWorldFunc = EnginePointOutsideWorldFunc;
	adapter.pTraceFilter = pTraceFilter;
	adapter.fMask = fMask;

	return UTIL_FindClosestPassableSpace( vOriginalCenter, vExtents, vIndecisivePush, iIterations, vCenterOut, nAxisRestrictionFlags, &adapter );
}

bool UTIL_FindClosestPassableSpace( CBaseEntity *pEntity, const Vector &vIndecisivePush, unsigned int fMask, unsigned int iIterations, Vector &vOriginOut, Vector *pStartingPosition, int nAxisRestrictionFlags ) //assumes the object is already in a mostly passable space
{
	// Don't ever do this to entities with a move parent
	if ( pEntity->GetMoveParent() )
	{
		vOriginOut = pEntity->GetAbsOrigin();
		return false;
	}

	Vector vEntityMaxs;
	Vector vEntityMins;
	pEntity->CollisionProp()->WorldSpaceAABB( &vEntityMins, &vEntityMaxs );

	Vector ptEntityCenter = ((vEntityMins + vEntityMaxs) / 2.0f);
	//vEntityMins -= ptEntityCenter;
	vEntityMaxs -= ptEntityCenter;
	
	Vector vCenterToOrigin = pEntity->GetAbsOrigin() - ptEntityCenter;
	if( pStartingPosition != nullptr )
	{
		Vector vOriginOffset = (*pStartingPosition) - pEntity->GetAbsOrigin();
		ptEntityCenter += vOriginOffset;
	}

	CTraceFilterSimple traceFilter( pEntity, pEntity->GetCollisionGroup() );

	Vector vResult;
	bool bSuccess = UTIL_FindClosestPassableSpace( ptEntityCenter, vEntityMaxs, vIndecisivePush, &traceFilter, fMask, iIterations, vResult );
	vResult += vCenterToOrigin;
	vOriginOut = vResult;

	return bSuccess;
}

bool UTIL_FindClosestPassableSpace( CBaseEntity *pEntity, const Vector &vIndecisivePush, unsigned int fMask, Vector *pStartingPosition, int nAxisRestrictionFlags )
{
	Vector vNewPos;
	bool bWorked = UTIL_FindClosestPassableSpace( pEntity, vIndecisivePush, fMask, 100, vNewPos, pStartingPosition, nAxisRestrictionFlags );
	if( bWorked )
	{
		pEntity->SetAbsOrigin( vNewPos );
	}
	return bWorked;
}

//-----------------------------------------------------------------------------
// Purpose: Retrieves the MOD directory for the active game (ie. "hl2")
//-----------------------------------------------------------------------------

bool UTIL_GetModDir( char *lpszTextOut, unsigned int nSize )
{
	// Must pass in a buffer at least large enough to hold the desired string
	const char *pGameDir = CommandLine()->ParmValue( "-game", "hl2" );
	contract_assert( std::strlen(pGameDir) <= nSize );
	if ( std::strlen(pGameDir) > nSize )
		return false;

	V_strncpy( lpszTextOut, pGameDir, nSize );
	if ( V_strnchr( lpszTextOut, '/', nSize ) || V_strnchr( lpszTextOut, '\\', nSize ) )
	{
		// Strip the last directory off (which will be our game dir)
		V_StripLastDir( lpszTextOut, nSize );

		// Find the difference in string lengths and take that difference from the original string as the mod dir
		int dirlen = V_strlen( lpszTextOut );
		V_strncpy( lpszTextOut, pGameDir + dirlen, V_strlen( pGameDir ) - dirlen + 1 );
	}

	return true;
}

//#define FRUSTUM_DEBUGGING //for dumping some clipping information in UTIL_CalcFrustumThroughConvexPolygon

int UTIL_CalcFrustumThroughConvexPolygon( const Vector *pPolyVertices, int iPolyVertCount, const Vector &vFrustumOrigin, const VPlane *pInputFrustumPlanes, int iInputFrustumPlanes, VPlane *pOutputFrustumPlanes, int iMaxOutputPlanes, int iPreserveCount )
{
	ks::ScratchArena<> scratch;
	contract_assert( iPreserveCount <= iMaxOutputPlanes );
	contract_assert( iPreserveCount <= iInputFrustumPlanes );
	if( iPolyVertCount < 3 )
		return 0;

	int iMaxComplexity = iMaxOutputPlanes - iPreserveCount;
	
	Vector *pClippedVerts;
	int iClippedVertCount;
	if( iInputFrustumPlanes > 0 )
	{
		//clip the polygon by the input frustum
		int iAllocSize = iPolyVertCount + iInputFrustumPlanes;

		Vector *pWorkVerts[2];
		pWorkVerts[0] = scratch.Alloc<Vector>( iAllocSize * 2 ); //possible to add 1 point per cut, iPolyVertCount starting points, iInputFrustumPlaneCount cuts
		pWorkVerts[1] = pWorkVerts[0] + iAllocSize;

		//clip by first plane and put output into pInVerts
		iClippedVertCount = ClipPolyToPlane( (Vector *)pPolyVertices, iPolyVertCount, pWorkVerts[0], pInputFrustumPlanes[0].m_Normal, pInputFrustumPlanes[0].m_Dist, 0.01f );

		//clip by other planes and flipflop in and out pointers
		for( int i = 1; i != iInputFrustumPlanes; ++i )
		{
			if( iClippedVertCount < 3 )
				return 0; //nothing left in the frustum

			iClippedVertCount = ClipPolyToPlane( pWorkVerts[(i & 1) ^ 1], iClippedVertCount, pWorkVerts[i & 1], pInputFrustumPlanes[i].m_Normal, pInputFrustumPlanes[i].m_Dist, 0.01f );
		}

		if( iClippedVertCount < 3 )
			return false; //nothing left in the frustum

		pClippedVerts = pWorkVerts[(iInputFrustumPlanes & 1) ^ 1];
	}
	else
	{
		//no input frustum
		if( iPolyVertCount > iMaxComplexity )
		{
			//we'll need to reduce our output frustum, copy the input polygon
			pClippedVerts = scratch.Alloc<Vector>( iPolyVertCount );
			std::memcpy( pClippedVerts, pPolyVertices, sizeof( Vector ) * iPolyVertCount );
		}
		else
		{
			//we won't need to simplify the polygon to reduce output planes, just point at the input polygon
			pClippedVerts = (Vector *)pPolyVertices;
		}
		iClippedVertCount = iPolyVertCount;
	}

	contract_assert( iClippedVertCount <= (iPolyVertCount + iInputFrustumPlanes) );

	if( iClippedVertCount > iMaxComplexity )
	{
		float *fLineLengthSqr = scratch.Alloc<float>( iClippedVertCount );

		for( int i = 0; i != (iClippedVertCount - 1); ++i )
		{
			fLineLengthSqr[i] = (pClippedVerts[i + 1] - pClippedVerts[i]).LengthSqr();
		}
		fLineLengthSqr[(iClippedVertCount - 1)] = (pClippedVerts[0] - pClippedVerts[(iClippedVertCount - 1)]).LengthSqr(); //wrap around

		while( iClippedVertCount > iMaxComplexity ) //vert count == number of planes we need to bound the polygon
		{
			//we have too many verts to represent this accurately in the output frustum plane count
			//so, we're going to eliminate the smallest sides one at a time and bridge the surrounding sides until we're down to iMaxComplexity
			float fMinSide = fLineLengthSqr[0];
			int iMinSideFirstPoint = 0;
			int iOldVertCount = iClippedVertCount;
			--iClippedVertCount; //we're going to decrement this sometime in this block, it makes math easier to do it now

			for( int i = 1; i != iOldVertCount; ++i )
			{
				if( fLineLengthSqr[i] < fMinSide )
				{
					fMinSide = fLineLengthSqr[i];
					iMinSideFirstPoint = i;
				}
			}

			int i1, i2, i3, i4;
			i1 = (iMinSideFirstPoint + iClippedVertCount)%(iOldVertCount); //-1 with a wrap
			i2 = iMinSideFirstPoint;
			i3 = (iMinSideFirstPoint + 1)%(iOldVertCount);
			i4 = (iMinSideFirstPoint + 2)%(iOldVertCount);

			Vector *p1, *p2, *p3, *p4;
			p1 = &pClippedVerts[i1];
			p2 = &pClippedVerts[i2];
			p3 = &pClippedVerts[i3]; //this is the one we'll actually be dropping in the merge
			p4 = &pClippedVerts[i4];

			//now we know the two points that we have to merge to one, project and make a merged point from the surrounding lines
			//if( fMinSide >= 0.1f ) //only worth doing the math if it's actually going to be accurate and make a difference
			{
				//http://mathworld.wolfram.com/Line-LineIntersection.html (20)
				Vector vA = *p2 - *p1;
				Vector vB = *p4 - *p3;
				Vector vC = *p3 - *p1;
				Vector vCxB = vC.Cross( vB );
				Vector vAxB = vA.Cross( vB );
				float fS = vCxB.Dot(vAxB)/vAxB.LengthSqr();

				*p2 = *p1 + (vA * fS);

				fLineLengthSqr[i1] = (*p2 - *p1).LengthSqr();
			}
			
			fLineLengthSqr[i2] = (*p4 - *p2).LengthSqr(); //must do this BEFORE possibly shifting points p4+ left

			if( i3 < i4 ) //not the last point in the array
			{
				int iElementShift = (iOldVertCount - i4);

				//eliminate p3, we merged p2+p3 and already stored the result in p2
				std::memmove( p3, p4, sizeof( Vector ) * iElementShift );
				memmove( &fLineLengthSqr[i3], &fLineLengthSqr[i4], sizeof( float ) * iElementShift );
			}
		}

	}

	//generate planes defined by each line around the convex and the frustum origin
	{
		int iFlipNormalsXOR = 0; //this algorithm was written assuming polygon vertices would be in a clockwise order from the perspective of vFrustumOrigin, some logic needs to flip if the inverse is true
		{
			Vector vLine1 = pPolyVertices[1] - pPolyVertices[0];
			Vector vLine2 = pPolyVertices[2] - pPolyVertices[1];
			Vector vFrontFace = vLine2.Cross( vLine1 );

			iFlipNormalsXOR = (vFrontFace.Dot( vFrustumOrigin - pPolyVertices[0] ) < 0.0f) ? 1 : 0; //this will assist in reversing the normal by flipping the cross product
		}		

		Vector vTemp[2];
		vTemp[0] = pClippedVerts[iClippedVertCount - 1] - vFrustumOrigin;
		for( int i = 0; i != iClippedVertCount; ++i )
		{
			int iIndexing = i & 1; //we can carry over the line computation from one iteration to the next, flip which order we look at the temps with
			vTemp[iIndexing ^ 1] = pClippedVerts[i] - vFrustumOrigin; 

			Vector vNormal = vTemp[iIndexing ^ iFlipNormalsXOR].Cross( vTemp[(iIndexing ^ iFlipNormalsXOR) ^ 1] ); //vLine1.Cross( vLine2 );
			vNormal.NormalizeInPlace();

			pOutputFrustumPlanes[i].Init( vNormal, vNormal.Dot( vFrustumOrigin ) );
		}
	}

	//preserve input planes on request
	if( iPreserveCount > 0 )
	{
		std::memcpy( &pOutputFrustumPlanes[iClippedVertCount], &pInputFrustumPlanes[iInputFrustumPlanes - iPreserveCount], sizeof( VPlane ) * iPreserveCount );
	}

	return (iClippedVertCount + iPreserveCount);
}

//-----------------------------------------------------------------------------
// class CFlaggedEntitiesEnum
//-----------------------------------------------------------------------------

CFlaggedEntitiesEnum::CFlaggedEntitiesEnum( CBaseEntity **pList, int listMax, int flagMask )
{
	m_pList = pList;
	m_listMax = listMax;
	m_flagMask = flagMask;
	m_count = 0;
}

bool CFlaggedEntitiesEnum::AddToList( CBaseEntity *pEntity )
{
	if ( m_count >= m_listMax )
	{
		contract_assert( false && "reached enumerated list limit.  Increase limit, decrease radius, or make it so entity flags will work for you" );
		return false;
	}
	m_pList[m_count] = pEntity;
	m_count++;
	return true;
}

IterationRetval_t CFlaggedEntitiesEnum::EnumElement( IHandleEntity *pHandleEntity )
{
	IClientEntity *pClientEntity = cl_entitylist->GetClientEntityFromHandle( pHandleEntity->GetRefEHandle() );
	CBaseEntity *pEntity = pClientEntity ? EntityCast<CBaseEntity>( pClientEntity->GetBaseEntity() ) : nullptr;
	if ( pEntity )
	{
		if ( m_flagMask && !(pEntity->GetFlags() & m_flagMask) )	// Does it meet the criteria?
			return ITERATION_CONTINUE;

		if ( !AddToList( pEntity ) )
			return ITERATION_STOP;
	}

	return ITERATION_CONTINUE;
}

int UTIL_EntitiesAlongRay( const Ray_t &ray, CFlaggedEntitiesEnum *pEnum )
{
	::partition->EnumerateElementsAlongRay( PARTITION_CLIENT_NON_STATIC_EDICTS, ray, false, pEnum );
	return pEnum->GetCount();
}

//-----------------------------------------------------------------------------
// Returns the length of the next command
//-----------------------------------------------------------------------------
void UTIL_GetNextCommandLength( const char *pText, int nMaxLen, int *pCommandLength, int *pNextCommandOffset )
{
	int nCommandLength = 0;
	int nNextCommandOffset;
	bool bIsQuoted = false;
	bool bIsCommented = false;
	for ( nNextCommandOffset=0; nNextCommandOffset < nMaxLen; ++nNextCommandOffset, nCommandLength += bIsCommented ? 0 : 1 )
	{
		char c = pText[nNextCommandOffset];
		if ( !bIsCommented )
		{
			if ( c == '"' )
			{
				bIsQuoted = !bIsQuoted;
				continue;
			}

			// don't break if inside a C++ style comment
			if ( !bIsQuoted && c == '/' )
			{
				bIsCommented = ( nNextCommandOffset < nMaxLen-1 ) && pText[nNextCommandOffset+1] == '/';
				if ( bIsCommented )
				{
					++nNextCommandOffset;
					continue;
				}
			}

			// don't break if inside a quoted string
			if ( !bIsQuoted && c == ';' )
				break;	
		}

		// FIXME: This is legacy behavior; should we not break if a \n is inside a quoted string?
		if ( c == '\n' )
			break;
	}

	*pCommandLength = nCommandLength;
	*pNextCommandOffset = nNextCommandOffset;
}

/**
 *  remove double spaces and empty bold/italic HTML tags from a string
 */
void UTIL_TrimEmptyWhitespaceFromHTML( wchar_t* pszDescWriter, std::size_t descWriterByteSize, const wchar_t* pszDescReader )
{
	std::size_t writerArraySize = descWriterByteSize / sizeof( wchar_t );
	wchar_t* pszDescWriterEnd = pszDescWriter + writerArraySize - 1; // leave 1 character for '\0' terminator

	while ( *pszDescReader != 0 && pszDescWriter < pszDescWriterEnd )
	{
		if ( !std::wcsncmp( pszDescReader, L" <b></b> ", 9 ) ||
			 !wcsncmp( pszDescReader, L" <i></i> ", 9 ) )
		{
			pszDescReader += 8;
		}
		else if ( !wcsncmp( pszDescReader, L"  ", 2 ) )
		{
			pszDescReader += 1;
		}
		else
		{
			*pszDescWriter = *pszDescReader;
			pszDescWriter++;
			pszDescReader++;
		}
	}

	// terminate buffer
	*pszDescWriter = 0;
}

void UTIL_TrimEmptyWhitespaceFromHTML( char* pszDescWriter, std::size_t descWriterByteSize, const char* pszDescReader )
{
	std::size_t writerArraySize = descWriterByteSize / sizeof( char );
	char* pszDescWriterEnd = pszDescWriter + writerArraySize - 1; // leave 1 character for '\0' terminator

	while ( *pszDescReader != 0 && pszDescWriter < pszDescWriterEnd )
	{
		if ( !std::strncmp( pszDescReader, " <b></b> ", 9 ) ||
			 !strncmp( pszDescReader, " <i></i> ", 9 ) )
		{
			pszDescReader += 8;
		}
		else if ( !strncmp( pszDescReader, "  ", 2 ) )
		{
			pszDescReader += 1;
		}
		else
		{
			*pszDescWriter = *pszDescReader;
			pszDescWriter++;
			pszDescReader++;
		}
	}

	// terminate buffer
	*pszDescWriter = 0;
}

//-----------------------------------------------------------------------------
// Converts an IHandleEntity to an CBaseEntity
//-----------------------------------------------------------------------------
const CBaseEntity *EntityFromEntityHandle( const IHandleEntity *pConstHandleEntity )
{
	IHandleEntity *pHandleEntity = const_cast<IHandleEntity*>(pConstHandleEntity);

	IClientUnknown *pUnk = (IClientUnknown*)pHandleEntity;
	return EntityCast<const CBaseEntity>( pUnk->GetBaseEntity() );
}

CBaseEntity *EntityFromEntityHandle( IHandleEntity *pHandleEntity )
{
	IClientUnknown *pUnk = (IClientUnknown*)pHandleEntity;
	return EntityCast<CBaseEntity>( pUnk->GetBaseEntity() );
}

void CountdownTimer::Start( float duration)
{
	m_timestamp = Now() + duration;
	m_duration = duration;
}

void CountdownTimer::StartFromTime( float startTime, float duration)
{
	m_timestamp = startTime + duration;
	m_duration = duration;
}

float CountdownTimer::GetElapsedRatio() const
{
	if ( HasStarted() )
	{
		float elapsed = GetElapsedTime() / m_duration;
		if ( elapsed < 0.0f )
			return 0.0f;
		if ( elapsed > 1.0f )
			return 1.0f;
		return elapsed;
	}

	return 1.0f;
}

float CTimeline::GetValueTime( int i) const
{
	contract_assert( i >= 0 && i < m_nBucketCount );
	return static_cast<float>( i ) * m_flInterval;
}

class IGameSystem;

//================================================================
// Global startup and shutdown functions for game code in the DLL.
//================================================================

class CViewportClientSystem : public IGameSystem
{
public:
	CViewportClientSystem()
	{
	}

	virtual char const *Name() { return "CViewportClientSystem"; }
	virtual bool IsPerFrame() { return false; }

	// Init, shutdown
	virtual bool Init()
	{
		for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD_UI( hh );
			GetClientMode()->Layout();
		}
		return true;
	}
	virtual void PostInit() {}
	virtual void Shutdown() {}
	virtual void LevelInitPreEntity() {}
	virtual void LevelInitPostEntity() {}
	virtual void LevelShutdownPreEntity() {}
	virtual void LevelShutdownPostEntity() {}
	virtual void SafeRemoveIfDesired() {}

	virtual void OnSave() {}
	virtual void OnRestore() {}

};

static CViewportClientSystem g_ViewportClientSystem;

IGameSystem *ViewportClientSystem()
{
	return &g_ViewportClientSystem;
}
