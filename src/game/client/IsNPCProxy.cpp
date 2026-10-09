// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2007 Valve Corporation

#include "rocketui/rmlui.h"
import ks.core;
import std;
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "functionproxy.h"

#include "imaterialproxydict.h"

//-----------------------------------------------------------------------------
// Returns the player health (from 0 to 1)
//-----------------------------------------------------------------------------
class CProxyIsNPC : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	CFloatInput	m_Factor;
};

bool CProxyIsNPC::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_Factor.Init( pMaterial, pKeyValues, "scale", 1 ))
		return false;

	return true;
}

void CProxyIsNPC::OnBind( void *pC_BaseEntity )
{
	if ( !pC_BaseEntity )
		return;

	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	if ( pEntity && pEntity->IsNPC() )
	{
		SetFloatResult( m_Factor.GetFloat() );
	}
	else
	{
		SetFloatResult( 0.0f );
	}
}

EXPOSE_MATERIAL_PROXY( CProxyIsNPC, IsNPC );
