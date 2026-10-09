// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
import ks.core;
import std;
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "materialsystem/imaterialproxy.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "c_world.h"

#include "imaterialproxydict.h"

class CWorldDimsProxy : public IMaterialProxy
{
public:
	CWorldDimsProxy();
	virtual ~CWorldDimsProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
	virtual void Release( void ) { delete this; }
	virtual IMaterial *GetMaterial();


public:
	IMaterialVar *m_pMinsVar;
	IMaterialVar *m_pMaxsVar;
};


CWorldDimsProxy::CWorldDimsProxy()
{
	m_pMinsVar = m_pMaxsVar = nullptr;
}

CWorldDimsProxy::~CWorldDimsProxy()
{
}

bool CWorldDimsProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	m_pMinsVar = pMaterial->FindVar( "$world_mins", nullptr, false );
	m_pMaxsVar = pMaterial->FindVar( "$world_maxs", nullptr, false );
	return true;
}

void CWorldDimsProxy::OnBind( void *pC_BaseEntity )
{
	if ( m_pMinsVar && m_pMaxsVar )
	{
		C_World *pWorld = GetClientWorldEntity();
		if ( pWorld )
		{
			m_pMinsVar->SetVecValue( (const float*)&pWorld->m_WorldMins, 3 );
			m_pMaxsVar->SetVecValue( (const float*)&pWorld->m_WorldMaxs, 3 );
		}
	}
}

IMaterial *CWorldDimsProxy::GetMaterial()
{
	return m_pMinsVar->GetOwningMaterial();
}

EXPOSE_MATERIAL_PROXY( CWorldDimsProxy, WorldDims );
