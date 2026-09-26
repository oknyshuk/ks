//========= Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: Provide interface to custom materials for entities that use them
//
//=============================================================================//
#pragma once 

#include "refcount.h"
#include "materialsystem/icustommaterial.h"

class IMaterial;
class CCustomMaterialOwner;
class CCompositeTexture;

// classes that want to have a custom material should derive from this
class CCustomMaterialOwner
{
public:
	virtual ~CCustomMaterialOwner()	{ ClearCustomMaterials( true ); }
	CCustomMaterialOwner() = default;
	CCustomMaterialOwner( const CCustomMaterialOwner & ) = delete;	// holds references; see DuplicateCustomMaterialsToOther
	CCustomMaterialOwner &operator=( const CCustomMaterialOwner & ) = delete;
	CCustomMaterialOwner( CCustomMaterialOwner && ) = default;
	CCustomMaterialOwner &operator=( CCustomMaterialOwner && ) = default;
	ICustomMaterial *GetCustomMaterial( int nIndex = 0 ) const; // returns NULL if the index is out of range
	virtual void SetCustomMaterial( ICustomMaterial* pCustomMaterial, int nIndex = 0 );	// either replaces and existing material (releasing the old one), or adds one to the vector
	bool IsCustomMaterialValid( int nIndex = 0 ) const;
	int GetCustomMaterialCount() const { return std::ssize( m_ppCustomMaterials ); }
	void ClearCustomMaterials( bool bPurge = false );
	virtual void OnCustomMaterialsUpdated() {}
	virtual void DuplicateCustomMaterialsToOther( CCustomMaterialOwner *pOther ) const;

private:
	// Pointers to custom materials owned by the mat system for this entity. Index
	// in this vector corresponds to the model material index to override with the custom material.
	std::vector< ICustomMaterial* > m_ppCustomMaterials;
};

inline ICustomMaterial *CCustomMaterialOwner::GetCustomMaterial( int nIndex ) const
{ 
	return ( std::ssize( m_ppCustomMaterials ) > nIndex ) ? m_ppCustomMaterials[ nIndex ] : nullptr; 
}

inline void CCustomMaterialOwner::SetCustomMaterial( ICustomMaterial* pCustomMaterial, int nIndex )
{
	while ( std::ssize( m_ppCustomMaterials ) <= nIndex )
	{
		m_ppCustomMaterials.push_back( nullptr );
	}

	pCustomMaterial->AddRef();
	if ( m_ppCustomMaterials[ nIndex ] )
		m_ppCustomMaterials[ nIndex ]->Release();

	m_ppCustomMaterials[ nIndex ] = pCustomMaterial;
}

inline bool CCustomMaterialOwner::IsCustomMaterialValid( int nIndex ) const
{
	return ( std::ssize( m_ppCustomMaterials ) > nIndex && m_ppCustomMaterials[ nIndex ] != nullptr ) ? m_ppCustomMaterials[ nIndex ]->IsValid() : false;
}

inline void CCustomMaterialOwner::ClearCustomMaterials( bool bPurge )
{
	for ( int i = 0; i < std::ssize( m_ppCustomMaterials ); i++ )
	{
		if ( m_ppCustomMaterials[ i ] != nullptr )
		{ 
			m_ppCustomMaterials[ i ]->Release();
			m_ppCustomMaterials[ i ] = nullptr;
		}
	}

	if ( bPurge )
	{
		m_ppCustomMaterials.clear(); m_ppCustomMaterials.shrink_to_fit();
	}
	else
	{
		m_ppCustomMaterials.clear();
	}
}

inline void CCustomMaterialOwner::DuplicateCustomMaterialsToOther( CCustomMaterialOwner *pOther ) const
{
	pOther->ClearCustomMaterials();
	for ( int i = 0; i < std::ssize( CCustomMaterialOwner::m_ppCustomMaterials ); i++ )
	{
		if ( m_ppCustomMaterials[ i ] == nullptr )
			continue;
		
		pOther->SetCustomMaterial( m_ppCustomMaterials[ i ], i );
	}
}
