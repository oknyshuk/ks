//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "reflect_predmap.h"
#include "reflect_annotations.h"
#ifdef CLIENT_DLL
#include "reflect_recvtable.h"
#endif
#include "reflect_annotations.h"
#ifdef GAME_DLL
#include "reflect_sendtable.h"
#endif
#include "weapon_csbase.h"
#include "gamerules.h"
#include "npcevent.h"
#include "engine/IEngineSound.h"
#include "weapon_hegrenade.h"


#ifdef CLIENT_DLL
	
#else

	#include "cs_player.h"
	#include "items.h"
	#include "hegrenade_projectile.h"

#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#define GRENADE_TIMER	3.0f //Seconds



IMPLEMENT_NETWORKCLASS_ALIASED( HEGrenade, DT_HEGrenade )

IMPLEMENT_REFLECT_TABLE( CHEGrenade, DT_HEGrenade );

#ifdef CLIENT_DLL
IMPLEMENT_REFLECT_PREDMAP( CHEGrenade );
#endif

LINK_ENTITY_TO_CLASS_ALIASED( weapon_hegrenade, HEGrenade );
PRECACHE_REGISTER( weapon_hegrenade );


#ifndef CLIENT_DLL


	void CHEGrenade::EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo )
	{
		CHEGrenadeProjectile::Create( vecSrc, vecAngles, vecVel, angImpulse, pPlayer, weaponInfo, GRENADE_TIMER );
	}
	
#endif

