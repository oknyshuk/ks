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
#include "player.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"

// -------------------------------------------------------------------------------- //
// An entity used to test traceline
// -------------------------------------------------------------------------------- //

class [[= ks::reflect::NetTable{ .name = "DT_TestTraceline", .base = false } ]]
      [[= ks::reflect::From<"m_clrRender", ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED }, SendProxy_Color32ToInt32>{} ]]
      [[= ks::reflect::From<"m_vecOrigin", ks::reflect::Net{ .bits = 19, .low = MIN_COORD_INTEGER, .high = MAX_COORD_INTEGER, .enc = ks::reflect::WireEnc::Vector }>{} ]]
      [[= ks::reflect::From<"m_angRotation", ks::reflect::Net{ .bits = 19, .low = MIN_COORD_INTEGER, .high = MAX_COORD_INTEGER, .enc = ks::reflect::WireEnc::Float, .index = 0 }>{} ]]
      [[= ks::reflect::From<"m_angRotation", ks::reflect::Net{ .bits = 19, .low = MIN_COORD_INTEGER, .high = MAX_COORD_INTEGER, .enc = ks::reflect::WireEnc::Float, .index = 1 }>{} ]]
      [[= ks::reflect::From<"m_angRotation", ks::reflect::Net{ .bits = 19, .low = MIN_COORD_INTEGER, .high = MAX_COORD_INTEGER, .enc = ks::reflect::WireEnc::Float, .index = 2 }>{} ]]
      [[= ks::reflect::From<"m_hMoveParent", ks::reflect::Net{ .wire = "moveparent" }>{} ]]
      CTestTraceline : public CPointEntity
{
public:
	DECLARE_CLASS( CTestTraceline, CPointEntity );

	void	Spawn_Impl( void );
	int  	UpdateTransmitState_Impl();

	DECLARE_SERVERCLASS();

private:
	void	Spin( void );
};
							  

// This table encodes the CBaseEntity data.
IMPLEMENT_REFLECT_SERVERCLASS( CTestTraceline, DT_TestTraceline )

LINK_ENTITY_TO_CLASS( test_traceline, CTestTraceline );



void	CTestTraceline::Spawn_Impl( void )
{
	SetRenderColor( 255, 255, 255 );
	SetRenderAlpha( 255 );
	SetNextThink( gpGlobals->curtime );

	SetThink( &CTestTraceline::Spin );
}

void	CTestTraceline::Spin( void )
{
	static ConVar	traceline_spin( "traceline_spin","1" );

	if (traceline_spin.GetInt())
	{
		float s = std::sin( gpGlobals->curtime );
		QAngle angles = GetLocalAngles();

		angles[0] = 180.0 * 0.5 * (s * s * s + 1.0f) + 90;
		angles[1] = gpGlobals->curtime * 10;
		   
		SetLocalAngles( angles );

	}
	SetNextThink( gpGlobals->curtime );
}

int CTestTraceline::UpdateTransmitState_Impl()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}
