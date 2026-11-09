// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2004 Valve Corporation

#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
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
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "baseentity.h"
//#include "convar.h"
#include "env_dof_controller.h"
#include "ai_utils.h"

LINK_ENTITY_TO_CLASS( env_dof_controller, CEnvDOFController );

IMPLEMENT_REFLECT_DATAMAP( CEnvDOFController )

IMPLEMENT_REFLECT_SERVERCLASS( CEnvDOFController, DT_EnvDOFController )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvDOFController::Spawn_Impl( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvDOFController::Activate_Impl( void )
{
	BaseClass::Activate_Impl();

	// Find our target entity and hold on to it
	m_hFocusTarget = gEntList.FindEntityByName( nullptr, m_strFocusTargetName );

	// Update if we have a focal target
	if ( m_hFocusTarget )
	{
		SetThink( &CEnvDOFController::UpdateParamBlend );
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CEnvDOFController::UpdateTransmitState_Impl()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvDOFController::InputSetNearBlurDepth( inputdata_t &inputdata )
{
	m_flNearBlurDepth = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvDOFController::InputSetNearFocusDepth( inputdata_t &inputdata )
{
	m_flNearFocusDepth = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvDOFController::InputSetFarFocusDepth( inputdata_t &inputdata )
{
	m_flFarFocusDepth = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvDOFController::InputSetFarBlurDepth( inputdata_t &inputdata )
{
	m_flFarBlurDepth = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvDOFController::InputSetNearBlurRadius( inputdata_t &inputdata )
{
	m_flNearBlurRadius = inputdata.value.Float();
	m_bDOFEnabled = ( m_flNearBlurRadius > 0.0f ) || ( m_flFarBlurRadius > 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvDOFController::InputSetFarBlurRadius( inputdata_t &inputdata )
{
	m_flFarBlurRadius = inputdata.value.Float();
	m_bDOFEnabled = ( m_flNearBlurRadius > 0.0f ) || ( m_flFarBlurRadius > 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvDOFController::SetControllerState( DOFControlSettings_t setting )
{
	m_flNearBlurDepth = setting.flNearBlurDepth;
	m_flNearBlurRadius = setting.flNearBlurRadius;
	m_flNearFocusDepth = setting.flNearFocusDistance;
	
	m_flFarBlurDepth = setting.flFarBlurDepth;
	m_flFarBlurRadius = setting.flFarBlurRadius;
	m_flFarFocusDepth = setting.flFarFocusDistance;

	m_bDOFEnabled = ( m_flNearBlurRadius > 0.0f ) || ( m_flFarBlurRadius > 0.0f );
}

const float BLUR_DEPTH = 500.0f;

//-----------------------------------------------------------------------------
// Purpose: Blend the parameters to the specified value
//-----------------------------------------------------------------------------
void CEnvDOFController::UpdateParamBlend( void )
{ 
	// Update our focal target if we have one
	if ( m_hFocusTarget )
	{
		CBasePlayer *pPlayer = AI_GetSinglePlayer();
		float flDistToFocus = ( m_hFocusTarget->GetAbsOrigin() - pPlayer->GetAbsOrigin() ).Length();
		m_flFarFocusDepth.GetForModify() = flDistToFocus + m_flFocusTargetRange;
		m_flFarBlurDepth.GetForModify() = m_flFarFocusDepth + BLUR_DEPTH;
	}

	SetThink( &CEnvDOFController::UpdateParamBlend );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: Set the "focus" target entity
//-----------------------------------------------------------------------------
void CEnvDOFController::InputSetFocusTarget( inputdata_t &inputdata )
{
	m_hFocusTarget = gEntList.FindEntityByName( nullptr, inputdata.value.String() );
	
	// Update if we have a focal target
	if ( m_hFocusTarget )
	{
		SetThink( &CEnvDOFController::UpdateParamBlend );
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the range behind the focus entity that we'll blur (in units)
//-----------------------------------------------------------------------------
void CEnvDOFController::InputSetFocusTargetRange( inputdata_t &inputdata )
{
	m_flFocusTargetRange = inputdata.value.Float();
}
