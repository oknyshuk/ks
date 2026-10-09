// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

//
// hud_redraw.cpp
//
#include "rocketui/rmlui.h"
import ks.core;
import std;
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "hudelement.h"
#include "iclientmode.h"

//For progress bar orientations
const int CHud::HUDPB_HORIZONTAL = 0;
const int CHud::HUDPB_VERTICAL = 1;
const int CHud::HUDPB_HORIZONTAL_INV = 2;

// Called when a ConVar changes value
static void FovChanged_Callback( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );
	if ( engine->IsInGame() )
	{
		engine->ServerCmd( VarArgs( "fov %f\n", var.GetFloat() ) );
	}
}

static ConVar fov_watcher( "_fov", "0", 0, "Automates fov command to server.", FovChanged_Callback );

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
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		C_BaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
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
