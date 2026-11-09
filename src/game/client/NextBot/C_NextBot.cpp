// C_NextBot.cpp
// Client-side implementation of Next generation bot system
// Author: Michael Booth, April 2005
// Copyright (c) 2005 Turtle Rock Studios, Inc. - All Rights Reserved

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"
#include "C_NextBot.h"
#include "viewrender.h"

#undef NextBot

ConVar NextBotShadowDist( "nb_shadow_dist", "400" );

//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS( C_NextBotCombatCharacter, DT_NextBot, NextBotCombatCharacter )


//-----------------------------------------------------------------------------
C_NextBotCombatCharacter::C_NextBotCombatCharacter()
{
	// Left4Dead have surfaces too steep for IK to work properly
	m_EntClientFlags |= ENTCLIENTFLAG_DONTUSEIK;

	m_shadowType = SHADOWS_SIMPLE;
	m_forcedShadowType = SHADOWS_NONE;
	m_bForceShadowType = false;

	TheClientNextBots().Register( this );
}


//-----------------------------------------------------------------------------
C_NextBotCombatCharacter::~C_NextBotCombatCharacter()
{
	TheClientNextBots().UnRegister( this );
}


//-----------------------------------------------------------------------------
void C_NextBotCombatCharacter::Spawn( void )
{
	BaseClass::Spawn();
}


//-----------------------------------------------------------------------------
void C_NextBotCombatCharacter::UpdateClientSideAnimation()
{
	if (IsDormant())
	{
		return;
	}

	BaseClass::UpdateClientSideAnimation();
}


//--------------------------------------------------------------------------------------------------------
void C_NextBotCombatCharacter::UpdateShadowLOD( void )
{
	ShadowType_t oldShadowType = m_shadowType;

	if ( m_bForceShadowType )
	{
		m_shadowType = m_forcedShadowType;
	}
	else
	{
		{
			if ( C_BasePlayer::GetLocalPlayer() )
			{
				Vector delta = GetAbsOrigin() - C_BasePlayer::GetLocalPlayer()->GetAbsOrigin();
				if ( delta.IsLengthLessThan( NextBotShadowDist.GetFloat() ) )
				{
					m_shadowType = SHADOWS_RENDER_TO_TEXTURE_DYNAMIC;
				}
				else
				{
					m_shadowType = SHADOWS_SIMPLE;
				}
			}
			else
			{
				m_shadowType = SHADOWS_SIMPLE;
			}
		}
	}

	if ( oldShadowType != m_shadowType )
	{
		DestroyShadow();
	}
}


//--------------------------------------------------------------------------------------------------------
ShadowType_t C_NextBotCombatCharacter::ShadowCastType( void ) 
{
	if ( !IsVisible() )
		return SHADOWS_NONE;

	if ( m_shadowTimer.IsElapsed() )
	{
		m_shadowTimer.Start( 0.15f );
		UpdateShadowLOD();
	}

	return m_shadowType;
}


//--------------------------------------------------------------------------------------------------------
bool C_NextBotCombatCharacter::GetForcedShadowCastType( ShadowType_t* pForcedShadowType ) const
{
	if ( pForcedShadowType )
	{
		*pForcedShadowType = m_forcedShadowType;
	}
	return m_bForceShadowType;
}

//--------------------------------------------------------------------------------------------------------
/**
 * Singleton accessor.
 * By returning a reference, we guarantee construction of the 
 * instance before its first use.
 */
C_NextBotManager &TheClientNextBots( void )
{
	static C_NextBotManager manager;
	return manager;
}


//--------------------------------------------------------------------------------------------------------
C_NextBotManager::C_NextBotManager( void )
{
}


//--------------------------------------------------------------------------------------------------------
C_NextBotManager::~C_NextBotManager()
{
}


//--------------------------------------------------------------------------------------------------------
void C_NextBotManager::Register( C_NextBotCombatCharacter *bot )
{
	m_botList.push_back( bot );
}


//--------------------------------------------------------------------------------------------------------
void C_NextBotManager::UnRegister( C_NextBotCombatCharacter *bot )
{
	ks::erase_first( m_botList, bot );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool C_NextBotManager::SetupInFrustumData( void )
{

	return true;
}

//--------------------------------------------------------------------------------------------------------
