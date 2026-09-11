//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
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
#include "weapon_smokegrenade.h"


#ifdef CLIENT_DLL
	
#else

	#include "cs_player.h"
	#include "items.h"
	#include "smokegrenade_projectile.h"

#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( SmokeGrenade, DT_SmokeGrenade )

IMPLEMENT_REFLECT_TABLE( CSmokeGrenade, DT_SmokeGrenade );

#ifdef CLIENT_DLL
IMPLEMENT_REFLECT_PREDMAP( CSmokeGrenade );
#endif

LINK_ENTITY_TO_CLASS_ALIASED( weapon_smokegrenade, SmokeGrenade );
PRECACHE_REGISTER( weapon_smokegrenade );


#ifndef CLIENT_DLL


	void CSmokeGrenade::EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo )
	{
		CSmokeGrenadeProjectile::Create( vecSrc, vecAngles, vecVel, angImpulse, pPlayer, weaponInfo );
	}

#endif

