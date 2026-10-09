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
#include "proxyentity.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imaterial.h"
#include "view.h"

#include "imaterialproxydict.h"

//-----------------------------------------------------------------------------
// Purpose: Used for halos on lamps, this material fades the sprite IN
// as the viewer nears.
//-----------------------------------------------------------------------------
class CLampHaloProxy : public CEntityMaterialProxy
{
public:
						CLampHaloProxy( void );
	virtual				~CLampHaloProxy( void );
	virtual bool		Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void		OnBind( C_BaseEntity *pC_BaseEntity );

	virtual IMaterial *	GetMaterial();

private:
	IMaterialVar		*m_pFadeValue;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CLampHaloProxy::CLampHaloProxy( void )
{
	m_pFadeValue = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CLampHaloProxy::~CLampHaloProxy( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Get pointer to the color value
// Input  : *pMaterial - 
//-----------------------------------------------------------------------------
bool CLampHaloProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	contract_assert( pMaterial );

	// Get pointers to material vars.

	// Need to get the color variable.
	bool found;
	m_pFadeValue = pMaterial->FindVar( "$alpha", &found );
	pMaterial->SetMaterialVarFlag( MATERIAL_VAR_ALPHA_MODIFIED_BY_PROXY, true );
	return found;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pC_BaseEntity - 
//-----------------------------------------------------------------------------
constexpr auto FADE_DIST = 150;

void CLampHaloProxy::OnBind( C_BaseEntity *pEnt )
{
	if ( !m_pFadeValue )
		return;
	
	Vector vecLocal = pEnt->GetAbsOrigin() - CurrentViewOrigin();
	VectorNormalize( vecLocal );

	float fade = fabs( vecLocal.z );

	// I hate these magic numbers here, will have to revise
	// (sjb)
	if( fade < 0.25 )
	{
		fade = 0.0;
	}
	else
	{
		fade = std::min<std::common_type_t<double, float>>( (fade - 0.25) * 1.35, 1.0f );
	}

	m_pFadeValue->SetFloatValue( fade );
}

IMaterial *CLampHaloProxy::GetMaterial()
{
	if ( !m_pFadeValue )
		return nullptr;

	return m_pFadeValue->GetOwningMaterial();
}

EXPOSE_MATERIAL_PROXY( CLampHaloProxy, lamphalo );
