// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: contains entities who have no physical representation in the game, and who
//		must be triggered by other entities to cause their effects.

import std;
import ks.tier1;
#include "vphysics_interface.h"
#include "edict.h"
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
#include "base_transmit_proxy.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "physics.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "npcevent.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "player.h"

//-----------------------------------------------------------------------------
// Purpose: when fired, it changes which track the CD is playing
//-----------------------------------------------------------------------------
class CTargetCDAudioRep : public CPointEntity
{
public:
	DECLARE_CLASS( CTargetCDAudioRep, CPointEntity );

	[[= ks::reflect::Input{ .name = "ChangeCDTrack", .type = FIELD_VOID } ]] void InputChangeCDTrack( inputdata_t &inputdata );
	
	DECLARE_DATADESC();

private:
	[[= ks::reflect::Key{ .name = "track" } ]] int m_iTrack;  // CD track to change to when fired
};

LINK_ENTITY_TO_CLASS( target_cdaudio, CTargetCDAudioRep );

IMPLEMENT_REFLECT_DATAMAP( CTargetCDAudioRep )



//-----------------------------------------------------------------------------
// Purpose: Changes the current playing CD track
//-----------------------------------------------------------------------------
void CTargetCDAudioRep::InputChangeCDTrack( inputdata_t &inputdata )
{
	int iTrack = m_iTrack;
	

	edict_t *pClient = nullptr;
	if ( gpGlobals->maxClients == 1 )
	{
		pClient = INDEXENT( 1 );
	}
	else
	{
		// In multiplayer, send it back to the activator
		CBasePlayer *player = dynamic_cast< CBasePlayer * >( inputdata.pActivator );
		if ( player )
		{
			pClient = player->edict();
		}
	}
	
	// Can't play if the client is not connected!
	if ( !pClient )
		return;

	if ( iTrack < -1 || iTrack > 30 )
	{
		Warning( "TargetCDAudio - Track %d out of range\n", iTrack );
		return;
	}

	if ( iTrack == -1 )
	{
		engine->ClientCommand( pClient, "cd pause\n");
	}
	else
	{
		engine->ClientCommand ( pClient, "cd play %3d\n", iTrack );
	}
}

//-----------------------------------------------------------------------------
// Purpose: changes the gravity of the player who activates this entity
//-----------------------------------------------------------------------------
class CTargetChangeGravity : public CPointEntity
{
public:
	DECLARE_CLASS( CTargetChangeGravity, CPointEntity );

	DECLARE_DATADESC();

	[[= ks::reflect::Input{ .name = "ChangeGrav", .type = FIELD_VOID } ]] void InputChangeGrav( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "ResetGrav", .type = FIELD_VOID } ]] void InputResetGrav( inputdata_t &inputdata );

	[[= ks::reflect::Key{ .name = "gravity" } ]] int m_iGravity;

	int m_iOldGrav;
};

LINK_ENTITY_TO_CLASS( target_changegravity, CTargetChangeGravity );

IMPLEMENT_REFLECT_DATAMAP( CTargetChangeGravity )



//-----------------------------------------------------------------------------
// Purpose: Input handler for changing the activator's gravity.
//-----------------------------------------------------------------------------
void CTargetChangeGravity::InputChangeGrav( inputdata_t &inputdata )
{
	CBasePlayer *pl = ToBasePlayer( inputdata.pActivator );
	if ( !pl )
		return;

	// Save the gravity to restore it in InputResetGrav
	if ( m_iOldGrav )
		m_iOldGrav = pl->GetGravity();

	pl->SetGravity(m_iGravity);
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for resetting the activator's gravity.
//-----------------------------------------------------------------------------
void CTargetChangeGravity::InputResetGrav( inputdata_t &inputdata )
{
	CBasePlayer *pl = ToBasePlayer( inputdata.pActivator );
	if ( !pl )
		return;

	pl->SetGravity(m_iOldGrav);
}
