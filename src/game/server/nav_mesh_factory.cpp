// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// NavMeshFactory.cpp
// Factory to create the NavMesh singleton
// Author: Michael S. Booth, June 2005

import std;
import ks.math;
#include "string_t.h"
#include "vphysics_interface.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "shareddefs.h"
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
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "basetempentity.h"
#include "player.h"
#include "nav_mesh.h"

#ifdef CSTRIKE_DLL
#include "cs_nav_mesh.h"
#endif

CNavMesh *NavMeshFactory( void )
{

#ifdef CSTRIKE_DLL
	return new CSNavMesh;
#endif

	return new CNavMesh;
}
