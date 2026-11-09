// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
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
class CProxyHealth : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	CFloatInput	m_Factor;
};

bool CProxyHealth::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_Factor.Init( pMaterial, pKeyValues, "scale", 1 ))
		return false;

	return true;
}

void CProxyHealth::OnBind( void *pC_BaseEntity )
{
	if (!pC_BaseEntity)
		return;

	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );

	contract_assert( m_pResult );
	SetFloatResult( pEntity->HealthFraction() * m_Factor.GetFloat() );
}

EXPOSE_MATERIAL_PROXY( CProxyHealth, Health );
