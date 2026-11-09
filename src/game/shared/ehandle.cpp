// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: This module implements functions to support ehandles.

#if defined(CLIENT_DLL)
#elif defined(GAME_DLL)
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
#include "entitylist.h"
#include "ndebugoverlay.h"
#endif

#if defined( GAME_DLL )
	
	#include "entitylist.h"

extern bool g_bDisableEhandleAccess;
	void DebugCheckEHandleAccess( void *pEnt )
	{

		if ( g_bDisableEhandleAccess )
		{
			Msg( "Access of EHANDLE/CHandle for class %s:%p in destructor!\n",
				STRING(((CBaseEntity*)pEnt)->m_iClassname ), pEnt );
		}
	}

#else
	
#endif
