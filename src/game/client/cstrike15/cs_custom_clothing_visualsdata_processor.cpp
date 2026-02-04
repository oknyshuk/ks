//=========== Copyright © Valve Corporation, All rights reserved. =============//
//
// Purpose: Stub - clothing skin visualization removed with econ
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "utlbufferutil.h"
#include "cs_custom_clothing_visualsdata_processor.h"

void CCSClothingVisualsDataCompare::SerializeToBuffer( CUtlBuffer &buf )
{
	CBaseVisualsDataCompare::SerializeToBuffer( buf );
	Serialize( buf, m_nTeamId );
	Serialize( buf, m_bMirrorPattern );
	Serialize( buf, m_nMaterialId );
}

CCSClothingVisualsDataProcessor::CCSClothingVisualsDataProcessor( CCSClothingVisualsDataCompare &&compareObject, const char *szCompositingShaderName )
	: m_szCompositingShaderName( NULL )
{
	m_compareObject = Move( compareObject );
	m_compareObject.FillCompareBlob();
	memset( &m_visualsData, 0, sizeof( m_visualsData ) );
}

CCSClothingVisualsDataProcessor::~CCSClothingVisualsDataProcessor()
{
	if ( m_szCompositingShaderName )
	{
		delete [] m_szCompositingShaderName;
		m_szCompositingShaderName = NULL;
	}
}

void CCSClothingVisualsDataProcessor::Refresh()
{
}

void CCSClothingVisualsDataProcessor::SetVisualsData( const char *pCompositingShaderName )
{
}

KeyValues *CCSClothingVisualsDataProcessor::GenerateCustomMaterialKeyValues()
{
	return NULL;
}

KeyValues *CCSClothingVisualsDataProcessor::GenerateCompositeMaterialKeyValues( int nMaterialParamId )
{
	return NULL;
}

bool CCSClothingVisualsDataProcessor::HasCustomMaterial() const
{
	return false;
}

const char* CCSClothingVisualsDataProcessor::GetSkinMaterialName() const
{
	return "";
}

const char* CCSClothingVisualsDataProcessor::GetOriginalMaterialName() const
{
	return "";
}

const char* CCSClothingVisualsDataProcessor::GetOriginalMaterialBaseName() const
{
	return "";
}
