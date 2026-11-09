#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "string_t.h"
#include "vphysics_interface.h"
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
#include "baseanimating.h"
#include "triggers.h"
#include "cvisibilitymonitor.h"
#include "cs_shareddefs.h"
#include "cs_player_resource.h"

class CHostageRescueZone : public CBaseTrigger
{
public:
	DECLARE_CLASS( CHostageRescueZone, CBaseTrigger );

	void CHostageRescue();
	void Spawn_Impl();
	virtual void ReInitOnRoundStart( void );

	void HostageRescueTouch( CBaseEntity* pOther );

	EHANDLE		m_hInstructorHint;		// Hint that's used by the instructor system
};

//-----------------------------------------------------------------------------
// Purpose: A generic target entity that gets replicated to the client for displaying a hint for the hostage rescue zone
//-----------------------------------------------------------------------------
class CInfoInstructorHintHostageRescueZone : public CPointEntity
{
public:
	DECLARE_CLASS( CInfoInstructorHintHostageRescueZone, CPointEntity );

	void Spawn_Impl( void );
	int UpdateTransmitState_Impl( void )	// set transmit filter to transmit always
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

};
