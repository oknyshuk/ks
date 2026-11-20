// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

module;
#include <stdarg.h>
#include <string.h>
#include <wchar.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include "usermessages_schema.h"
#include "cdll_client_int.h"
#include "cdll_int.h"
#include "edict.h"
#include "engine/IEngineSound.h"
#include "igameevents.h"
#include "localize/ilocalize.h"
#include "tier2/resourceprecacher.h"
#include "datacache/imdlcache.h"
#include "client_class.h"
#include "declare_class.h"
#include "engine/ICollideable.h"
#include "engine/ivmodelinfo.h"
#include "irecipientfilter.h"
#include "reflect_annotations.h"
#include "vphysics_interface.h"
#include "te_effect_dispatch.h"
#include "reflect_recvtable.h"
module ks.client;

import std;
import ks.core.color;
import ks.core.dbg;
import ks.core.platform;
import ks.game.in_buttons;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.fixed_pool;
import ks.tier1.iconvar;
import ks.tier1.createinterface;
import ks.tier1.keyvalues;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.strtools;
import ks.tier1.vector_ops;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :sharedvar;
import :util;
import :particlemgr;
import :cstrike15.cs_player;
import :precache;
import :baseentity;
import ks.game.shareddefs;
import :achievements_and_stats_interface;
import :cstrike15.cs_player_shared;
import :cstrike15.cs_shareddefs;
import :econ.econ_item_schema_minimal;
import ks.game.gamemovement;
import :noopanimcontroller;
import ks.tier1.exprevaluator;
import ks.tier1.map_ops;
import ks.core.vprof;
import ks.tier1.bitbuf;
import ks.tier1.interfaces;
import :triggers;
import ks.game.mapentities_shared;
import ks.core.types;
import ks.math.ssequaternion;
import ks.core.basetypes;
import ks.core.wchartypes;
import ks.math.compressed_vector;
import ks.math.vector;
import ks.tier1.intrusive_ptr;

class CHud;
class CHudElement;
class CHudRenderGroup;
class CHudTexture;
class CBasePlayer;
class CCSPlayer;

using namespace ::ks::reflect;

//
// hud.cpp
//
// implementation of CHud class
//

static 	ks::object_pool< CHudTexture >	 g_HudTextureMemoryPool( 128 );

//-----------------------------------------------------------------------------
// Purpose: Parses the weapon txt files to get the sprites needed.
//-----------------------------------------------------------------------------
void LoadHudTextures( CHudTextureList& list, const char *szFilenameWithoutExtension, const unsigned char *pICEKey )
{
	KeyValues *pTemp, *pTextureSection;

	KeyValues *pKeyValuesData = ReadEncryptedKVFile( filesystem, szFilenameWithoutExtension, pICEKey );
	if ( pKeyValuesData )
	{
		pTextureSection = pKeyValuesData->FindKey( "TextureData" );
		if ( pTextureSection  )
		{
			// Read the sprite data
			pTemp = pTextureSection->GetFirstSubKey();
			while ( pTemp )
			{
				auto pTex = std::make_unique< CHudTexture >();
				CHudTexture *tex = pTex.get();
				// Key Name is the sprite name
				V_strncpy( tex->szShortName, pTemp->GetName(), sizeof( tex->szShortName ) );

				if ( pTemp->GetString( "font", nullptr ) )
				{
					// it's a font-based icon
					tex->bRenderUsingFont = true;
					tex->cCharacterInFont = *( pTemp->GetString( "character", "" ));
					V_strncpy( tex->szTextureFile, pTemp->GetString( "font" ), sizeof( tex->szTextureFile ) );
				}
				else
				{
					tex->bRenderUsingFont = false;
					V_strncpy( tex->szTextureFile, pTemp->GetString( "file" ), sizeof( tex->szTextureFile ) );
					tex->rc.left	= pTemp->GetInt( "x", 0 );
					tex->rc.top		= pTemp->GetInt( "y", 0 );
					tex->rc.right	= pTemp->GetInt( "width", 0 )	+ tex->rc.left;
					tex->rc.bottom	= pTemp->GetInt( "height", 0 )	+ tex->rc.top;
				}

				list.emplace( tex->szShortName, std::move( pTex ) );

				pTemp = pTemp->GetNextKey();
			}
		}
	}

	// Failed for some reason. Delete the Key data and abort.
	// lwss - fix null deref
	// pKeyValuesData->deleteThis();
	// lwss end
}

//=======================================================================================================================
// Hud Element Visibility handling
//=======================================================================================================================
typedef struct hudelement_hidden_s
{
	char	*sElementName;
	int		iHiddenBits;	// Bits in which this hud element is hidden
} hudelement_hidden_t;

ConVar hidehud( side_registry(), "hidehud", "0", FCVAR_CHEAT | FCVAR_SS );

//=======================================================================================================================
//  CHudElement
//	All hud elements are derived from this class.
//=======================================================================================================================
//-----------------------------------------------------------------------------
// Purpose: Registers the hud element in a global list, in CHud
//-----------------------------------------------------------------------------
CHudElement::CHudElement( const char *pElementName )
{
	InitCHudElementAfterConstruction( pElementName );
}

void CHudElement::InitCHudElementAfterConstruction( const char* pElementName )
{
	m_pHud = nullptr;
	m_bActive = false;
	m_iHiddenBits = 0;
	m_pElementName = pElementName;
	m_nSplitScreenPlayerSlot = -1;
	SetNeedsRemove( false );
	m_bIsParentedToClientDLLRootPanel = false;
	m_ignoreGlobalHudDisable = false;
    m_bWantLateUpdate = false;

	// Make this for all hud elements, but when its a bit safer
}

//-----------------------------------------------------------------------------
// Purpose: Remove this hud element from the global list in CHUD
//-----------------------------------------------------------------------------
CHudElement::~CHudElement()
{
	if ( m_bNeedsRemove )
	{
		GetHud().RemoveHudElement( this );
	}
}

