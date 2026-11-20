// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

import std;


import ks.core.dbg;
import ks.platform;
import ks.simd;
import ks.tier1.intern_pool;
import ks.tier1.keyvalues;

#include "filesystem.h"

#include "materialsystem_global.h"
#include "materialsystem/imesh.h"

#include "imaterialinternal.h"
#include "imaterialsysteminternal.h"


#include "cmaterialdict.h"

#include "materialsystem_global.h"
#include "filesystem.h"
#include "imaterialinternal.h"

void CMaterialDict::Shutdown( )
{
	// Clean up all materials..
	RemoveAllMaterials();

	// FIXME: Could dump list here...
	m_MissingList.clear();

}

//-----------------------------------------------------------------------------
// Adds/removes the material to the list of all materials
//-----------------------------------------------------------------------------
void CMaterialDict::AddMaterialToMaterialList( IMaterialInternal *pMaterial )
{
	MaterialKey_t lookup;
	lookup.m_Name = ks::global_symbols().intern( pMaterial->GetName() );
	lookup.m_bManuallyCreated = pMaterial->IsManuallyCreated();
	MaterialHandle_t h = AllocMaterialHandle();
	m_MaterialSlots[h] = { pMaterial, m_MaterialDict.emplace( lookup, h ) };
}

MaterialHandle_t CMaterialDict::AllocMaterialHandle()
{
	if ( m_FreeMaterialSlots.empty() )
	{
		MaterialHandle_t h = (MaterialHandle_t)m_MaterialSlots.size();
		if ( h == InvalidMaterial() )
		{
			Error( "CMaterialDict overflow!\n" );
		}
		m_MaterialSlots.emplace_back();
		return h;
	}
	MaterialHandle_t h = m_FreeMaterialSlots.back();
	m_FreeMaterialSlots.pop_back();
	return h;
}

void CMaterialDict::RemoveMaterialFromMaterialList( IMaterialInternal *pMaterial )
{
	// Gotta iterate over this manually; name-based lookup is bogus if there are two
	// materials with the same name, which can happen for procedural materials
	// First remove all the subrect materials, because they'll point at their material pages.
	MaterialHandle_t i;
	MaterialHandle_t iNext = InvalidMaterial();
	for (i = FirstMaterial(); i != InvalidMaterial(); i = iNext )
	{
		iNext = NextMaterial(i);
		if ( m_MaterialSlots[i].m_pMaterial == pMaterial )
		{
			RemoveMaterialFromMaterialList( i );
			break;
		}
	}

	contract_assert( i != InvalidMaterial() );

#ifdef _DEBUG
	for ( i = iNext; i != InvalidMaterial(); i = NextMaterial(i) )
	{
		contract_assert( m_MaterialSlots[i].m_pMaterial != pMaterial );
	}
#endif
}


//-----------------------------------------------------------------------------
// Adds, removes materials
//-----------------------------------------------------------------------------
IMaterialInternal* CMaterialDict::AddMaterial( char const* pName, const char *pTextureGroupName )
{
	IMaterialInternal *pMaterial = IMaterialInternal::CreateMaterial( pName, pTextureGroupName );
	contract_assert( pMaterial && pMaterial->IsRealTimeVersion() );
	AddMaterialToMaterialList( pMaterial );
	return pMaterial;
}

void CMaterialDict::RemoveMaterial( IMaterialInternal* pMaterial )
{
	contract_assert( (pMaterial == nullptr) || pMaterial->IsRealTimeVersion() );
	RemoveMaterialFromMaterialList( pMaterial );
	IMaterialInternal::DestroyMaterial( pMaterial );
}

IMaterialInternal *CMaterialDict::AddMaterialSubRect( const char *pName, const char *pTextureGroupName, KeyValues *pKeyValues, KeyValues *pPatchKeyValues )
{
	IMaterialInternal *pMaterial = IMaterialInternal::CreateMaterialSubRect( pName, pTextureGroupName, pKeyValues, pPatchKeyValues, true );
	contract_assert( pMaterial );
	AddMaterialToMaterialList( pMaterial );
	return pMaterial;
}

void CMaterialDict::RemoveMaterialSubRect( IMaterialInternal *pMaterial )
{
	RemoveMaterialFromMaterialList( pMaterial );
	IMaterialInternal::DestroyMaterialSubRect( pMaterial );
}

void CMaterialDict::RemoveMaterialFromMaterialList( MaterialHandle_t h )
{
	m_MaterialDict.erase( m_MaterialSlots[h].m_It );
	m_MaterialSlots[h] = { nullptr, m_MaterialDict.end() };
	m_FreeMaterialSlots.push_back( h );
}

void CMaterialDict::RemoveAllMaterialsFromMaterialList()
{
	m_MaterialDict.clear();
	m_MaterialSlots.clear();
	m_FreeMaterialSlots.clear();
}

void CMaterialDict::RemoveAllMaterials()
{
	// First remove all the subrect materials, because they'll point at their material pages.
	MaterialHandle_t i, iNext;
	for (i = FirstMaterial(); i != InvalidMaterial(); i = iNext )
	{
		iNext = NextMaterial(i);
		IMaterialInternal *pMaterial = GetMaterialInternal( i );
		if ( pMaterial->InMaterialPage() )
		{
			IMaterialInternal::DestroyMaterialSubRect( pMaterial );
			RemoveMaterialFromMaterialList( i );
		}
	}

	// Now get rid of the rest of the materials, including the pages.
	for (i = FirstMaterial(); i != InvalidMaterial(); i = NextMaterial(i) )
	{
		IMaterialInternal::DestroyMaterial( GetMaterialInternal( i ) );
	}

	RemoveAllMaterialsFromMaterialList();
}
