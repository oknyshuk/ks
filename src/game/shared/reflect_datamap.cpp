// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

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
#include "gamerules.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "player.h"
#include "ndebugoverlay.h"
#endif
#include "reflect_datamap.h"

namespace ksr::dmap
{
using namespace ::ks::reflect;

typedescription_t make_field( const FieldDesc &d, bool output )
{
	FieldDesc f = d;
#ifdef GAME_DLL
	// eventFuncs is entityoutput.h's, and is_output is always false on the client.
	if ( output ) f.ops = eventFuncs;
#else
	( void )output;
#endif
	return MakeField( f );
}

} // namespace ksr::dmap
