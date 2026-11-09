// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Implements visual effects entities: sprites, beams, bubbles, etc.

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
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
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
#include "basetempentity.h"
#include "player.h"
#include "te.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "engine/IEngineSound.h"
#include "baseentity.h"
#include "recipientfilter.h"
#include "usermessages.h"

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
