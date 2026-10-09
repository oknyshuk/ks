// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;
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
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "physics.h"
#include "ndebugoverlay.h"
#include "recipientfilter.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"

#include "particle_fire.h"

IMPLEMENT_REFLECT_SERVERCLASS( CParticleFire, DT_ParticleFire )

LINK_ENTITY_TO_CLASS( env_particlefire, CParticleFire );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------


CParticleFire::CParticleFire()
{
#ifdef _DEBUG
	m_vOrigin.Init();
	m_vDirection.Init();
#endif
}
