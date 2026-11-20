#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 2005 Valve Corporation

import ks.platform;
import ks.simd;
import ks.tier1.keyvalues;
import std;

#include "imaterialinternal.h"

//-----------------------------------------------------------------------------

class IMaterial;
class IMaterialInternal;
typedef unsigned short MaterialHandle_t;

//-----------------------------------------------------------------------------
// Dictionary of all known materials
//-----------------------------------------------------------------------------

class CMaterialDict
{
public:
	CMaterialDict()
	{
	}

	void Shutdown();

	int					GetNumMaterials( void ) const;

	IMaterial*			GetMaterial( MaterialHandle_t h ) const;
	IMaterialInternal * GetMaterialInternal( MaterialHandle_t idx ) const;

	MaterialHandle_t	FirstMaterial() const;
	MaterialHandle_t	NextMaterial( MaterialHandle_t h ) const;
	MaterialHandle_t	InvalidMaterial() const;

	IMaterialInternal *	FindMaterial( const char *pszName, bool bManuallyCreated ) const;

	void				AddMaterialToMaterialList( IMaterialInternal *pMaterial );
	void				RemoveMaterialFromMaterialList( IMaterialInternal *pMaterial );

	void				RemoveMaterial( IMaterialInternal *pMaterial );
	void				RemoveMaterialSubRect( IMaterialInternal *pMaterial );

	IMaterialInternal*	AddMaterial( const char* pName, const char *pTextureGroupName );
	// pKeyValues and pPatchKeyValues should come from LoadVMTFile()
	IMaterialInternal*  AddMaterialSubRect( const char* pName, const char *pTextureGroupName, KeyValues *pKeyValues, KeyValues *pPatchKeyValues ); 

	bool				NoteMissing( const char *pszName );
	bool				IsMissing( const char *pszName );

protected: /*private:*/
	void				RemoveAllMaterials();
	void				RemoveAllMaterialsFromMaterialList();
	void				RemoveMaterialFromMaterialList( MaterialHandle_t h );


	MaterialHandle_t	AllocMaterialHandle();

	// Stores a dictionary of materials, searched by name
	struct MaterialKey_t
	{
		bool m_bManuallyCreated;
		std::string_view m_Name;	// in ks::global_symbols() for stored keys
	};

	struct MaterialKeyLess
	{
		bool operator()( const MaterialKey_t& src1, const MaterialKey_t& src2 ) const
		{
			// Always sort manually-created materials to the front
			if ( src1.m_bManuallyCreated != src2.m_bManuallyCreated )
				return src1.m_bManuallyCreated;

			return src1.m_Name < src2.m_Name;
		}
	};

	// Names can repeat (procedural materials).
	typedef std::multimap< MaterialKey_t, MaterialHandle_t, MaterialKeyLess > MaterialDict_t;

	// A MaterialHandle_t indexes m_MaterialSlots and stays valid until its material is removed.
	struct MaterialSlot_t
	{
		IMaterialInternal* m_pMaterial;
		MaterialDict_t::iterator m_It;
	};

	MaterialDict_t m_MaterialDict;
	std::vector< MaterialSlot_t > m_MaterialSlots;
	std::vector< MaterialHandle_t > m_FreeMaterialSlots;

	// Stores a dictionary of missing materials to cut down on redundant warning messages
	// TODO:  1) Could add a counter
	//        2) Could dump to file/console at exit for exact list of missing materials
	std::set< std::string, std::less<> > m_MissingList;
};

//-----------------------------------------------------------------------------
// Material iteration methods
//-----------------------------------------------------------------------------
inline MaterialHandle_t CMaterialDict::FirstMaterial() const
{
	return m_MaterialDict.empty() ? InvalidMaterial() : m_MaterialDict.begin()->second;
}

inline MaterialHandle_t CMaterialDict::NextMaterial( MaterialHandle_t h ) const
{
	auto it = std::next( m_MaterialSlots[h].m_It );
	return it == m_MaterialDict.end() ? InvalidMaterial() : it->second;
}

inline int CMaterialDict::GetNumMaterials( )	const
{
	return (int)m_MaterialDict.size();
}


//-----------------------------------------------------------------------------
// Invalid index handle....
//-----------------------------------------------------------------------------
inline MaterialHandle_t CMaterialDict::InvalidMaterial() const
{
	return (MaterialHandle_t)~0;
}


//-----------------------------------------------------------------------------
// Handle to material
//-----------------------------------------------------------------------------
inline IMaterial* CMaterialDict::GetMaterial( MaterialHandle_t idx ) const
{
	return m_MaterialSlots[idx].m_pMaterial;
}

inline IMaterialInternal* CMaterialDict::GetMaterialInternal( MaterialHandle_t idx ) const
{
	contract_assert( (m_MaterialSlots[idx].m_pMaterial == nullptr) || m_MaterialSlots[idx].m_pMaterial->IsRealTimeVersion() );	
	return m_MaterialSlots[idx].m_pMaterial;
}

inline IMaterialInternal* CMaterialDict::FindMaterial( const char *pszName, bool bManuallyCreated ) const
{
	MaterialKey_t lookup;
	lookup.m_Name = pszName;
	lookup.m_bManuallyCreated = bManuallyCreated;	// This causes the search to find only file-created materials

	auto it = m_MaterialDict.find( lookup );

	if ( it != m_MaterialDict.end() )
	{
		return m_MaterialSlots[it->second].m_pMaterial;
	}

	return nullptr;
}


inline bool CMaterialDict::IsMissing( const char *pszName )
{
	return m_MissingList.contains( std::string_view( pszName ) );
}

inline bool CMaterialDict::NoteMissing( const char *pszName )
{
	return m_MissingList.emplace( pszName ).second;
}

//-----------------------------------------------------------------------------
