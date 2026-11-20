// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

module;
#include <math.h>
#include <stddef.h>
#include <string.h>
#include "const.h"
#include "filesystem.h"
#include "con_nprint.h"
#include "eiface.h"
#include "iclientnetworkable.h"
#include "materialsystem/imesh.h"
#include "usermessages_schema.h"
#include "cdll_client_int.h"
#include "hud_macros.h"
#include "client_class.h"
#include "edict.h"
#include "engine/ivmodelinfo.h"
#include "igameevents.h"
#include "reflect_recvtable.h"
#include "vscript/ivscript.h"
#include "tier2/resourceprecacher.h"
#include "declare_class.h"
#include "reflect_annotations.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialproxy.h"
#include "materialsystem/imaterialvar.h"
#include "imaterialproxydict.h"
#include "datamap.h"
#include "string_t.h"
#include "inputsystem/ButtonCode.h"
#include "hud_element_helper.h"
#include "cdll_int.h"
#include "shake.h"
module ks.client;

import std;
import :util;
import :achievements_and_stats_interface;
import :baseanimatedtextureproxy;
import :baseentity;
import :sharedvar;
import ks.tier1.keyvalues;
import ks.core.basetypes;
import ks.core.dbg;
import ks.core.types;
import ks.math.compressed_vector;
import ks.math.ssemath;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.bit_ops;
import ks.tier1.convar;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.interfaces;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.strtools;
import :props;
import :iinput;
import :particlemgr;
import :cstrike15.cs_player;
import :cstrike15.cs_shareddefs;
import :precache;
import ks.game.shareddefs;
import :cstrike15.cs_player_shared;
import :econ.econ_item_schema_minimal;
import ks.game.gamemovement;
import :noopanimcontroller;
import ks.core.wchartypes;
import ks.core.cmdline;
import ks.core.logging;
import ks.tier1.fmtstr;
import :basetoggle;
import ks.game.mapentities_shared;
import ks.tier1.byte_reader;
import ks.core.mem;
import ks.core.threadtools;
import ks.tier1.vector_ops;
import ks.core.platform;
import ks.game.in_buttons;
import ks.core.vprof;
import ks.tier1.map_ops;

static constexpr auto BANMGR_FILEVERSION = 1;
const char *g_pBanMgrFilename = "voice_ban.dt";

// Hash a player ID to a byte.
unsigned char HashPlayerID(char const playerID[SIGNED_GUID_LEN])
{
	unsigned char curHash = 0;

	for(int i=0; i < SIGNED_GUID_LEN; i++)
		curHash = (unsigned char)(curHash + playerID[i]);

	return curHash;
}

CVoiceBanMgr::CVoiceBanMgr()
{
	Clear();
}

CVoiceBanMgr::~CVoiceBanMgr()
{
	Term();
}

bool CVoiceBanMgr::Init(const char *pGameDir)
{
	Term();

	// Load in the squelch file.
	FileHandle_t fh = filesystem->Open(g_pBanMgrFilename, "rb");
	if (fh)
	{
		int version;
		filesystem->Read(&version, sizeof(version), fh);
		if(version == BANMGR_FILEVERSION)
		{
			filesystem->Seek(fh, 0, FILESYSTEM_SEEK_TAIL);
			int nIDs = (filesystem->Tell(fh) - sizeof(version)) / SIGNED_GUID_LEN;
			filesystem->Seek(fh, sizeof(version), FILESYSTEM_SEEK_CURRENT);

			for(int i=0; i < nIDs; i++)
			{
				char playerID[SIGNED_GUID_LEN];
				filesystem->Read(playerID, SIGNED_GUID_LEN, fh);
				AddBannedPlayer(playerID);
			}			
		}

		filesystem->Close(fh);
	}

	return true;
}

void CVoiceBanMgr::Term()
{
	// Free all the player structures.
	for(int i=0; i < 256; i++)
	{
		BannedPlayer *pListHead = &m_PlayerHash[i];
		BannedPlayer *pNext;
		for(BannedPlayer *pCur=pListHead->m_pNext; pCur != pListHead; pCur=pNext)
		{
			pNext = pCur->m_pNext;
			delete pCur;
		}
	}

	Clear();
}

void CVoiceBanMgr::SaveState(const char *pGameDir)
{
	// Save the file out.
	FileHandle_t fh = filesystem->Open(g_pBanMgrFilename, "wb");
	if(fh)
	{
		int version = BANMGR_FILEVERSION;
		filesystem->Write(&version, sizeof(version), fh);

		for(int i=0; i < 256; i++)
		{
			BannedPlayer *pListHead = &m_PlayerHash[i];
			for(BannedPlayer *pCur=pListHead->m_pNext; pCur != pListHead; pCur=pCur->m_pNext)
			{
				filesystem->Write(pCur->m_PlayerID, SIGNED_GUID_LEN, fh);
			}
		}

		filesystem->Close(fh);
	}
}

bool CVoiceBanMgr::GetPlayerBan(char const playerID[SIGNED_GUID_LEN])
{
	return !!InternalFindPlayerSquelch(playerID);
}

void CVoiceBanMgr::SetPlayerBan(char const playerID[SIGNED_GUID_LEN], bool bSquelch)
{
	if(bSquelch)
	{
		// Is this guy already squelched?
		if(GetPlayerBan(playerID))
			return;
	
		AddBannedPlayer(playerID);
	}
	else
	{
		BannedPlayer *pPlayer = InternalFindPlayerSquelch(playerID);
		if(pPlayer)
		{
			pPlayer->m_pPrev->m_pNext = pPlayer->m_pNext;
			pPlayer->m_pNext->m_pPrev = pPlayer->m_pPrev;
			delete pPlayer;
		}
	}
}

void CVoiceBanMgr::Clear()
{
	// Tie off the hash table entries.
	for(int i=0; i < 256; i++)
		m_PlayerHash[i].m_pNext = m_PlayerHash[i].m_pPrev = &m_PlayerHash[i];

	// TODO memory leaks ???
}

CVoiceBanMgr::BannedPlayer* CVoiceBanMgr::InternalFindPlayerSquelch(char const playerID[SIGNED_GUID_LEN])
{
	int index = HashPlayerID(playerID);
	BannedPlayer *pListHead = &m_PlayerHash[index];
	for(BannedPlayer *pCur=pListHead->m_pNext; pCur != pListHead; pCur=pCur->m_pNext)
	{
		if(std::memcmp(playerID, pCur->m_PlayerID, SIGNED_GUID_LEN) == 0)
			return pCur;
	}

	return nullptr;
}

CVoiceBanMgr::BannedPlayer* CVoiceBanMgr::AddBannedPlayer(char const playerID[SIGNED_GUID_LEN])
{
	BannedPlayer *pNew = new BannedPlayer;
	if(!pNew)
		return nullptr;

	int index = HashPlayerID(playerID);
	std::memcpy(pNew->m_PlayerID, playerID, SIGNED_GUID_LEN);
	pNew->m_pNext = &m_PlayerHash[index];
	pNew->m_pPrev = m_PlayerHash[index].m_pPrev;
	pNew->m_pPrev->m_pNext = pNew->m_pNext->m_pPrev = pNew;
	return pNew;
}

class CVoiceStatus;
class CBasePlayer;
class CCSPlayer;

using namespace ::ks::reflect;

extern int cam_thirdperson;

static constexpr auto VOICE_MODEL_INTERVAL = 0.3;
static constexpr auto SQUELCHOSCILLATE_PER_SECOND = 2.0f;

ConVar voice_modenable( side_registry(), "voice_modenable", "1", FCVAR_ARCHIVE | FCVAR_CLIENTCMD_CAN_EXECUTE, "Enable/disable voice in this mod." );
ConVar voice_clientdebug( side_registry(), "voice_clientdebug", "0" );
ConVar voice_head_icon_size( side_registry(), "voice_head_icon_size", "6", FCVAR_NONE, "Size of voice icon over player heads in inches" );
ConVar voice_head_icon_height( side_registry(), "voice_head_icon_height", "20", FCVAR_NONE, "Voice icons are this many inches over player eye positions" );
ConVar voice_local_icon( side_registry(), "voice_local_icon", "0", FCVAR_NONE, "Draw local player's voice icon" );
ConVar voice_all_icons( side_registry(), "voice_all_icons", "0", FCVAR_NONE, "Draw all players' voice icons" );
ConVar voice_icons_method( side_registry(), "voice_icons_method", "2", FCVAR_NONE, "0 = classic style, 1 = particles, 2 = integrated into target ID" );

ConVar cl_mute_enemy_team( side_registry(), "cl_mute_enemy_team", "0", FCVAR_ARCHIVE, "Block all communication from players on the enemy team." );

// ---------------------------------------------------------------------- //
// The voice manager for the client.
// ---------------------------------------------------------------------- //
static CVoiceStatus *g_VoiceStatus = nullptr;

CVoiceStatus* GetClientVoiceMgr()
{
	if ( !g_VoiceStatus )
	{
		ClientVoiceMgr_Init();
	}

	return g_VoiceStatus;
}

void ClientVoiceMgr_Init()
{
	if ( g_VoiceStatus )
		return;

	g_VoiceStatus = new CVoiceStatus();
}

void ClientVoiceMgr_Shutdown()
{
	delete g_VoiceStatus;
	g_VoiceStatus = nullptr;
}

void ClientVoiceMgr_LevelInit()
{
	if ( g_VoiceStatus )
	{
		g_VoiceStatus->LevelInit();
	}
}

void ClientVoiceMgr_LevelShutdown()
{
	if ( g_VoiceStatus )
	{
		g_VoiceStatus->LevelShutdown();
	}
}

// ---------------------------------------------------------------------- //
// CVoiceStatus.
// ---------------------------------------------------------------------- //

static CVoiceStatus *g_pInternalVoiceStatus = nullptr;

bool __MsgFunc_VoiceMask(const ks::net::CCSUsrMsg_VoiceMask &msg)
{
	if(g_pInternalVoiceStatus)
		return g_pInternalVoiceStatus->HandleVoiceMaskMsg(msg);

	return true;
}

bool __MsgFunc_RequestState(const ks::net::CCSUsrMsg_RequestState &msg)
{
	if(g_pInternalVoiceStatus)
		return g_pInternalVoiceStatus->HandleReqStateMsg(msg);

	return true;
}

// ---------------------------------------------------------------------- //
// CVoiceStatus.
// ---------------------------------------------------------------------- //

CVoiceStatus::CVoiceStatus()
{
	m_nControlSize = 0;
	m_bBanMgrInitialized = false;
	m_LastUpdateServerState = 0;

	for ( int k = 0; k < MAX_SPLITSCREEN_CLIENTS; ++ k )
	{
		m_bTalking[k] = false;
		m_bServerAcked[k] = false;
		m_bAboveThreshold[k] = false;

		m_bAboveThresholdTimer[k].Invalidate();
	}

	m_bServerModEnable = -1;

	m_pHeadLabelMaterial = nullptr;

	m_bHeadLabelsDisabled = false;
}

CVoiceStatus::~CVoiceStatus()
{
	if ( m_pHeadLabelMaterial )
	{
		m_pHeadLabelMaterial->DecrementReferenceCount();
	}

	g_pInternalVoiceStatus = nullptr;			

	const char *pGameDir = engine->GetGameDirectory();
	if( pGameDir )
	{
		if(m_bBanMgrInitialized)
		{
			m_BanMgr.SaveState( pGameDir );
		}
	}
}

int CVoiceStatus::Init(
	IVoiceStatusHelper *pHelper,
	uintp pParentPanel)
{
	const char *pGameDir = engine->GetGameDirectory();
	if( pGameDir )
	{
		m_BanMgr.Init( pGameDir );
		m_bBanMgrInitialized = true;
	}

	contract_assert( !g_pInternalVoiceStatus );
	g_pInternalVoiceStatus = this;

	m_pHeadLabelMaterial = materials->FindMaterial( "voice/icntlk_pl", TEXTURE_GROUP_VGUI );
	m_pHeadLabelMaterial->IncrementReferenceCount();

	m_bInSquelchMode = false;

	m_pHelper = pHelper;
	m_pParentPanel = pParentPanel;

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		HOOK_MESSAGE(VoiceMask);
		HOOK_MESSAGE(RequestState);
	}

	return 1;
}

void CVoiceStatus::VidInit()
{
}

void CVoiceStatus::LevelInit( void )
{
	for ( int k = 0; k < MAX_SPLITSCREEN_CLIENTS; ++ k )
	{
		m_bTalking[k] = false;
		m_bAboveThreshold[k] = false;
		m_bAboveThresholdTimer[k].Invalidate();
	}

	for ( int k = 0; k < VOICE_MAX_PLAYERS; ++k )
	{
		m_flTalkTime[k] = 0.0f;
		m_flTimeLastUpdate[k] = 0.0f;
	}
}

void CVoiceStatus::LevelShutdown( void )
{
	for ( int k = 0; k < MAX_SPLITSCREEN_CLIENTS; ++ k )
	{
		m_bTalking[k] = false;
		m_bAboveThreshold[k] = false;
		m_bAboveThresholdTimer[k].Invalidate();
	}

	for ( int k = 0; k < VOICE_MAX_PLAYERS; ++k )
	{
		m_flTalkTime[k] = 0.0f;
		m_flTimeLastUpdate[k] = 0.0f;
	}
}

void CVoiceStatus::Frame(double frametime)
{
	// check server banned players once per second
	if (gpGlobals->curtime - m_LastUpdateServerState > 1)
	{
		UpdateServerState(false);
	}
}

