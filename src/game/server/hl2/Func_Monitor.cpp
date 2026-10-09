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
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "base_transmit_proxy.h"
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
#include "point_camera.h"
#include "modelentities.h"
#include "info_camera_link.h"

class [[= ks::reflect::NetTable{ .name = "DT_FuncMonitor" } ]]
      CFuncMonitor : public CFuncBrush
{
	DECLARE_DATADESC();
	DECLARE_CLASS( CFuncMonitor, CFuncBrush );
	DECLARE_SERVERCLASS();

public:
	virtual void Activate();
	virtual void UpdateOnRemove();

private:
	[[= ks::reflect::Input{ .name = "SetCamera", .type = FIELD_STRING } ]] void InputSetCamera(inputdata_t &inputdata);
	void SetCameraByName(const char *szName);
	void ReleaseCameraLink();

	EHANDLE m_hInfoCameraLink;
};

// automatically hooks in the system's callbacks
IMPLEMENT_REFLECT_DATAMAP( CFuncMonitor )


LINK_ENTITY_TO_CLASS( func_monitor, CFuncMonitor );


IMPLEMENT_REFLECT_SERVERCLASS( CFuncMonitor, DT_FuncMonitor )


//-----------------------------------------------------------------------------
// Purpose: Called after all entities have spawned and after a load game.
//-----------------------------------------------------------------------------
void CFuncMonitor::Activate()
{
 	BaseClass::Activate();
	SetCameraByName(STRING(m_target));
}

void CFuncMonitor::UpdateOnRemove()
{
	ReleaseCameraLink();
	BaseClass::UpdateOnRemove();
}


//-----------------------------------------------------------------------------
// Frees the camera.
//-----------------------------------------------------------------------------
void CFuncMonitor::ReleaseCameraLink()
{
	if ( m_hInfoCameraLink )
	{
		UTIL_Remove( m_hInfoCameraLink );
		m_hInfoCameraLink = nullptr;

		// Keep the target up-to-date for save/load
		m_target = NULL_STRING;
	}
}


//-----------------------------------------------------------------------------
// Sets camera 
//-----------------------------------------------------------------------------
void CFuncMonitor::SetCameraByName(const char *szName)
{
	ReleaseCameraLink();
	CBaseEntity *pBaseEnt = gEntList.FindEntityByName( nullptr, szName );
	if( pBaseEnt )
	{
		CPointCamera *pCamera = dynamic_cast<CPointCamera *>( pBaseEnt );
		if( pCamera )
		{
			// Keep the target up-to-date for save/load
			m_target = MAKE_STRING( szName );
			m_hInfoCameraLink = CreateInfoCameraLink( this, pCamera ); 
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncMonitor::InputSetCamera(inputdata_t &inputdata)
{
	SetCameraByName( inputdata.value.String() );
}
