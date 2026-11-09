// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2013 Valve Corporation


import ks.tier1;
#include "materialsystem/imaterial.h"
#include "custom_material.h"
#include "composite_texture.h"
#include "materialsystem/base_visuals_data_processor.h"
#include "materialsystem_global.h"
import std;

#ifndef DEDICATED
#include "filesystem.h"
#endif

//#define DEBUG_CUSTOMMATERIALS

int CCustomMaterial::m_nMaterialCount = 0;

//
// Material applied to the item to give it a custom look
//

// ICustomMaterial: non-virtual facade over CCustomMaterial
int ICustomMaterial::AddRef() { return static_cast< CCustomMaterial * >( this )->AddRef(); }
int ICustomMaterial::Release() { return static_cast< CCustomMaterial * >( this )->Release(); }
IMaterial *ICustomMaterial::GetMaterial() { return static_cast< CCustomMaterial * >( this )->GetMaterial(); }
void ICustomMaterial::AddTexture( ICompositeTexture *pTexture ) { static_cast< CCustomMaterial * >( this )->AddTexture( pTexture ); }
ICompositeTexture *ICustomMaterial::GetTexture( int nIndex ) { return static_cast< CCustomMaterial * >( this )->GetTexture( nIndex ); }
bool ICustomMaterial::IsValid() const { return static_cast< const CCustomMaterial * >( this )->IsValid(); }
bool ICustomMaterial::CheckRegenerate( int nSize ) { return static_cast< CCustomMaterial * >( this )->CheckRegenerate( nSize ); }
const char *ICustomMaterial::GetBaseMaterialName( void ) { return static_cast< CCustomMaterial * >( this )->GetBaseMaterialName(); }

CCustomMaterial::CCustomMaterial( KeyValues *pKeyValues )
	: m_bValid( false )
	, m_nModelMaterialIndex( -1 )
	, m_szBaseMaterialName( nullptr)
{
	// we need to copy this, because the passed in one was allocated outside materialsystem.dll
	m_pVMTKeyValues = ( pKeyValues != nullptr ) ? pKeyValues->MakeCopy() : nullptr;
}

CCustomMaterial::~CCustomMaterial()
{
	Shutdown();
}

void CCustomMaterial::AddTexture( ICompositeTexture * pTextureInterface )
{
	CCompositeTexture *pTexture = static_cast< CCompositeTexture * >( pTextureInterface );
	if ( pTexture )
	{
		m_pTextures.push_back( pTexture );
		pTexture->AddRef();
	}
}

ICompositeTexture *CCustomMaterial::GetTexture( int nIndex )
{
	if ( nIndex >= 0 && nIndex < std::ssize( m_pTextures ) )
	{
		return m_pTextures[ nIndex ];
	}
	return nullptr;
}

bool CCustomMaterial::CheckRegenerate( int nSize )
{
	if ( !m_pTextures.empty() )
	{
		return nSize != ( 2048 >> m_pTextures[ 0 ]->Size() );
	}

	return false;
}

void CCustomMaterial::SetBaseMaterialName( const char* szName )
{
	contract_assert( !m_szBaseMaterialName );
	m_szBaseMaterialName = __builtin_strdup( szName );
}

void CCustomMaterial::Shutdown()
{
	DestroyProceduralMaterial();

	if ( m_pVMTKeyValues != nullptr )
	{
		m_pVMTKeyValues->deleteThis();
		m_pVMTKeyValues = nullptr;
	}

	for ( int i = 0; i < std::ssize( m_pTextures ); i++ )
	{
		if ( m_pTextures[ i ] )
		{
			m_pTextures[ i ]->Release();
			m_pTextures[ i ] = nullptr;
		}
	}
	if ( m_szBaseMaterialName )
		std::free( ( void * ) m_szBaseMaterialName );

	m_pTextures.clear();
}

void CCustomMaterial::Usage( int& nTextures, int& nBackingTextures )
{
	for ( int i = 0; i < std::ssize( m_pTextures ); i++ )
	{
		m_pTextures[ i ]->Usage( nTextures, nBackingTextures );
	}
}

bool CCustomMaterial::TexturesReady() const
{
	if ( m_pTextures.empty() )
	{
		return false;
	}

	for ( int i = 0; i < std::ssize( m_pTextures ); i++ )
	{
		if ( !m_pTextures[ i ]->IsReady() )
		{
			return false;
		}
	}

	return true;
}

void CCustomMaterial::RegenerateTextures()
{
	for ( int i = 0; i < std::ssize( m_pTextures ); i++ )
	{
		m_pTextures[ i ]->ForceRegenerate();
	}
}

bool CCustomMaterial::ShouldRelease()
{
	return ( ( GetRefCount() == 1 ) && IsValid() );
}

bool CCustomMaterial::Compare( const std::vector< SCompositeTextureInfo > &vecTextures )
{
	if ( std::ssize( m_pTextures ) == std::ssize( vecTextures ) )
	{
		for ( int i = 0; i < std::ssize( m_pTextures ); i++ )
		{
			if ( !m_pTextures[ i ]->Compare( vecTextures[ i ] ) )
			{
				return false;
			}
		}
		return true;
	}

	return false;
}

