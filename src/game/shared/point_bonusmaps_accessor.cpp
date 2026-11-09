// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#if defined(CLIENT_DLL)
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
#include "recipientfilter.h"
#endif

#include "GameUI/IGameUI.h"
#include "igameevents.h"

#ifndef CLIENT_DLL
#include "reflect_annotations.h"
#include "reflect_datamap.h"
#endif

// See interface.h/.cpp for specifics:  basically this ensures that we actually Sys_UnloadModule the dll and that we don't call Sys_LoadModule 
//  over and over again.
static CDllDemandLoader g_GameUI( "gameui" );

#ifndef CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPointBonusMapsAccessor : public CPointEntity
{
public:
	DECLARE_CLASS( CPointBonusMapsAccessor, CPointEntity );
	DECLARE_DATADESC();

	KS_VIRTUAL void	Activate_Impl( void );

	[[= ks::reflect::Input{ .name = "Unlock", .type = FIELD_VOID } ]] void InputUnlock( inputdata_t& inputdata );
	[[= ks::reflect::Input{ .name = "Complete", .type = FIELD_VOID } ]] void InputComplete( inputdata_t& inputdata );
	[[= ks::reflect::Input{ .name = "Save", .type = FIELD_VOID } ]] void InputSave( inputdata_t& inputdata );

private:
	[[= ks::reflect::Key{ .name = "filename" } ]] string_t	m_String_tFileName;
	[[= ks::reflect::Key{ .name = "mapname" } ]] string_t	m_String_tMapName;
	IGameUI		*m_pGameUI;
};

IMPLEMENT_REFLECT_DATAMAP( CPointBonusMapsAccessor )

LINK_ENTITY_TO_CLASS( point_bonusmaps_accessor, CPointBonusMapsAccessor );

void CPointBonusMapsAccessor::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
	if ( gameUIFactory )
	{
		m_pGameUI = (IGameUI *) gameUIFactory(GAMEUI_INTERFACE_VERSION, nullptr );
	}
}

void CPointBonusMapsAccessor::InputUnlock( inputdata_t& inputdata )
{
}

void CPointBonusMapsAccessor::InputComplete( inputdata_t& inputdata )
{
	if ( m_pGameUI )
	{
		int iNumAdvancedComplete = 0;


		IGameEvent *event = gameeventmanager->CreateEvent( "advanced_map_complete" );
		if ( event )
		{
			event->SetInt( "numadvanced", iNumAdvancedComplete );
			gameeventmanager->FireEvent( event );
		}
	}
}

void CPointBonusMapsAccessor::InputSave( inputdata_t& inputdata )
{
}

#endif

void BonusMapChallengeUpdate( const char *pchFileName, const char *pchMapName, const char *pchChallengeName, int iBest )
{
	CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
	if ( gameUIFactory )
	{
		IGameUI *pGameUI = (IGameUI *) gameUIFactory(GAMEUI_INTERFACE_VERSION, nullptr );
		if ( pGameUI )
		{
			int piNumMedals[ 3 ] = {0};


			IGameEvent *event = gameeventmanager->CreateEvent( "challenge_map_complete" );
			if ( event )
			{
				event->SetInt( "numbronze", piNumMedals[ 0 ] );
				event->SetInt( "numsilver", piNumMedals[ 1 ] );
				event->SetInt( "numgold", piNumMedals[ 2 ] );
				gameeventmanager->FireEvent( event );
			}
		}	
	}
}

void BonusMapChallengeNames( char *pchFileName, char *pchMapName, char *pchChallengeName )
{
}

void BonusMapChallengeObjectives( int &iBronze, int &iSilver, int &iGold )
{
}
