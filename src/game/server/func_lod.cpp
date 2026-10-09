// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;
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
#include "base_transmit_proxy.h"
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
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"

class [[= ks::reflect::NetTable{ .name = "DT_Func_LOD" } ]]
      CFunc_LOD : public CBaseEntity
{
	DECLARE_DATADESC();
	DECLARE_CLASS( CFunc_LOD, CBaseEntity );
public:
	DECLARE_SERVERCLASS();

					CFunc_LOD();
	virtual 		~CFunc_LOD();


	// When the viewer is between:
	// (0 and m_fNonintrusiveDist):					the bmodel is forced to be visible
	// (m_fNonintrusiveDist and m_fDisappearDist):	the bmodel is trying to appear or disappear nonintrusively
	//												(waits until it's out of the view frustrum or until there's a lot of motion)
	// (m_fDisappearDist+):							the bmodel is forced to be invisible
	CNetworkVar( int, m_nDisappearMinDist, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "DisappearMinDist" } ]] );
	CNetworkVar( int, m_nDisappearMaxDist, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "DisappearMaxDist" } ]] );

// CBaseEntity overrides.
public:

	virtual void	Spawn();
	bool			CreateVPhysics();
	virtual void	Activate();
	virtual bool	KeyValue( const char *szKeyName, const char *szValue );
};


IMPLEMENT_REFLECT_SERVERCLASS( CFunc_LOD, DT_Func_LOD )


LINK_ENTITY_TO_CLASS(func_lod, CFunc_LOD);


//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CFunc_LOD )


// ------------------------------------------------------------------------------------- //
// CFunc_LOD implementation.
// ------------------------------------------------------------------------------------- //
CFunc_LOD::CFunc_LOD()
{
}


CFunc_LOD::~CFunc_LOD()
{
}


void CFunc_LOD::Spawn()
{
	// Bind to our bmodel.
	SetModel( STRING( GetModelName() ) );
	SetSolid( SOLID_BSP );
	BaseClass::Spawn();

	CreateVPhysics();
}

bool CFunc_LOD::CreateVPhysics()
{
	VPhysicsInitStatic();
	return true;
}

void CFunc_LOD::Activate()
{
	BaseClass::Activate();
}


bool CFunc_LOD::KeyValue( const char *szKeyName, const char *szValue )
{
	// NOTE: Backward compat
	if ( FStrEq(szKeyName, "DisappearDist") )
	{
		m_nDisappearMinDist = std::atoi(szValue);
		m_nDisappearMaxDist = m_nDisappearMinDist + 800;
	}
	else if (FStrEq(szKeyName, "Solid"))
	{
		if (std::atoi(szValue) != 0)
		{
			AddSolidFlags( FSOLID_NOT_SOLID );
		}
	}
	else
	{
		return BaseClass::KeyValue(szKeyName, szValue);
	}

	return true;
}