void CCustomMaterial::Finalize()
{
	char szUniqueMaterialName[ 256 ];
	V_snprintf( szUniqueMaterialName, sizeof( szUniqueMaterialName ), "cs_custom_material_%i", m_nMaterialCount++ );

	DestroyProceduralMaterial();

	if ( m_pVMTKeyValues != nullptr )
	{
		CreateProceduralMaterial( szUniqueMaterialName, m_pVMTKeyValues->MakeCopy() );
	}
	else
	{
		// Create default material key values
		m_pVMTKeyValues = new KeyValues( "VertexLitGeneric" );

		m_pVMTKeyValues->SetInt( "$phongalbedoboost", 45 );
		m_pVMTKeyValues->SetInt( "$phong", 1 );
		m_pVMTKeyValues->SetFloat( "$phongboost", 0.4 );
		m_pVMTKeyValues->SetString( "$phongfresnelranges", "[.8 .8 1]" );
		m_pVMTKeyValues->SetInt( "$basemapalphaphongmask", 1 );
		m_pVMTKeyValues->SetString( "$envmap", "env_cubemap" );
		m_pVMTKeyValues->SetInt( "$envmapfresnel", 1 );
		m_pVMTKeyValues->SetString( "$envmapFresnelMinMaxExp", "[0 5 .4]" );
		m_pVMTKeyValues->SetString( "$envmaptint", "[.02 .02 .02]" );
		m_pVMTKeyValues->SetInt( "$phongalbedotint", 1 );

		CreateProceduralMaterial( szUniqueMaterialName, m_pVMTKeyValues->MakeCopy() );
	}
}

void CCustomMaterial::CreateProceduralMaterial( const char *pMaterialName, KeyValues *pVMTKeyValues )
{
	// Replace parts of existing material key values
	// loop over m_pTextures and set the material params
	for ( int i = 0; i < std::ssize( m_pTextures ); i++ )
	{
		pVMTKeyValues->SetString( g_szMaterialParamNames[ m_pTextures[ i ]->GetMaterialParamNameId() ], m_pTextures[ i ]->GetName() );
	}

	m_Material.Init( pMaterialName, pVMTKeyValues );
	m_Material->Refresh();
}

void CCustomMaterial::DestroyProceduralMaterial()
{
	m_Material.Shutdown( true );
}


//
// global custom material manager
//  the game uses this to make/get a custom material
//

CCustomMaterialManager::CCustomMaterialManager()
{
#ifndef DEDICATED
	m_pCustomMaterials.reserve( 128 );
#endif
}

CCustomMaterialManager::~CCustomMaterialManager()
{	
}

// this is called at the end of each frame
bool ProcessDynamicCustomMaterialGenerator()
{
	return MaterialSystem()->GetCustomMaterialManager()->Process();
}

bool CCustomMaterialManager::Init()
{
#ifndef DEDICATED
	MaterialSystem()->AddEndFramePriorToNextContextFunc( ::ProcessDynamicCustomMaterialGenerator );

	KeyValues *pVMTCache = new KeyValues( "VMTCache" );
	if ( pVMTCache->LoadFromFile( g_pFullFileSystem, "resource/vmtcache.txt", "MOD" ) )
	{
		KeyValues *pValue = pVMTCache->GetFirstValue();
		while ( pValue )
		{
			const char *pszVMTToCache = pValue->GetString();
			if ( pszVMTToCache && pszVMTToCache[0] != 0 )
			{
				auto pVMTKeyValues = std::make_unique< KeyValues >( "VertexLitGeneric" );
				bool bVMTExists = pVMTKeyValues->LoadFromFile( g_pFullFileSystem , pszVMTToCache, "MOD" );
				if ( bVMTExists )
				{
					// A repeated entry keeps the first copy.
					m_mapVMTKeyValues.try_emplace( pszVMTToCache, std::move( pVMTKeyValues ) );
					DevMsg( "CustomMaterialManager: Cached KeyValues %s.\n", pszVMTToCache );
				}
				else
				{
					DevMsg( "Failed to load VMT: %s\n", pszVMTToCache );
				}
			}
			pValue = pValue->GetNextValue();
		}
	}
#endif
	return true;
}

void CCustomMaterialManager::Shutdown()
{
#ifndef DEDICATED
	MaterialSystem()->RemoveEndFramePriorToNextContextFunc( ::ProcessDynamicCustomMaterialGenerator );
	m_mapVMTKeyValues.clear();
	DestroyMaterials();
#endif
}

bool CCustomMaterialManager::GetVMTKeyValues( const char *pszVMTName, KeyValues **ppVMTKeyValues )
{
	// lookup VMT KeyValues in container
	auto it = m_mapVMTKeyValues.find( pszVMTName );
	if ( it != m_mapVMTKeyValues.end() )
	{
		// need to return a copy here, since we don't want the caller to change our copy.
		*ppVMTKeyValues = it->second->MakeCopy();
		return true;
	}
	return false;
}

