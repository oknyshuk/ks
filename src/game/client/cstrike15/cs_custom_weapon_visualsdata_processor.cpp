//========= Copyright © 1996-2012, Valve Corporation, All rights reserved. ============//
//
// Purpose: Stub - weapon skin visualization removed with econ
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "utlbufferutil.h"
#include "cs_custom_weapon_visualsdata_processor.h"

void CCSWeaponVisualsDataCompare::SerializeToBuffer( CUtlBuffer &buf )
{
	CBaseVisualsDataCompare::SerializeToBuffer( buf );
	Serialize( buf, m_flWeaponLength );
	Serialize( buf, m_flUVScale );
}

CCSWeaponVisualsDataProcessor::CCSWeaponVisualsDataProcessor( CCSWeaponVisualsDataCompare &&compareObject, const char *pCompositingShaderName )
	: m_pCompositingShaderName( NULL )
	, m_bIgnoreWeaponSizeScale( false )
	, m_flPhongAlbedoFactor( 1.0f )
	, m_nPhongIntensity( 0 )
{
	m_compareObject = Move( compareObject );
	m_compareObject.FillCompareBlob();
	memset( &m_visualsData, 0, sizeof( m_visualsData ) );
}

CCSWeaponVisualsDataProcessor::~CCSWeaponVisualsDataProcessor()
{
	if ( m_pCompositingShaderName )
	{
		delete [] m_pCompositingShaderName;
		m_pCompositingShaderName = NULL;
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
	return NULL;
}

KeyValues* CCSWeaponVisualsDataProcessor::GenerateCompositeMaterialKeyValues( int nMaterialParamId )
{
	return NULL;
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
