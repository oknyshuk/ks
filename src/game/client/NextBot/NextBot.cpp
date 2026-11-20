// Copyright (c) 2005 Turtle Rock Studios, Inc. - All Rights Reserved

module;
#include "client_class.h"
#include "../reflect_recvtable.h"
module ks.client;

import ks.math.compressed_vector;
import ks.math.vector;
import ks.platform;
import ks.simd;
import ks.tier1.convar;
import ks.tier1.createinterface;
import ks.tier1.reflect_wirevar;
import ks.tier1.registry;
import ks.tier1.vector_ops;
import :NextBot.NextBot;
import :props;
import :baseanimatedtextureproxy;
import :iinput;
import :baseentity;
import :sharedvar;
import :util;
import :precache;
import :noopanimcontroller;
import :achievements_and_stats_interface;
import :particlemgr;
import ks.game.gamemovement;
import ks.game.mapentities_shared;
import ks.game.shareddefs;

class CBasePlayer;

using namespace ::ks::reflect;

// NextBot.cpp
// Client-side implementation of Next generation bot system
// Author: Michael Booth, April 2005

#undef NextBot

ConVar NextBotShadowDist( side_registry(), "nb_shadow_dist", "400" );

//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_CLIENTCLASS( CNextBotCombatCharacter, DT_NextBot, NextBotCombatCharacter )

//-----------------------------------------------------------------------------
CNextBotCombatCharacter::CNextBotCombatCharacter()
{
	// Left4Dead have surfaces too steep for IK to work properly
	m_EntClientFlags |= ENTCLIENTFLAG_DONTUSEIK;

	m_shadowType = SHADOWS_SIMPLE;
	m_forcedShadowType = SHADOWS_NONE;
	m_bForceShadowType = false;

	TheClientNextBots().Register( this );
}

//-----------------------------------------------------------------------------
CNextBotCombatCharacter::~CNextBotCombatCharacter()
{
	TheClientNextBots().UnRegister( this );
}

//-----------------------------------------------------------------------------
void CNextBotCombatCharacter::Spawn( void )
{
	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
void CNextBotCombatCharacter::UpdateClientSideAnimation()
{
	if (IsDormant())
	{
		return;
	}

	BaseClass::UpdateClientSideAnimation();
}

//--------------------------------------------------------------------------------------------------------
void CNextBotCombatCharacter::UpdateShadowLOD( void )
{
	ShadowType_t oldShadowType = m_shadowType;

	if ( m_bForceShadowType )
	{
		m_shadowType = m_forcedShadowType;
	}
	else
	{
		{
			if ( CBasePlayer::GetLocalPlayer() )
			{
				Vector delta = GetAbsOrigin() - CBasePlayer::GetLocalPlayer()->GetAbsOrigin();
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
ShadowType_t CNextBotCombatCharacter::ShadowCastType( void ) 
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
bool CNextBotCombatCharacter::GetForcedShadowCastType( ShadowType_t* pForcedShadowType ) const
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
CNextBotManager &TheClientNextBots( void )
{
	static CNextBotManager manager;
	return manager;
}

//--------------------------------------------------------------------------------------------------------
CNextBotManager::CNextBotManager( void )
{
}

//--------------------------------------------------------------------------------------------------------
CNextBotManager::~CNextBotManager()
{
}

//--------------------------------------------------------------------------------------------------------
void CNextBotManager::Register( CNextBotCombatCharacter *bot )
{
	m_botList.push_back( bot );
}

//--------------------------------------------------------------------------------------------------------
void CNextBotManager::UnRegister( CNextBotCombatCharacter *bot )
{
	ks::erase_first( m_botList, bot );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CNextBotManager::SetupInFrustumData( void )
{

	return true;
}

//--------------------------------------------------------------------------------------------------------
