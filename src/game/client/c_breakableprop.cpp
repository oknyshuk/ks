// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
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
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "reflect_recvtable.h"
#include "vcollide_parse.h"
#include "bone_setup.h"
#include "engine/ivmodelinfo.h"
#include "physics.h"
#include "c_breakableprop.h"

void RecvProxy_UnmodifiedQAngles( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	const float *v = pData->m_Value.m_Vector;

	((float*)pOut)[0] = v[0];
	((float*)pOut)[1] = v[1];
	((float*)pOut)[2] = v[2];
}

IMPLEMENT_REFLECT_CLIENTCLASS( C_BreakableProp, DT_BreakableProp, CBreakableProp )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_BreakableProp::C_BreakableProp( void )
{
	m_takedamage = DAMAGE_YES;
}


//-----------------------------------------------------------------------------
// Copy fade from another breakable prop
//-----------------------------------------------------------------------------
void C_BreakableProp::CopyFadeFrom( C_BreakableProp *pSource )
{
	SetGlobalFadeScale( pSource->GetGlobalFadeScale() );
	SetDistanceFade( pSource->GetMinFadeDist(), pSource->GetMaxFadeDist() );
}

void C_BreakableProp::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );
	if ( m_bClientPhysics )
	{
		bool bCreate = (type == DATA_UPDATE_CREATED) ? true : false;
		VPhysicsShadowDataChanged(bCreate, this);
	}
}

//IPlayerPickupVPhysics
bool C_BreakableProp::HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer )
{
	return (m_qPreferredPlayerCarryAngles.x < std::numeric_limits<float>::max());
}

QAngle C_BreakableProp::PreferredCarryAngles( void )
{
	return (m_qPreferredPlayerCarryAngles.x < std::numeric_limits<float>::max()) ? m_qPreferredPlayerCarryAngles : vec3_angle;
}


bool C_BreakableProp::ShouldPredict( void )
{
	return false;
}

C_BasePlayer *C_BreakableProp::GetPredictionOwner( void )
{
	return nullptr;
}