void CHudElement::SetHud( CHud *pHud )
{
	m_pHud = pHud;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudElement::SetActive( bool bActive )
{
	m_bActive = bActive;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : needsremove - 
//-----------------------------------------------------------------------------
void CHudElement::SetNeedsRemove( bool needsremove )
{
	m_bNeedsRemove = needsremove;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudElement::SetHiddenBits( int iBits )
{
	m_iHiddenBits = iBits;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudElement::SetIgnoreGlobalHudDisable( bool hide )
{
	m_ignoreGlobalHudDisable = hide;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudElement::GetIgnoreGlobalHudDisable( void )
{
	return m_ignoreGlobalHudDisable;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudElement::ShouldDraw( void )
{
	bool bShouldDraw = m_pHud && !m_pHud->IsHidden( m_iHiddenBits );
	if ( bShouldDraw )
	{
		// for each render group
		int iNumGroups = std::ssize( m_HudRenderGroups );
		for ( int iGroupIndex = 0; iGroupIndex < iNumGroups; iGroupIndex++ )
		{
			if ( GetHud().IsRenderGroupLockedFor( this, m_HudRenderGroups[ iGroupIndex ] ) )
				return false;
		}
	}

	return bShouldDraw;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHudElement::IsParentedToClientDLLRootPanel() const
{
	return m_bIsParentedToClientDLLRootPanel;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : parented - 
//-----------------------------------------------------------------------------
void CHudElement::SetParentedToClientDLLRootPanel( bool parented )
{
	m_bIsParentedToClientDLLRootPanel = parented;
}

//-----------------------------------------------------------------------------
// Purpose: We can register to be affected by multiple hud render groups
//-----------------------------------------------------------------------------
void CHudElement::RegisterForRenderGroup( const char *pszGroupName )
{
	int iGroupIndex = GetHud().RegisterForRenderGroup( pszGroupName );

	// add group index to our list of registered groups
	if ( ks::find_index( m_HudRenderGroups, iGroupIndex ) == -1 )
	{
		m_HudRenderGroups.push_back( iGroupIndex );
	}
}

void CHudElement::UnregisterForRenderGroup( const char *pszGroupName )
{
	int iGroupIndex = GetHud().RegisterForRenderGroup( pszGroupName );

	ks::erase_first( m_HudRenderGroups, iGroupIndex );
}

//-----------------------------------------------------------------------------
// Purpose: We want to obscure other elements in this group
//-----------------------------------------------------------------------------
void CHudElement::HideLowerPriorityHudElementsInGroup( const char *pszGroupName )
{
	// look up the render group
	int iGroupIndex = GetHud().LookupRenderGroupIndexByName( pszGroupName );

	// lock the group
	GetHud().LockRenderGroup( iGroupIndex, this );
}

//-----------------------------------------------------------------------------
// Purpose: Stop obscuring other elements in this group
//-----------------------------------------------------------------------------
void CHudElement::UnhideLowerPriorityHudElementsInGroup( const char *pszGroupName )
{	
	// look up the render group
	int iGroupIndex = GetHud().LookupRenderGroupIndexByName( pszGroupName );

	// unlock the group
	GetHud().UnlockRenderGroup( iGroupIndex, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CHudElement::GetRenderGroupPriority( void )
{
	return 0;
}

int CHudElement::GetSplitScreenPlayerSlot() const
{
	return m_nSplitScreenPlayerSlot;
}

void CHudElement::SetSplitScreenPlayerSlot( int nSlot )
{
	m_nSplitScreenPlayerSlot = nSlot;
}

CHud gHUD[ MAX_SPLITSCREEN_PLAYERS ];  // global HUD objects

CHud &GetHud( int nSlot /*= -1*/ )
{
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	}
	return gHUD[ nSlot ];
}

bool MsgFunc_ResetHUD( const ks::net::CCSUsrMsg_ResetHud& msg )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return gHUD[ GET_ACTIVE_SPLITSCREEN_SLOT() ].MsgFunc_ResetHUD( msg );
}

bool MsgFunc_SendAudio( const ks::net::CCSUsrMsg_SendAudio& msg )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return gHUD[ GET_ACTIVE_SPLITSCREEN_SLOT() ].MsgFunc_SendAudio( msg );
}

class CSplitHudHelper
{
public:
	CUserMessageBinder m_UMCMsgResetHud;
	CUserMessageBinder m_UMCMsgSendAudio;

	void Init()
	{
		m_UMCMsgResetHud.Bind< ks::net::CS_UM_ResetHud, ks::net::CCSUsrMsg_ResetHud >( MsgFunc_ResetHUD );
		m_UMCMsgSendAudio.Bind< ks::net::CS_UM_SendAudio, ks::net::CCSUsrMsg_SendAudio >( MsgFunc_SendAudio );
	}

};

static CSplitHudHelper g_HudHelper;

CHud::CHud()
{
	m_flScreenShotTime = -1;
	m_nSplitScreenSlot = -1;
	m_bEngineIsInGame = false;
}

std::vector< CHudElement * > &CHud::GetHudList()
{
	return m_HudList;
}

const std::vector< CHudElement * > &CHud::GetHudList() const
{
	return m_HudList;
}

//-----------------------------------------------------------------------------
// Purpose: This is called every time the DLL is loaded
//-----------------------------------------------------------------------------
void CHud::Init( void )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();

	m_nSplitScreenSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	g_HudHelper.Init();

	m_iDisabledCount = 0;

	// Create all the Hud elements
	CHudElementHelper::CreateAllElements();

	// Initialize all created elements
	for ( int i = 0; i < std::ssize( GetHudList() ); i++ )
	{
		GetHudList()[ i ]->Init();
	}

	if ( GET_ACTIVE_SPLITSCREEN_SLOT() == 0 )
	{
		HudIcons().Init();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHud::Shutdown( void )
{
	// Delete all the Hud elements
	int iMax = std::ssize( GetHudList() );
	for ( int i = iMax-1; i >= 0; i-- )
	{
		delete GetHudList()[i];
	}
	GetHudList().clear();

	if ( GET_ACTIVE_SPLITSCREEN_SLOT() == 0 )
	{
		HudIcons().Shutdown();
	}
}

//-----------------------------------------------------------------------------
// Purpose: LevelInit's called whenever a new level's starting
//-----------------------------------------------------------------------------
void CHud::LevelInit( void )
{
	// Tell all the registered hud elements to LevelInit
	for ( int i = 0; i < std::ssize( GetHudList() ); i++ )
	{
		GetHudList()[ i ]->LevelInit();
	}

	// Unhide all render groups
	for ( auto &[iGroupIndex, group] : m_RenderGroups )
	{
		group->bHidden = false;
		group->m_pLockingElements.clear();
		group->m_pLockingElements.shrink_to_fit();
	}
}

//-----------------------------------------------------------------------------
// Purpose: LevelShutdown's called whenever a level's finishing
//-----------------------------------------------------------------------------
void CHud::LevelShutdown( void )
{
	// Tell all the registered hud elements to LevelInit
	for ( int i = 0; i < std::ssize( GetHudList() ); i++ )
	{
		GetHudList()[ i ]->LevelShutdown();
	}
}

//-----------------------------------------------------------------------------
// Purpose: cleans up memory allocated for m_rg* arrays
//-----------------------------------------------------------------------------
CHud::~CHud()
{
}

void CHudTexture::Precache( void )
{
}

void CHudTexture::DrawSelf( int x, int y, const Color& clr, float flApparentZ ) const
{
	DrawSelf( x, y, Width(), Height(), clr, flApparentZ );
}

void CHudTexture::DrawSelf( int x, int y, int w, int h, const Color& clr, float flApparentZ ) const
{
}

void CHudTexture::DrawSelfCropped( int x, int y, int cropx, int cropy, int cropw, int croph, int finalWidth, int finalHeight, Color clr, float flApparentZ ) const
{
}

void CHudTexture::DrawSelfCropped( int x, int y, int cropx, int cropy, int cropw, int croph, Color clr, float flApparentZ ) const
{
	DrawSelfCropped( x, y, cropx, cropy, cropw, croph, cropw, croph, clr, flApparentZ );
}

void CHudTexture::DrawSelfScalableCorners( int drawX, int drawY, int w, int h, int iSrcCornerW, int iSrcCornerH, int iDrawCornerW, int iDrawCornerH, Color clr, float flApparentZ ) const
{
}

//-----------------------------------------------------------------------------
// Purpose: returns width of texture with scale factor applied.  ( If rendered
//			using font, scale factor is ignored. )
//-----------------------------------------------------------------------------
int CHudTexture::EffectiveWidth( float flScale ) const
{
	return ( int ) ( Width() * flScale );
}

//-----------------------------------------------------------------------------
// Purpose: returns height of texture with scale factor applied.  ( If rendered
//			using font, scale factor is ignored. )
//-----------------------------------------------------------------------------
int CHudTexture::EffectiveHeight( float flScale ) const
{
	return ( int ) ( Height() * flScale );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHud::OnRestore()
{
	ResetHUD();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHud::VidInit( void )
{
	for ( int i = 0; i < std::ssize( GetHudList() ); i++ )
	{
		GetHudList()[ i ]->VidInit();
	}

	ResetHUD();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudElement *CHud::FindElement( const char *pName )
{
	for ( int i = 0; i < std::ssize( GetHudList() ); i++ )
	{
		if ( stricmp( GetHudList()[ i ]->GetName(), pName ) == 0 )
			return GetHudList()[i];
	}

	DevWarning( 1, "[%d] Could not find Hud Element: %s\n", m_nSplitScreenSlot, pName );
	contract_assert( 0 );
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Adds a member to the HUD
//-----------------------------------------------------------------------------
void CHud::AddHudElement( CHudElement *pHudElement ) 
{
	pHudElement->SetSplitScreenPlayerSlot( GET_ACTIVE_SPLITSCREEN_SLOT() );

	// Add the hud element to the end of the array
	GetHudList().push_back( pHudElement );

	pHudElement->SetHud( this );
	pHudElement->SetNeedsRemove( true );
}

//-----------------------------------------------------------------------------
// Purpose: Remove an element from the HUD
//-----------------------------------------------------------------------------
void CHud::RemoveHudElement( CHudElement *pHudElement ) 
{
	int location = ks::find_index( GetHudList(), pHudElement );
	{ auto &list = GetHudList(); list.erase( list.begin() + location ); }
}

//-----------------------------------------------------------------------------
// Purpose: Returns current mouse sensitivity setting
// Output : float - the return value
//-----------------------------------------------------------------------------
float CHud::GetSensitivity( void )
{
	return m_flMouseSensitivity;
}

float CHud::GetFOVSensitivityAdjust()
{
	return m_flFOVSensitivityAdjust;
}

void CHud::DisableHud( void )
{
	if ( m_iDisabledCount < 1 )
		m_iDisabledCount = 1;
	else
		m_iDisabledCount++;
}

void CHud::EnableHud( void )
{
	if ( m_iDisabledCount <= 0 )
		DebuggerBreakIfDebugging();

	m_iDisabledCount--;
}

bool CHud::HudDisabled( void )
{
	return m_iDisabledCount > 0;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the passed in sections of the HUD shouldn't be drawn
//-----------------------------------------------------------------------------
bool CHud::IsHidden( int iHudFlags )
{
	// Not in game?
	if ( !m_bEngineIsInGame )
		return true;

	// Grab the local or observed player
	CBasePlayer *pPlayer = GetHudPlayer();

	// Grab the local player
	CCSPlayer *localPlayer = CCSPlayer::GetLocalCSPlayer();

	if ( !pPlayer )
		return true;

	int iHideHud = pPlayer->m_Local.m_iHideHUD;
	ConVarRef hidehudref( "hidehud" );
	if ( hidehudref.GetInt() )
	{
		iHideHud = hidehudref.GetInt();
	}

	// Everything hidden?
	if ( iHideHud & HIDEHUD_ALL )
		return true;

	// hide health if not chasing a target
	if ( localPlayer->GetObserverMode() == OBS_MODE_ROAMING || 
		localPlayer->GetObserverMode() == OBS_MODE_FIXED || 
		localPlayer->GetObserverMode() == OBS_MODE_FREEZECAM || 
		localPlayer->GetObserverMode() == OBS_MODE_DEATHCAM )
	{
		if ( (iHudFlags & HIDEHUD_HEALTH) || (iHudFlags & HIDEHUD_WEAPONSELECTION) )
			return true;
	}

	// Hide all hud elements if we're blurring the background, since they don't blur properly
	if ( GetClientMode()->GetBlurFade() )
		return true;

	// Don't show hud elements when we're at the mainmenu with a background map running
	if ( engine->IsLevelMainMenuBackground() )
		return true;

	// Local player dead?
	if ( ( iHudFlags & HIDEHUD_PLAYERDEAD ) && ( pPlayer->GetHealth() <= 0 ) )
		return true;

	// Need the HEV suit ( HL2 )
	if ( ( iHudFlags & HIDEHUD_NEEDSUIT ) && ( !pPlayer->IsSuitEquipped() ) )
		return true;

	if ( CSGameRules() && CSGameRules()->IsPlayingTraining() )
	{
		CCSPlayer *pCSPlayer = static_cast<CCSPlayer *>( pPlayer );
		// hide the mini scoreboard?
		if ( ( iHudFlags & HIDEHUD_MINISCOREBOARD ) && ( pCSPlayer && pCSPlayer->IsMiniScoreHidden() ) )
			return true;

		// hide the radar?
		if ( ( iHudFlags & HIDEHUD_RADAR ) && ( pCSPlayer && pCSPlayer->IsRadarHidden() ) )
			return true;
	}

	return ( ( iHudFlags & iHideHud ) != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Allows HUD to modify input data
//-----------------------------------------------------------------------------
void CHud::ProcessInput( bool bActive )
{
	if ( bActive )
	{
		m_iKeyBits = input->GetButtonBits( false );

		// Weaponbits need to be sent down as a UserMsg now.
		GetHud().Think();
	}
}

int CHud::LookupRenderGroupIndexByName( const char *pszGroupName )
{
	int iIndex = ks::find_index( m_RenderGroupNames, pszGroupName );

	contract_assert( ks::is_valid_index( m_RenderGroupNames, iIndex ) );

	return iIndex;
}

//-----------------------------------------------------------------------------
// Purpose: A hud element wants to lock this render group so other panels in the
// group do not draw
//-----------------------------------------------------------------------------
bool CHud::LockRenderGroup( int iGroupIndex, CHudElement *pLocker /* = NULL */ )
{
	// does this index exist?
	if ( !DoesRenderGroupExist( iGroupIndex ) )
		return false;

	CHudRenderGroup *group = m_RenderGroups.at( iGroupIndex ).get();

	contract_assert( group );

	if ( group )
	{
		// NULL pLocker means some higher power is globally hiding this group
		if ( pLocker == nullptr )
		{
			group->bHidden = true;
		}
		else
		{
			// lock us unless we have it locked already
			if ( !ks::contains( group->m_pLockingElements, pLocker ) )
				ks::heap_push( group->m_pLockingElements, pLocker, RenderGroupLessFunc );
		}

		return true;
	}
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: A hud element wants to release the lock on this render group 
//-----------------------------------------------------------------------------
bool CHud::UnlockRenderGroup( int iGroupIndex, CHudElement *pLocker /* = NULL */ )
{
	// does this index exist?
	if ( !DoesRenderGroupExist( iGroupIndex ) )
		return false;

	CHudRenderGroup *group = m_RenderGroups.at( iGroupIndex ).get();

	if ( group )
	{
		// NULL pLocker means some higher power is globally hiding this group
		if ( group->bHidden && pLocker == nullptr )
		{
			group->bHidden = false;
			return true;
		}

		int i = ks::find_index( group->m_pLockingElements, pLocker );
		if ( i >= 0 )
		{
			ks::heap_erase( group->m_pLockingElements, i, RenderGroupLessFunc );
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: See if we should draw based on a hud render group
//			Return true if this group is locked, hud elem will be hidden
//-----------------------------------------------------------------------------
bool CHud::IsRenderGroupLockedFor( CHudElement *pHudElement, int iGroupIndex )
{
	// does this index exist?
	if ( !DoesRenderGroupExist( iGroupIndex ) )
		return false;

	CHudRenderGroup *group = m_RenderGroups.at( iGroupIndex ).get();

	if ( !group )
		return false;

	// hidden for everyone!
	if ( group->bHidden )
		return true;

	if ( group->m_pLockingElements.empty() )
		return false;

	if ( !pHudElement )
		return true;

	CHudElement *pLocker = group->m_pLockingElements.front();

	return ( pLocker != pHudElement && pLocker->GetRenderGroupPriority() > pHudElement->GetRenderGroupPriority() );
}

//-----------------------------------------------------------------------------
// Purpose: CHudElements can ask for the index of hud element render groups
//			returns a group index
//-----------------------------------------------------------------------------
int CHud::RegisterForRenderGroup( const char *pszGroupName )
{
	int iGroupNameIndex = ks::find_index( m_RenderGroupNames, pszGroupName );

	if ( iGroupNameIndex != -1 )
	{	
		return iGroupNameIndex;
	}

	// otherwise add the group
	return AddHudRenderGroup( pszGroupName );
}

//-----------------------------------------------------------------------------
// Purpose: Create a new hud render group
//			returns a group index
//-----------------------------------------------------------------------------
int CHud::AddHudRenderGroup( const char *pszGroupName )
{
	// we tried to register for a group but didn't find it, add a new one

	int iGroupNameIndex = std::ssize( m_RenderGroupNames ); m_RenderGroupNames.push_back( pszGroupName );

	// Group indices are the keys: dense and never removed
	m_RenderGroups.emplace( iGroupNameIndex, std::make_unique< CHudRenderGroup >() );
	return iGroupNameIndex;
}

//-----------------------------------------------------------------------------
// Purpose:  
//-----------------------------------------------------------------------------
bool CHud::DoesRenderGroupExist( int iGroupIndex )
{
	return m_RenderGroups.contains( iGroupIndex );
}

//-----------------------------------------------------------------------------
// Purpose: Allows HUD to Think and modify input data
// Input  : *cdata - 
//			time - 
// Output : int - 1 if there were changes, 0 otherwise
//-----------------------------------------------------------------------------
void CHud::UpdateHud( bool bActive )
{
	// clear the weapon bits.
	m_iKeyBits &= ( ~( IN_WEAPON1|IN_WEAPON2 ));

	GetClientMode()->Update();
}

void CHud::OnSplitScreenStateChanged()
{
	for ( int i = 0; i < std::ssize( GetHudList() ); ++i )
	{
		GetHudList()[ i ]->OnSplitScreenStateChanged();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Force a Hud UI anim to play
//-----------------------------------------------------------------------------
static void testhudanim( const CCommand &args );
static ConCommand testhudanim_command( side_registry(), "testhudanim", testhudanim, "Test a hud element animation.\n\tArguments: <anim name>\n", FCVAR_CHEAT );
static void testhudanim( const CCommand &args )
{
	if ( args.ArgC() != 2 )
	{
		Msg( "Usage:\n   testhudanim <anim name>\n" );
		return;
	}

	GetClientMode()->GetViewportAnimationController()->StartAnimationSequence( args[1] );
}

CHudIcons::CHudIcons() :
	m_bHudTexturesLoaded( false )
{
}

CHudIcons::~CHudIcons()
{
	for ( auto &[name, tex] : m_Icons )
	{
		g_HudTextureMemoryPool.free( tex );
	}
	m_Icons.clear();
}

void CHudIcons::Init()
{
	if ( m_bHudTexturesLoaded )
		return;

	m_bHudTexturesLoaded = true;
	CHudTextureList	textureList;

	// check to see if we have sprites for this res; if not, step down
	LoadHudTextures( textureList, "scripts/hud_textures", nullptr );
	LoadHudTextures( textureList, "scripts/mod_textures", nullptr );
	LoadHudTextures( textureList, "scripts/instructor_textures", nullptr );
	LoadHudTextures( textureList, "scripts/instructor_modtextures", nullptr );
	// PORTAL2

	for ( auto &[name, tex] : textureList )
	{
		AddSearchableHudIconToList( *tex );
	}
}

void CHudIcons::Shutdown()
{
	m_bHudTexturesLoaded = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudTexture *CHudIcons::AddUnsearchableHudIconToList( CHudTexture& texture )
{
	// These names are composed based on the texture file name
	char composedName[ 512 ];

	if ( texture.bRenderUsingFont )
	{
		V_snprintf( composedName, sizeof( composedName ), "%s_c%i",
			texture.szTextureFile, texture.cCharacterInFont );
	}
	else
	{
		V_snprintf( composedName, sizeof( composedName ), "%s_%i_%i_%i_%i",
			texture.szTextureFile, texture.rc.left, texture.rc.top, texture.rc.right, texture.rc.bottom );
	}

	CHudTexture *icon = GetIcon( composedName );
	if ( icon )
	{
		return icon;
	}

	CHudTexture *newTexture = g_HudTextureMemoryPool.alloc();
	*newTexture = texture;

	SetupNewHudTexture( newTexture );

	return m_Icons.emplace( composedName, newTexture ).first->second;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudTexture *CHudIcons::AddSearchableHudIconToList( CHudTexture& texture )
{
	CHudTexture *icon = GetIcon( texture.szShortName );
	if ( icon )
	{
		return icon;
	}

	CHudTexture *newTexture = g_HudTextureMemoryPool.alloc();
	*newTexture = texture;

	SetupNewHudTexture( newTexture );

	return m_Icons.emplace( texture.szShortName, newTexture ).first->second;
}

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to an icon in the list
//-----------------------------------------------------------------------------
CHudTexture *CHudIcons::GetIcon( const char *szIcon )
{
	auto it = m_Icons.find( szIcon );
	if ( it == m_Icons.end() )
		return nullptr;

	return it->second;
}

//-----------------------------------------------------------------------------
// Purpose: Gets texture handles for the hud icon
//-----------------------------------------------------------------------------
void CHudIcons::SetupNewHudTexture( CHudTexture *t )
{
	if ( t->bRenderUsingFont )
	{
		t->rc.top = 0;
		t->rc.left = 0;
	}
	else
	{
		t->textureId = -1;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudIcons::RefreshHudTextures()
{
	if ( !m_bHudTexturesLoaded )
	{
		contract_assert( 0 );
		return;
	}

	CHudTextureList	textureList;

	// check to see if we have sprites for this res; if not, step down
	LoadHudTextures( textureList, "scripts/hud_textures", nullptr );
	LoadHudTextures( textureList, "scripts/mod_textures", nullptr );
	LoadHudTextures( textureList, "scripts/instructor_textures", nullptr );

	// fix up all the texture icons first
	for ( auto &[name, pTex] : textureList )
	{
		CHudTexture *tex = pTex.get();
		contract_assert( tex );

		CHudTexture *icon = GetIcon( tex->szShortName );
		if ( !icon )
			continue;

		// Update file
		V_strncpy( icon->szTextureFile, tex->szTextureFile, sizeof( icon->szTextureFile ) );

		if ( !icon->bRenderUsingFont )
		{
			icon->rc = tex->rc;
		}
	}
}

static CHudIcons g_HudIcons;

CHudIcons &HudIcons()
{
	return g_HudIcons;
}

CHudTexture::CHudTexture()
{
	V_memset( szShortName, 0, sizeof( szShortName ) );
	V_memset( szTextureFile, 0, sizeof( szTextureFile ) );
	V_memset( texCoords, 0, sizeof( texCoords ) );
	V_memset( &rc, 0, sizeof( rc ) );
	textureId = -1;
	bRenderUsingFont = false;
	bPrecached = false;
	cCharacterInFont = 0;
}

class CBaseHudChat;
class CHudElement;
class CBaseEntity;
class CBasePlayer;

using namespace ::ks::reflect;

static constexpr auto CHAT_WIDTH_PERCENTAGE = 0.6f;

ConVar hud_saytext_time( side_registry(), "hud_saytext_time", "12", 0 );
ConVar cl_showtextmsg( side_registry(), "cl_showtextmsg", "1", 0, "Enable/disable text messages printing on the screen." );
ConVar cl_chat_active( side_registry(), "cl_chat_active", "0" );
ConVar cl_chatfilters( side_registry(), "cl_chatfilters", "63", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Stores the chat filter settings " );
ConVar cl_chatfilter_version( side_registry(), "cl_chatfilter_version", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_HIDDEN, "Stores the chat filter version" );

static const int kChatFilterVersion = 1;

Color g_ColorBlue( 153, 204, 255, 255 );
Color g_ColorRed( 255, 63.75, 63.75, 255 );
Color g_ColorGreen( 153, 255, 153, 255 );
Color g_ColorDarkGreen( 64, 255, 64, 255 );
Color g_ColorYellow( 255, 178.5, 0.0, 255 );
Color g_ColorGrey( 204, 204, 204, 255 );

// removes all color markup characters, so Msg can deal with the string properly
// returns a pointer to str
char* RemoveColorMarkup( char *str )
{
	char *out = str;
	for ( char *in = str; *in != 0; ++in )
	{
		if ( *in > 0 && *in < COLOR_MAX )
		{
			continue;
		}
		*out = *in;
		++out;
	}
	*out = 0;

	return str;
}

// converts all '\r' characters to '\n', so that the engine can deal with the properly
// returns a pointer to str
char* ConvertCRtoNL( char *str )
{
	for ( char *ch = str; *ch != 0; ch++ )
		if ( *ch == '\r' )
			*ch = '\n';
	return str;
}

// converts all '\r' characters to '\n', so that the engine can deal with the properly
// returns a pointer to str
wchar_t* ConvertCRtoNL( wchar_t *str )
{
	for ( wchar_t *ch = str; *ch != 0; ch++ )
		if ( *ch == L'\r' )
			*ch = L'\n';
	return str;
}

void StripEndNewlineFromString( char *str )
{
	int s = strlen( str ) - 1;
	if ( s >= 0 )
	{
		if ( str[s] == '\n' || str[s] == '\r' )
			str[s] = 0;
	}
}

void StripEndNewlineFromString( wchar_t *str )
{
	int s = wcslen( str ) - 1;
	if ( s >= 0 )
	{
		if ( str[s] == L'\n' || str[s] == L'\r' )
			str[s] = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reads a string from the current message and checks if it is translatable
//-----------------------------------------------------------------------------
wchar_t* ReadLocalizedString( const char *szString, wchar_t *pOut, int outSize, bool bStripNewline, char *originalString, int originalSize )
{
	if ( originalString )
	{
		V_strncpy( originalString, szString, originalSize );
	}

	const wchar_t *pBuf = g_pLocalize->Find( szString );
	if ( pBuf )
	{
		wcsncpy( pOut, pBuf, outSize/sizeof( wchar_t) );
		pOut[outSize/sizeof( wchar_t)-1] = 0;
	}
	else
	{
		g_pLocalize->ConvertANSIToUnicode( szString, pOut, outSize );
	}

	if ( bStripNewline )
		StripEndNewlineFromString( pOut );

	return pOut;
}

//-----------------------------------------------------------------------------
// Purpose: Reads a string from the current message, converts it to unicode, and strips out color codes
//-----------------------------------------------------------------------------
wchar_t* ReadChatTextString( const char *szString, wchar_t *pOut, int outSize, bool stripBugData )
{
	if ( outSize <= 0 )
		return pOut;

	// Allow localizing player names
	pOut[0] = 0;
	if ( const char *pszEntIndex = StringAfterPrefix( szString, "#ENTNAME[" ) )
	{
		int iEntIndex = V_atoi( pszEntIndex );
		if ( CCS_PlayerResource *pCSPR = ( CCS_PlayerResource* ) GameResources() )
		{
			pCSPR->GetDecoratedPlayerName( iEntIndex, pOut, outSize, ( EDecoratedPlayerNameFlag_t ) ( k_EDecoratedPlayerNameFlag_DontUseNameOfControllingPlayer | k_EDecoratedPlayerNameFlag_DontUseAssassinationTargetName ) );
		}
		if ( !pOut[0] )
		{
			if ( const char *pszCloseBracket = V_strnchr( pszEntIndex, ']', 64 ) )
				szString = pszCloseBracket + 1;
		}
	}

	if ( !pOut[0] )
	{
		g_pLocalize->ConvertANSIToUnicode( szString, pOut, outSize );
		StripEndNewlineFromString( pOut );
	}

	// converts color control characters into control characters for the normal color
	for ( wchar_t *test = pOut; test && *test; ++test )
	{
		if ( *test && ( *test < COLOR_MAX ) )
		{
			*test = COLOR_NORMAL;
		}
	}

	return pOut;
}

CBaseHudChat *g_pHudChat = nullptr;

CBaseHudChat *CBaseHudChat::GetHudChat( void )
{
	contract_assert( g_pHudChat );
	return g_pHudChat;
}

int CBaseHudChat::m_nLineCounter = 1;
//-----------------------------------------------------------------------------
// Purpose: Text chat input/output hud element
//-----------------------------------------------------------------------------
CBaseHudChat::CBaseHudChat( const char *pElementName )
: CHudElement( pElementName )
{
	contract_assert( g_pHudChat == nullptr );
	g_pHudChat = this;

	m_nMessageMode = MM_NONE;
	cl_chat_active.SetValue( m_nMessageMode );

	SetHiddenBits( HIDEHUD_CHAT );

	m_iFilterFlags = cl_chatfilters.GetInt();
}

CBaseHudChat::~CBaseHudChat()
{
	g_pHudChat = nullptr;
}

void CBaseHudChat::CreateChatInputLine( void )
{
}

void CBaseHudChat::CreateChatLines( void )
{
}

CHudChatFilterPanel *CBaseHudChat::GetChatFilterPanel( void )
{
	return m_pFilterPanel;
}

void CBaseHudChat::Reset( void )
{
	Clear();
}

CHudChatHistory *CBaseHudChat::GetChatHistory( void )
{
	return m_pChatHistory;
}

void CBaseHudChat::Init( void )
{
	ListenForGameEvent( "hltv_chat" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszName - 
//			iSize - 
//			*pbuf - 
//-----------------------------------------------------------------------------
bool CBaseHudChat::MsgFunc_SayText( const ks::net::CCSUsrMsg_SayText &msg )
{
	int client = msg.ent_idx;
	const char *szString =  msg.text->c_str();
	bool bWantsToChat = msg.chat ? true : false;

	if ( bWantsToChat )
	{
		// print raw chat text
		ChatPrintf( client, CHAT_FILTER_NONE, "%s", szString );
	}
	else
	{
		// try to lookup translated string
		Printf( CHAT_FILTER_NONE, "%s", hudtextmessage->LookupString( szString ) );
	}

	CLocalPlayerFilter filter;
	CBaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "HudChat.Message" );

	return true;
}

int CBaseHudChat::GetFilterForString( const char *pString )
{
	if ( !V_stricmp( pString, "#HL_Name_Change" ) ) 
	{
		return CHAT_FILTER_NAMECHANGE;
	}

	return CHAT_FILTER_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: Reads in a player's Chat text from the server
//-----------------------------------------------------------------------------
bool CBaseHudChat::MsgFunc_SayText2( const ks::net::CCSUsrMsg_SayText2 &msg )
{
	// Got message during connection
	if ( !g_PR )
		return true;;

	int client = msg.ent_idx;
	bool bWantsToChat = msg.chat ? true : false;

	wchar_t szBuf[6][256];
	char untranslated_msg_text[256];
	wchar_t *msg_text = ReadLocalizedString( msg.msg_name->c_str(), szBuf[0], sizeof( szBuf[0] ), false, untranslated_msg_text, sizeof( untranslated_msg_text ) );

	// keep reading strings and using C format strings for subsituting the strings into the localised text string
	ReadChatTextString ( msg.params[ 0 ].c_str(), szBuf[1], sizeof( szBuf[1] ) );		// player name
	ReadChatTextString ( msg.params[ 1 ].c_str(), szBuf[2], sizeof( szBuf[2] ), true );		// chat text
	ReadLocalizedString( msg.params[ 2 ].c_str(), szBuf[3], sizeof( szBuf[3] ), true );
	ReadLocalizedString( msg.params[ 3 ].c_str(), szBuf[4], sizeof( szBuf[4] ), true );

	g_pLocalize->ConstructString( szBuf[5], sizeof( szBuf[5] ), msg_text, 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );

	char ansiString[512];
	g_pLocalize->ConvertUnicodeToANSI( ConvertCRtoNL( szBuf[5] ), ansiString, sizeof( ansiString ) );

	if ( bWantsToChat )
	{
		int iFilter = CHAT_FILTER_NONE;

		if ( client > 0 && ( g_PR->GetTeam( client ) != g_PR->GetTeam( GetLocalPlayerIndex() )) )
		{
			iFilter = CHAT_FILTER_PUBLICCHAT;
		}

		// print raw chat text
		ChatPrintf( client, iFilter, "%s", ansiString );

		CLocalPlayerFilter filter;
		CBaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "HudChat.Message" );
	}
	else
	{
		// print raw chat text
		ChatPrintf( client, GetFilterForString( untranslated_msg_text), "%s", ansiString );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Message handler for text messages
// displays a string, looking them up from the titles.txt file, which can be localised
// parameters:
//   byte:   message direction  ( HUD_PRINTCONSOLE, HUD_PRINTNOTIFY, HUD_PRINTCENTER, HUD_PRINTTALK )
//   string: message
// optional parameters:
//   string: message parameter 1
//   string: message parameter 2
//   string: message parameter 3
//   string: message parameter 4
// any string that starts with the character '#' is a message name, and is used to look up the real message in titles.txt
// the next ( optional) one to four strings are parameters for that string ( which can also be message names if they begin with '#')
//-----------------------------------------------------------------------------
bool CBaseHudChat::MsgFunc_TextMsg( const ks::net::CCSUsrMsg_TextMsg &msg )
{
	char szString[2048] = {};
	int msg_dest = msg.msg_dst;

	wchar_t szBuf[5][256] = {};
	wchar_t outputBuf[256] = {};

	for ( int i=0; i<5; ++i )
	{
		// Allow localizing player names
		if ( const char *pszEntIndex = StringAfterPrefix( msg.params[ i ].c_str(), "#ENTNAME[" ) )
		{
			int iEntIndex = V_atoi( pszEntIndex );
			wchar_t wszPlayerName[MAX_DECORATED_PLAYER_NAME_LENGTH] = {};
			if ( CCS_PlayerResource *pCSPR = ( CCS_PlayerResource* ) GameResources() )
			{
				pCSPR->GetDecoratedPlayerName( iEntIndex, wszPlayerName, sizeof( wszPlayerName ), ( EDecoratedPlayerNameFlag_t ) ( k_EDecoratedPlayerNameFlag_DontUseNameOfControllingPlayer | k_EDecoratedPlayerNameFlag_DontUseAssassinationTargetName ) );
			}
			if ( wszPlayerName[0] )
			{
				szString[0] = 0;
				V_wcscpy_safe( szBuf[ i ], wszPlayerName );
			}
			else if ( const char *pszEndBracket = V_strnchr( pszEntIndex, ']', 64 ) )
			{
				V_strcpy_safe( szString, pszEndBracket + 1 );
			}
			else
			{
				V_strcpy_safe( szString, msg.params[ i ].c_str() );
			}
		}
		else
		{
			V_strcpy_safe( szString, msg.params[ i ].c_str() );
		}

		if ( szString[0] )
		{
		    static char tmpStrBuf[1024];
			V_strncpy( tmpStrBuf, hudtextmessage->LookupString( szString, &msg_dest ), sizeof(tmpStrBuf) );
			bool bTranslated = false;
			if ( tmpStrBuf[ 0 ] == '#' )	// only translate parameters intended as localization tokens
			{
				const wchar_t *pBuf = g_pLocalize->Find( tmpStrBuf );
				if ( pBuf )
				{
					// Copy pBuf into szBuf[i].
					int nMaxChars = sizeof( szBuf[ i ] ) / sizeof( wchar_t );
					wcsncpy( szBuf[ i ], pBuf, nMaxChars );
					szBuf[ i ][ nMaxChars - 1 ] = 0;
					bTranslated = true;
				}
			}

			if ( !bTranslated )
			{
				if ( i )
				{
					StripEndNewlineFromString( tmpStrBuf );  // these strings are meant for substitution into the main strings, so cull the automatic end newlines
				}
				g_pLocalize->ConvertANSIToUnicode( tmpStrBuf, szBuf[ i ], sizeof( szBuf[ i ] ) );
			}
		}
	}

	if ( !cl_showtextmsg.GetInt() )
		return true;

	int len;
	switch ( msg_dest )
	{
	case HUD_PRINTCENTER:
		g_pLocalize->ConstructString( outputBuf, sizeof( outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );
		GetCenterPrint()->Print( ConvertCRtoNL( outputBuf ) );
		break;

	case HUD_PRINTNOTIFY:
		g_pLocalize->ConstructString( outputBuf, sizeof( outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );
		g_pLocalize->ConvertUnicodeToANSI( outputBuf, szString, sizeof( szString) );
		len = strlen( szString );
		if ( len && szString[len-1] != '\n' && szString[len-1] != '\r' )
		{
			V_strncat( szString, "\n", sizeof( szString), 1 );
		}
		Msg( "%s", ConvertCRtoNL( szString ) );
		break;

	case HUD_PRINTTALK:
		g_pLocalize->ConstructString( outputBuf, sizeof( outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );
		g_pLocalize->ConvertUnicodeToANSI( outputBuf, szString, sizeof( szString) );
		len = strlen( szString );
		if ( len && szString[len-1] != '\n' && szString[len-1] != '\r' )
		{
			V_strncat( szString, "\n", sizeof( szString), 1 );
		}
		Printf( CHAT_FILTER_NONE, "%s", ConvertCRtoNL( szString ) );
		break;

	case HUD_PRINTCONSOLE:
		g_pLocalize->ConstructString( outputBuf, sizeof( outputBuf), szBuf[0], 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );
		g_pLocalize->ConvertUnicodeToANSI( outputBuf, szString, sizeof( szString) );
		len = strlen( szString );
		if ( len && szString[len-1] != '\n' && szString[len-1] != '\r' )
		{
			V_strncat( szString, "\n", sizeof( szString), 1 );
		}
		Msg( "%s", ConvertCRtoNL( szString ) );
		break;
	}

	return true;
}

void CBaseHudChat::MsgFunc_VoiceSubtitle( bf_read &msg )
{
	// Got message during connection
	if ( !g_PR )
		return;

	if ( !cl_showtextmsg.GetInt() )
		return;

	char szString[2048];
	char szPrefix[64];	//( Voice)
	wchar_t szBuf[128];

	int client = msg.ReadByte();
	int iMenu = msg.ReadByte();
	int iItem = msg.ReadByte();

	const char *pszSubtitle = "";

	CGameRules *pGameRules = GameRules();

	CMultiplayRules *pMultiRules = dynamic_cast< CMultiplayRules * >( pGameRules );

	contract_assert( pMultiRules );

	if ( pMultiRules )
	{
		pszSubtitle = pMultiRules->GetVoiceCommandSubtitle( iMenu, iItem );
	}

	SetVoiceSubtitleState( true );

	const wchar_t *pBuf = g_pLocalize->Find( pszSubtitle );
	if ( pBuf )
	{
		// Copy pBuf into szBuf[i].
		int nMaxChars = sizeof( szBuf ) / sizeof( wchar_t );
		wcsncpy( szBuf, pBuf, nMaxChars );
		szBuf[nMaxChars-1] = 0;
	}
	else
	{
		g_pLocalize->ConvertANSIToUnicode( pszSubtitle, szBuf, sizeof( szBuf) );
	}

	int len;
	g_pLocalize->ConvertUnicodeToANSI( szBuf, szString, sizeof( szString) );
	len = strlen( szString );
	if ( len && szString[len-1] != '\n' && szString[len-1] != '\r' )
	{
		V_strncat( szString, "\n", sizeof( szString), 1 );
	}

	const wchar_t *pVoicePrefix = g_pLocalize->Find( "#Voice" );
	g_pLocalize->ConvertUnicodeToANSI( pVoicePrefix, szPrefix, sizeof( szPrefix) );
	
	ChatPrintf( client, CHAT_FILTER_NONE, "%c(%s) %s%c: %s", COLOR_PLAYERNAME, szPrefix, GetDisplayedSubtitlePlayerName( client ), COLOR_NORMAL, ConvertCRtoNL( szString ) );

	SetVoiceSubtitleState( false );
}

const char *CBaseHudChat::GetDisplayedSubtitlePlayerName( int clientIndex )
{
	return g_PR->GetPlayerName( clientIndex );
}

//-----------------------------------------------------------------------------
// Purpose: Allow inheriting classes to change this spacing behavior
//-----------------------------------------------------------------------------
int CBaseHudChat::GetChatInputOffset( void )
{
	return m_iFontHeight;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CBaseHudChat::Printf( int iFilter, const char *fmt, ... )
{
	va_list marker;
	char msg[4096];

	__builtin_va_start( marker, fmt);
	V_vsnprintf( msg, sizeof( msg), fmt, marker);
	__builtin_va_end( marker);

	ChatPrintf( 0, iFilter, "%s", msg );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHudChat::StartMessageMode( int iMessageModeType )
{
	m_nMessageMode = iMessageModeType;
	cl_chat_active.SetValue( m_nMessageMode );

	m_flHistoryFadeTime = gpGlobals->curtime + CHAT_HISTORY_FADE_TIME;

	engine->ClientCmd_Unrestricted( "gameui_preventescapetoshow\n" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHudChat::SetChatPrompt( int iMessageModeType )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHudChat::StopMessageMode( bool bFade )
{
	engine->ClientCmd_Unrestricted( "gameui_allowescapetoshow\n" );

	m_nMessageMode = MM_NONE;
	cl_chat_active.SetValue( m_nMessageMode );

	if ( bFade )
	{
		m_flHistoryFadeTime = gpGlobals->curtime + CHAT_HISTORY_FADE_TIME;
	}
	else
	{
		m_flHistoryFadeTime = gpGlobals->curtime;
	}
}

void CBaseHudChat::FadeChatHistory( void )
{
}

void CBaseHudChat::SetFilterFlag( int iFilter )
{
	m_iFilterFlags = iFilter;

	cl_chatfilters.SetValue( m_iFilterFlags );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Color CBaseHudChat::GetTextColorForClient( TextColor colorNum, int clientIndex )
{
	Color c;
	switch ( colorNum )
	{
	case COLOR_PLAYERNAME:
		c = GetClientColor( clientIndex );
	break;

	case COLOR_LOCATION:
		c = g_ColorDarkGreen;
		break;

	case COLOR_ACHIEVEMENT:
		c = GetDefaultTextColor();
		break;

	default:
		c = GetDefaultTextColor();
	}

	return Color( c[0], c[1], c[2], 255 );
}

//-----------------------------------------------------------------------------
Color CBaseHudChat::GetDefaultTextColor( void )
{
	return g_ColorYellow;
}

//-----------------------------------------------------------------------------
Color CBaseHudChat::GetClientColor( int clientIndex )
{
	if ( clientIndex == 0 ) // console msg
	{
		return g_ColorGreen;
	}
	else if( g_PR )
	{
		return g_ColorGrey;
	}

	return g_ColorYellow;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBaseHudChatLine
//-----------------------------------------------------------------------------
CBaseHudChatLine *CBaseHudChat::FindUnusedChatLine( void )
{
	return m_ChatLine;
}

void CBaseHudChat::Send( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHudChat::Clear( void )
{
	// Kill input prompt
	StopMessageMode();

	m_flHistoryFadeTime = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *newmap - 
//-----------------------------------------------------------------------------
void CBaseHudChat::LevelInit( const char *newmap )
{
	Clear();

	// [pfreese] initialize new chat filters to defaults. We do this because
	// unused filter bits are zero, and we might want them on for new filters that
	// are added.
	//
	// Also, we have to do this here instead of somewhere more sensible like the 
	// c'tor or Init() method, because cvars are loaded after HUD elements
	// have been constructed and initialized.

	switch ( cl_chatfilter_version.GetInt() )
	{
	case 0:
		m_iFilterFlags |= CHAT_FILTER_ACHIEVEMENT;
		// fall through
	case kChatFilterVersion:
		break;
	}

	if ( cl_chatfilter_version.GetInt() != kChatFilterVersion )
	{
		cl_chatfilters.SetValue( m_iFilterFlags );
		cl_chatfilter_version.SetValue( kChatFilterVersion );
	}
}

void CBaseHudChat::LevelShutdown( void )
{
	Clear();
}

void	CBaseHudChat::ChatPrintfW( int iPlayerIndex, int iFilter, const wchar_t *wszNotice )
{

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return; // cannot print potentially personal details
	}

	char ansi[4096];
	g_pLocalize->ConvertUnicodeToANSI( wszNotice, ansi, sizeof( ansi ) );
	ChatPrintf( iPlayerIndex, iFilter, "%s", ansi );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CBaseHudChat::ChatPrintf( int iPlayerIndex, int iFilter, const char *fmt, ... )
{

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return; // cannot print potentially personal details
	}

	va_list marker;
	char msg[4096];

	__builtin_va_start( marker, fmt);
	V_vsnprintf( msg, sizeof( msg), fmt, marker);
	__builtin_va_end( marker);

	// Strip any trailing '\n'
	if ( strlen( msg ) > 0 && msg[ strlen( msg )-1 ] == '\n' )
	{
		msg[ strlen( msg ) - 1 ] = 0;
	}

	// Strip leading \n characters ( or notify/color signifiers ) for empty string check
	char *pmsg = msg;
	while ( *pmsg && ( *pmsg == '\n' || ( *pmsg > 0 && *pmsg < COLOR_MAX ) ) )
	{
		pmsg++;
	}

	if ( !*pmsg )
		return;

	// Now strip just newlines, since we want the color info for printing
	pmsg = msg;
	while ( *pmsg && ( *pmsg == '\n' ) )
	{
		pmsg++;
	}

	if ( !*pmsg )
		return;

	if ( iFilter != CHAT_FILTER_NONE )
	{
		if ( !( iFilter & GetFilterFlags() ) )
			return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHudChat::FireGameEvent( IGameEvent *event )
{
	const char *eventname = event->GetName();

	if ( V_strcmp( "hltv_chat", eventname ) == 0 )
	{
		CBasePlayer *player = CBasePlayer::GetLocalPlayer();

		if ( !player )
			return;
		
		ChatPrintf( player->entindex(), CHAT_FILTER_NONE, "(GOTV) %s", event->GetString( "text" ) );
	}
}

class CHudElement;

using namespace ::ks::reflect;

// Purpose: Helper for the CHudElement class to add themselves to the list of hud elements

// Start with empty list
CHudElementHelper *CHudElementHelper::m_sHelpers = nullptr;

//-----------------------------------------------------------------------------
// Purpose: Constructs a technology factory
// Input  : pfnCreate - fn Ptr to the constructor for this element 
//			depth - determines position in the creation list, 100 is farthest into the screen
//					0 is nearest, default is 50
//
//-----------------------------------------------------------------------------
CHudElementHelper::CHudElementHelper( CHudElement *( *pfnCreate )( void ), int depth, int flags /* = 0 */ )
{
	//Insert into the list based on depth

	//List is empty, or element belongs at front, insert here
	if( m_sHelpers == nullptr || depth >= m_sHelpers->m_iDepth )
	{
		m_pNext	= m_sHelpers;
		m_sHelpers = this;
	}
	else
	{
		//Scan to insert in decreasing depth order
		CHudElementHelper *pPrev = m_sHelpers;
		CHudElementHelper *pCurrent = m_sHelpers->m_pNext;

		while( pCurrent != nullptr && depth < pCurrent->m_iDepth )
		{
			pPrev = pCurrent;
			pCurrent = pCurrent->m_pNext;
		}

		pPrev->m_pNext = this;
		m_pNext = pCurrent;
	}

	m_iDepth = depth;
	m_iFlags = flags;

	// Set attributes
	contract_assert( pfnCreate );
	m_pfnCreate		= pfnCreate;
}

//-----------------------------------------------------------------------------
// Purpose: Returns next object in list
// Output : CHudElementHelper
//-----------------------------------------------------------------------------
CHudElementHelper *CHudElementHelper::GetNext( void )
{ 
	return m_pNext;
}

//-----------------------------------------------------------------------------
// Purpose: Static class creation factory
//  Searches list of registered factories for a match and then instances the 
//  requested technology by name
// Input  : *name - 
// Output : CBaseTFTechnology
//-----------------------------------------------------------------------------
void CHudElementHelper::CreateAllElements( void )
{
	// Start of list
	CHudElementHelper *p = m_sHelpers;
	while ( p )
	{
		// only create this element in slot 0
		if ( ( p->m_iFlags & HUDELEMENT_SS_FULLSCREEN_ONLY ) && ( GET_ACTIVE_SPLITSCREEN_SLOT() != 0 ) )
		{
			p = p->m_pNext;
			continue;
		}

		// Dispatch creation function directly
		CHudElement *( *fCreate )( void ) = p->m_pfnCreate;
		CHudElement *newElement = (fCreate)();
		if ( newElement )
		{
			GetHud().AddHudElement( newElement );				
		}
		p = p->GetNext();
	}
}

using namespace ::ks::reflect;

// Purpose: See header file

ConVar locator_fade_time( side_registry(), "locator_fade_time", "0.3", FCVAR_NONE, "Number of seconds it takes for a lesson to fully fade in/out." );
ConVar locator_lerp_rest( side_registry(), "locator_lerp_rest", "0.25f", FCVAR_NONE, "Number of seconds before moving from the center." );

ConVar locator_target_offset_x( side_registry(), "locator_target_offset_x", "-17", FCVAR_NONE, "How many pixels to offset the locator from the target position." );
ConVar locator_target_offset_y( side_registry(), "locator_target_offset_y", "-64", FCVAR_NONE, "How many pixels to offset the locator from the target position." );

//Precahce the effects

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheLocatorTarget )
	PRECACHE( MATERIAL, "vgui/hud/icon_arrow_left" )
	PRECACHE( MATERIAL, "vgui/hud/icon_arrow_right" )
	PRECACHE( MATERIAL, "vgui/hud/icon_arrow_up" )
	PRECACHE( MATERIAL, "vgui/hud/icon_arrow_down" )
	PRECACHE( MATERIAL, "vgui/hud/icon_arrow_plain" )
PRECACHE_REGISTER_END()

//------------------------------------
CLocatorTarget::CLocatorTarget( void )
{
	Deactivate( true );
}

//------------------------------------
void CLocatorTarget::Activate( int serialNumber )
{ 
	m_serialNumber = serialNumber; 
	m_frameLastUpdated = gpGlobals->framecount;
	m_isActive = true;

	m_bVisible = true;
	m_bOnscreen = true;
	m_alpha = 0;
	m_fadeStart = gpGlobals->curtime;
	m_bIconNoTarget = false;

	m_offsetX = m_offsetY = 0;

	int iStartX = ScreenWidth() / 2;
	int iStartY = ScreenHeight() / 4;

	m_lastXPos = iStartX;
	m_lastYPos = iStartY;

	m_bDrawArrow = false;
	m_fDrawArrowAngle = 0.0f;
	m_lerpStart = gpGlobals->curtime;
	m_pulseStart = gpGlobals->curtime;
	m_declutterIndex = 0;
	m_lastDeclutterIndex = 0;
	AddIconEffects( LOCATOR_ICON_FX_FADE_IN );
}

//------------------------------------
void CLocatorTarget::Deactivate( bool bNoFade )						
{
	if ( ( engine && engine->IsPaused() ) || ( engineui && engineui->IsGameUIVisible() ) )
	{
		bNoFade = true;
	}

	if ( bNoFade || m_alpha == 0 || 
		 ( m_bOccluded && !( m_iEffectsFlags & LOCATOR_ICON_FX_FORCE_CAPTION ) ) || 
		 ( !m_bOnscreen && ( m_iEffectsFlags & LOCATOR_ICON_FX_NO_OFFSCREEN ) ) )
	{
		m_bOriginInScreenspace = false;

		m_serialNumber = -1;
		m_isActive = false;
		m_frameLastUpdated = 0;
		m_pIcon_onscreen = nullptr;
		m_pIcon_offscreen = nullptr;
		m_bDrawControllerButton = false;
		m_bDrawControllerButtonOffscreen = false;
		m_iEffectsFlags = LOCATOR_ICON_FX_NONE;
		m_rgbaIconColor = Color( 255, 255, 255, 255 );
		m_captionWide = 0;

		m_pchDrawBindingName = nullptr;
		m_pchDrawBindingNameOffscreen = nullptr;
		m_widthScale_onscreen = 1.0f;
		m_bOccluded = false;
		m_alpha = 0;
		m_bIsDrawing = false;
		m_bVisible = false;

		m_szUITargetName = "";
		m_szUITargetLookup = "";
		m_nUITargetEdge = 0;

		m_szBinding = "";
		m_iBindingTick = 0;
		m_flNextBindingTick = 0.0f;
		m_flNextOcclusionTest = 0.0f;
		m_iBindingChoicesCount = 0;

		m_wszCaption.clear();
		m_wszCaption.push_back( (wchar_t)0 );

		m_bWasControllerLast = false;
	}
	else if ( !( m_iEffectsFlags & LOCATOR_ICON_FX_FADE_OUT ) )
	{
		// Determine home much time it would have spent fading to reach the current alpha
		float flAssumedFadeTime;
		flAssumedFadeTime = ( 1.0f - static_cast<float>( m_alpha ) / 255.0f ) * locator_fade_time.GetFloat();

		// Set the fade
		m_fadeStart = gpGlobals->curtime - flAssumedFadeTime;
		AddIconEffects( LOCATOR_ICON_FX_FADE_OUT );
		RemoveIconEffects( LOCATOR_ICON_FX_FADE_IN );
	}
}

//------------------------------------
void CLocatorTarget::Update()							
{
	m_frameLastUpdated = gpGlobals->framecount;

	if ( m_bVisible && ( m_iEffectsFlags & LOCATOR_ICON_FX_FADE_OUT ) )
	{
		// Determine home much time it would have spent fading to reach the current alpha
		float flAssumedFadeTime;
		flAssumedFadeTime = ( 1.0f - static_cast<float>( m_alpha ) / 255.0f ) * locator_fade_time.GetFloat();

		// Set the fade
		m_fadeStart = gpGlobals->curtime - flAssumedFadeTime;
		AddIconEffects( LOCATOR_ICON_FX_FADE_OUT );
		RemoveIconEffects( LOCATOR_ICON_FX_FADE_OUT );
	}
}

int CLocatorTarget::GetIconX( void )
{
	return m_iconX + ( IsOnScreen() ? locator_target_offset_x.GetInt()+m_offsetX : 0 );
}

int CLocatorTarget::GetIconY( void )
{
	return m_iconY + ( IsOnScreen() ? locator_target_offset_y.GetInt()+m_offsetY : 0 );
}

int CLocatorTarget::GetIconCenterX( void )
{
	return m_centerX + locator_target_offset_x.GetInt() + m_offsetX;
}

int CLocatorTarget::GetIconCenterY( void )
{
	return m_centerY + locator_target_offset_y.GetInt() + m_offsetY;
}

void CLocatorTarget::SetVisible( bool bVisible )
{
	if ( m_bVisible == bVisible )
	{
		// They are already the same
		return;
	}

	m_bVisible = bVisible;

	if ( bVisible )
	{
		// Determine home much time it would have spent fading to reach the current alpha
		float flAssumedFadeTime;
		flAssumedFadeTime = ( static_cast<float>( m_alpha ) / 255.0f ) * locator_fade_time.GetFloat();

		// Set the fade
		m_fadeStart = gpGlobals->curtime - flAssumedFadeTime;
		AddIconEffects( LOCATOR_ICON_FX_FADE_IN );
		RemoveIconEffects( LOCATOR_ICON_FX_FADE_OUT );
	}
	else
	{
		// Determine home much time it would have spent fading to reach the current alpha
		float flAssumedFadeTime;
		flAssumedFadeTime = ( 1.0f - static_cast<float>( m_alpha ) / 255.0f ) * locator_fade_time.GetFloat();

		// Set the fade
		m_fadeStart = gpGlobals->curtime - flAssumedFadeTime;
		AddIconEffects( LOCATOR_ICON_FX_FADE_OUT );
		RemoveIconEffects( LOCATOR_ICON_FX_FADE_IN );
	}
}
bool CLocatorTarget::IsVisible( void )
{
	return m_bVisible;
}

void CLocatorTarget::SetCaptionText( const char *pszText, const char *pszParam )
{
	wchar_t outbuf[ 256 ];
	outbuf[ 0 ] = L'\0';

	if ( pszParam && pszParam[ 0 ] != '\0' )
	{
		wchar_t wszParamBuff[ 128 ];
		wchar_t *pLocalizedParam = nullptr;

		if ( pszParam[ 0 ] == '#' )
		{
			pLocalizedParam = g_pLocalize->Find( pszParam );
		}

		if ( !pLocalizedParam )
		{
			g_pLocalize->ConvertANSIToUnicode( pszParam, wszParamBuff, sizeof( wszParamBuff ) );
			pLocalizedParam = wszParamBuff;
		}

		wchar_t wszTextBuff[ 128 ];
		wchar_t *pLocalizedText = nullptr;

		if ( pszText[ 0 ] == '#' )
		{
			pLocalizedText = g_pLocalize->Find( pszText );
		}

		if ( !pLocalizedText )
		{
			g_pLocalize->ConvertANSIToUnicode( pszText, wszTextBuff, sizeof( wszTextBuff ) );
			pLocalizedText = wszTextBuff;
		}

		wchar_t buf[ 256 ];
		g_pLocalize->ConstructString( buf, sizeof(buf), pLocalizedText, 1, pLocalizedParam );

		UTIL_ReplaceKeyBindings( buf, sizeof( buf ), outbuf, sizeof( outbuf ) );
	}
	else
	{
		wchar_t wszTextBuff[ 128 ];
		wchar_t *pLocalizedText = nullptr;

		if ( pszText[ 0 ] == '#' )
		{
			pLocalizedText = g_pLocalize->Find( pszText );
		}

		if ( !pLocalizedText )
		{
			g_pLocalize->ConvertANSIToUnicode( pszText, wszTextBuff, sizeof( wszTextBuff ) );
			pLocalizedText = wszTextBuff;
		}

		wchar_t buf[ 256 ];
		V_wcsncpy( buf, pLocalizedText, sizeof( buf ) );

		UTIL_ReplaceKeyBindings( buf, sizeof(buf), outbuf, sizeof( outbuf ) );
	}

	int len = wcslen( outbuf ) + 1;
	m_wszCaption.clear();
	ks::ensure_size( m_wszCaption, len );
	V_wcsncpy( m_wszCaption.data(), outbuf, len * sizeof( wchar_t ) );
}

void CLocatorTarget::SetCaptionColor( const char *pszCaptionColor )
{
	int r,g,b;
	r = g = b = 0;

	std::vector< std::string > colorValues;
	V_SplitString( pszCaptionColor, ",", colorValues );

	if( std::ssize( colorValues ) == 3 )
	{
		r = atoi( colorValues[0].c_str() );
		g = atoi( colorValues[1].c_str() );
		b = atoi( colorValues[2].c_str() );

		m_captionColor.SetColor( r,g,b, 255 );
	}
	else
	{
		DevWarning( "caption_color format incorrect. RRR,GGG,BBB expected.\n");
	}
}

bool CLocatorTarget::IsStatic()
{
	return ( ( m_iEffectsFlags & LOCATOR_ICON_FX_STATIC ) || IsPresenting() );
}

bool CLocatorTarget::IsPresenting()
{
	return ( gpGlobals->curtime - m_lerpStart < locator_lerp_rest.GetFloat() );
}

void CLocatorTarget::StartTimedLerp()
{
	if ( gpGlobals->curtime - m_lerpStart > locator_lerp_rest.GetFloat() )
	{
		m_lerpStart = gpGlobals->curtime - locator_lerp_rest.GetFloat();
	}
}

void CLocatorTarget::StartPresent()
{
	m_lerpStart = gpGlobals->curtime;
}

void CLocatorTarget::EndPresent()
{
	if ( gpGlobals->curtime - m_lerpStart < locator_lerp_rest.GetFloat() )
	{
		m_lerpStart = gpGlobals->curtime - locator_lerp_rest.GetFloat();
	}
}

void CLocatorTarget::UpdateVguiTarget( void )
{
}

void CLocatorTarget::SetVguiTargetName( const char *pchUITargetName )
{
	if ( V_strcmp( m_szUITargetName.String(), pchUITargetName ) == 0 )
		return;

	m_szUITargetName = pchUITargetName;

	UpdateVguiTarget();
}

void CLocatorTarget::SetVguiTargetLookup( const char *pchUITargetLookup )
{
	m_szUITargetLookup = pchUITargetLookup;
}

void CLocatorTarget::SetVguiTargetEdge( int nVguiEdge )
{
	m_nUITargetEdge = nVguiEdge;
}

void *CLocatorTarget::GetVguiTarget( void )
{
	return nullptr;
}

//------------------------------------
void CLocatorTarget::SetOnscreenIconTextureName( const char *pszTexture )
{
	if ( V_strcmp( m_szOnscreenTexture.String(), pszTexture ) == 0 )
		return;

	m_szOnscreenTexture = pszTexture;
	m_pIcon_onscreen = nullptr; // Dirty the onscreen icon so that the Locator will look up the new icon by name.

	m_pulseStart = gpGlobals->curtime;
}

//------------------------------------
void CLocatorTarget::SetOffscreenIconTextureName( const char *pszTexture )
{
	if ( V_strcmp( m_szOffscreenTexture.String(), pszTexture ) == 0 )
		return;

	m_szOffscreenTexture = pszTexture;
	m_pIcon_offscreen = nullptr; // Ditto

	m_pulseStart = gpGlobals->curtime;
}

//------------------------------------
void CLocatorTarget::SetBinding( const char *pszBinding )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	BindingLookupOption_t nBindingLookupFlags = BINDINGLOOKUP_ALL;

	// Only show joystick binds if it's enabled and non-joystick if it's disabled
	nBindingLookupFlags = input->ControllerModeActive() ? BINDINGLOOKUP_JOYSTICK_ONLY : BINDINGLOOKUP_KEYBOARD_ONLY;

	bool bIsControllerNow = ( nBindingLookupFlags != 0 );

	if ( m_bWasControllerLast == bIsControllerNow )
	{
		// We haven't toggled joystick enabled recently, so if it's the same bind, bail
		if ( V_strcmp( m_szBinding.String(), pszBinding ) == 0 )
		{
			return;
		}
	}

	m_bWasControllerLast = bIsControllerNow;

	m_szBinding = pszBinding;
	m_pIcon_onscreen = nullptr; // Dirty the onscreen icon so that the Locator will look up the new icon by name.
	m_pIcon_offscreen = nullptr; // ditto.
	m_flNextBindingTick = gpGlobals->curtime + 0.75f;

	// Get a list of all the keys bound to these actions
	m_iBindingChoicesCount = 0;

	// Tokenize the binding name (could be more than one binding)
	int nOriginalToken = 0;
	const char	*pchToken = m_szBinding.String();
	char		szToken[ 128 ];

	pchToken = nexttoken( szToken, pchToken, ';' );

	while ( pchToken )
	{
		// Get the first parameter
		int iTokenBindingCount = 0;
		const char *pchBinding = engine->Key_LookupBindingEx( szToken, nSlot, iTokenBindingCount, nBindingLookupFlags );

		while ( m_iBindingChoicesCount < MAX_LOCATOR_BINDINGS_SHOWN && pchBinding )
		{
			m_pchBindingChoices[ m_iBindingChoicesCount ] = pchBinding;
			m_iBindChoicesOriginalToken[ m_iBindingChoicesCount ] = nOriginalToken;
			++m_iBindingChoicesCount;
			++iTokenBindingCount;

			pchBinding = engine->Key_LookupBindingEx( szToken, nSlot, iTokenBindingCount, nBindingLookupFlags );
		}

		nOriginalToken++;
		pchToken = nexttoken( szToken, pchToken, ';' );
	}

	m_pulseStart = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CLocatorTarget::UseBindingImage( char *pchIconTextureName, size_t bufSize )
{
	if ( m_iBindingChoicesCount <= 0 )
	{
		{
			V_strncpy( pchIconTextureName, "icon_key_wide", bufSize );
			return "#GameUI_Icons_NONE";
		}

		return nullptr;
	}

	// Cycle through the list of binds at a rate of 2 per second
	const char *pchBinding = m_pchBindingChoices[ m_iBindingTick % m_iBindingChoicesCount ];

	// We counted at least one binding... this should not be NULL!
	contract_assert( pchBinding );

	//icon_blank_wide
	/*
	if ( input->ControllerModeActive() && 
		 ( V_strcmp( pchBinding, "A_BUTTON" ) == 0 || 
		   V_strcmp( pchBinding, "B_BUTTON" ) == 0 || 
		   V_strcmp( pchBinding, "X_BUTTON" ) == 0 || 
		   V_strcmp( pchBinding, "Y_BUTTON" ) == 0 || 
		   V_strcmp( pchBinding, "L_SHOULDER" ) == 0 || 
		   V_strcmp( pchBinding, "R_SHOULDER" ) == 0 || 
		   V_strcmp( pchBinding, "L_TRIGGER" ) == 0 || 
		   V_strcmp( pchBinding, "R_TRIGGER" ) == 0 || 
		   V_strcmp( pchBinding, "BACK" ) == 0 || 
		   V_strcmp( pchBinding, "START" ) == 0 || 
		   V_strcmp( pchBinding, "STICK1" ) == 0 || 
		   V_strcmp( pchBinding, "STICK2" ) == 0 || 
		   V_strcmp( pchBinding, "UP" ) == 0 || 
		   V_strcmp( pchBinding, "DOWN" ) == 0 || 
		   V_strcmp( pchBinding, "LEFT" ) == 0 || 
		   V_strcmp( pchBinding, "RIGHT" ) == 0 ) )
	{
		// Use a blank background for the button icons
		V_strncpy( pchIconTextureName, "icon_blank", bufSize );
		return pchBinding;
	}
	*/

	if ( V_strcmp( pchBinding, "MOUSE1" ) == 0 )
	{
		V_strncpy( pchIconTextureName, "icon_mouseLeft", bufSize );
		return nullptr;
	}
	else if ( V_strcmp( pchBinding, "MOUSE2" ) == 0 )
	{
		V_strncpy( pchIconTextureName, "icon_mouseRight", bufSize );
		return nullptr;
	}
	else if ( V_strcmp( pchBinding, "MOUSE3" ) == 0 )
	{
		V_strncpy( pchIconTextureName, "icon_mouseThree", bufSize );
		return nullptr;
	}
	else if ( V_strcmp( pchBinding, "MWHEELUP" ) == 0 )
	{
		V_strncpy( pchIconTextureName, "icon_mouseWheel_up", bufSize );
		return nullptr;
	}
	else if ( V_strcmp( pchBinding, "MWHEELDOWN" ) == 0 )
	{
		V_strncpy( pchIconTextureName, "icon_mouseWheel_down", bufSize );
		return nullptr;
	}
	else if ( V_strcmp( pchBinding, "UPARROW" ) == 0 )
	{
		V_strncpy( pchIconTextureName, "icon_key_up", bufSize );
		return nullptr;
	}
	else if ( V_strcmp( pchBinding, "LEFTARROW" ) == 0 )
	{
		V_strncpy( pchIconTextureName, "icon_key_left", bufSize );
		return nullptr;
	}
	else if ( V_strcmp( pchBinding, "DOWNARROW" ) == 0 )
	{
		V_strncpy( pchIconTextureName, "icon_key_down", bufSize );
		return nullptr;
	}
	else if ( V_strcmp( pchBinding, "RIGHTARROW" ) == 0 )
	{
		V_strncpy( pchIconTextureName, "icon_key_right", bufSize );
		return nullptr;
	}
	else if ( V_strcmp( pchBinding, "SEMICOLON" ) == 0 || 
		V_strcmp( pchBinding, "INS" ) == 0 || 
		V_strcmp( pchBinding, "DEL" ) == 0 || 
		V_strcmp( pchBinding, "HOME" ) == 0 || 
		V_strcmp( pchBinding, "END" ) == 0 || 
		V_strcmp( pchBinding, "PGUP" ) == 0 || 
		V_strcmp( pchBinding, "PGDN" ) == 0 || 
		V_strcmp( pchBinding, "PAUSE" ) == 0 || 
		V_strcmp( pchBinding, "F10" ) == 0 || 
		V_strcmp( pchBinding, "F11" ) == 0 || 
		V_strcmp( pchBinding, "F12" ) == 0 )
	{
		V_strncpy( pchIconTextureName, "icon_key_generic", bufSize );
		return pchBinding;
	}
	else if ( V_strlen( pchBinding ) <= 2 )
	{
		V_strncpy( pchIconTextureName, "icon_key_generic", bufSize );
		return pchBinding;
	}
	else if ( V_strlen( pchBinding ) <= 6 )
	{
		V_strncpy( pchIconTextureName, "icon_key_wide", bufSize );
		return pchBinding;
	}
	else
	{
		V_strncpy( pchIconTextureName, "icon_key_wide", bufSize );
		return pchBinding;
	}

	return pchBinding;
}

//-----------------------------------------------------------------------------
int CLocatorTarget::GetIconWidth( void )
{
	return m_wide;
}

//-----------------------------------------------------------------------------
int CLocatorTarget::GetIconHeight( void )
{
	return m_tall;
}

static CLocatorTarget	g_LocatorTargets[ MAX_LOCATOR_TARGETS ];
static int				g_LocatorSerializer = 1000;

static CLocatorTarget *Locator_FindTargetByHandle( int hTarget )
{
	for( int i = 0 ; i < MAX_LOCATOR_TARGETS ; i++ )
	{
		if( g_LocatorTargets[ i ].m_isActive && g_LocatorTargets[ i ].m_serialNumber == hTarget )
		{
			return &g_LocatorTargets[ i ];
		}
	}

	return nullptr;
}

int Locator_AddTarget()
{
	for( int i = 0 ; i < MAX_LOCATOR_TARGETS ; i++ )
	{
		if( !g_LocatorTargets[ i ].m_isActive )
		{
			g_LocatorTargets[ i ].Activate( g_LocatorSerializer );
			g_LocatorSerializer++;

			return g_LocatorTargets[ i ].m_serialNumber;
		}
	}

	DevWarning( "Locator has no free targets!\n" );
	return -1;
}

void Locator_RemoveTarget( int hTarget )
{
	CLocatorTarget *pTarget = Locator_FindTargetByHandle( hTarget );

	if( pTarget )
	{
		pTarget->Deactivate();
	}
}

CLocatorTarget *Locator_GetTargetFromHandle( int hTarget )
{
	return Locator_FindTargetByHandle( hTarget );
}

void Locator_ComputeTargetIconPositionFromHandle( int hTarget )
{
	CLocatorTarget *pTarget = Locator_FindTargetByHandle( hTarget );

	if( pTarget )
	{
		pTarget->m_targetX = 0;
		pTarget->m_targetY = 0;
		pTarget->m_iconX = 0;
		pTarget->m_iconY = 0;
		pTarget->m_centerX = 0;
		pTarget->m_centerY = 0;
	}
}

class CHud;
class CBaseEntity;

using namespace ::ks::reflect;

//
//  hud_msg.cpp
//

/// USER-DEFINED SERVER MESSAGE HANDLERS

bool CHud::MsgFunc_ResetHUD( const ks::net::CCSUsrMsg_ResetHud& msg )
{
	ResetHUD();
	return true;
}

void CHud::ResetHUD()
{
	// clear all hud data
	GetClientMode()->GetViewportAnimationController()->CancelAllAnimations();

	for ( int i = 0; i < std::ssize( GetHudList() ); i++ )
	{
		GetHudList()[i]->Reset();
	}

	GetClientMode()->GetViewportAnimationController()->RunAllAnimationsToCompletion();

	// reset sensitivity
	m_flMouseSensitivity = 0;
	m_flMouseSensitivityFactor = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

bool CHud::MsgFunc_SendAudio(const ks::net::CCSUsrMsg_SendAudio& msg )
{
	CLocalPlayerFilter filter;
	CBaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, msg.radio_sound->c_str() );
	return true;
}

class CHud;
class CHudElement;
class CHudTexture;
class CBaseCombatWeapon;
class CBasePlayer;

using namespace ::ks::reflect;

//
// hud_redraw.cpp
//

//For progress bar orientations
const int CHud::HUDPB_HORIZONTAL = 0;
const int CHud::HUDPB_VERTICAL = 1;
const int CHud::HUDPB_HORIZONTAL_INV = 2;

// Called when a ConVar changes value
static void FovChanged_Callback( ConVar *pConVar, const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );
	if ( engine->IsInGame() )
	{
		engine->ServerCmd( VarArgs( "fov %f\n", var.GetFloat() ) );
	}
}

static ConVar fov_watcher( side_registry(), "_fov", "0", 0, "Automates fov command to server.", FovChanged_Callback );

void CHud::DoElementThink( CHudElement* pElement )
{
    bool visible = true;

    //if we are globally disabling the HUD (and the pElement obeys global hiding), override the ShouldDraw check
    if ( m_iDisabledCount > 0 && !pElement->GetIgnoreGlobalHudDisable() )
    {
	    visible = false;
    }
    else
    {
	    visible = pElement->ShouldDraw();		
    }

    pElement->SetActive( visible );
    pElement->Think();

	bool bProcessInput = visible;
    if ( bProcessInput )
    {
	    pElement->ProcessInput();
    }
}

//-----------------------------------------------------------------------------
// Purpose: Think
//-----------------------------------------------------------------------------
void CHud::Think( void )
{
	// Determine the visibility of all hud elements
	std::vector< CHudElement * > & list = GetHudList();

	int c = std::ssize( list );

	m_bEngineIsInGame = engine->IsInGame() && ( engine->IsLevelMainMenuBackground() == false );

	for ( int i = 0; i < c; ++i )
	{
		CHudElement* pElement = list[i];
        if ( !pElement->m_bWantLateUpdate )
        {
            DoElementThink( pElement );
        }
	}

	// Let the active weapon at the keybits
	CBasePlayer *pPlayer = CBasePlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		CBaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
		if ( pWeapon )
		{
			pWeapon->HandleInput();
		}
	}

	if ( ( m_flScreenShotTime > 0 ) && ( m_flScreenShotTime < gpGlobals->curtime ) )
	{
		engine->ClientCmd( "screenshot" );

		m_flScreenShotTime = -1;
	}
}

void CHud::OnTimeJump(void)
{
	std::vector< CHudElement * > & list = GetHudList();
	int c = std::ssize( list );

	for ( int i = 0; i < c; ++i )
	{
		CHudElement* pElement = list[i];
        if ( !pElement->m_bWantLateUpdate )
        {
            pElement->OnTimeJump();
        }
	}
}
void CHud::LateThink( void )
{

	std::vector< CHudElement * > & list = GetHudList();

	int c = std::ssize( list );

	m_bEngineIsInGame = engine->IsInGame() && ( engine->IsLevelMainMenuBackground() == false );

	for ( int i = 0; i < c; ++i )
	{
		CHudElement* pElement = list[i];
        if ( pElement->m_bWantLateUpdate )
        {
            DoElementThink( pElement );
        }
	}
}

//-----------------------------------------------------------------------------
// Purpose:  The percentage passed in is expected and clamped to 0.0f to 1.0f
// Input  : x - 
//			y - 
//			width - 
//			height - 
//			percentage - 
//			clr - 
//			type - 
//-----------------------------------------------------------------------------
void CHud::DrawProgressBar( int x, int y, int width, int height, float percentage, Color& clr, unsigned char type )
{
}

//-----------------------------------------------------------------------------
// Purpose:  The percentage passed in is expected and clamped to 0.0f to 1.0f
// Input  : x - 
//			y - 
//			*icon - 
//			percentage - 
//			clr - 
//			type - 
//-----------------------------------------------------------------------------
void CHud::DrawIconProgressBar( int x, int y, CHudTexture *icon, CHudTexture *icon2, float percentage, Color& clr, int type )
{
	if ( icon == nullptr )
		return;

	//Clamp our percentage
	percentage = std::min( 1.0f, percentage );
	percentage = std::max( 0.0f, percentage );

	int	height = icon->Height();
	int	width  = icon->Width();

	//Draw a vertical progress bar
	if ( type == HUDPB_VERTICAL )
	{
		int	barOfs = height * percentage;

		icon2->DrawSelfCropped( 
			x, y,  // Pos
			0, 0, width, barOfs, // Cropped subrect
			clr );

		icon->DrawSelfCropped( 
			x, y + barOfs, 
			0, barOfs, width, height - barOfs, // Cropped subrect
			clr );
	}
	else if ( type == HUDPB_HORIZONTAL )
	{
		int	barOfs = width * percentage;

		icon2->DrawSelfCropped( 
			x, y,  // Pos
			0, 0, barOfs, height, // Cropped subrect
			clr );

		icon->DrawSelfCropped( 
			x + barOfs, y, 
			barOfs, 0, width - barOfs, height, // Cropped subrect
			clr );
	}
}

class CUserCmd;
class CBasePlayer;
class IGameSystem;

using namespace ::ks::reflect;

// Purpose: Deals with singleton

// enable this #define (here or in your vpc) to have 
// each GameSystem accounted for individually in the vprof.
// You must enable VPROF_LEVEL 1 too.
// #define VPROF_ACCOUNT_GAMESYSTEMS

// Pointer to a member method of IGameSystem
typedef void (IGameSystem::*GameSystemFunc_t)();

// Pointer to a member method of IGameSystem
typedef void (IGameSystemPerFrame::*PerFrameGameSystemFunc_t)();

// Used to invoke a method of all added Game systems in order
static void InvokeMethod( GameSystemFunc_t f, char const *timed = 0 );
// Used to invoke a method of all added Game systems in order
static void InvokeMethodTickProgress( GameSystemFunc_t f, char const *timed = 0 );
// Used to invoke a method of all added Game systems in reverse order
static void InvokeMethodReverseOrder( GameSystemFunc_t f );

// Used to invoke a method of all added Game systems in order
static void InvokePerFrameMethod( PerFrameGameSystemFunc_t f, char const *timed = 0 );

static bool s_bSystemsInitted = false; 

// List of all installed Game systems
static std::vector< IGameSystem* > s_GameSystems;
// List of all installed Game systems
static std::vector< IGameSystemPerFrame* > s_GameSystemsPerFrame;

// The map name
static char* s_pMapName = 0;

static CBasePlayer *s_pRunCommandPlayer = nullptr;
static CUserCmd *s_pRunCommandUserCmd = nullptr;

//-----------------------------------------------------------------------------
// Auto-registration of game systems
//-----------------------------------------------------------------------------
static	CAutoGameSystem *s_pSystemList = nullptr;

CAutoGameSystem::CAutoGameSystem( char const *name ) :
	m_pszName( name )
{
	// If s_GameSystems hasn't been initted yet, then add ourselves to the global list
	// because we don't know if the constructor for s_GameSystems has happened yet.
	// Otherwise, we can add ourselves right into that list.
	if ( s_bSystemsInitted )
	{
		Add( this );
	}
	else
	{
		m_pNext = s_pSystemList;
		s_pSystemList = this;
	}
}

static	CAutoGameSystemPerFrame *s_pPerFrameSystemList = nullptr;

//-----------------------------------------------------------------------------
// Purpose: This is a CAutoGameSystem which also cares about the "per frame" hooks
//-----------------------------------------------------------------------------
CAutoGameSystemPerFrame::CAutoGameSystemPerFrame( char const *name ) :
	m_pszName( name )
{
	// If s_GameSystems hasn't been initted yet, then add ourselves to the global list
	// because we don't know if the constructor for s_GameSystems has happened yet.
	// Otherwise, we can add ourselves right into that list.
	if ( s_bSystemsInitted )
	{
		Add( this );
	}
	else
	{
		m_pNext = s_pPerFrameSystemList;
		s_pPerFrameSystemList = this;
	}
}

//-----------------------------------------------------------------------------
// destructor, cleans up automagically....
//-----------------------------------------------------------------------------
IGameSystem::~IGameSystem()
{
	Remove( this );
}

//-----------------------------------------------------------------------------
// destructor, cleans up automagically....
//-----------------------------------------------------------------------------
IGameSystemPerFrame::~IGameSystemPerFrame()
{
	Remove( this );
}

//-----------------------------------------------------------------------------
// Adds a system to the list of systems to run
//-----------------------------------------------------------------------------
void IGameSystem::Add( IGameSystem* pSys )
{
	s_GameSystems.push_back( pSys );
	if ( dynamic_cast< IGameSystemPerFrame * >( pSys ) != nullptr )
	{
		s_GameSystemsPerFrame.push_back( static_cast< IGameSystemPerFrame * >( pSys ) );
	}
}

//-----------------------------------------------------------------------------
// Removes a system from the list of systems to update
//-----------------------------------------------------------------------------
void IGameSystem::Remove( IGameSystem* pSys )
{
	ks::erase_first( s_GameSystems, pSys );
	if ( dynamic_cast< IGameSystemPerFrame * >( pSys ) != nullptr )
	{
		ks::erase_first( s_GameSystemsPerFrame, static_cast< IGameSystemPerFrame * >( pSys ) );
	}
}

//-----------------------------------------------------------------------------
// Removes *all* systems from the list of systems to update
//-----------------------------------------------------------------------------
void IGameSystem::RemoveAll(  )
{
	s_GameSystems.clear();
	s_GameSystemsPerFrame.clear();
}

//-----------------------------------------------------------------------------
// Client systems can use this to get at the map name
//-----------------------------------------------------------------------------
char const*	IGameSystem::MapName()
{
	return s_pMapName;
}

//-----------------------------------------------------------------------------
// Invokes methods on all installed game systems
//-----------------------------------------------------------------------------
bool IGameSystem::InitAllSystems()
{
	int i;

	{
		// first add any auto systems to the end
		CAutoGameSystem *pSystem = s_pSystemList;
		while ( pSystem )
		{
			if ( ks::find_index( s_GameSystems, pSystem ) == -1 )
			{
				Add( pSystem );
			}
			else
			{
				DevWarning( 1, "AutoGameSystem already added to game system list!!!\n" );
			}
			pSystem = pSystem->m_pNext;
		}
		s_pSystemList = nullptr;
	}

	{
		CAutoGameSystemPerFrame *pSystem = s_pPerFrameSystemList;
		while ( pSystem )
		{
			if ( ks::find_index( s_GameSystems, pSystem ) == -1 )
			{
				Add( pSystem );
			}
			else
			{
				DevWarning( 1, "AutoGameSystem already added to game system list!!!\n" );
			}

			pSystem = pSystem->m_pNext;
		}
		s_pSystemList = nullptr;
	}
	// Now remember that we are initted so new CAutoGameSystems will add themselves automatically.
	s_bSystemsInitted = true;

	// PS3: haul the fontlib into memory; some systems (eg vgui) need it.
	for ( i = 0; i < std::ssize( s_GameSystems ); ++i )
	{
		MDLCACHE_COARSE_LOCK();
		MDLCACHE_CRITICAL_SECTION();

		IGameSystem *sys = s_GameSystems[i];

		bool valid = sys->Init();

		if ( !valid )
		{
			DevWarning( 1, "Failed to load %s\n", sys->Name() );
			return false;
		}
	}

	return true;
}

void IGameSystem::PostInitAllSystems( void )
{
	InvokeMethod( &IGameSystem::PostInit, "PostInit" );
}

void IGameSystem::ShutdownAllSystems()
{
	InvokeMethodReverseOrder( &IGameSystem::Shutdown );
}

void IGameSystem::LevelInitPreEntityAllSystems( char const* pMapName )
{
	// Store off the map name
	if ( s_pMapName )
	{
		delete[] s_pMapName;
	}

	int len = V_strlen(pMapName) + 1;
	s_pMapName = new char [ len ];
	V_strncpy( s_pMapName, pMapName, len );
	
	InvokeMethodTickProgress( &IGameSystem::LevelInitPreEntity, "LevelInitPreEntity" );
}

void IGameSystem::LevelInitPostEntityAllSystems( void )
{
	InvokeMethod( &IGameSystem::LevelInitPostEntity, "LevelInitPostEntity" );
}

void IGameSystem::LevelShutdownPreEntityAllSystems()
{
	InvokeMethodReverseOrder( &IGameSystem::LevelShutdownPreEntity );
}

void IGameSystem::LevelShutdownPostEntityAllSystems()
{
	InvokeMethodReverseOrder( &IGameSystem::LevelShutdownPostEntity );

	if ( s_pMapName )
	{
		delete[] s_pMapName;
		s_pMapName = 0;
	}
}

void IGameSystem::OnSaveAllSystems()
{
	InvokeMethod( &IGameSystem::OnSave );
}

void IGameSystem::OnRestoreAllSystems()
{
	InvokeMethod( &IGameSystem::OnRestore );
}

void IGameSystem::SafeRemoveIfDesiredAllSystems()
{
	InvokeMethodReverseOrder( &IGameSystem::SafeRemoveIfDesired );
}

void IGameSystem::PreRenderAllSystems()
{
	CVProfScope _( "IGameSystem::PreRenderAllSystems", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	InvokePerFrameMethod( &IGameSystemPerFrame::PreRender );
}

void IGameSystem::UpdateAllSystems( float frametime )
{
	SafeRemoveIfDesiredAllSystems();

	int i;
	int c = std::ssize( s_GameSystemsPerFrame );
	MDLCACHE_CRITICAL_SECTION();
	for ( i = 0; i < c; ++i )
	{
		IGameSystemPerFrame *sys = s_GameSystemsPerFrame[i];
		sys->Update( frametime );
	}
}

void IGameSystem::PostRenderAllSystems()
{
	InvokePerFrameMethod( &IGameSystemPerFrame::PostRender );
}

//-----------------------------------------------------------------------------
// Invokes a method on all installed game systems in proper order
//-----------------------------------------------------------------------------
void InvokeMethod( GameSystemFunc_t f, char const *timed /*=0*/ )
{
	(void)timed;

	MDLCACHE_COARSE_LOCK();
	MDLCACHE_CRITICAL_SECTION();
	int i;
	int c = std::ssize( s_GameSystems );
	for ( i = 0; i < c ; ++i )
	{
		IGameSystem *sys = s_GameSystems[i];

		(sys->*f)();
	}
}

//-----------------------------------------------------------------------------
// Invokes a method on all installed game systems in proper order
//-----------------------------------------------------------------------------
void InvokeMethodTickProgress( GameSystemFunc_t f, char const *timed /*=0*/ )
{
	(void)timed;

	int i;
	int c = std::ssize( s_GameSystems );
	for ( i = 0; i < c ; ++i )
	{
		IGameSystem *sys = s_GameSystems[i];

		MDLCACHE_COARSE_LOCK();
		MDLCACHE_CRITICAL_SECTION();
		engine->TickProgressBar();
		(sys->*f)();
	}
}
//-----------------------------------------------------------------------------
// Invokes a method on all installed game systems in proper order
//-----------------------------------------------------------------------------
void InvokePerFrameMethod( PerFrameGameSystemFunc_t f, char const *timed /*=0*/ )
{
	(void)timed;

	int i;
	int c = std::ssize( s_GameSystemsPerFrame );
	for ( i = 0; i < c ; ++i )
	{
		IGameSystemPerFrame *sys  = s_GameSystemsPerFrame[i];
		(sys->*f)();
	}
}

//-----------------------------------------------------------------------------
// Invokes a method on all installed game systems in reverse order
//-----------------------------------------------------------------------------
void InvokeMethodReverseOrder( GameSystemFunc_t f )
{
	int i;
	int c = std::ssize( s_GameSystems );
	MDLCACHE_CRITICAL_SECTION();
	for ( i = c; --i >= 0; )
	{
		IGameSystem *sys = s_GameSystems[i];
		(sys->*f)();
	}
}

class CEffectData;
class CBaseEntity;
class SimpleParticle;

using namespace ::ks::reflect;

//Precahce the effects
PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheEffectImpacts )
PRECACHE( MATERIAL, "effects/fleck_cement1" )
PRECACHE( MATERIAL, "effects/fleck_cement2" )
PRECACHE( MATERIAL, "effects/fleck_antlion1" )
PRECACHE( MATERIAL, "effects/fleck_antlion2" )
PRECACHE( MATERIAL, "effects/fleck_wood1" )
PRECACHE( MATERIAL, "effects/fleck_wood2" )
PRECACHE( MATERIAL, "effects/blood" )
PRECACHE( MATERIAL, "effects/blood2" )
PRECACHE( MATERIAL, "sprites/bloodspray" )
PRECACHE( MATERIAL, "particle/particle_noisesphere" )
PRECACHE_REGISTER_END()

// Cached handles to commonly used materials
PMaterialHandle g_Mat_Fleck_Wood[2] = { nullptr, nullptr };
PMaterialHandle g_Mat_Fleck_Cement[2] = { nullptr, nullptr };
PMaterialHandle g_Mat_Fleck_Antlion[2] = { nullptr, nullptr };
PMaterialHandle g_Mat_Fleck_Glass[2] = { nullptr, nullptr };
PMaterialHandle g_Mat_Fleck_Tile[2] = { nullptr, nullptr };
PMaterialHandle g_Mat_DustPuff[2] = { nullptr, nullptr };
PMaterialHandle g_Mat_BloodPuff[2] = { nullptr, nullptr };
PMaterialHandle g_Mat_SMG_Muzzleflash[4] = { nullptr, nullptr, nullptr, nullptr };
PMaterialHandle g_Mat_Combine_Muzzleflash[3] = { nullptr, nullptr, nullptr };

static ConVar fx_drawimpactdebris( side_registry(), "fx_drawimpactdebris", "1", FCVAR_DEVELOPMENTONLY, "Draw impact debris effects." );

PRECACHE_REGISTER_BEGIN( GLOBAL, FX_CacheMaterialHandles )

	PRECACHE_INDEX( PARTICLE_MATERIAL, "effects/fleck_wood1", g_Mat_Fleck_Wood[0] )
	PRECACHE_INDEX( PARTICLE_MATERIAL, "effects/fleck_wood2", g_Mat_Fleck_Wood[1] )

	PRECACHE_INDEX( PARTICLE_MATERIAL, "effects/fleck_cement1", g_Mat_Fleck_Cement[0])
	PRECACHE_INDEX( PARTICLE_MATERIAL, "effects/fleck_cement2", g_Mat_Fleck_Cement[1] )

	PRECACHE_INDEX( PARTICLE_MATERIAL, "effects/fleck_antlion1", g_Mat_Fleck_Antlion[0] )
	PRECACHE_INDEX( PARTICLE_MATERIAL, "effects/fleck_antlion2", g_Mat_Fleck_Antlion[1] )

	PRECACHE_INDEX( PARTICLE_MATERIAL, "effects/fleck_glass1", g_Mat_Fleck_Glass[0] )
	PRECACHE_INDEX( PARTICLE_MATERIAL, "effects/fleck_glass2", g_Mat_Fleck_Glass[1] )

	PRECACHE_INDEX( PARTICLE_MATERIAL, "effects/fleck_tile1", g_Mat_Fleck_Tile[0] )
	PRECACHE_INDEX( PARTICLE_MATERIAL, "effects/fleck_tile2", g_Mat_Fleck_Tile[1] )

	PRECACHE_INDEX( PARTICLE_MATERIAL, "particle/particle_smokegrenade", g_Mat_DustPuff[0] )
	PRECACHE_INDEX( PARTICLE_MATERIAL, "particle/particle_noisesphere", g_Mat_DustPuff[1] )

	PRECACHE_INDEX( PARTICLE_MATERIAL, "effects/blood", g_Mat_BloodPuff[0] )
	PRECACHE_INDEX( PARTICLE_MATERIAL, "effects/blood2", g_Mat_BloodPuff[1] )
	
	PRECACHE_INDEX( PARTICLE_MATERIAL, "effects/muzzleflash1", g_Mat_SMG_Muzzleflash[0] )
	PRECACHE_INDEX( PARTICLE_MATERIAL, "effects/muzzleflash2", g_Mat_SMG_Muzzleflash[1] )
	PRECACHE_INDEX( PARTICLE_MATERIAL, "effects/muzzleflash3", g_Mat_SMG_Muzzleflash[2] )
	PRECACHE_INDEX( PARTICLE_MATERIAL, "effects/muzzleflash4", g_Mat_SMG_Muzzleflash[3] )

PRECACHE_REGISTER_END()

extern PMaterialHandle g_Material_Spark;

//-----------------------------------------------------------------------------
// Purpose: Returns the color given trace information
// Input  : *trace - trace to get results for
//			*color - return color, gamma corrected (0.0f to 1.0f)
//-----------------------------------------------------------------------------
void GetColorForSurface( trace_t *trace, Vector *color )
{
	Vector	baseColor, diffuseColor;
	Vector	end = trace->startpos + ( ( Vector )trace->endpos - ( Vector )trace->startpos ) * 1.1f;
	
	if ( TraceDidHitWorld( *trace ) )
	{
		if ( trace->hitbox == 0 )
		{
			// If we hit the world, then ask the world for the fleck color
			engine->TraceLineMaterialAndLighting( trace->startpos, end, diffuseColor, baseColor );
		}
		else
		{
			// In this case we hit a static prop.
			staticpropmgr->GetStaticPropMaterialColorAndLighting( trace, trace->hitbox - 1, diffuseColor, baseColor );
		}
	}
	else
	{
		// In this case, we hit an entity. Find out the model associated with it
		CBaseEntity *pEnt = trace->Ent<CBaseEntity>();
		if ( !pEnt )
		{
			Msg("Couldn't find surface in GetColorForSurface()\n");
			color->x = 255;
			color->y = 255;
			color->z = 255;
			return;
		}

		ICollideable *pCollide = pEnt->GetCollideable();
		int modelIndex = pCollide->GetCollisionModelIndex();
		model_t* pModel = const_cast<model_t*>(modelinfo->GetModel( modelIndex ));

		// Ask the model info about what we need to know
		modelinfo->GetModelMaterialColorAndLighting( pModel, pCollide->GetCollisionOrigin(),
			pCollide->GetCollisionAngles(), trace, diffuseColor, baseColor );
	}

	//Get final light value
	color->x = std::pow( diffuseColor[0], 1.0f/2.2f ) * baseColor[0];
	color->y = pow( diffuseColor[1], 1.0f/2.2f ) * baseColor[1];
	color->z = pow( diffuseColor[2], 1.0f/2.2f ) * baseColor[2];
}

//-----------------------------------------------------------------------------
// This does the actual debris flecks
//-----------------------------------------------------------------------------
static constexpr auto FLECK_MIN_SPEED = 64.0f;
static constexpr auto FLECK_MAX_SPEED = 128.0f;
static constexpr auto FLECK_GRAVITY = 800.0f;
static constexpr auto FLECK_DAMPEN = 0.3f;
static constexpr auto FLECK_ANGULAR_SPRAY = 0.6f;

//
// PC ONLY!
//

static void CreateFleckParticles( const Vector& origin, const Vector &color, trace_t *trace, char materialType, int iScale )
{
	Vector	spawnOffset	= trace->endpos + ( trace->plane.normal * 1.0f );

	ks::intrusive_ptr<CFleckParticles> fleckEmitter = CFleckParticles::Create( "FX_DebrisFlecks", spawnOffset, Vector(5,5,5) );

	if ( !fleckEmitter )
		return;

	// Handle increased scale
	float flMaxSpeed = FLECK_MAX_SPEED * iScale;
	float flAngularSpray = std::max<std::common_type_t<double, float>>( 0.2, FLECK_ANGULAR_SPRAY - ( (float)iScale * 0.2f) ); // More power makes the spray more controlled
	// Setup our collision information
	fleckEmitter->m_ParticleCollision.Setup( spawnOffset, &trace->plane.normal, flAngularSpray, FLECK_MIN_SPEED, flMaxSpeed, FLECK_GRAVITY, FLECK_DAMPEN );

	PMaterialHandle	*hMaterial;
	switch ( materialType )
	{
	case CHAR_TEX_WOOD:
		hMaterial = g_Mat_Fleck_Wood;
		break;

	case CHAR_TEX_CONCRETE:
	case CHAR_TEX_TILE:
	default:
		hMaterial = g_Mat_Fleck_Cement;
		break;
	}

	Vector	dir, end;

	float	colorRamp;

	int	numFlecks = random_valve->RandomInt( 4, 16 ) * iScale;

	FleckParticle	*pFleckParticle;

	//Dump out flecks
	int i;
	for ( i = 0; i < numFlecks; i++ )
	{
		pFleckParticle = (FleckParticle *) fleckEmitter->AddParticle( sizeof(FleckParticle), hMaterial[random_valve->RandomInt(0,1)], spawnOffset );

		if ( pFleckParticle == nullptr )
			break;

		pFleckParticle->m_flLifetime	= 0.0f;
		pFleckParticle->m_flDieTime		= 3.0f;

		dir[0] = trace->plane.normal[0] + random_valve->RandomFloat( -flAngularSpray, flAngularSpray );
		dir[1] = trace->plane.normal[1] + random_valve->RandomFloat( -flAngularSpray, flAngularSpray );
		dir[2] = trace->plane.normal[2] + random_valve->RandomFloat( -flAngularSpray, flAngularSpray );

		pFleckParticle->m_uchSize		= random_valve->RandomInt( 1, 2 );

		pFleckParticle->m_vecVelocity	= dir * ( random_valve->RandomFloat( FLECK_MIN_SPEED, flMaxSpeed) * ( 3 - pFleckParticle->m_uchSize ) );

		pFleckParticle->m_flRoll		= random_valve->RandomFloat( 0, 360 );
		pFleckParticle->m_flRollDelta	= random_valve->RandomFloat( 0, 360 );

		colorRamp = random_valve->RandomFloat( 0.75f, 1.25f );

		pFleckParticle->m_uchColor[0] = std::min( 1.0f, color[0]*colorRamp )*255.0f;
		pFleckParticle->m_uchColor[1] = std::min( 1.0f, color[1]*colorRamp )*255.0f;
		pFleckParticle->m_uchColor[2] = std::min( 1.0f, color[2]*colorRamp )*255.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Debris flecks caused by impacts
// Input  : origin - start
//			*trace - trace information
//			*materialName - material hit
//			materialType - type of material hit
//-----------------------------------------------------------------------------
void FX_DebrisFlecks( const Vector& origin, trace_t *tr, char materialType, int iScale, bool bNoFlecks )
{
	CVProfScope _( "FX_DebrisFlecks", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );

	if ( !fx_drawimpactdebris.GetBool() )
		return;

	Vector	color;
	GetColorForSurface( tr, &color );

	if ( !bNoFlecks )
	{
		CreateFleckParticles( origin, color, tr, materialType, iScale );
	}

	//
	// Dust trail
	//
	Vector	offset = tr->endpos + ( tr->plane.normal * 2.0f );

	SimpleParticle newParticle;

	int i;
	for ( i = 0; i < 2; i++ )
	{
		newParticle.m_Pos = offset;

		newParticle.m_flLifetime	= 0.0f;
		newParticle.m_flDieTime	= 1.0f;

		Vector dir;
		dir[0] = tr->plane.normal[0] + random_valve->RandomFloat( -0.8f, 0.8f );
		dir[1] = tr->plane.normal[1] + random_valve->RandomFloat( -0.8f, 0.8f );
		dir[2] = tr->plane.normal[2] + random_valve->RandomFloat( -0.8f, 0.8f );

		newParticle.m_uchStartSize	= random_valve->RandomInt( 2, 4 ) * iScale;
		newParticle.m_uchEndSize	= newParticle.m_uchStartSize * 8 * iScale;

		newParticle.m_vecVelocity = dir * random_valve->RandomFloat( 2.0f, 24.0f )*(i+1);
		newParticle.m_vecVelocity[2] -= random_valve->RandomFloat( 8.0f, 32.0f )*(i+1);

		newParticle.m_uchStartAlpha	= random_valve->RandomInt( 100, 200 );
		newParticle.m_uchEndAlpha	= 0;

		newParticle.m_flRoll			= random_valve->RandomFloat( 0, 360 );
		newParticle.m_flRollDelta	= random_valve->RandomFloat( -1, 1 );

		float colorRamp = random_valve->RandomFloat( 0.5f, 1.25f );

		newParticle.m_uchColor[0] = std::min( 1.0f, color[0]*colorRamp )*255.0f;
		newParticle.m_uchColor[1] = std::min( 1.0f, color[1]*colorRamp )*255.0f;
		newParticle.m_uchColor[2] = std::min( 1.0f, color[2]*colorRamp )*255.0f;

		AddSimpleParticle( &newParticle, g_Mat_DustPuff[0] );
	}

	for ( i = 0; i < 4; i++ )
	{
		newParticle.m_Pos = offset;

		newParticle.m_flLifetime	= 0.0f;
		newParticle.m_flDieTime	= random_valve->RandomFloat( 0.25f, 0.5f );

		Vector dir;
		dir[0] = tr->plane.normal[0] + random_valve->RandomFloat( -0.8f, 0.8f );
		dir[1] = tr->plane.normal[1] + random_valve->RandomFloat( -0.8f, 0.8f );
		dir[2] = tr->plane.normal[2] + random_valve->RandomFloat( -0.8f, 0.8f );

		newParticle.m_uchStartSize	= random_valve->RandomInt( 1, 4 );
		newParticle.m_uchEndSize	= newParticle.m_uchStartSize * 4;

		newParticle.m_vecVelocity = dir * random_valve->RandomFloat( 8.0f, 32.0f );
		newParticle.m_vecVelocity[2] -= random_valve->RandomFloat( 8.0f, 64.0f );

		newParticle.m_uchStartAlpha	= 255;
		newParticle.m_uchEndAlpha	= 0;

		newParticle.m_flRoll			= random_valve->RandomFloat( 0, 360 );
		newParticle.m_flRollDelta	= random_valve->RandomFloat( -2.0f, 2.0f );

		float colorRamp = random_valve->RandomFloat( 0.5f, 1.25f );

		newParticle.m_uchColor[0] = std::min( 1.0f, color[0]*colorRamp )*255.0f;
		newParticle.m_uchColor[1] = std::min( 1.0f, color[1]*colorRamp )*255.0f;
		newParticle.m_uchColor[2] = std::min( 1.0f, color[2]*colorRamp )*255.0f;

		AddSimpleParticle( &newParticle, g_Mat_BloodPuff[0] );
	}

	//
	// Bullet hole capper
	//
	newParticle.m_Pos = offset;

	newParticle.m_flLifetime		= 0.0f;
	newParticle.m_flDieTime		= random_valve->RandomFloat( 1.0f, 1.5f );

	Vector dir;
	dir[0] = tr->plane.normal[0] + random_valve->RandomFloat( -0.8f, 0.8f );
	dir[1] = tr->plane.normal[1] + random_valve->RandomFloat( -0.8f, 0.8f );
	dir[2] = tr->plane.normal[2] + random_valve->RandomFloat( -0.8f, 0.8f );

	newParticle.m_uchStartSize	= random_valve->RandomInt( 4, 8 );
	newParticle.m_uchEndSize		= newParticle.m_uchStartSize * 4.0f;

	newParticle.m_vecVelocity = dir * random_valve->RandomFloat( 2.0f, 24.0f );
	newParticle.m_vecVelocity[2] = random_valve->RandomFloat( -2.0f, 2.0f );

	newParticle.m_uchStartAlpha	= random_valve->RandomInt( 100, 200 );
	newParticle.m_uchEndAlpha	= 0;

	newParticle.m_flRoll			= random_valve->RandomFloat( 0, 360 );
	newParticle.m_flRollDelta	= random_valve->RandomFloat( -2, 2 );

	float colorRamp = random_valve->RandomFloat( 0.5f, 1.25f );

	newParticle.m_uchColor[0] = std::min( 1.0f, color[0]*colorRamp )*255.0f;
	newParticle.m_uchColor[1] = std::min( 1.0f, color[1]*colorRamp )*255.0f;
	newParticle.m_uchColor[2] = std::min( 1.0f, color[2]*colorRamp )*255.0f;

	AddSimpleParticle( &newParticle, g_Mat_DustPuff[0] );
}

static constexpr auto GLASS_SHARD_MIN_LIFE = 2.5f;
static constexpr auto GLASS_SHARD_MAX_LIFE = 5.0f;
static constexpr auto GLASS_SHARD_NOISE = 0.8;
static constexpr auto GLASS_SHARD_GRAVITY = 800;
static constexpr auto GLASS_SHARD_DAMPING = 0.3;
static constexpr auto GLASS_SHARD_MIN_SPEED = 1;
static constexpr auto GLASS_SHARD_MAX_SPEED = 300;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void GlassImpactCallback( const CEffectData &data )
{
	CVProfScope _( "GlassImpactCallback", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );

	Vector pos = data.m_vOrigin;
	Vector normal = data.m_vNormal;

	ks::intrusive_ptr<CSimple3DEmitter> pGlassEmitter = CSimple3DEmitter::Create( "FX_GlassImpact" );
	pGlassEmitter->SetSortOrigin( pos );

	Vector vecColor;
	engine->ComputeLighting( pos, nullptr, true, vecColor );

	// HACK: Blend a little toward white to match the materials...
	VectorLerp( vecColor, Vector( 1, 1, 1 ), 0.3, vecColor );

	float flShardSize	= random_valve->RandomFloat( 2.0f, 6.0f );

	unsigned char color[3] = { 200, 200, 210 };

	// ---------------------
	// Create glass shards
	// ----------------------

	int numShards = random_valve->RandomInt( 2, 4 );

	for ( int i = 0; i < numShards; i++ )
	{
		Particle3D *pParticle;

		pParticle = (Particle3D *) pGlassEmitter->AddParticle( sizeof(Particle3D), g_Mat_Fleck_Glass[random_valve->RandomInt(0,1)], pos );

		if ( pParticle )
		{
			pParticle->m_flLifeRemaining	= random_valve->RandomFloat(GLASS_SHARD_MIN_LIFE,GLASS_SHARD_MAX_LIFE);

			pParticle->m_vecVelocity[0]		= ( normal[0] + random_valve->RandomFloat( -0.8f, 0.8f ) ) * random_valve->RandomFloat( GLASS_SHARD_MIN_SPEED, GLASS_SHARD_MAX_SPEED );
			pParticle->m_vecVelocity[1]		= ( normal[1] + random_valve->RandomFloat( -0.8f, 0.8f ) ) * random_valve->RandomFloat( GLASS_SHARD_MIN_SPEED, GLASS_SHARD_MAX_SPEED );
			pParticle->m_vecVelocity[2]		= ( normal[2] + random_valve->RandomFloat( -0.8f, 0.8f ) ) * random_valve->RandomFloat( GLASS_SHARD_MIN_SPEED, GLASS_SHARD_MAX_SPEED );

			pParticle->m_uchSize			= flShardSize + random_valve->RandomFloat(-0.5*flShardSize,0.5*flShardSize);
			pParticle->m_vAngles			= RandomAngle( 0, 360 );
			pParticle->m_flAngSpeed			= random_valve->RandomFloat(-800,800);

			pParticle->m_uchFrontColor[0]	= (byte)(color[0] * vecColor.x);
			pParticle->m_uchFrontColor[1]	= (byte)(color[1] * vecColor.y);
			pParticle->m_uchFrontColor[2]	= (byte)(color[2] * vecColor.z);
			pParticle->m_uchBackColor[0]	= (byte)(color[0] * vecColor.x);
			pParticle->m_uchBackColor[1]	= (byte)(color[1] * vecColor.y);
			pParticle->m_uchBackColor[2]	= (byte)(color[2] * vecColor.z);
		}
	}

	pGlassEmitter->m_ParticleCollision.Setup( pos, &normal, GLASS_SHARD_NOISE, GLASS_SHARD_MIN_SPEED, GLASS_SHARD_MAX_SPEED, GLASS_SHARD_GRAVITY, GLASS_SHARD_DAMPING );

	color[0] = 64;
	color[1] = 64;
	color[2] = 92;

	// ---------------------------
	// Dust
	// ---------------------------

	Vector	dir;
	Vector	offset = pos + ( normal * 2.0f );
	float	colorRamp;

	SimpleParticle newParticle;

	for ( int i = 0; i < 4; i++ )
	{
		newParticle.m_Pos = offset;

		newParticle.m_flLifetime= 0.0f;
		newParticle.m_flDieTime	= random_valve->RandomFloat( 0.1f, 0.25f );

		dir[0] = normal[0] + random_valve->RandomFloat( -0.8f, 0.8f );
		dir[1] = normal[1] + random_valve->RandomFloat( -0.8f, 0.8f );
		dir[2] = normal[2] + random_valve->RandomFloat( -0.8f, 0.8f );

		newParticle.m_uchStartSize	= random_valve->RandomInt( 1, 4 );
		newParticle.m_uchEndSize	= newParticle.m_uchStartSize * 8;

		newParticle.m_vecVelocity	= dir * random_valve->RandomFloat( 8.0f, 16.0f )*(i+1);
		newParticle.m_vecVelocity[2] -= random_valve->RandomFloat( 16.0f, 32.0f )*(i+1);

		newParticle.m_uchStartAlpha	= random_valve->RandomInt( 128, 255 );
		newParticle.m_uchEndAlpha	= 0;

		newParticle.m_flRoll		= random_valve->RandomFloat( 0, 360 );
		newParticle.m_flRollDelta	= random_valve->RandomFloat( -1, 1 );

		colorRamp = random_valve->RandomFloat( 0.5f, 1.25f );

		newParticle.m_uchColor[0] = std::min( 1.0f, color[0]*colorRamp )*255.0f;
		newParticle.m_uchColor[1] = std::min( 1.0f, color[1]*colorRamp )*255.0f;
		newParticle.m_uchColor[2] = std::min( 1.0f, color[2]*colorRamp )*255.0f;

		AddSimpleParticle( &newParticle, g_Mat_BloodPuff[0] );
	}

	//
	// Bullet hole capper
	//
	newParticle.m_Pos = offset;

	newParticle.m_flLifetime		= 0.0f;
	newParticle.m_flDieTime		= random_valve->RandomFloat( 1.0f, 1.5f );

	dir[0] = normal[0] + random_valve->RandomFloat( -0.8f, 0.8f );
	dir[1] = normal[1] + random_valve->RandomFloat( -0.8f, 0.8f );
	dir[2] = normal[2] + random_valve->RandomFloat( -0.8f, 0.8f );

	newParticle.m_uchStartSize	= random_valve->RandomInt( 4, 8 );
	newParticle.m_uchEndSize		= newParticle.m_uchStartSize * 4.0f;

	newParticle.m_vecVelocity = dir * random_valve->RandomFloat( 2.0f, 8.0f );
	newParticle.m_vecVelocity[2] = random_valve->RandomFloat( -2.0f, 2.0f );

	newParticle.m_uchStartAlpha	= random_valve->RandomInt( 32, 64 );
	newParticle.m_uchEndAlpha	= 0;

	newParticle.m_flRoll			= random_valve->RandomFloat( 0, 360 );
	newParticle.m_flRollDelta	= random_valve->RandomFloat( -2, 2 );

	colorRamp = random_valve->RandomFloat( 0.5f, 1.25f );

	newParticle.m_uchColor[0] = std::min( 1.0f, color[0]*colorRamp )*255.0f;
	newParticle.m_uchColor[1] = std::min( 1.0f, color[1]*colorRamp )*255.0f;
	newParticle.m_uchColor[2] = std::min( 1.0f, color[2]*colorRamp )*255.0f;

	AddSimpleParticle( &newParticle, g_Mat_DustPuff[0] );
}

DECLARE_CLIENT_EFFECT( GlassImpact, GlassImpactCallback )

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &pos - 
//			*tr - 
//-----------------------------------------------------------------------------
void FX_AntlionImpact( const Vector &pos, trace_t *trace )
{

	CVProfScope _( "FX_AntlionImpact", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );

	ks::intrusive_ptr<CSimple3DEmitter> fleckEmitter = CSimple3DEmitter::Create( "FX_DebrisFlecks" );
	if ( fleckEmitter == nullptr )
		return;

	Vector	shotDir = ( trace->startpos - trace->endpos );
	VectorNormalize( shotDir );

	Vector	spawnOffset	= trace->endpos + ( shotDir * 2.0f );

	Vector vWorldMins, vWorldMaxs;
	if ( trace->Ent<CBaseEntity>() )
	{
		float scale = trace->Ent<CBaseEntity>()->CollisionProp()->BoundingRadius();
		vWorldMins[0] = spawnOffset[0] - scale;
		vWorldMins[1] = spawnOffset[1] - scale;
		vWorldMins[2] = spawnOffset[2] - scale;
		vWorldMaxs[0] = spawnOffset[0] + scale;
		vWorldMaxs[1] = spawnOffset[1] + scale;
		vWorldMaxs[2] = spawnOffset[2] + scale;
	}
	else
	{
		return;
	}

	fleckEmitter->SetSortOrigin( spawnOffset );
	fleckEmitter->GetBinding().SetBBox( spawnOffset-Vector(32,32,32), spawnOffset+Vector(32,32,32), true );

	// Handle increased scale
	float flMaxSpeed = 256.0f;
	float flAngularSpray = 1.0f;

	// Setup our collision information
	fleckEmitter->m_ParticleCollision.Setup( spawnOffset, &shotDir, flAngularSpray, 8.0f, flMaxSpeed, FLECK_GRAVITY, FLECK_DAMPEN );

	Vector	dir, end;
	Vector	color = Vector( 1, 0.9, 0.75 );
	float	colorRamp;

	int	numFlecks = random_valve->RandomInt( 8, 16 );

	Particle3D *pFleckParticle;

	// Dump out flecks
	int i;
	for ( i = 0; i < numFlecks; i++ )
	{
		pFleckParticle = (Particle3D *) fleckEmitter->AddParticle( sizeof(Particle3D), g_Mat_Fleck_Antlion[random_valve->RandomInt(0,1)], spawnOffset );
		if ( pFleckParticle == nullptr )
			break;

		pFleckParticle->m_flLifeRemaining = 3.0f;

		dir[0] = shotDir[0] + random_valve->RandomFloat( -flAngularSpray, flAngularSpray );
		dir[1] = shotDir[1] + random_valve->RandomFloat( -flAngularSpray, flAngularSpray );
		dir[2] = shotDir[2] + random_valve->RandomFloat( -flAngularSpray, flAngularSpray );

		pFleckParticle->m_uchSize		= random_valve->RandomInt( 1, 6 );

		pFleckParticle->m_vecVelocity	= dir * random_valve->RandomFloat( FLECK_MIN_SPEED, flMaxSpeed);
		pFleckParticle->m_vAngles.Random( 0, 360 );
		pFleckParticle->m_flAngSpeed	= random_valve->RandomFloat(-800,800);

		pFleckParticle->m_uchFrontColor[0] = 255;
		pFleckParticle->m_uchFrontColor[1] = 255;
		pFleckParticle->m_uchFrontColor[2] = 255;

		pFleckParticle->m_uchBackColor[0] = 128;
		pFleckParticle->m_uchBackColor[1] = 128;
		pFleckParticle->m_uchBackColor[2] = 128;
	}

	//
	// Dust trail
	//

	SimpleParticle	*pParticle;

	ks::intrusive_ptr<CSimpleEmitter> dustEmitter = CSimpleEmitter::Create( "FX_DebrisFlecks" );					
	if ( !dustEmitter )
		return;

	Vector	offset = trace->endpos + ( shotDir * 4.0f );

	dustEmitter->SetSortOrigin( offset );
	dustEmitter->GetBinding().SetBBox( spawnOffset-Vector(32,32,32), spawnOffset+Vector(32,32,32), true );

	for ( i = 0; i < 4; i++ )
	{
		pParticle = (SimpleParticle *) dustEmitter->AddParticle( sizeof(SimpleParticle), g_Mat_DustPuff[0], offset );

		if ( pParticle == nullptr )
			break;

		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime	= 1.0f;
		
		dir[0] = shotDir[0] + random_valve->RandomFloat( -0.8f, 0.8f );
		dir[1] = shotDir[1] + random_valve->RandomFloat( -0.8f, 0.8f );
		dir[2] = shotDir[2] + random_valve->RandomFloat( -0.8f, 0.8f );

		pParticle->m_uchStartSize	= random_valve->RandomInt( 8, 16 );
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 4.0f;

		pParticle->m_vecVelocity = dir * random_valve->RandomFloat( 4.0f, 64.0f );

		pParticle->m_uchStartAlpha	= random_valve->RandomInt( 32, 64);
		pParticle->m_uchEndAlpha	= 0;
		
		pParticle->m_flRoll			= random_valve->RandomFloat( 0, 2.0f*std::numbers::pi );
		pParticle->m_flRollDelta	= random_valve->RandomFloat( -0.5f, 0.5f );

		colorRamp = random_valve->RandomFloat( 0.5f, 1.0f );

		pParticle->m_uchColor[0] = std::min( 1.0f, color[0]*colorRamp )*255.0f;
		pParticle->m_uchColor[1] = std::min( 1.0f, color[1]*colorRamp )*255.0f;
		pParticle->m_uchColor[2] = std::min( 1.0f, color[2]*colorRamp )*255.0f;
	}

	CLocalPlayerFilter filter;
	CBaseEntity::EmitSound( filter, 0, "FX_AntlionImpact.ShellImpact", &trace->endpos );
}

//-----------------------------------------------------------------------------
// Purpose: Spurt out bug blood
// Input  : &pos - 
//			&dir - 
//-----------------------------------------------------------------------------
static constexpr auto NUM_BUG_BLOOD = 32;
static constexpr auto NUM_BUG_BLOOD2 = 16;
static constexpr auto NUM_BUG_SPLATS = 16;

void FX_BugBlood( Vector &pos, Vector &dir, Vector &vWorldMins, Vector &vWorldMaxs )
{
	CVProfScope _( "FX_BugBlood", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );

	ks::intrusive_ptr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "FX_BugBlood" );
	if ( !pSimple )
		return;

	pSimple->SetSortOrigin( pos );
	pSimple->GetBinding().SetBBox( vWorldMins, vWorldMaxs, true );
	pSimple->GetBinding().SetBBox( pos-Vector(32,32,32), pos+Vector(32,32,32), true );

	Vector	vDir;
	vDir[0] = dir[0] + random_valve->RandomFloat( -2.0f, 2.0f );
	vDir[1] = dir[1] + random_valve->RandomFloat( -2.0f, 2.0f );
	vDir[2] = dir[2] + random_valve->RandomFloat( -2.0f, 2.0f );

	VectorNormalize( vDir );

	int i;
	for ( i = 0; i < NUM_BUG_BLOOD; i++ )
	{
		SimpleParticle *sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), g_Mat_BloodPuff[0], pos );
			
		if ( sParticle == nullptr )
			return;

		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= 0.25f;
			
		float	speed = random_valve->RandomFloat( 32.0f, 150.0f );

		sParticle->m_vecVelocity	= vDir * -speed;
		sParticle->m_vecVelocity[2] -= 32.0f;

		sParticle->m_uchColor[0]	= 255;
		sParticle->m_uchColor[1]	= 200;
		sParticle->m_uchColor[2]	= 32;
		sParticle->m_uchStartAlpha	= 255;
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchStartSize	= random_valve->RandomInt( 1, 2 );
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize*random_valve->RandomInt( 1, 4 );
		sParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= random_valve->RandomFloat( -2.0f, 2.0f );
	}

	for ( i = 0; i < NUM_BUG_BLOOD2; i++ )
	{
		SimpleParticle *sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), g_Mat_BloodPuff[1], pos );
			
		if ( sParticle == nullptr )
		{
			return;
		}

		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= random_valve->RandomFloat( 0.25f, 0.5f );
			
		float	speed = random_valve->RandomFloat( 8.0f, 255.0f );

		sParticle->m_vecVelocity	= vDir * -speed;
		sParticle->m_vecVelocity[2] -= 16.0f;

		sParticle->m_uchColor[0]	= 255;
		sParticle->m_uchColor[1]	= 200;
		sParticle->m_uchColor[2]	= 32;
		sParticle->m_uchStartAlpha	= random_valve->RandomInt( 16, 32 );
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchStartSize	= random_valve->RandomInt( 1, 3 );
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize*random_valve->RandomInt( 1, 4 );
		sParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= random_valve->RandomFloat( -2.0f, 2.0f );
	}

	Vector	offset;

	for ( i = 0; i < NUM_BUG_SPLATS; i++ )
	{
		offset.Random( -2, 2 );
		offset += pos;

		SimpleParticle *sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), g_Mat_BloodPuff[1], offset );
			
		if ( sParticle == nullptr )
		{
			return;
		}
		
		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= random_valve->RandomFloat( 0.25f, 0.5f );
			
		float speed = 75.0f * ((i/(float)NUM_BUG_SPLATS)+1);

		sParticle->m_vecVelocity.Random( -16.0f, 16.0f );

		sParticle->m_vecVelocity	+= vDir * -speed;
		sParticle->m_vecVelocity[2] -= ( 64.0f * ((i/(float)NUM_BUG_SPLATS)+1) );

		sParticle->m_uchColor[0]	= 255;
		sParticle->m_uchColor[1]	= 200;
		sParticle->m_uchColor[2]	= 32;
		sParticle->m_uchStartAlpha	= 255;
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchStartSize	= random_valve->RandomInt( 1, 2 );
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize*4;
		sParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= random_valve->RandomFloat( -2.0f, 2.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Blood puff
//-----------------------------------------------------------------------------
void FX_Blood( Vector &pos, Vector &dir, float r, float g, float b, float a )
{
	CVProfScope _( "FX_Blood", 0, VPROF_BUDGETGROUP_PARTICLE_RENDERING, false, BUDGETFLAG_OTHER );

	// Cloud
	ks::intrusive_ptr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "FX_Blood" );
	if ( !pSimple )
		return;
	pSimple->SetSortOrigin( pos );

	Vector	vDir;

	vDir[0] = dir[0] + random_valve->RandomFloat( -1.0f, 1.0f );
	vDir[1] = dir[1] + random_valve->RandomFloat( -1.0f, 1.0f );
	vDir[2] = dir[2] + random_valve->RandomFloat( -1.0f, 1.0f );

	VectorNormalize( vDir );

	int i;
	for ( i = 0; i < 2; i++ )
	{
		SimpleParticle *sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), g_Mat_BloodPuff[0], pos );
			
		if ( sParticle == nullptr )
		{
			return;
		}

		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= random_valve->RandomFloat( 0.25f, 0.5f );
			
		float	speed = random_valve->RandomFloat( 2.0f, 8.0f );

		sParticle->m_vecVelocity	= vDir * (speed*i);
		sParticle->m_vecVelocity[2] += random_valve->RandomFloat( -32.0f, -16.0f );

		sParticle->m_uchColor[0]	= r;
		sParticle->m_uchColor[1]	= g;
		sParticle->m_uchColor[2]	= b;
		sParticle->m_uchStartAlpha	= a;
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchStartSize	= 2;
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize*4;
		sParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= random_valve->RandomFloat( -2.0f, 2.0f );
	}

	for ( i = 0; i < 2; i++ )
	{
		SimpleParticle *sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), g_Mat_BloodPuff[1], pos );
			
		if ( sParticle == nullptr )
		{
			return;
		}

		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= 0.5f;
			
		float	speed = random_valve->RandomFloat( 4.0f, 16.0f );

		sParticle->m_vecVelocity	= vDir * (speed*i);

		sParticle->m_uchColor[0]	= r;
		sParticle->m_uchColor[1]	= g;
		sParticle->m_uchColor[2]	= b;
		sParticle->m_uchStartAlpha	= 128;
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchStartSize	= 2;
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize*4;
		sParticle->m_flRoll			= random_valve->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= random_valve->RandomFloat( -4.0f, 4.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &pos - 
//			&dir - 
//			type - 
//-----------------------------------------------------------------------------
void FX_GaussExplosion( const Vector &pos, const Vector &dir, int type )
{
	Vector	vDir;

	vDir[0] = dir[0] + random_valve->RandomFloat( -1.0f, 1.0f );
	vDir[1] = dir[1] + random_valve->RandomFloat( -1.0f, 1.0f );
	vDir[2] = dir[2] + random_valve->RandomFloat( -1.0f, 1.0f );

	VectorNormalize( vDir );

	int i;

	//
	// PC version
	//
	ks::intrusive_ptr<CTrailParticles> pSparkEmitter = CTrailParticles::Create( "FX_ElectricSpark" );

	if ( !pSparkEmitter )
	{
		contract_assert( 0 );
		return;
	}

	PMaterialHandle	hMaterial	= pSparkEmitter->GetPMaterial( "effects/spark" );

	pSparkEmitter->SetSortOrigin( pos );

	pSparkEmitter->m_ParticleCollision.SetGravity( 800.0f );
	pSparkEmitter->SetFlag( bitsPARTICLE_TRAIL_VELOCITY_DAMPEN|bitsPARTICLE_TRAIL_COLLIDE );

	//Setup our collision information
	pSparkEmitter->m_ParticleCollision.Setup( pos, &vDir, 0.8f, 128, 512, 800, 0.3f );

	int numSparks = random_valve->RandomInt( 16, 32 );
	TrailParticle	*pParticle;

	// Dump out sparks
	for ( i = 0; i < numSparks; i++ )
	{
		pParticle = (TrailParticle *) pSparkEmitter->AddParticle( sizeof(TrailParticle), hMaterial, pos );

		if ( pParticle == nullptr )
			return;

		pParticle->m_flLifetime	= 0.0f;

		vDir.Random( -0.6f, 0.6f );
		vDir += dir;
		VectorNormalize( vDir );

		pParticle->m_flWidth		= random_valve->RandomFloat( 1.0f, 4.0f );
		pParticle->m_flLength		= random_valve->RandomFloat( 0.01f, 0.1f );
		pParticle->m_flDieTime		= random_valve->RandomFloat( 0.25f, 1.0f );

		pParticle->m_vecVelocity	= vDir * random_valve->RandomFloat( 128, 512 );

		Color32Init( pParticle->m_color, 255, 255, 255, 255 );
	}

	FX_ElectricSpark( pos, 1, 1, &vDir );
}

class CTEGaussExplosion : public CTEParticleSystem
{
	[[= ks::reflect::NetTable{ .name = "DT_TEGaussExplosion" } ]]
	static constexpr bool ks_class = true;

public:
	DECLARE_CLASS( CTEGaussExplosion, CTEParticleSystem );
	DECLARE_CLIENTCLASS();

				CTEGaussExplosion();
	virtual		~CTEGaussExplosion();

public:
	virtual void	PostDataUpdate( DataUpdateType_t updateType );
	virtual	bool	ShouldDraw() { return true; }

public:

	[[= ks::reflect::Net{} ]] int			m_nType;
	[[= ks::reflect::Net{} ]] Vector		m_vecDirection;
};

IMPLEMENT_REFLECT_CLIENTCLASS_EVENT( CTEGaussExplosion, DT_TEGaussExplosion, CTEGaussExplosion )

//==================================================
// CTEGaussExplosion
//==================================================

CTEGaussExplosion::CTEGaussExplosion()
{
}

CTEGaussExplosion::~CTEGaussExplosion()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bNewEntity - whether or not to start a new entity
//-----------------------------------------------------------------------------
void CTEGaussExplosion::PostDataUpdate( DataUpdateType_t updateType )
{
	FX_GaussExplosion( m_vecOrigin, m_vecDirection, m_nType );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : filter - 
//			delay - 
//			&pos - 
//			&dir - 
//			type - 
//-----------------------------------------------------------------------------
void TE_GaussExplosion( IRecipientFilter& filter, float delay, const Vector &pos, const Vector &dir, int type )
{
	FX_GaussExplosion( pos, dir, type );
}
