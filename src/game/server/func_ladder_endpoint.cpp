// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

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
#include "ndebugoverlay.h"
#include "func_ladder.h"

//-----------------------------------------------------------------------------
// Purpose: A transient entity used to construct a true CFuncLadder
// FIXME:  THIS ENTITY IS OBSOLETE NOW, SHOULD BE REMOVED FROM HERE AND .FGD AT SOME POINT!!!
//-----------------------------------------------------------------------------
class CFuncLadderEndPoint : public CBaseEntity
{
public:
	DECLARE_CLASS( CFuncLadderEndPoint, CBaseEntity );

	void Activate_Impl();

private:
	bool		Validate();
};

LINK_ENTITY_TO_CLASS( func_ladderendpoint, CFuncLadderEndPoint );

void CFuncLadderEndPoint::Activate_Impl()
{
	BaseClass::Activate_Impl();

	if ( IsMarkedForDeletion() )
		return;

	Validate();
}

bool CFuncLadderEndPoint::Validate()
{
	// Find the the other end
	Vector startPos = GetAbsOrigin();
	
	CFuncLadderEndPoint *other = dynamic_cast< CFuncLadderEndPoint * >( GetNextTarget() );
	if ( !other )
	{
		DevMsg( 1, "func_ladderendpoint(%s) without matching target\n", GetEntityName().ToCStr() );
		return false;
	}

	Vector endPos = other->GetAbsOrigin();

	CFuncLadder *ladder = ( CFuncLadder * )CreateEntityByName( "func_useableladder" );
	if ( ladder )
	{
		ladder->SetEndPoints( startPos, endPos );
		ladder->SetAbsOrigin( GetAbsOrigin() );
		ladder->SetParent( GetParent() );
		ladder->SetName( GetEntityName() );
		ladder->Spawn();
	}

	// Delete both endpoints
	UTIL_Remove( other );
	UTIL_Remove( this );

	return true;
}