float CVoiceStatus::GetHeadLabelOffset( void ) const
{
	return voice_head_icon_height.GetFloat();
}

void CVoiceStatus::DrawHeadLabels()
{
	if ( voice_all_icons.GetBool() )
	{
		for(int i=0; i < VOICE_MAX_PLAYERS; i++)
		{
			IClientNetworkable *pClient = cl_entitylist->GetClientEntity( i+1 );

			// Don't show an icon if the player is not in our PVS.
			if ( !pClient || pClient->IsDormant() )
				continue;

			m_VoicePlayers[i] = voice_all_icons.GetInt() > 0;
		}
	}
	else if ( voice_local_icon.GetBool() )
	{
		CBasePlayer *localPlayer = CBasePlayer::GetLocalPlayer();
		m_VoicePlayers[ localPlayer->entindex() - 1 ] = IsLocalPlayerSpeakingAboveThreshold( localPlayer->GetSplitScreenPlayerSlot() );
	}

	if ( m_bHeadLabelsDisabled )
		return;

	if( !m_pHeadLabelMaterial )
		return;

	CMatRenderContextPtr pRenderContext( materials );

	for ( int i=0; i < VOICE_MAX_PLAYERS; i++)
	{
		IClientNetworkable *pClient = cl_entitylist->GetClientEntity( i+1 );

		CBasePlayer *pPlayer = dynamic_cast< CBasePlayer* >( pClient );
		if( !pPlayer )
			continue;

		if ( !m_VoicePlayers[i] )
		{
			if ( voice_icons_method.GetInt() > 0 )
			{
				pPlayer->UpdateSpeechVOIP( false );
			}

			continue;
		}

		// Don't show an icon if the player is not in our PVS.
		if ( !pClient || pClient->IsDormant() )
		{
			if ( voice_icons_method.GetInt() > 0 )
			{
				pPlayer->UpdateSpeechVOIP( false );
			}

			continue;
		}
		
		// Don't show an icon for dead or spectating players (ie: invisible entities).
		if( pPlayer->IsPlayerDead() )
		{
			if ( voice_icons_method.GetInt() > 0 )
			{
				pPlayer->UpdateSpeechVOIP( false );
			}

			continue;
		}

		if ( voice_icons_method.GetInt() > 0 )
		{
			pPlayer->UpdateSpeechVOIP( true );
			return;
		}

		// Don't show an icon for players we can't hear
		if ( !IsPlayerAudible( i+1 ) )
			continue;

		// Place it a fixed height above his head.
		Vector vOrigin = pPlayer->EyePosition( );
		vOrigin.z += GetHeadLabelOffset();
		
		// Align it so it never points up or down.
		Vector vUp( 0, 0, 1 );
		Vector vRight = CurrentViewRight();
		if ( fabs( vRight.z ) > 0.95 )	// don't draw it edge-on
			continue;

		vRight.z = 0;
		VectorNormalize( vRight );

		float flSize = voice_head_icon_size.GetFloat();

		pRenderContext->Bind( pPlayer->GetHeadLabelMaterial() );
		IMesh *pMesh = pRenderContext->GetDynamicMesh();
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,0,0 );
		meshBuilder.Position3fv( (vOrigin + (vRight * -flSize) + (vUp * flSize)).Base() );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,1,0 );
		meshBuilder.Position3fv( (vOrigin + (vRight * flSize) + (vUp * flSize)).Base() );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,1,1 );
		meshBuilder.Position3fv( (vOrigin + (vRight * flSize) + (vUp * -flSize)).Base() );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,0,1 );
		meshBuilder.Position3fv( (vOrigin + (vRight * -flSize) + (vUp * -flSize)).Base() );
		meshBuilder.AdvanceVertex();
		meshBuilder.End();
		pMesh->Draw();
	}
}

void CVoiceStatus::UpdateSpeakerStatus(int entindex, int iSsSlot, bool bTalking)
{
	if( !m_pParentPanel )
		return;

	if( voice_clientdebug.GetInt() == 1 )
	{
		Msg( "CVoiceStatus::UpdateSpeakerStatus: ent %d ss[%d] talking = %d\n",
			entindex, iSsSlot, bTalking );
	}
	else if ( voice_clientdebug.GetInt() == 2 )
	{
		con_nprint_t np;
		np.index = 0;
		np.color[0] = 1.0f;
		np.color[1] = 1.0f;
		np.color[2] = 1.0f;
		np.time_to_live = 2.0f;
		np.fixed_width_font = true;

		int numActiveChannels = VOICE_MAX_PLAYERS;
		engine->Con_NXPrintf ( &np, "Total Players: %i", numActiveChannels);

		for ( int i = 1; i <= numActiveChannels; i++ )
		{
			np.index++;
			np.color[0] = np.color[1] = np.color[2] = ( i % 2 == 0 ? 0.9f : 0.7f );

			if ( !IsPlayerBlocked( i ) && IsPlayerAudible( i ) && !ShouldHideCommunicationFromPlayer( i ) && IsPlayerSpeaking( i ) )
			{
				np.color[0] = 0.0f;
				np.color[1] = 1.0f;
				np.color[2] = 0.0f;
			}

			engine->Con_NXPrintf ( &np, "%02i enabled(%s) blocked(%s) audible(%s) speaking(%s)", 
				i, 
				m_VoiceEnabledPlayers[ i - 1 ] != 0 ? "YES" : " NO",
				IsPlayerBlocked( i ) ? "YES" : " NO", 
				IsPlayerAudible( i ) ? "YES" : " NO", 
				IsPlayerSpeaking( i ) ? "YES" : " NO" );
		}

		np.color[0] = 1.0f;
		np.color[1] = 1.0f;
		np.color[2] = 1.0f;

		np.index += 2;
		numActiveChannels = MAX_SPLITSCREEN_CLIENTS;
		engine->Con_NXPrintf ( &np, "Local Players: %i", numActiveChannels);

		for ( int i = 0; i < numActiveChannels; i++ )
		{
			np.index++;
			np.color[0] = np.color[1] = np.color[2] = ( i % 2 == 0 ? 0.9f : 0.7f );

			if ( IsLocalPlayerSpeaking( i ) && IsLocalPlayerSpeakingAboveThreshold( i ) )
			{
				np.color[0] = 0.0f;
				np.color[1] = 1.0f;
				np.color[2] = 0.0f;
			}

			engine->Con_NXPrintf ( &np, "%02i speaking(%s) above_threshold(%s)", 
				i, 
				IsLocalPlayerSpeaking( i ) ? "YES" : " NO", 
				IsLocalPlayerSpeakingAboveThreshold( i ) ? "YES" : " NO" );
		}
	}

	// Is it the local player talking?
	if( entindex == -1 && iSsSlot >= 0 )
	{
		m_bTalking[ iSsSlot ] = !!bTalking;
	}
	
	if( entindex == -2 && iSsSlot >= 0 )
	{
		m_bServerAcked[ iSsSlot ] = !!bTalking;
	}
	
	if ( entindex == -3 && iSsSlot >= 0 )
	{
		m_bAboveThreshold[ iSsSlot ] = !!bTalking;

		if ( bTalking )
		{
			const float AboveThresholdMinDuration = 0.5f;
			m_bAboveThresholdTimer[ iSsSlot ].Start( AboveThresholdMinDuration );
		}
	}
	
	if( entindex > 0 && entindex <= VOICE_MAX_PLAYERS )
	{
		int iClient = entindex - 1;
		if(iClient < 0)
			return;

		if(bTalking)
		{
			m_VoicePlayers[iClient] = true;
			m_VoiceEnabledPlayers[iClient] = true;
		}
		else
		{
			m_VoicePlayers[iClient] = false;
		}

		if ( bTalking && m_bAboveThresholdTimer[ iClient ].IsElapsed() )
		{
			m_flTalkTime[ iClient ] +=  (gpGlobals->curtime - m_flTimeLastUpdate[ iClient ]);
		}
		else 
		{
			m_flTalkTime[ iClient ] = 0.0f;
		}
		m_flTimeLastUpdate[ iClient ] = gpGlobals->curtime;
	}
}

void CVoiceStatus::UpdateServerState(bool bForce)
{
	// Can't do anything when we're not in a level.
	if( !g_bLevelInitialized )
	{
		if( voice_clientdebug.GetInt() == 1 )
		{
			Msg( "CVoiceStatus::UpdateServerState: g_bLevelInitialized\n" );
		}

		return;
	}
	
	int bCVarModEnable = !!voice_modenable.GetInt();
	if(bForce || m_bServerModEnable != bCVarModEnable)
	{
		m_bServerModEnable = bCVarModEnable;

		char str[256];
		V_snprintf(str, sizeof(str), "VModEnable %d", m_bServerModEnable);
		
		{
			HACK_GETLOCALPLAYER_GUARD( "CVoiceStatus::UpdateServerState" );
			engine->ServerCmd(str);
		}

		if( voice_clientdebug.GetInt() == 1 )
		{
			Msg( "CVoiceStatus::UpdateServerState: Sending '%s'\n", str );
		}
	}

	char str[2048];
	V_strncpy(str,"vban",sizeof(str));
	bool bChange = false;

	for(unsigned long dw=0; dw < VOICE_MAX_PLAYERS_DW; dw++)
	{	
		unsigned long serverBanMask = 0;
		unsigned long banMask = 0;
		for(unsigned long i=0; i < 32; i++)
		{
			int playerIndex = ( dw * 32 + i );
			if ( playerIndex >= MAX_PLAYERS )
				break;

			player_info_t pi;

			if ( !engine->GetPlayerInfo( i+1, &pi ) )
				continue;

			if ( m_BanMgr.GetPlayerBan( pi.guid ) || ShouldHideCommunicationFromPlayer( i + 1 ) )
			{
				banMask |= 1 << i;
			}

			if ( m_ServerBannedPlayers[playerIndex] )
			{
				serverBanMask |= 1 << i;
			}
		}

		if ( serverBanMask != banMask )
		{
			bChange = true;
		}

		// Ok, the server needs to be updated.
		char numStr[512];
		V_snprintf(numStr,sizeof(numStr), " %lx", banMask);
		V_strncat(str, numStr, sizeof(str), COPY_ALL_CHARACTERS);
	}

	if(bChange || bForce)
	{
		if( voice_clientdebug.GetInt() == 1 )
		{
			Msg( "CVoiceStatus::UpdateServerState: Sending '%s'\n", str );
		}

		engine->ServerCmd( str, false );	// Tell the server..
	}
	else
	{
		if( voice_clientdebug.GetInt() == 1 )
		{
			Msg( "CVoiceStatus::UpdateServerState: no change\n" );
		}
	}
	
	m_LastUpdateServerState = gpGlobals->curtime;
}

bool CVoiceStatus::HandleVoiceMaskMsg(const ks::net::CCSUsrMsg_VoiceMask &msg)
{
	unsigned long dw;
	for(dw=0; dw < VOICE_MAX_PLAYERS_DW; dw++)
	{
		ks::set_word( m_AudiblePlayers, dw, msg.player_masks[ dw ].game_rules_mask );
		ks::set_word( m_ServerBannedPlayers, dw, msg.player_masks[ dw ].ban_masks );

		if( voice_clientdebug.GetInt() == 1 )
		{
			Msg("CVoiceStatus::HandleVoiceMaskMsg\n");
			Msg("    - m_AudiblePlayers[%lu] = %u\n", dw, ks::get_word( m_AudiblePlayers, dw ));
			Msg("    - m_ServerBannedPlayers[%lu] = %u\n", dw, ks::get_word( m_ServerBannedPlayers, dw ));
		}
	}

	m_bServerModEnable = msg.player_mod_enable;

	return true;
}

bool CVoiceStatus::HandleReqStateMsg(const ks::net::CCSUsrMsg_RequestState &msg)
{
	if( voice_clientdebug.GetInt() == 1 )
	{
		Msg("CVoiceStatus::HandleReqStateMsg\n");
	}

	UpdateServerState(true);

	return true;
}

void CVoiceStatus::StartSquelchMode()
{
	if(m_bInSquelchMode)
		return;

	m_bInSquelchMode = true;
	m_pHelper->UpdateCursorState();
}

void CVoiceStatus::StopSquelchMode()
{
	m_bInSquelchMode = false;
	m_pHelper->UpdateCursorState();
}