// handles finalizing materials once all the textures are ready, swapping materials that are pending swap and ready, and cleans up materials that are no longer used
bool CCustomMaterialManager::Process()
{

	for ( int i = 0; i < std::ssize( m_pCustomMaterials ); ++i )
	{
		if ( m_pCustomMaterials[ i ] &&
			 m_pCustomMaterials[ i ]->TexturesReady() &&
			 !m_pCustomMaterials[ i ]->IsValid() )
		{
			m_pCustomMaterials[ i ]->Finalize();
			m_pCustomMaterials[ i ]->SetValid( true );
		}
		else if ( !m_pCustomMaterials[ i ]->TexturesReady() &&
				  m_pCustomMaterials[ i ]->IsValid() )
		{
			// this happens when textures regenerate because of mat_picmip changes
			m_pCustomMaterials[ i ]->SetValid( false );
		}
	}

	for ( int i = std::ssize( m_pCustomMaterials ) - 1; i >= 0; i-- )
	{
		if ( m_pCustomMaterials[ i ] )
		{
			// clean up materials that are no longer used (we are the only reference)
			if ( m_pCustomMaterials[ i ]->ShouldRelease() )
			{
				m_pCustomMaterials[ i ]->Release();
				m_pCustomMaterials[ i ] = nullptr;
				m_pCustomMaterials.erase( m_pCustomMaterials.begin() + i );
			}
		}
	}

	return false;
}

ICustomMaterial * CCustomMaterialManager::GetOrCreateCustomMaterial( KeyValues *pKeyValues, const std::vector< SCompositeTextureInfo > &vecTextureInfos, bool bIgnorePicMip /*= false */ )
{
#if defined( DEDICATED ) || defined( DISABLE_CUSTOM_MATERIAL_GENERATION )
	return nullptr;
#endif

	for ( int i = 0; i < std::ssize( m_pCustomMaterials ); ++i )
	{
		CCustomMaterial *pMaterial = m_pCustomMaterials[ i ];
		if ( pMaterial && pMaterial->Compare( vecTextureInfos ) )
		{
			return pMaterial;
		}
	}

	CCustomMaterial *pMaterial = new CCustomMaterial( pKeyValues );
	pMaterial->SetValid( false );
	pMaterial->SetBaseMaterialName( vecTextureInfos[ 0 ].m_pVisualsDataProcessor->GetOriginalMaterialBaseName() );

	for ( const SCompositeTextureInfo &textureInfo : vecTextureInfos )
	{
		ICompositeTexture *pTexture = g_pMaterialSystem->GetCompositeTextureGenerator()->GetCompositeTexture( textureInfo );
		if ( pTexture )
		{
			pMaterial->AddTexture( pTexture );
		}
		else
		{
			contract_assert( ( pTexture != nullptr ) && "Unable to get/create composite texture for custom material!" );
			pMaterial->Release();
			return nullptr;
		}
	}

	m_pCustomMaterials.push_back( pMaterial );

	// The material may not be complete yet, but it will be completed over the next few frames via Process()
	return pMaterial;
}

void CCustomMaterialManager::ReloadAllMaterials( const CCommand &args )
{
	for ( int i = 0; i < std::ssize( m_pCustomMaterials ); ++i )
	{
		CCustomMaterial *pMaterial = m_pCustomMaterials[ i ];
		if ( pMaterial )
		{
			pMaterial->RegenerateTextures();
		}
	}
}

void CCustomMaterialManager::ReloadVmtCache( const CCommand &args )
{
}

int CCustomMaterialManager::DebugGetNumActiveCustomMaterials( )
{
	int nActive = 0;

	for ( int i = 0; i < std::ssize( m_pCustomMaterials ); ++i )
	{
		CCustomMaterial *pMaterial = m_pCustomMaterials[ i ];

		if ( pMaterial && pMaterial->IsValid() )
		{
			nActive++;
		}
	}
	return nActive;
}

void CCustomMaterialManager::Usage( const CCommand &args )
{
	int nTextures = 0;
	int nBackingTextures = 0;
	int nActive = 0;

	for ( int i = 0; i < std::ssize( m_pCustomMaterials ); ++i )
	{
		CCustomMaterial *pMaterial = m_pCustomMaterials[ i ];

		if ( pMaterial && pMaterial->IsValid() )
		{
			int nBackingTexture = 0;
			pMaterial->Usage( nTextures, nBackingTexture );
			Msg( "%2d. %s, %d \n", i, pMaterial->GetMaterial() ? pMaterial->GetMaterial()->GetName() : "*pending*", nBackingTexture );
			nBackingTextures += nBackingTexture;
			nActive++;
		}
	}

	Msg( "Custom Weapon Material Usage: Total: %d Active: %d  Textures: %d  BackingTextures: %d \n", (int)std::ssize( m_pCustomMaterials ), nActive, nTextures, nBackingTextures );
}

void CCustomMaterialManager::DestroyMaterials( void )
{
	for ( int i = 0; i < std::ssize( m_pCustomMaterials ); ++i )
	{
		if ( m_pCustomMaterials[ i ] )
		{
			m_pCustomMaterials[ i ]->Release();
			m_pCustomMaterials[ i ] = nullptr;
		}
	}
	m_pCustomMaterials.clear();
}
