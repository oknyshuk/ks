//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"

#include "GameUI/IGameUI.h"
#include "fmtstr.h"
#include "igameevents.h"

#ifndef CLIENT_DLL
#include "reflect_annotations.h"
#include "reflect_datamap.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

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

	virtual void	Activate( void );

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

void CPointBonusMapsAccessor::Activate( void )
{
	BaseClass::Activate();

	CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
	if ( gameUIFactory )
	{
		m_pGameUI = (IGameUI *) gameUIFactory(GAMEUI_INTERFACE_VERSION, NULL );
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
		IGameUI *pGameUI = (IGameUI *) gameUIFactory(GAMEUI_INTERFACE_VERSION, NULL );
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
