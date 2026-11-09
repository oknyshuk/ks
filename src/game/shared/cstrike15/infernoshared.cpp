// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
#if defined(CLIENT_DLL)
#include "string_t.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "npcevent.h"
#include "debugoverlay_shared.h"
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

#if defined ( GAME_DLL )
	#include "Effects/inferno.h"
	#define INFERNOCLASS		CInferno
	#define FIRECRACKERBLASTCLASS	CFireCrackerBlast
#endif

#if defined( CLIENT_DLL )
	#include "Effects/clientinferno.h"
	#define INFERNOCLASS		C_Inferno
	#define FIRECRACKERBLASTCLASS	C_FireCrackerBlast
#endif

//---------------------------------------------------------
const char *INFERNOCLASS::GetParticleEffectName()
{
	return "molotov_groundfire";
}
#if defined( GAME_DLL )
const char *INFERNOCLASS::GetImpactParticleEffectName()
{
	return "molotov_explosion";
}
#endif


//---------------------------------------------------------
const char *FIRECRACKERBLASTCLASS::GetParticleEffectName()
{
	return "firework_crate_ground_effect";
}
#if defined( GAME_DLL )
const char *FIRECRACKERBLASTCLASS::GetImpactParticleEffectName()
{
	return "firework_crate_explosion_01";
}
#endif
