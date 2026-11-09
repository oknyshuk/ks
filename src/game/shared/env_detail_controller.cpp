// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#if defined(CLIENT_DLL)
#include "rocketui/rmlui.h"
#elif defined(GAME_DLL)
#endif
#if defined(CLIENT_DLL)
#include "predictable_entity.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "engine/ivmodelinfo.h"
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
#endif
#ifdef CLIENT_DLL
#include "reflect_recvtable.h"
#endif
#ifdef GAME_DLL
#include "reflect_sendtable.h"
#endif

#include "env_detail_controller.h"

IMPLEMENT_NETWORKCLASS_ALIASED( EnvDetailController, DT_DetailController )
LINK_ENTITY_TO_CLASS_ALIASED(env_detail_controller,	EnvDetailController);

IMPLEMENT_REFLECT_TABLE( CEnvDetailController, DT_DetailController );

static CEnvDetailController *s_detailController = nullptr;
CEnvDetailController * GetDetailController()
{
	return s_detailController;
}

CEnvDetailController::CEnvDetailController()
{
	s_detailController = this;
}

CEnvDetailController::~CEnvDetailController()
{
	if ( s_detailController == this )
	{
		s_detailController = nullptr;
	}
}

//--------------------------------------------------------------------------------------------------------------
int CEnvDetailController::UpdateTransmitState_Impl()
{
#ifndef CLIENT_DLL
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
#else
	return 0;
#endif
}

#ifndef CLIENT_DLL

	bool CEnvDetailController::KeyValue_Impl( const char *szKeyName, const char *szValue )
	{
		if (FStrEq(szKeyName, "fademindist"))
		{
			m_flFadeStartDist = std::atof(szValue);
		}
		else if (FStrEq(szKeyName, "fademaxdist"))
		{
			m_flFadeEndDist = std::atof(szValue);
		}

		return true;
	}

#endif // !CLIENT_DLL
