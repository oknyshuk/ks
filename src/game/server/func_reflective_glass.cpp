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
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "modelentities.h"

class [[= ks::reflect::NetTable{ .name = "DT_FuncReflectiveGlass" } ]]
      CFuncReflectiveGlass : public CFuncBrush
{
	DECLARE_CLASS( CFuncReflectiveGlass, CFuncBrush );
	DECLARE_SERVERCLASS();
};

// automatically hooks in the system's callbacks

LINK_ENTITY_TO_CLASS( func_reflective_glass, CFuncReflectiveGlass );

IMPLEMENT_REFLECT_SERVERCLASS( CFuncReflectiveGlass, DT_FuncReflectiveGlass )