bool CVoiceStatus::IsInSquelchMode()
{
	return m_bInSquelchMode;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the target client has been banned
// Input  : playerID - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CVoiceStatus::IsPlayerBlocked(int iPlayer)
{
	player_info_t pi;

	if ( !engine->GetPlayerInfo( iPlayer, &pi ) )
		return false;

	return m_BanMgr.GetPlayerBan( pi.guid );
}

bool CVoiceStatus::ShouldHideCommunicationFromPlayer( int iPlayerIndex )
{
	CCSPlayer* pLocalPlayer = CCSPlayer::GetLocalCSPlayer();
	if ( pLocalPlayer && pLocalPlayer->entindex() == iPlayerIndex )
		return false;

	if ( cl_mute_enemy_team.GetBool() )
	{
		if ( pLocalPlayer && pLocalPlayer->IsOtherEnemy( iPlayerIndex ) && 
			( pLocalPlayer->GetTeamNumber() == TEAM_CT || pLocalPlayer->GetTeamNumber() == TEAM_TERRORIST ) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Purpose: blocks/unblocks the target client from being heard
// Input  : playerID - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CVoiceStatus::SetPlayerBlockedState( int iPlayer )
{
	if ( voice_clientdebug.GetInt() == 1 )
	{
		Msg( "CVoiceStatus::SetPlayerBlockedState part 1\n" );
	}

	player_info_t pi;
	if ( !engine->GetPlayerInfo( iPlayer, &pi ) )
		return;

	if ( voice_clientdebug.GetInt() == 1 )
	{
		Msg( "CVoiceStatus::SetPlayerBlockedState part 2\n" );
	}

	// Squelch or (try to) unsquelch this player.
	if ( voice_clientdebug.GetInt() == 1 )
	{
		Msg("CVoiceStatus::SetPlayerBlockedState: setting player %d ban to %d\n", iPlayer, !m_BanMgr.GetPlayerBan(pi.guid));
	}

	m_BanMgr.SetPlayerBan(pi.guid, !m_BanMgr.GetPlayerBan(pi.guid));
	UpdateServerState(false);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVoiceStatus::SetHeadLabelMaterial( const char *pszMaterial )
{
	if ( m_pHeadLabelMaterial )
	{
		m_pHeadLabelMaterial->DecrementReferenceCount();
		m_pHeadLabelMaterial = nullptr;
	}

	m_pHeadLabelMaterial = materials->FindMaterial( pszMaterial, TEXTURE_GROUP_VGUI );
	m_pHeadLabelMaterial->IncrementReferenceCount();
}

class CBasePlayer;

using namespace ::ks::reflect;

// Purpose: CS's custom CVoteController

IMPLEMENT_REFLECT_CLIENTCLASS( CVoteController, DT_VoteController, CVoteController )

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoteController::RecvProxy_VoteType( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CVoteController *pMe = (CVoteController *)pStruct;
	if( pMe->m_iActiveIssueIndex == pData->m_Value.m_Int )
		return;

	pMe->m_iActiveIssueIndex = pData->m_Value.m_Int;
	pMe->m_bTypeDirty = true;

	// Since the contents of a new vote are in three parts, we can't directly send an event to the Hud
	// because we don't really know if we have all three parts yet.  So we'll mark dirty, and our think
	// can notice that and send the event.
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoteController::RecvProxy_VoteOption( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	int index = pData->m_pRecvProp->GetOffset() / sizeof(int);
	
	size_t offset = __builtin_offsetof( CVoteController, m_nVoteOptionCount );
	CVoteController *pMe = (CVoteController *)((byte *)pStruct - offset );
	if( pMe->m_nVoteOptionCount[index] == pData->m_Value.m_Int )
		return;
	
	pMe->m_nVoteOptionCount[index] = pData->m_Value.m_Int;
	pMe->m_bVotesDirty = true;
	pMe->SetNextClientThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CVoteController::CVoteController()
{
	ResetData();

	ListenForGameEvent( "vote_cast" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CVoteController::~CVoteController()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoteController::ResetData()
{
	m_iActiveIssueIndex = INVALID_ISSUE;
	m_iOnlyTeamToVote = TEAM_INVALID;
	for( int i = 0; i < MAX_VOTE_OPTIONS; i++ )
	{
		m_nVoteOptionCount[i] = 0;
	}
	m_nPotentialVotes = 0;
	m_bVotesDirty = false;
	m_bTypeDirty = false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoteController::Spawn( void )
{
	ResetData();
	BaseClass::Spawn();
	SetNextClientThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoteController::ClientThink()
{
	BaseClass::ClientThink();

	CBasePlayer *pLocalPlayer = CBasePlayer::GetLocalPlayer();

	if ( pLocalPlayer && ( (m_iOnlyTeamToVote == TEAM_INVALID) || (m_iOnlyTeamToVote == pLocalPlayer->GetTeamNumber()) ) )
	{
		if ( m_bTypeDirty )
		{
			m_bTypeDirty = false;
			m_bVotesDirty = true;
		}

		if ( m_bVotesDirty )
		{
			if ( m_nPotentialVotes > 0 )
			{
				// Currently hard-coded to MAX_VOTE_COUNT options per issue
				DevMsg( "Votes: Option1 - %d, Option2 - %d, Option3 - %d, Option4 - %d, Option5 - %d\n",
					m_nVoteOptionCount[ 0 ], m_nVoteOptionCount[ 1 ], m_nVoteOptionCount[ 2 ], m_nVoteOptionCount[ 3 ], m_nVoteOptionCount[ 4 ] );

				IGameEvent *event = gameeventmanager->CreateEvent( "vote_changed" );
				if ( event )
				{
					for ( int index = 0; index < MAX_VOTE_OPTIONS; index++ )
					{
						char szOption[ 2 ];
						V_snprintf( szOption, sizeof( szOption ), "%i", index + 1 );

						char szVoteOption[ 13 ] = "vote_option";
						V_strncat( szVoteOption, szOption, sizeof( szVoteOption ), COPY_ALL_CHARACTERS );

						event->SetInt( szVoteOption, m_nVoteOptionCount[ index ] );
					}
					event->SetInt( "potentialVotes", m_nPotentialVotes );
					gameeventmanager->FireEventClientSide( event );
				}
			}

			m_bVotesDirty = false;
		}
	}

	SetNextClientThink( gpGlobals->curtime + 0.5f );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoteController::FireGameEvent( IGameEvent *event )
{
	//CHudVote *pHudVote = GET_HUDELEMENT( CHudVote );
	//if ( pHudVote && pHudVote->IsVisible() )
	{
		const char *eventName = event->GetName();
		if ( !eventName )
			return;

		CBasePlayer *pLocalPlayer = CBasePlayer::GetLocalPlayer();
		if ( !pLocalPlayer )
			return;

		int team = event->GetInt( "team", TEAM_UNASSIGNED );
		if ( team > TEAM_UNASSIGNED && team != pLocalPlayer->GetTeamNumber() )
			return;

		if ( FStrEq( eventName, "vote_cast" ) )
		{
			if ( m_bIsYesNoVote )
			{
				int vote_option = event->GetInt( "vote_option", TEAM_UNASSIGNED );
				if( vote_option == VOTE_OPTION2 )
				{
					pLocalPlayer->EmitSound( "Vote.Cast.No" );
				}
				else if( vote_option == VOTE_OPTION1 )
				{
					pLocalPlayer->EmitSound( "Vote.Cast.Yes" );
				}
			}
			else
			{
				pLocalPlayer->EmitSound( "Vote.Cast.Yes" );
			}
		}
	}
}

class CBaseEntity;

using namespace ::ks::reflect;

extern IScriptManager *scriptmanager;
extern ScriptClassDesc_t * GetScriptDesc( CBaseEntity * );
//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static float Time()
{
	return gpGlobals->curtime;
}

static const char *GetMapName()
{
	return engine->GetLevelName();
}

static const char *DoUniqueString( const char *pszBase )
{
	static char szBuf[512];
	g_pScriptVM->GenerateUniqueKey( pszBase, szBuf, std::size( szBuf ) );
	return szBuf;
}

bool DoIncludeScript( const char *pszScript, HSCRIPT hScope )
{
	if ( !VScriptRunScript( pszScript, hScope, true ) )
	{
		g_pScriptVM->RaiseException( CFmtStr( "Failed to include script \"%s\"", ( pszScript ) ? pszScript : "unknown" ) );
		return false;
	}
	return true;
}

int GetDeveloperLevel()
{
	return developer.GetInt();
}

bool VScriptClientInit()
{
	if( scriptmanager != nullptr )
	{
		ScriptLanguage_t scriptLanguage = SL_DEFAULT;

		char const *pszScriptLanguage;
		if ( CommandLine()->CheckParm( "-scriptlang", &pszScriptLanguage ) )
		{
			if( !V_stricmp(pszScriptLanguage, "gamemonkey") )
			{
				scriptLanguage = SL_GAMEMONKEY;
			}
			else if( !V_stricmp(pszScriptLanguage, "squirrel") )
			{
				scriptLanguage = SL_SQUIRREL;
			}
			else if( !V_stricmp(pszScriptLanguage, "python") )
			{
				scriptLanguage = SL_PYTHON;
			}
			else
			{
				DevWarning("-scriptlang does not recognize a language named '%s'. virtual machine did NOT start.\n", pszScriptLanguage );
				scriptLanguage = SL_NONE;
			}

		}
		if( scriptLanguage != SL_NONE )
		{
			if ( g_pScriptVM == nullptr )
				g_pScriptVM = scriptmanager->CreateVM( scriptLanguage );

			if( g_pScriptVM )
			{
				Log_Msg( LOG_VScript, "VSCRIPT: Started VScript virtual machine using script language '%s'\n", g_pScriptVM->GetLanguageName() );
				ScriptRegisterFunction( g_pScriptVM, GetMapName, "Get the name of the map.");
				ScriptRegisterFunction( g_pScriptVM, Time, "Get the current server time" );
				ScriptRegisterFunction( g_pScriptVM, DoIncludeScript, "Execute a script (internal)" );
				ScriptRegisterFunction( g_pScriptVM, GetDeveloperLevel, "Gets the level of 'develoer'" );
				
				if ( GameRules() )
				{
					GameRules()->RegisterScriptFunctions();
				}

				//g_pScriptVM->RegisterInstance( &g_ScriptEntityIterator, "Entities" );

				if ( scriptLanguage == SL_SQUIRREL )
				{
					g_pScriptVM->Run( g_Script_vscript_client );
				}

				VScriptRunScript( "mapspawn", false );
				return true;
			}
			else
			{
				DevWarning("VM Did not start!\n");
			}
		}
	}
	else
	{
		Log_Msg( LOG_VScript, "\nVSCRIPT: Scripting is disabled.\n" );
	}
	g_pScriptVM = nullptr;
	return false;
}

void VScriptClientTerm()
{
	if( g_pScriptVM != nullptr )
	{
		if( g_pScriptVM )
		{
			scriptmanager->DestroyVM( g_pScriptVM );
			g_pScriptVM = nullptr;
		}
	}
}

class CVScriptGameSystem : public CAutoGameSystemPerFrame
{
public:
	// Inherited from IAutoServerSystem
	virtual void LevelInitPreEntity( void )
	{
		// <sergiy> Note: we may need script VM garbage collection at this point in the future. Currently, VM does not persist 
		//          across level boundaries. GC is not necessary because our scripts are supposed to never create circular references
		//          and everything else is handled with ref counting. For the case of bugs creating circular references, the plan is to add
		//          diagnostics that detects such loops and warns the developer.

		m_bAllowEntityCreationInScripts = true;
		VScriptClientInit();
	}

	virtual void LevelInitPostEntity( void )
	{
		m_bAllowEntityCreationInScripts = false;
	}

	virtual void LevelShutdownPostEntity( void )
	{
		VScriptClientTerm();
	}

	virtual void FrameUpdatePostEntityThink() 
	{ 
		if ( g_pScriptVM )
			g_pScriptVM->Frame( gpGlobals->frametime );
	}

	bool m_bAllowEntityCreationInScripts;
};

CVScriptGameSystem g_VScriptGameSystem;

bool IsEntityCreationAllowedInScripts( void )
{
	return g_VScriptGameSystem.m_bAllowEntityCreationInScripts;
}

class CBaseEntity;

using namespace ::ks::reflect;

IScriptVM * g_pScriptVM;
extern ScriptClassDesc_t * GetScriptDesc( CBaseEntity * );

LoggingChannelID_t LOG_VScript = LoggingSystem_RegisterLoggingChannel( "VScript", nullptr, LCF_CONSOLE_ONLY, LS_WARNING );;
HSCRIPT VScriptCompileScript( const char *pszScriptName, bool bWarnMissing )
{
	if ( !g_pScriptVM )
	{
		return nullptr;
	}

	static const char *pszExtensions[] =
	{
		"",		// SL_NONE
		".gm",	// SL_GAMEMONKEY
		".nut",	// SL_SQUIRREL
		".lua", // SL_LUA
		".py",  // SL_PYTHON
	};

	const char *pszVMExtension = pszExtensions[g_pScriptVM->GetLanguage()];
	const char *pszIncomingExtension = V_strrchr( pszScriptName , '.' );
	if ( pszIncomingExtension && V_strcmp( pszIncomingExtension, pszVMExtension ) != 0 )
	{
		Log_Warning( LOG_VScript, "Script file type does not match VM type\n" );
		return nullptr;
	}

	CFmtStr scriptPath;
	if ( pszIncomingExtension )
	{
		scriptPath.sprintf( "scripts/vscripts/%s", pszScriptName );
	}
	else
	{	
		scriptPath.sprintf( "scripts/vscripts/%s%s", pszScriptName,  pszVMExtension );
	}

	const char *pBase;
	std::string scriptText;

	if ( g_pScriptVM->GetLanguage() == SL_PYTHON )
	{
		// python auto-loads raw or precompiled modules - don't load data here
		pBase = nullptr;
	}
	else
	{
		auto scriptData = filesystem->ReadFile( scriptPath, "GAME" );

		if( !scriptData )
		{
			Log_Warning( LOG_VScript, "Script not found (%s) \n", scriptPath.operator const char *() );
			contract_assert( "Error running script" );
		}

		if ( scriptData )
		{
			scriptText.assign( ks::text_view( *scriptData ) );
		}
		pBase = scriptText.empty() ? nullptr : scriptText.c_str();

		if ( !pBase || !*pBase )
		{
			return nullptr;
		}
	}

	const char *pszFilename = V_strrchr( scriptPath, '/' );
	pszFilename++;
	HSCRIPT hScript = g_pScriptVM->CompileScript( pBase, pszFilename );
	if ( !hScript )
	{
		Log_Warning( LOG_VScript, "FAILED to compile and execute script file named %s\n", scriptPath.operator const char *() );
		contract_assert( "Error running script" );
	}
	return hScript;
}

static int g_ScriptServerRunScriptDepth;

bool VScriptRunScript( const char *pszScriptName, HSCRIPT hScope, bool bWarnMissing )
{
	if ( !g_pScriptVM )
	{
		return false;
	}

	if ( !pszScriptName || !*pszScriptName )
	{
		Log_Warning( LOG_VScript, "Cannot run script: NULL script name\n" );
		return false;
	}

	// Prevent infinite recursion in VM
	if ( g_ScriptServerRunScriptDepth > 16 )
	{
		Log_Warning( LOG_VScript, "IncludeScript stack overflow\n" );
		return false;
	}

	g_ScriptServerRunScriptDepth++;
	HSCRIPT	hScript = VScriptCompileScript( pszScriptName, bWarnMissing );
	bool bSuccess = false;
	if ( hScript )
	{
		bSuccess = ( g_pScriptVM->Run( hScript, hScope ) != SCRIPT_ERROR );
		if ( !bSuccess )
		{
			Log_Warning( LOG_VScript, "Error running script named %s\n", pszScriptName );
			contract_assert( "Error running script" );
		}
	}
	g_ScriptServerRunScriptDepth--;
	return bSuccess;
}

static void script_client( const CCommand &args );
static ConCommand script_client_command( side_registry(), "script_client", script_client, "Run the text as a script" );
static void script_client( const CCommand &args )
{
	if ( !engine->IsClientLocalToActiveServer() )
		return;

	if ( !*args[1] )
	{
		Log_Warning( LOG_VScript, "No function name specified\n" );
		return;
	}

	if ( !g_pScriptVM )
	{
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
		return;
	}

	const char *pszScript = args.GetCommandString();

	pszScript += 13;
	
	while ( *pszScript == ' ' )
	{
		pszScript++;
	}

	if ( !*pszScript )
	{
		return;
	}

	if ( *pszScript != '\"' )
	{
		g_pScriptVM->Run( pszScript );
	}
	else
	{
		pszScript++;
		const char *pszEndQuote = pszScript;
		while ( *pszEndQuote !=  '\"' )
		{
			pszEndQuote++;
		}
		if ( !*pszEndQuote )
		{
			return;
		}
		*((char *)pszEndQuote) = 0;
		g_pScriptVM->Run( pszScript );
		*((char *)pszEndQuote) = '\"';
	}
}

static void script_execute_client( const CCommand &args );
static ConCommand script_execute_client_command( side_registry(), "script_execute_client", script_execute_client, "Run a vscript file" );
static void script_execute_client( const CCommand &args )
{
	if ( !engine->IsClientLocalToActiveServer() )
		return;

	if ( !*args[1] )
	{
		Log_Warning( LOG_VScript, "No script specified\n" );
		return;
	}

	if ( !g_pScriptVM )
	{
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
		return;
	}

	VScriptRunScript( args[1], true );
}

static void script_debug_client( const CCommand &args );
static ConCommand script_debug_client_command( side_registry(), "script_debug_client", script_debug_client, "Connect the vscript VM to the script debugger" );
static void script_debug_client( const CCommand &args )
{
	if ( !engine->IsClientLocalToActiveServer() )
		return;

	if ( !g_pScriptVM )
	{
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
		return;
	}
	g_pScriptVM->ConnectDebugger();
}

static void script_help_client( const CCommand &args );
static ConCommand script_help_client_command( side_registry(), "script_help_client", script_help_client, "Output help for script functions, optionally with a search string" );
static void script_help_client( const CCommand &args )
{
	if ( !engine->IsClientLocalToActiveServer() )
		return;

	if ( !g_pScriptVM )
	{
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
		return;
	}
	const char *pszArg1 = "*";
	if ( *args[1] )
	{
		pszArg1 = args[1];
	}

	g_pScriptVM->Run( CFmtStr( "PrintHelp( \"%s\" );", pszArg1 ) );
}

static void script_dump_all_client( const CCommand &args );
static ConCommand script_dump_all_client_command( side_registry(), "script_dump_all_client", script_dump_all_client, "Dump the state of the VM to the console" );
static void script_dump_all_client( const CCommand &args )
{
	if ( !engine->IsClientLocalToActiveServer() )
		return;

	if ( !g_pScriptVM )
	{
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
		return;
	}
	g_pScriptVM->DumpState();
}

using namespace ::ks::reflect;

// Purpose: Screen warp overlay

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheEffectWarp )
PRECACHE( MATERIAL, "sun/overlay" )
PRECACHE_REGISTER_END()

//-----------------------------------------------------------------------------
// Purpose: Special draw for the warped overlay
//-----------------------------------------------------------------------------
void CWarpOverlay::Draw( bool bCacheFullSceneState )
{
	// Get the vector to the sun.
	Vector vToGlow;
	
	if( m_bDirectional )
		vToGlow = m_vDirection;
	else
		vToGlow = m_vPos - CurrentViewOrigin();

	VectorNormalize( vToGlow );

	float flDot = vToGlow.Dot( CurrentViewForward() );

	if( flDot <= g_flOverlayRange )
		return;

	UpdateGlowObstruction( vToGlow, bCacheFullSceneState );
	if( m_flGlowObstructionScale == 0 )
		return;
	
	CMatRenderContextPtr pRenderContext( materials );
	
	//FIXME: Allow multiple?
	for( int iSprite=0; iSprite < m_nSprites; iSprite++ )
	{
		CGlowSprite *pSprite = &m_Sprites[iSprite];

		// Figure out the color and size to draw it.
		float flHorzSize, flVertSize;
		Vector vColor;
		CalcSpriteColorAndSize( flDot, pSprite, &flHorzSize, &flVertSize, &vColor );
	
		// Setup the basis to draw the sprite.
		Vector vBasePt, vUp, vRight;
		CalcBasis( vToGlow, flHorzSize, flVertSize, vBasePt, vUp, vRight );

		// Draw the sprite.
		IMaterial *pMaterial = materials->FindMaterial( "sun/overlay", TEXTURE_GROUP_CLIENT_EFFECTS );
		IMesh *pMesh = pRenderContext->GetDynamicMesh( false, 0, 0, pMaterial );

		CMeshBuilder builder;
		builder.Begin( pMesh, MATERIAL_QUADS, 1 );
		
		Vector vPt;

		vPt = vBasePt - vRight + vUp;
		builder.Position3fv( vPt.Base() );
		builder.Color4f( vColor.x, vColor.y, vColor.z, 1 );
		builder.TexCoord2f( 0, 0, 1 );
		builder.AdvanceVertex();

		vPt = vBasePt + vRight + vUp;
		builder.Position3fv( vPt.Base() );
		builder.Color4f( vColor.x, vColor.y, vColor.z, 1 );
		builder.TexCoord2f( 0, 1, 1 );
		builder.AdvanceVertex();

		vPt = vBasePt + vRight - vUp;
		builder.Position3fv( vPt.Base() );
		builder.Color4f( vColor.x, vColor.y, vColor.z, 1 );
		builder.TexCoord2f( 0, 1, 0 );
		builder.AdvanceVertex();

		vPt = vBasePt - vRight - vUp;
		builder.Position3fv( vPt.Base() );
		builder.Color4f( vColor.x, vColor.y, vColor.z, 1 );
		builder.TexCoord2f( 0, 0, 0 );
		builder.AdvanceVertex();
		
		builder.End( false, true );
	}
}

class CBaseEntity;

using namespace ::ks::reflect;

// Purpose: Water LOD control entity.

//------------------------------------------------------------------------------
// FIXME: This really should inherit from something	more lightweight
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Purpose : Water LOD control entity
//------------------------------------------------------------------------------
class CWaterLODControl : public CBaseEntity
{
	[[= ks::reflect::NetTable{ .name = "DT_WaterLODControl" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CWaterLODControl, CBaseEntity );

	DECLARE_CLIENTCLASS();

	void OnDataChanged(DataUpdateType_t updateType);
	bool ShouldDraw();

private:
	[[= ks::reflect::Net{} ]] float m_flCheapWaterStartDistance;
	[[= ks::reflect::Net{} ]] float m_flCheapWaterEndDistance;
};

IMPLEMENT_REFLECT_CLIENTCLASS( CWaterLODControl, DT_WaterLODControl, CWaterLODControl )

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CWaterLODControl::OnDataChanged(DataUpdateType_t updateType)
{
	view->SetCheapWaterStartDistance( m_flCheapWaterStartDistance );
	view->SetCheapWaterEndDistance( m_flCheapWaterEndDistance );
}

//------------------------------------------------------------------------------
// We don't draw...
//------------------------------------------------------------------------------
bool CWaterLODControl::ShouldDraw()
{
	return false;
}

// no inputs, assumes that the results go into $CHEAPWATERSTARTDISTANCE and $CHEAPWATERENDDISTANCE
class CWaterLODMaterialProxy : public IMaterialProxy
{
public:
	CWaterLODMaterialProxy();
	virtual ~CWaterLODMaterialProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
	virtual void Release( void ) { delete this; }
	virtual IMaterial *GetMaterial();

private:
	IMaterialVar *m_pCheapWaterStartDistanceVar;
	IMaterialVar *m_pCheapWaterEndDistanceVar;
};

CWaterLODMaterialProxy::CWaterLODMaterialProxy()
{
	m_pCheapWaterStartDistanceVar = nullptr;
	m_pCheapWaterEndDistanceVar = nullptr;
}

CWaterLODMaterialProxy::~CWaterLODMaterialProxy()
{
}

bool CWaterLODMaterialProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool foundVar;
	m_pCheapWaterStartDistanceVar = pMaterial->FindVar( "$CHEAPWATERSTARTDISTANCE", &foundVar, false );
	if( !foundVar )
		return false;

	m_pCheapWaterEndDistanceVar = pMaterial->FindVar( "$CHEAPWATERENDDISTANCE", &foundVar, false );
	if( !foundVar )
		return false;

	return true;
}

void CWaterLODMaterialProxy::OnBind( void *pC_BaseEntity )
{
	if( !m_pCheapWaterStartDistanceVar || !m_pCheapWaterEndDistanceVar )
	{
		return;
	}
	float start, end;
	view->GetWaterLODParams( start, end );
	m_pCheapWaterStartDistanceVar->SetFloatValue( start );
	m_pCheapWaterEndDistanceVar->SetFloatValue( end );
}

IMaterial *CWaterLODMaterialProxy::GetMaterial()
{
	return m_pCheapWaterStartDistanceVar->GetOwningMaterial();
}

EXPOSE_MATERIAL_PROXY( CWaterLODMaterialProxy, WaterLOD );

class CEconItemDefinition;
class CEconItemView;
class FileWeaponInfo_t;

using namespace ::ks::reflect;

// Purpose: Weapon data file parsing, shared by game & client dlls.

// Static default item definition for fallback (never returns NULL for backwards compatibility)
static CEconItemDefinition s_DefaultItemDef;

CEconItemDefinition *CEconItemView::GetItemDefinition() const
{
	return m_pItemDef ? m_pItemDef : &s_DefaultItemDef;
}

CEconItemDefinition *CEconItemView::GetStaticData() const
{
	return m_pItemDef ? m_pItemDef : &s_DefaultItemDef;
}

CWeaponDatabase g_WeaponDatabase;

extern void LoadEquipmentData();

// The sound categories found in the weapon classname.txt files
// This needs to match the WeaponSound_t enum in weapon_parse.h
const char *pWeaponSoundCategories[ NUM_SHOOT_SOUND_TYPES ] = 
{
	"empty",
	"single_shot",
	"single_shot_accurate",
	"single_shot_npc",
	"double_shot",
	"double_shot_npc",
	"burst",
	"reload",
	"reload_npc",
	"melee_miss",
	"melee_hit",
	"melee_hit_world",
	"special1",
	"special2",
	"special3",
	"taunt",
	"nearlyempty",
	"fastreload"
};

int GetWeaponSoundFromString( const char *pszString )
{
	for ( int i = EMPTY; i < NUM_SHOOT_SOUND_TYPES; i++ )
	{
		if ( !V_stricmp(pszString,pWeaponSoundCategories[i]) )
			return (WeaponSound_t)i;
	}
	return -1;
}

// Item flags that we parse out of the file.
typedef struct
{
	const char *m_pFlagName;
	int m_iFlagValue;
} itemFlags_t;
itemFlags_t g_ItemFlags[8] =
{
	{ "ITEM_FLAG_SELECTONEMPTY",	ITEM_FLAG_SELECTONEMPTY },
	{ "ITEM_FLAG_NOAUTORELOAD",		ITEM_FLAG_NOAUTORELOAD },
	{ "ITEM_FLAG_NOAUTOSWITCHEMPTY", ITEM_FLAG_NOAUTOSWITCHEMPTY },
	{ "ITEM_FLAG_LIMITINWORLD",		ITEM_FLAG_LIMITINWORLD },
	{ "ITEM_FLAG_EXHAUSTIBLE",		ITEM_FLAG_EXHAUSTIBLE },
	{ "ITEM_FLAG_DOHITLOCATIONDMG", ITEM_FLAG_DOHITLOCATIONDMG },
	{ "ITEM_FLAG_NOAMMOPICKUPS",	ITEM_FLAG_NOAMMOPICKUPS },
	{ "ITEM_FLAG_NOITEMPICKUP",		ITEM_FLAG_NOITEMPICKUP }
};

// Find a weapon slot, assuming the weapon's data has already been loaded.
WEAPON_FILE_INFO_HANDLE LookupWeaponInfoSlot( const char *name )
{
	return g_WeaponDatabase.FindWeaponInfo( name );
}

FileWeaponInfo_t *GetFileWeaponInfoFromHandle( WEAPON_FILE_INFO_HANDLE handle )
{
	return g_WeaponDatabase.GetFileWeaponInfoFromHandle( handle );
}

void PrecacheFileWeaponInfoDatabase()
{
	g_WeaponDatabase.PrecacheAllWeapons();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : WEAPON_FILE_INFO_HANDLE
//-----------------------------------------------------------------------------
WEAPON_FILE_INFO_HANDLE GetInvalidWeaponInfoHandle( void )
{
	return (WEAPON_FILE_INFO_HANDLE)-1;
}

KeyValues* ReadEncryptedKVFile( IFileSystem *filesystem, const char *szFilenameWithoutExtension, const unsigned char *pICEKey, bool bForceReadEncryptedFile )
{
	contract_assert( std::strchr( szFilenameWithoutExtension, '.' ) == nullptr );
	char szFullName[512];

	const char *pSearchPath = "GAME";

	// Open the weapon data file, and abort if we can't
	KeyValues *pKV = new KeyValues( "WeaponDatafile" );
	pKV->UsesEscapeSequences( true );

	V_snprintf(szFullName,sizeof(szFullName), "%s.txt", szFilenameWithoutExtension);

	if ( bForceReadEncryptedFile || !pKV->LoadFromFile( filesystem, szFullName, pSearchPath ) ) // try to load the normal .txt file first
	{
		if ( pICEKey )
		{
			V_snprintf(szFullName,sizeof(szFullName), "%s.ctx", szFilenameWithoutExtension); // fall back to the .ctx file

			FileHandle_t f = filesystem->Open( szFullName, "rb", pSearchPath );

			if (!f)
			{
				pKV->deleteThis();
				return nullptr;
			}
			// load file into a null-terminated buffer
			int fileSize = filesystem->Size(f);
			char *buffer = (char*)MemAllocScratch(fileSize + 1);
		
			contract_assert( buffer );
		
			filesystem->Read(buffer, fileSize, f); // read into local buffer
			buffer[fileSize] = 0; // null terminate file as EOF
			filesystem->Close( f );	// close file after reading

			UTIL_DecodeICE( (unsigned char*)buffer, fileSize, pICEKey );

			bool retOK = pKV->LoadFromBuffer( szFullName, buffer, filesystem );

			MemFreeScratch();

			if ( !retOK )
			{
				pKV->deleteThis();
				return nullptr;
			}
		}
		else
		{
			pKV->deleteThis();
			return nullptr;
		}
	}

	return pKV;
}

std::vector< WeaponInfoLookup* > FileWeaponInfo_t::ms_vecWeaponInfoLookup;

bool							FileWeaponInfo_t::ms_bWeaponInfoLookupInitialized;

WeaponInfoLookup::WeaponInfoLookup( size_t offset, _fieldtypes p_fieldType, const char *szAttribClassName )
{
	m_nWeaponParseDataOffset = offset;
	m_fieldType = p_fieldType;
	m_iszAttribClassName.Set( szAttribClassName );
}

WeaponInfoLookup::WeaponInfoLookup( const WeaponInfoLookup &WepInfoLookup )
{
	m_nWeaponParseDataOffset = WepInfoLookup.m_nWeaponParseDataOffset;
	m_fieldType = WepInfoLookup.m_fieldType;
	m_iszAttribClassName.Set( STRING( WepInfoLookup.m_iszAttribClassName.Get() ) );
}

//-----------------------------------------------------------------------------
// FileWeaponInfo_t implementation.
//-----------------------------------------------------------------------------

FileWeaponInfo_t::FileWeaponInfo_t()
{
	bParsedScript = false;
	bLoadedHudElements = false;
	szClassName[0] = 0;
	szPrintName[0] = 0;

	szViewModel[0] = 0;
	szWorldModel[0] = 0;
	szAnimationPrefix[0] = 0;
	iSlot = 0;
	iPosition = 0;
	iMaxClip1 = 0;
	iMaxClip2 = 0;
	iDefaultClip1 = 0;
	iDefaultClip2 = 0;
	iWeight = 0;
	iRumbleEffect = -1;
	bAutoSwitchTo = false;
	bAutoSwitchFrom = false;
	iFlags = 0;
	szAmmo1[0] = 0;
	szAmmo2[0] = 0;
	szAIAddOn[0] = 0;
	std::memset( aShootSounds, 0, sizeof( aShootSounds ) );
	iAmmoType = 0;
	iAmmo2Type = 0;
	m_bMeleeWeapon = false;
	iSpriteCount = 0;
	iconActive = 0;
	iconInactive = 0;
	iconAmmo = 0;
	iconAmmo2 = 0;
	iconCrosshair = 0;
	iconAutoaim = 0;
	iconZoomedCrosshair = 0;
	iconZoomedAutoaim = 0;
	bShowUsageHint = false;
	m_bAllowFlipping = true;
	m_bBuiltRightHanded = true;

	if ( !ms_bWeaponInfoLookupInitialized )
	{
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( FileWeaponInfo_t, iMaxClip1 ), FIELD_INTEGER, "primary clip size" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( FileWeaponInfo_t, iMaxClip2 ), FIELD_INTEGER, "secondary clip size" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( FileWeaponInfo_t, iDefaultClip1 ), FIELD_INTEGER, "primary default clip size" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( FileWeaponInfo_t, iDefaultClip2 ), FIELD_INTEGER, "secondary default clip size" ) );
		ms_vecWeaponInfoLookup.push_back( new WeaponInfoLookup( __builtin_offsetof( FileWeaponInfo_t, iSlot ), FIELD_INTEGER, "bucket slot" ) );

		std::ranges::stable_sort( ms_vecWeaponInfoLookup, CWeaponInfoLookupListLess() );
		ms_bWeaponInfoLookupInitialized = true;
	}
}

extern ConVar hud_fastswitch;

void FileWeaponInfo_t::Parse( KeyValues *pKeyValuesData, const char *szWeaponName )
{
	// Okay, we tried at least once to look this up...
	bParsedScript = true;

	// Classname
	V_strncpy( szClassName, szWeaponName, MAX_WEAPON_STRING );
	// Printable name
	V_strncpy( szPrintName, pKeyValuesData->GetString( "printname", WEAPON_PRINTNAME_MISSING ), MAX_WEAPON_STRING );
	// View model & world model
	V_strncpy( szViewModel, pKeyValuesData->GetString( "viewmodel" ), MAX_WEAPON_STRING );
	V_strncpy( szWorldModel, pKeyValuesData->GetString( "playermodel" ), MAX_WEAPON_STRING );

	V_StripExtension( szWorldModel, szWorldDroppedModel, sizeof( szWorldDroppedModel ) );
	V_strcat_safe( szWorldDroppedModel, "_dropped.mdl" );
	
	V_strncpy( szAnimationPrefix, pKeyValuesData->GetString( "anim_prefix" ), MAX_WEAPON_PREFIX );
	iSlot = pKeyValuesData->GetInt( "bucket", 0 );
	iPosition = pKeyValuesData->GetInt( "bucket_position", 0 );
	
	// Use the console (X360) buckets if hud_fastswitch is set to 2.
	if ( hud_fastswitch.GetInt() == 2 )
	{
		iSlot = pKeyValuesData->GetInt( "bucket_360", iSlot );
		iPosition = pKeyValuesData->GetInt( "bucket_position_360", iPosition );
	}
	iMaxClip1 = pKeyValuesData->GetInt( "clip_size", WEAPON_NOCLIP );					// Max primary clips gun can hold (assume they don't use clips by default)
	iMaxClip2 = pKeyValuesData->GetInt( "clip2_size", WEAPON_NOCLIP );					// Max secondary clips gun can hold (assume they don't use clips by default)
	iDefaultClip1 = pKeyValuesData->GetInt( "default_clip", iMaxClip1 );		// amount of primary ammo placed in the primary clip when it's picked up
	iDefaultClip2 = pKeyValuesData->GetInt( "default_clip2", iMaxClip2 );		// amount of secondary ammo placed in the secondary clip when it's picked up
	iWeight = pKeyValuesData->GetInt( "weight", 0 );

	iRumbleEffect = pKeyValuesData->GetInt( "rumble", -1 );
	
	// LAME old way to specify item flags.
	// Weapon scripts should use the flag names.
	iFlags = pKeyValuesData->GetInt( "item_flags", ITEM_FLAG_LIMITINWORLD );

	for ( int i=0; i < std::size( g_ItemFlags ); i++ )
	{
		int iVal = pKeyValuesData->GetInt( g_ItemFlags[i].m_pFlagName, -1 );
		if ( iVal == 0 )
		{
			iFlags &= ~g_ItemFlags[i].m_iFlagValue;
		}
		else if ( iVal == 1 )
		{
			iFlags |= g_ItemFlags[i].m_iFlagValue;
		}
	}

	bShowUsageHint = pKeyValuesData->GetBool( "showusagehint", false );
	bAutoSwitchTo = pKeyValuesData->GetBool( "autoswitchto", true );
	bAutoSwitchFrom = pKeyValuesData->GetBool( "autoswitchfrom", true );
	m_bBuiltRightHanded = pKeyValuesData->GetBool( "BuiltRightHanded", true );
	m_bAllowFlipping = pKeyValuesData->GetBool( "AllowFlipping", true );
	m_bMeleeWeapon = pKeyValuesData->GetBool( "MeleeWeapon", false );

	// Primary ammo used
	const char *pAmmo = pKeyValuesData->GetString( "primary_ammo", "None" );
	if ( std::strcmp("None", pAmmo) == 0 )
		V_strncpy( szAmmo1, "", sizeof( szAmmo1 ) );
	else
		V_strncpy( szAmmo1, pAmmo, sizeof( szAmmo1 )  );
	iAmmoType = GetAmmoDef()->Index( szAmmo1 );
	
	// Secondary ammo used
	pAmmo = pKeyValuesData->GetString( "secondary_ammo", "None" );
	if ( std::strcmp("None", pAmmo) == 0)
		V_strncpy( szAmmo2, "", sizeof( szAmmo2 ) );
	else
		V_strncpy( szAmmo2, pAmmo, sizeof( szAmmo2 )  );
	iAmmo2Type = GetAmmoDef()->Index( szAmmo2 );

	// AI AddOn
	const char *pAIAddOn = pKeyValuesData->GetString( "ai_addon", "ai_addon_basecombatweapon" );
	if ( std::strcmp("None", pAIAddOn) == 0)
		V_strncpy( szAIAddOn, "", sizeof( szAIAddOn ) );
	else
		V_strncpy( szAIAddOn, pAIAddOn, sizeof( szAIAddOn )  );

	// Now read the weapon sounds
	std::memset( aShootSounds, 0, sizeof( aShootSounds ) );
	KeyValues *pSoundData = pKeyValuesData->FindKey( "SoundData" );
	if ( pSoundData )
	{
		for ( int i = EMPTY; i < NUM_SHOOT_SOUND_TYPES; i++ )
		{
			const char *soundname = pSoundData->GetString( pWeaponSoundCategories[i] );
			if ( soundname && soundname[0] )
			{
				V_strncpy( aShootSounds[i], soundname, MAX_WEAPON_STRING );
			}
			else
			{
				// FIXME: find a better way to populate defaults
			
				if ( i == NEARLYEMPTY ) // explicit check because it looks like most weapon sound categories don't fall back by default
				{
					V_sprintf_safe( aShootSounds[i], "Default.%s", pWeaponSoundCategories[i] );
				}
			}
		}
	}
}

const char* FileWeaponInfo_t::GetWorldModel( const CEconItemView* pWepView, int iTeam ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		const char *pchWorldOverride = pWepView->GetStaticData()->GetEntityOverrideModel();
		if ( pchWorldOverride )
		{
			return pchWorldOverride;
		}

		return pWepView->GetItemDefinition()->GetWorldDisplayModel();
	}
	else
	{
		return szWorldModel;
	}
}

const char* FileWeaponInfo_t::GetViewModel( const CEconItemView* pWepView, int iTeam ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		const char *pchViewOverride = pWepView->GetStaticData()->GetViewOverrideModel();
		if ( pchViewOverride )
		{
			return pchViewOverride;
		}

		return pWepView->GetItemDefinition()->GetBasePlayerDisplayModel();
	}
	else
	{
		return szViewModel;
	}
}

const char* FileWeaponInfo_t::GetWorldDroppedModel( const CEconItemView* pWepView, int iTeam ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		const char *pchWorldDroppedModel = pWepView->GetItemDefinition()->GetWorldDroppedModel();
		if ( pchWorldDroppedModel )
		{
			return pchWorldDroppedModel;
		}
	}
	
	return szWorldDroppedModel;
}

const char* FileWeaponInfo_t::GetPrimaryAmmo( const CEconItemView* pWepView ) const
{

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = pWepView->GetStaticData()->GetPrimaryAmmo();

		if ( pszString )
		{
			return pszString;
		}
	}

	return szAmmo1;
}

int FileWeaponInfo_t::GetPrimaryAmmoType( const CEconItemView* pWepView ) const
{

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = GetPrimaryAmmo( pWepView );

		if ( pszString )
		{
			return GetAmmoDef()->Index( pszString );
		}
	}

	return iAmmoType;
}

static int WeaponInfoLookupCompare( WeaponInfoLookup * const * src1, WeaponInfoLookup * const * src2 )
{
	if ( ( *src1 )->m_iszAttribClassName.Get() == ( *src2 )->m_iszAttribClassName.Get() )
		return 0;

	if ( ( *src1 )->m_iszAttribClassName.Get() < ( *src2 )->m_iszAttribClassName.Get() )
		return -1;

	return 1;
}

// Convert a schema attribute classname into an index of g_WeaponInfoTable
int FileWeaponInfo_t::GetIndexofAttribute( string_t iszAttribClassName ) const
{
	if ( ms_vecWeaponInfoLookup.empty() )
		return -1;

	if ( ms_vecWeaponInfoLookup[ 0 ]->m_iszAttribClassName.IsSerialNumberOutOfDate() )
	{
		// If the strings are out of date, rebuild and resort them
		for ( WeaponInfoLookup *pWeaponInfoLookup : ms_vecWeaponInfoLookup )
		{
			pWeaponInfoLookup->m_iszAttribClassName.Get();
		}

		ks::qsort( ms_vecWeaponInfoLookup, &WeaponInfoLookupCompare );
	}

	WeaponInfoLookup searchKey;
	searchKey.m_iszAttribClassName.SetFastNoCopy( iszAttribClassName );
	auto it = std::ranges::lower_bound( ms_vecWeaponInfoLookup, &searchKey, CWeaponInfoLookupListLess() );
	if ( it == ms_vecWeaponInfoLookup.end() || CWeaponInfoLookupListLess()( &searchKey, *it ) )
		return -1;
	return (int)( it - ms_vecWeaponInfoLookup.begin() );
}

CWeaponDatabase::CWeaponDatabase() : m_bPreCached( false )
{
}

bool CWeaponDatabase::Init()
{
	ListenForGameEvent( "weapon_reload_database" );
	return LoadManifest();
}

bool CWeaponDatabase::LoadManifest()
{
	// FIXME[pmf]: this is defined as a virtual in g_pGameRules, but it isn't construction yet...
	const unsigned char szEncryptionKey[] = "d7NSuLq2";

	// KeyValues::AutoDelete manifest = ReadEncryptedKVFile( filesystem, "scripts/weapon_manifest.txt", szEncryptionKey );
	KeyValues::AutoDelete manifest( "weaponscripts" );
	manifest->UsesEscapeSequences( true );
	if ( manifest->LoadFromFile( filesystem, "scripts/weapon_manifest.txt", "GAME" ) )
	{
		for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != nullptr ; sub = sub->GetNextKey() )
		{
			if ( !V_stricmp( sub->GetName(), "file" ) )
			{
				char fileBase[512];
				V_FileBase( sub->GetString(), fileBase, sizeof(fileBase) );

				LoadWeaponDataFromFile( filesystem, fileBase, szEncryptionKey );
			}
			else
			{
				Error( "Expecting 'file', got %s\n", sub->GetName() );
			}
		}
	}

	LoadEquipmentData();

	return true;
}

void CWeaponDatabase::PrecacheAllWeapons()
{
// NOTE[pmf]: removed this precache check for now, since it seems that the caches are getting evicted on map change
// 	if ( m_bPreCached )
// 		return;

	for ( const auto &[name, handle] : m_WeaponInfoDatabase )
	{
		const char* szClassName = name.c_str();

		// don't try to precache items that don't have entities (e.g. the items don't exist as entities on the client)
		if ( GetClassMap().GetClassSize(szClassName) <= 0 )
// 		if ( V_strncasecmp(szClassName, "item_", 5) == 0)
			continue;

		UTIL_PrecacheOther( szClassName );
	}
	m_bPreCached = true;
}

void CWeaponDatabase::RefreshAllWeapons()
{
	for ( const auto &[name, handle] : m_WeaponInfoDatabase )
	{
		FileWeaponInfo_t* pWeaponInfo = m_WeaponInfos[handle].get();
		if ( pWeaponInfo )
		{
			pWeaponInfo->RefreshDynamicParameters();
		}
	}
}

WEAPON_FILE_INFO_HANDLE CWeaponDatabase::FindWeaponInfo( const char *name )
{
	auto it = m_WeaponInfoDatabase.find( name );
	return it != m_WeaponInfoDatabase.end() ? it->second : GetInvalidWeaponInfoHandle();
}

WEAPON_FILE_INFO_HANDLE CWeaponDatabase::FindOrCreateWeaponInfo( const char *name )
{
	// Complain about duplicately defined metaclass names...
	auto it = m_WeaponInfoDatabase.find( name );
	if ( it != m_WeaponInfoDatabase.end() )
	{
		return it->second;
	}

	WEAPON_FILE_INFO_HANDLE lookup = (WEAPON_FILE_INFO_HANDLE)m_WeaponInfos.size();
	contract_assert( lookup != GetInvalidWeaponInfoHandle() );
	m_WeaponInfos.emplace_back( CreateWeaponInfo() );
	m_WeaponInfoDatabase.emplace( name, lookup );
	return lookup;
}

static FileWeaponInfo_t * Helper_GetNullWeaponInfo()
{
	static FileWeaponInfo_t gNullWeaponInfo;
	return &gNullWeaponInfo;
}

FileWeaponInfo_t * CWeaponDatabase::GetFileWeaponInfoFromHandle( WEAPON_FILE_INFO_HANDLE handle )
{
	if ( handle >= m_WeaponInfos.size() )
	{
		return Helper_GetNullWeaponInfo();
	}

	return m_WeaponInfos[ handle ].get();
}

//-----------------------------------------------------------------------------
// Purpose: Read data on weapon from script file
// Output:  true  - if data2 successfully read
//			false - if data load fails
//-----------------------------------------------------------------------------
bool CWeaponDatabase::LoadWeaponDataFromFile( IFileSystem* filesystem, const char *szWeaponName, const unsigned char *pICEKey )
{
	char sz[128];
	V_snprintf( sz, sizeof( sz ), "scripts/%s", szWeaponName );
	KeyValues *pKV = ReadEncryptedKVFile( filesystem, sz, pICEKey );
	if ( !pKV )
		return false;

	WEAPON_FILE_INFO_HANDLE handle = FindOrCreateWeaponInfo( szWeaponName );
	FileWeaponInfo_t *pFileInfo = GetFileWeaponInfoFromHandle( handle );
	contract_assert( pFileInfo );

	pFileInfo->Parse( pKV, szWeaponName );

	pKV->deleteThis();

	return true;
}

void CWeaponDatabase::Reset()
{
	m_WeaponInfoDatabase.clear();
	m_WeaponInfos.clear();

	m_bPreCached = false;
}

void CWeaponDatabase::FireGameEvent( IGameEvent *pEvent )
{
	const char *pEventName = pEvent->GetName();
	if ( 0 == V_strcmp( pEventName, "weapon_reload_database" ) )
	{
		LoadManifest();
	}
}

class CBaseHudWeaponSelection;
class CHudElement;
class CBaseCombatWeapon;
class CBasePlayer;

using namespace ::ks::reflect;

// Purpose: Weapon selection handling

inline constexpr const char *HISTORY_DRAW_TIME = "5";

ConVar hud_drawhistory_time( side_registry(), "hud_drawhistory_time", HISTORY_DRAW_TIME, 0 );
ConVar hud_fastswitch( side_registry(), "hud_fastswitch", "1", FCVAR_SS ); // [hpe:jason] Enabling weapon fastswitch by default

//-----------------------------------------------------------------------------
// Purpose: Weapon Selection commands
//-----------------------------------------------------------------------------
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, Slot1, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, Slot2, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, Slot3, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, Slot4, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, Slot5, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, Slot6, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, Slot7, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, Slot8, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, Slot9, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, Slot0, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, Slot10, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, Slot11, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, Close, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, NextWeapon, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, PrevWeapon, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, LastWeapon, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, NextGrenadeWeapon, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, NextItemWeapon, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, NextNonGrenadeWeapon, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, GamePadSlot1, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, GamePadSlot2, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, GamePadSlot3, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, GamePadSlot4, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, GamePadSlot5, "CHudWeaponSelection");
DECLARE_HUD_COMMAND_NAME(CBaseHudWeaponSelection, GamePadSlot6, "CHudWeaponSelection");

HOOK_COMMAND( slot1, Slot1 );
HOOK_COMMAND( slot2, Slot2 );
HOOK_COMMAND( slot3, Slot3 );
HOOK_COMMAND( slot4, Slot4 );
HOOK_COMMAND( slot5, Slot5 );
HOOK_COMMAND( slot6, Slot6 );
HOOK_COMMAND( slot7, Slot7 );
HOOK_COMMAND( slot8, Slot8 );
HOOK_COMMAND( slot9, Slot9 );
HOOK_COMMAND( slot0, Slot0 );
HOOK_COMMAND( slot10, Slot10 );
HOOK_COMMAND( slot11, Slot11 );
static ConCommand cancelselect( side_registry(), "cancelselect", __CmdFunc_Close, "", FCVAR_SERVER_CAN_EXECUTE | FCVAR_CLIENTCMD_CAN_EXECUTE ); // REI: Was HOOK_COMMAND( cancelselect, Close ), but that doesn't have CLIENTCMD_CAN_EXECUTE on it
HOOK_COMMAND( invnext, NextWeapon );
HOOK_COMMAND( invprev, PrevWeapon );
HOOK_COMMAND( lastinv, LastWeapon );
HOOK_COMMAND( invnextgrenade, NextGrenadeWeapon);
HOOK_COMMAND( invnextitem, NextItemWeapon);
HOOK_COMMAND( invnextnongrenade, NextNonGrenadeWeapon);
HOOK_COMMAND( gamepadslot1, GamePadSlot1 );
HOOK_COMMAND( gamepadslot2, GamePadSlot2 );
HOOK_COMMAND( gamepadslot3, GamePadSlot3 );
HOOK_COMMAND( gamepadslot4, GamePadSlot4 );
HOOK_COMMAND( gamepadslot5, GamePadSlot5 );
HOOK_COMMAND( gamepadslot6, GamePadSlot6 );

// instance info
CBaseHudWeaponSelection *CBaseHudWeaponSelection::s_pInstance[MAX_SPLITSCREEN_PLAYERS];
CBaseHudWeaponSelection *CBaseHudWeaponSelection::GetInstance()
{
	return s_pInstance[GET_ACTIVE_SPLITSCREEN_SLOT()];
}
CBaseHudWeaponSelection *GetHudWeaponSelection()
{
	return CBaseHudWeaponSelection::GetInstance();
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBaseHudWeaponSelection::CBaseHudWeaponSelection( const char *pElementName ) : CHudElement( pElementName )
{
	s_pInstance[GET_ACTIVE_SPLITSCREEN_SLOT()] = this;
	
		SetHiddenBits( HIDEHUD_WEAPONSELECTION | HIDEHUD_NEEDSUIT | HIDEHUD_PLAYERDEAD | HIDEHUD_INVEHICLE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHudWeaponSelection::Init(void)
{
	Reset();

	// Initialise the weapons resource
	gWR.Init();

	m_flSelectionTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHudWeaponSelection::Reset(void)
{
	gWR.Reset();

	// Start hidden
	m_bSelectionVisible = false;
	m_flSelectionTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHudWeaponSelection::UpdateSelectionTime( void )
{
	m_flSelectionTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHudWeaponSelection::VidInit(void)
{
	// If we've already loaded weapons, let's get new sprites
	gWR.LoadAllWeaponSprites();

	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHudWeaponSelection::OnThink( void )
{
	// Don't allow weapon selection if we're frozen in place
	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	if ( pPlayer->GetFlags() & FL_FROZEN || pPlayer->IsPlayerDead() )
	{
		if ( IsInSelectionMode() )
		{
			CancelWeaponSelection();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Think used for selection of weapon menu item.
//-----------------------------------------------------------------------------
void CBaseHudWeaponSelection::ProcessInput()
{
	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	// Check to see if the player is in VGUI mode...
	if ( pPlayer->IsInVGuiInputMode() && !pPlayer->IsInViewModelVGuiInputMode() )
	{
		// If so, close weapon selection when they press fire
		if ( GetHud().m_iKeyBits & IN_ATTACK )
		{
			if ( HUDTYPE_PLUS != hud_fastswitch.GetInt() )
			{
				// Swallow the button
				GetHud().m_iKeyBits &= ~IN_ATTACK;
				input->ClearInputButton( IN_ATTACK );
			}

			engine->ClientCmd( "cancelselect\n" );
		}
		return;
	}

	// Has the player selected a weapon?
	if ( GetHud().m_iKeyBits & ( IN_ATTACK | IN_ATTACK2 | IN_ZOOM ) )
	{
		if ( IsWeaponSelectable() )
		{
			if ( HUDTYPE_PLUS != hud_fastswitch.GetInt() )
			{
				// Swallow the button
				GetHud().m_iKeyBits &= ~( IN_ATTACK | IN_ATTACK2 | IN_ZOOM );
				input->ClearInputButton( IN_ATTACK );
				input->ClearInputButton( IN_ATTACK2 );
				input->ClearInputButton( IN_ZOOM );
			}

			// select weapon
			SelectWeapon();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseHudWeaponSelection::IsInSelectionMode()
{
	return m_bSelectionVisible;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHudWeaponSelection::OpenSelection( void )
{
	m_bSelectionVisible = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHudWeaponSelection::HideSelection( void )
{
	m_bSelectionVisible = false;
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether a weapon can be selected in the HUD, based on hud type
//-----------------------------------------------------------------------------
bool CBaseHudWeaponSelection::CanBeSelectedInHUD( CBaseCombatWeapon *pWeapon )
{
	// Xbox: In plus type, weapons without ammo can still be selected in the HUD
	if( HUDTYPE_PLUS == hud_fastswitch.GetInt() )
	{
		return pWeapon->VisibleInWeaponSelection();
	}

	if ( !pWeapon->VisibleInWeaponSelection() )
	{
		return false;
	}

	// All other current hud types
	return pWeapon->CanBeSelected();
}

//-----------------------------------------------------------------------------
// Purpose: handles keyboard input
//-----------------------------------------------------------------------------
int	CBaseHudWeaponSelection::KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding ) 
{
	if (IsInSelectionMode() && pszCurrentBinding && !stricmp(pszCurrentBinding, "cancelselect"))
	{
		HideSelection();
		// returning 0 indicates, we've handled it, no more action needs to be taken
		return 0;
	}

	if ( down >= 1 && keynum >= KEY_1 && keynum <= KEY_9 )
	{
		if ( HandleHudMenuInput( keynum - KEY_0 ) )
			return 0;
	}

	// let someone else handle it
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: called when a weapon has been picked up
//-----------------------------------------------------------------------------
void CBaseHudWeaponSelection::OnWeaponPickup( CBaseCombatWeapon *pWeapon )
{
}

//------------------------------------------------------------------------
// Command Handlers
//------------------------------------------------------------------------
void CBaseHudWeaponSelection::UserCmd_Slot1(void)
{
	if( HUDTYPE_CAROUSEL == hud_fastswitch.GetInt() )
	{
		UserCmd_LastWeapon();
	}
	else
	{
		SelectSlot( 1 );
	}
}

void CBaseHudWeaponSelection::UserCmd_Slot2(void)
{
	if( HUDTYPE_CAROUSEL == hud_fastswitch.GetInt() )
	{
		UserCmd_NextWeapon();
	}
	else
	{
		SelectSlot( 2 );
	}
}

void CBaseHudWeaponSelection::UserCmd_Slot3(void)
{
	if( HUDTYPE_CAROUSEL == hud_fastswitch.GetInt() )
	{
		engine->ClientCmd( "phys_swap" );
	}
	else
	{
		SelectSlot( 3 );
	}
}

void CBaseHudWeaponSelection::UserCmd_Slot4(void)
{
	if( HUDTYPE_CAROUSEL == hud_fastswitch.GetInt() )
	{
		UserCmd_PrevWeapon();
	}
	else
	{
		SelectSlot( 4 );
	}
}

void CBaseHudWeaponSelection::UserCmd_Slot5(void)
{
	SelectSlot( 5 );
}

void CBaseHudWeaponSelection::UserCmd_Slot6(void)
{
	SelectSlot( 6 );
}

void CBaseHudWeaponSelection::UserCmd_Slot7(void)
{
	SelectSlot( 7 );
}

void CBaseHudWeaponSelection::UserCmd_Slot8(void)
{
	SelectSlot( 8 );
}

void CBaseHudWeaponSelection::UserCmd_Slot9(void)
{
	SelectSlot( 9 );
}

void CBaseHudWeaponSelection::UserCmd_Slot0(void)
{
	SelectSlot( 0 );
}

void CBaseHudWeaponSelection::UserCmd_Slot10(void)
{
	SelectSlot( 10 );
}

void CBaseHudWeaponSelection::UserCmd_Slot11(void)
{
	SelectSlot( 11 );
}

//void CBaseHudWeaponSelection::UserCmd_GamePadSlot1( void )
//{
//
//}
//
//void CBaseHudWeaponSelection::UserCmd_GamePadSlot2( void )
//{
//
//}
//
//void CBaseHudWeaponSelection::UserCmd_GamePadSlot3( void )
//{
//
//}
//
//void CBaseHudWeaponSelection::UserCmd_GamePadSlot4( void )
//{
//
//}
//
//void CBaseHudWeaponSelection::UserCmd_GamePadSlot5( void )
//{
//
//}
//
//void CBaseHudWeaponSelection::UserCmd_GamePadSlot6( void )
//{
//
//}

//-----------------------------------------------------------------------------
// Purpose: returns true if the CHudMenu should take slot1, etc commands
//-----------------------------------------------------------------------------
bool CBaseHudWeaponSelection::IsHudMenuTakingInput()
{
	CHudMenu *pHudMenu = GET_HUDELEMENT( CHudMenu );
	return ( pHudMenu && pHudMenu->IsMenuOpen() );
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the CHudMenu handles the slot command
//-----------------------------------------------------------------------------
bool CBaseHudWeaponSelection::HandleHudMenuInput( int iSlot )
{
	CHudMenu *pHudMenu = GET_HUDELEMENT( CHudMenu );
	if ( !pHudMenu || !pHudMenu->IsMenuOpen() )
		return false;

	pHudMenu->SelectMenuItem( iSlot );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the weapon selection hud should be hidden because
//          the CHudMenu is open
//-----------------------------------------------------------------------------
bool CBaseHudWeaponSelection::IsHudMenuPreventingWeaponSelection()
{
	// Don't allow weapon selection if we're frozen in place
	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	if ( pPlayer->GetFlags() & FL_FROZEN || pPlayer->IsPlayerDead() )
		return true;

	return IsHudMenuTakingInput();
}

//-----------------------------------------------------------------------------
// Purpose: Menu Selection Code
//-----------------------------------------------------------------------------
void CBaseHudWeaponSelection::SelectSlot( int iSlot )
{
	// A menu may be overriding weapon selection commands
	if ( HandleHudMenuInput( iSlot ) )
	{
		return;
	}

	// If we're not allowed to draw, ignore weapon selections
	if ( !BaseClass::ShouldDraw() )
	{
		return;
	}

	UpdateSelectionTime();
	SelectWeaponSlot( iSlot );
}

//-----------------------------------------------------------------------------
// Purpose: Close the weapon selection
//-----------------------------------------------------------------------------
void CBaseHudWeaponSelection::UserCmd_Close(void)
{
	CancelWeaponSelection();
}

//-----------------------------------------------------------------------------
// Purpose: Selects the next item in the weapon menu
//-----------------------------------------------------------------------------
void CBaseHudWeaponSelection::UserCmd_NextWeapon(void)
{
	// If we're not allowed to draw, ignore weapon selections
	if ( !BaseClass::ShouldDraw() )
		return;

	CycleToNextWeapon();
	if( hud_fastswitch.GetInt() > 0 )
	{
		SelectWeapon();
	}
	UpdateSelectionTime();
}

//-----------------------------------------------------------------------------
// Purpose: Selects the next grenade or bom in the weapon menu
//-----------------------------------------------------------------------------
void CBaseHudWeaponSelection::UserCmd_NextGrenadeWeapon(void)
{
	// If we're not allowed to draw, ignore weapon selections
	if ( !BaseClass::ShouldDraw() )
		return;

	CycleToNextGrenadeOrBomb();
	if( hud_fastswitch.GetInt() > 0 )
	{
		SelectWeapon();
	}
	UpdateSelectionTime();
}

//-----------------------------------------------------------------------------
// Purpose: Selects the next grenade, bomb or melee weapon in the weapon menu
//-----------------------------------------------------------------------------
void CBaseHudWeaponSelection::UserCmd_NextItemWeapon(void)
{
	// If we're not allowed to draw, ignore weapon selections
	if ( !BaseClass::ShouldDraw() )
		return;

	CycleToNextGrenadeBombOrMelee();
	if( hud_fastswitch.GetInt() > 0 )
	{
		SelectWeapon();
	}
	UpdateSelectionTime();
}

//-----------------------------------------------------------------------------
// Purpose: Selects the next non grenade item in the weapon menu
//-----------------------------------------------------------------------------
void CBaseHudWeaponSelection::UserCmd_NextNonGrenadeWeapon(void)
{
	// If we're not allowed to draw, ignore weapon selections
	if ( !BaseClass::ShouldDraw() )
		return;

	CycleToNextNonGrenadeOrBomb();
	if( hud_fastswitch.GetInt() > 0 )
	{
		SelectWeapon();
	}
	UpdateSelectionTime();
}

//-----------------------------------------------------------------------------
// Purpose: Selects the previous item in the menu
//-----------------------------------------------------------------------------
void CBaseHudWeaponSelection::UserCmd_PrevWeapon(void)
{
	// If we're not allowed to draw, ignore weapon selections
	if ( !BaseClass::ShouldDraw() )
		return;

	CycleToPrevWeapon();

	if( hud_fastswitch.GetInt() > 0 )
	{
		SelectWeapon();
	}

	UpdateSelectionTime();
}

//-----------------------------------------------------------------------------
// Purpose: Switches the last weapon the player was using
//-----------------------------------------------------------------------------
void CBaseHudWeaponSelection::UserCmd_LastWeapon(void)
{
	// If we're not allowed to draw, ignore weapon selections
	if ( !BaseClass::ShouldDraw() )
		return;

	/*
	if ( IsHudMenuPreventingWeaponSelection() )	
	{ 
		return;
	}
	*/

	SwitchToLastWeapon();
}

//-----------------------------------------------------------------------------
// Purpose: Switches the last weapon the player was using
//-----------------------------------------------------------------------------
void CBaseHudWeaponSelection::SwitchToLastWeapon( void )
{
	// Get the player's last weapon
	CBasePlayer *player = CBasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	input->MakeWeaponSelection( player->GetLastWeapon() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHudWeaponSelection::SetWeaponSelected( void )
{
	// Mark selection so that it's placed into next CUserCmd created
	input->MakeWeaponSelection( GetSelectedWeapon() );
}

//-----------------------------------------------------------------------------
// Purpose: Player has chosen to draw the currently selected weapon
//-----------------------------------------------------------------------------
void CBaseHudWeaponSelection::SelectWeapon( void )
{
	if ( !GetSelectedWeapon() )
	{
		engine->ClientCmd( "cancelselect\n" );
		return;
	}

	CBasePlayer *player = CBasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	// Don't allow selections of weapons that can't be selected (out of ammo, etc)
	if ( !GetSelectedWeapon()->CanBeSelected() )
	{
		player->EmitSound( "Player.DenyWeaponSelection" );
	}
	else
	{
		SetWeaponSelected();
	
		m_hSelectedWeapon = nullptr;
	
		engine->ClientCmd( "cancelselect\n" );

		if (player->GetTeamNumber() == TEAM_CT)
		{
			// Play the "weapon selected" sound
			player->EmitSound("Player.WeaponSelected_CT");

		}
		else
		{
			// Play the "weapon selected" sound
			player->EmitSound("Player.WeaponSelected_T");
		}
		
	}
}

//-----------------------------------------------------------------------------
// Purpose: Abort selecting a weapon
//-----------------------------------------------------------------------------
void CBaseHudWeaponSelection::CancelWeaponSelection( void )
{
	CBasePlayer *player = CBasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	// Fastswitches happen in a single frame, so the Weapon Selection HUD Element isn't visible
	// yet, but it's going to be next frame. We need to ask it if it thinks it's going to draw,
	// instead of checking it's IsActive flag.
	if ( ShouldDraw() )
	{
		HideSelection();

		m_hSelectedWeapon = nullptr;

		// Play the "close weapon selection" sound based on faction
		//player->EmitSound( "Player.WeaponSelectionClose" );

		if (player->GetTeamNumber() == TEAM_CT)
		{
			// Play the CT Suit sound
			player->EmitSound("Player.WeaponSelectionClose_CT");

		}
		else
		{
			// Play the T Suit sound
			player->EmitSound("Player.WeaponSelectionClose_T");
		}

	}
	else
	{
		engine->ClientCmd("escape");
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the first weapon for a given slot.
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBaseHudWeaponSelection::GetFirstPos( int iSlot )
{
	int iLowestPosition = MAX_WEAPON_POSITIONS;
	CBaseCombatWeapon *pFirstWeapon = nullptr;

	CBasePlayer *player = CBasePlayer::GetLocalPlayer();
	if ( !player )
		return nullptr;

	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		CBaseCombatWeapon *pWeapon = player->GetWeapon( i );
		if ( !pWeapon )
			continue;

		if ( ( pWeapon->GetSlot() == iSlot ) && (pWeapon->VisibleInWeaponSelection()) )
		{
			// If this weapon is lower in the slot than the current lowest, it's our new winner
			if ( pWeapon->GetPosition() <= iLowestPosition )
			{
				iLowestPosition = pWeapon->GetPosition();
				pFirstWeapon = pWeapon;
			}
		}
	}

	return pFirstWeapon;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBaseHudWeaponSelection::GetNextActivePos( int iSlot, int iSlotPos )
{
	if ( iSlotPos >= MAX_WEAPON_POSITIONS || iSlot >= MAX_WEAPON_SLOTS )
		return nullptr;

	int iLowestPosition = MAX_WEAPON_POSITIONS;
	CBaseCombatWeapon *pNextWeapon = nullptr;

	CBasePlayer *player = CBasePlayer::GetLocalPlayer();
	if ( !player )
		return nullptr;
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		CBaseCombatWeapon *pWeapon = player->GetWeapon( i );
		if ( !pWeapon )
			continue;

		if ( CanBeSelectedInHUD( pWeapon ) && pWeapon->GetSlot() == iSlot )
		{
			// If this weapon is lower in the slot than the current lowest, and above our desired position, it's our new winner
			if ( pWeapon->GetPosition() <= iLowestPosition && pWeapon->GetPosition() >= iSlotPos )
			{
				iLowestPosition = pWeapon->GetPosition();
				pNextWeapon = pWeapon;
			}
		}
	}

	return pNextWeapon;
}

class CHudTexture;
class CBaseCombatWeapon;
class CBasePlayer;
class FileWeaponInfo_t;
class WeaponsResource;

using namespace ::ks::reflect;

// Purpose: Weapons Resource implementation

WeaponsResource gWR;

static CHudTexture *FindHudTextureInDict( CHudTextureList& list, const char *psz )
{
	// a repeated name resolves to the first loaded
	auto it = list.lower_bound( psz );
	if ( it == list.end() || ks::iless()( psz, it->first ) )
		return nullptr;

	return it->second.get();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
WeaponsResource::WeaponsResource( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
WeaponsResource::~WeaponsResource( void )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void WeaponsResource::Init( void )
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void WeaponsResource::Reset( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Load all the sprites needed for all registered weapons
//-----------------------------------------------------------------------------
void WeaponsResource::LoadAllWeaponSprites( void )
{
	CBasePlayer *player = CBasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		if ( player->GetWeapon(i) )
		{
			LoadWeaponSprites( player->GetWeapon(i)->GetWeaponFileInfoHandle() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void WeaponsResource::LoadWeaponSprites( WEAPON_FILE_INFO_HANDLE hWeaponFileInfo )
{
	// WeaponsResource is a friend of CBaseCombatWeapon
	FileWeaponInfo_t *pWeaponInfo = GetFileWeaponInfoFromHandle( hWeaponFileInfo );

	if ( !pWeaponInfo )
		return;

	// Already parsed the hud elements?
	if ( pWeaponInfo->bLoadedHudElements )
		return;

	pWeaponInfo->bLoadedHudElements = true;

	pWeaponInfo->iconActive = nullptr;
	pWeaponInfo->iconInactive = nullptr;
	pWeaponInfo->iconAmmo = nullptr;
	pWeaponInfo->iconAmmo2 = nullptr;
	pWeaponInfo->iconCrosshair = nullptr;
	pWeaponInfo->iconAutoaim = nullptr;
	pWeaponInfo->iconSmall = nullptr;

	char sz[128];
	V_snprintf(sz, sizeof( sz ), "scripts/%s", pWeaponInfo->szClassName);

	CHudTextureList tempList;

	LoadHudTextures( tempList, sz, g_pGameRules->GetEncryptionKey() );

	if ( tempList.empty() )
	{
		// no sprite description file for weapon, use default small blocks
		pWeaponInfo->iconActive = HudIcons().GetIcon( "selection" );
		pWeaponInfo->iconInactive = HudIcons().GetIcon( "selection" );
		pWeaponInfo->iconAmmo = HudIcons().GetIcon( "bucket1" );
		return;
	}

	CHudTexture *p;
	p = FindHudTextureInDict( tempList, "crosshair" );
	if ( p )
	{
		pWeaponInfo->iconCrosshair = HudIcons().AddUnsearchableHudIconToList( *p );
	}

	p = FindHudTextureInDict( tempList, "autoaim" );
	if ( p )
	{
		pWeaponInfo->iconAutoaim = HudIcons().AddUnsearchableHudIconToList( *p );
	}

	p = FindHudTextureInDict( tempList, "zoom" );
	if ( p )
	{
		pWeaponInfo->iconZoomedCrosshair = HudIcons().AddUnsearchableHudIconToList( *p );
	}
	else
	{
		pWeaponInfo->iconZoomedCrosshair = pWeaponInfo->iconCrosshair; //default to non-zoomed crosshair
	}

	p = FindHudTextureInDict( tempList, "zoom_autoaim" );
	if ( p )
	{
		pWeaponInfo->iconZoomedAutoaim = HudIcons().AddUnsearchableHudIconToList( *p );
	}
	else
	{
		pWeaponInfo->iconZoomedAutoaim = pWeaponInfo->iconZoomedCrosshair;  //default to zoomed crosshair
	}

	p = FindHudTextureInDict( tempList, "weapon" );
	if ( p )
	{
		pWeaponInfo->iconInactive = HudIcons().AddUnsearchableHudIconToList( *p );
		if ( pWeaponInfo->iconInactive )
		{
			pWeaponInfo->iconInactive->Precache();
		}
	}

	p = FindHudTextureInDict( tempList, "weapon_s" );
	if ( p )
	{
		pWeaponInfo->iconActive = HudIcons().AddUnsearchableHudIconToList( *p );
		if ( pWeaponInfo->iconActive )
		{
			pWeaponInfo->iconActive->Precache();
		}
	}

	p = FindHudTextureInDict( tempList, "weapon_small" );
	if ( p )
	{
		pWeaponInfo->iconSmall = HudIcons().AddUnsearchableHudIconToList( *p );
		if ( pWeaponInfo->iconSmall )
		{
			pWeaponInfo->iconSmall->Precache();
		}
	}

	p = FindHudTextureInDict( tempList, "ammo" );
	if ( p )
	{
		pWeaponInfo->iconAmmo = HudIcons().AddUnsearchableHudIconToList( *p );
		if ( pWeaponInfo->iconAmmo )
		{
			pWeaponInfo->iconAmmo->Precache();
		}
	}

	p = FindHudTextureInDict( tempList, "ammo2" );
	if ( p )
	{
		pWeaponInfo->iconAmmo2 = HudIcons().AddUnsearchableHudIconToList( *p );
		if ( pWeaponInfo->iconAmmo2 )
		{
			pWeaponInfo->iconAmmo2->Precache();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Helper function to return a Ammo pointer from id
//-----------------------------------------------------------------------------
CHudTexture *WeaponsResource::GetAmmoIconFromWeapon( int iAmmoId )
{
	CBasePlayer *player = CBasePlayer::GetLocalPlayer();
	if ( !player )
		return nullptr;

	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		CBaseCombatWeapon *weapon = player->GetWeapon( i );
		if ( !weapon )
			continue;

		if ( weapon->GetPrimaryAmmoType() == iAmmoId )
		{
			return weapon->GetWpnData().iconAmmo;
		}
		else if ( weapon->GetSecondaryAmmoType() == iAmmoId )
		{
			return weapon->GetWpnData().iconAmmo2;
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Get a pointer to a weapon using this ammo
//-----------------------------------------------------------------------------
const FileWeaponInfo_t *WeaponsResource::GetWeaponFromAmmo( int iAmmoId )
{
	CBasePlayer *player = CBasePlayer::GetLocalPlayer();
	if ( !player )
		return nullptr;

	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		CBaseCombatWeapon *weapon = player->GetWeapon( i );
		if ( !weapon )
			continue;

		if ( weapon->GetPrimaryAmmoType() == iAmmoId )
		{
			return &weapon->GetWpnData();
		}
		else if ( weapon->GetSecondaryAmmoType() == iAmmoId )
		{
			return &weapon->GetWpnData();
		}
	}

	return nullptr;
}

using namespace ::ks::reflect;

CGameRules *g_pGameRules = nullptr;
static CWorld *g_pClientWorld;

void ClientWorldFactoryInit()
{
	g_pClientWorld = new CWorld;
}

void ClientWorldFactoryShutdown()
{
	delete g_pClientWorld;
	g_pClientWorld = nullptr;
}

static IClientNetworkable* ClientWorldFactory( int entnum, int serialNum )
{
	contract_assert( g_pClientWorld != nullptr );

	g_pClientWorld->Init( entnum, serialNum );
	return g_pClientWorld;
}

IMPLEMENT_CLIENTCLASS_FACTORY( CWorld, DT_World, CWorld, ClientWorldFactory );

IMPLEMENT_REFLECT_TABLE( CWorld, DT_World );

CWorld::CWorld( void )
{
}

CWorld::~CWorld( void )
{
}

bool CWorld::Init( int entnum, int iSerialNum )
{
	m_flWaveHeight = 0.0f;

	return BaseClass::Init( entnum, iSerialNum );
}

void CWorld::Release()
{
	Term();
}

void CWorld::PreDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PreDataUpdate( updateType );
}

void CWorld::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	// Always force reset to normal mode upon receipt of world in new map
	if ( updateType == DATA_UPDATE_CREATED )
	{
		modemanager->SwitchMode( false, true );

		if ( m_bStartDark )
		{
			ScreenFade_t sf;
			memset( &sf, 0, sizeof( sf ) );
			sf.a = 255;
			sf.r = 0;
			sf.g = 0;
			sf.b = 0;
			sf.duration = (float)(1<<SCREENFADE_FRACBITS) * 5.0f;
			sf.holdTime = (float)(1<<SCREENFADE_FRACBITS) * 1.0f;
			sf.fadeFlags = FFADE_IN | FFADE_PURGE;
			FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
			{
				ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
				GetViewEffects()->Fade( sf );
			}
		}

		OcclusionParams_t params;
		params.m_flMaxOccludeeArea = m_flMaxOccludeeArea;
		params.m_flMinOccluderArea = m_flMinOccluderArea;
		engine->SetOcclusionParameters( params );

		modelinfo->SetLevelScreenFadeRange( m_flMinPropScreenSpaceWidth, m_flMaxPropScreenSpaceWidth );

	}
}

// -----------------------------------------
//	Sprite Index info
// -----------------------------------------
int		g_sModelIndexLaser;			// holds the index for the laser beam
int		g_sModelIndexLaserDot;		// holds the index for the laser beam dot
int		g_sModelIndexFireball;		// holds the index for the fireball
int		g_sModelIndexWExplosion;	// holds the index for the underwater explosion
int		g_sModelIndexBubbles;		// holds the index for the bubbles model

int		g_sModelIndexSmoke;			// holds the index for the smoke cloud
int		g_sModelIndexBloodSpray;	// holds the sprite index for splattered blood
int		g_sModelIndexBloodDrop;		// holds the sprite index for the initial blood

//-----------------------------------------------------------------------------
// Purpose: Precache global weapon resources
//-----------------------------------------------------------------------------
PRECACHE_REGISTER_BEGIN( GLOBAL, WeaponSprites )
	PRECACHE_INDEX( MODEL, "sprites/zerogxplode.vmt", g_sModelIndexFireball )
	PRECACHE_INDEX( MODEL, "sprites/WXplo1.vmt", g_sModelIndexWExplosion )
	PRECACHE_INDEX( MODEL, "sprites/steam1.vmt", g_sModelIndexSmoke )
	PRECACHE_INDEX( MODEL, "sprites/bubble.vmt", g_sModelIndexBubbles )
	PRECACHE_INDEX( MODEL, "sprites/bloodspray.vmt", g_sModelIndexBloodSpray )
	PRECACHE_INDEX( MODEL, "sprites/blood.vmt", g_sModelIndexBloodDrop )
	PRECACHE_INDEX( MODEL, "sprites/laserbeam.vmt", g_sModelIndexLaser )
	PRECACHE_INDEX( MODEL, "sprites/laserdot.vmt", g_sModelIndexLaserDot )
PRECACHE_REGISTER_END()

void W_Precache(void)
{
	PrecacheFileWeaponInfoDatabase();
}

void CWorld::Precache( void )
{
	// Get weapon precaches
	W_Precache();	
}

void CWorld::Spawn( void )
{
	Precache();
}

CWorld *GetClientWorldEntity()
{
	contract_assert( g_pClientWorld != nullptr );
	return g_pClientWorld;
}

float CWorld::GetWaveHeight() const
{
	return m_flWaveHeight;
}

const char *CWorld::GetDetailSpriteMaterial() const
{
	return m_iszDetailSpriteMaterial;
}

using namespace ::ks::reflect;

class CWorldDimsProxy : public IMaterialProxy
{
public:
	CWorldDimsProxy();
	virtual ~CWorldDimsProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
	virtual void Release( void ) { delete this; }
	virtual IMaterial *GetMaterial();

public:
	IMaterialVar *m_pMinsVar;
	IMaterialVar *m_pMaxsVar;
};

CWorldDimsProxy::CWorldDimsProxy()
{
	m_pMinsVar = m_pMaxsVar = nullptr;
}

CWorldDimsProxy::~CWorldDimsProxy()
{
}

bool CWorldDimsProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	m_pMinsVar = pMaterial->FindVar( "$world_mins", nullptr, false );
	m_pMaxsVar = pMaterial->FindVar( "$world_maxs", nullptr, false );
	return true;
}

void CWorldDimsProxy::OnBind( void *pC_BaseEntity )
{
	if ( m_pMinsVar && m_pMaxsVar )
	{
		CWorld *pWorld = GetClientWorldEntity();
		if ( pWorld )
		{
			m_pMinsVar->SetVecValue( (const float*)&pWorld->m_WorldMins, 3 );
			m_pMaxsVar->SetVecValue( (const float*)&pWorld->m_WorldMaxs, 3 );
		}
	}
}

IMaterial *CWorldDimsProxy::GetMaterial()
{
	return m_pMinsVar->GetOwningMaterial();
}

EXPOSE_MATERIAL_PROXY( CWorldDimsProxy, WorldDims );
