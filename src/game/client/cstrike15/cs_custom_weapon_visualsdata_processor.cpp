// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2012 Valve Corporation

// Purpose: Stub - weapon skin visualization removed with econ

import std;
import ks.tier1;
#include "cdll_util.h"
#include "cdll_client_int.h"
#include "cs_custom_weapon_visualsdata_processor.h"

void CCSWeaponVisualsDataCompare::SerializeToBuffer( ks::byte_writer &buf )
{
	CBaseVisualsDataCompare::SerializeToBuffer( buf );
	Serialize( buf, m_flWeaponLength );
	Serialize( buf, m_flUVScale );
}

CCSWeaponVisualsDataProcessor::CCSWeaponVisualsDataProcessor( CCSWeaponVisualsDataCompare &&compareObject, const char *pCompositingShaderName )
	: m_pCompositingShaderName( nullptr )
	, m_bIgnoreWeaponSizeScale( false )
	, m_flPhongAlbedoFactor( 1.0f )
	, m_nPhongIntensity( 0 )
{
	m_compareObject = std::move( compareObject );
	m_compareObject.FillCompareBlob();
	std::memset( &m_visualsData, 0, sizeof( m_visualsData ) );
}

CCSWeaponVisualsDataProcessor::~CCSWeaponVisualsDataProcessor()
{
	if ( m_pCompositingShaderName )
	{
		delete [] m_pCompositingShaderName;
		m_pCompositingShaderName = nullptr;
	}
}

void CCSWeaponVisualsDataProcessor::Refresh()
{
}

void CCSWeaponVisualsDataProcessor::SetVisualsData( const char *pCompositingShaderName )
{
}

KeyValues* CCSWeaponVisualsDataProcessor::GenerateCustomMaterialKeyValues()
{
	return nullptr;
}

KeyValues* CCSWeaponVisualsDataProcessor::GenerateCompositeMaterialKeyValues( int nMaterialParamId )
{
	return nullptr;
}

bool CCSWeaponVisualsDataProcessor::HasCustomMaterial() const
{
	return false;
}

const char* CCSWeaponVisualsDataProcessor::GetOriginalMaterialName() const
{
	return "";
}

const char* CCSWeaponVisualsDataProcessor::GetOriginalMaterialBaseName() const
{
	return "";
}
