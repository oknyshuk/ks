//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements visual effects entities: sprites, beams, bubbles, etc.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "engine/IEngineSound.h"
#include "baseentity.h"
#include "entityoutput.h"
#include "recipientfilter.h"
#include "usermessages.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEnvHudHint : public CPointEntity
{
public:
	DECLARE_CLASS( CEnvHudHint, CPointEntity );

	void	Spawn( void );
	void	Precache( void );

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
void CEnvHudHint::Spawn( void )
{
	Precache();

	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvHudHint::Precache( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for showing the message and/or playing the sound.
//-----------------------------------------------------------------------------
void CEnvHudHint::InputShowHudHint( inputdata_t &inputdata )
{
	CBaseEntity *pPlayer = NULL;

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
	CBaseEntity *pPlayer = NULL;

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
