//========= Copyright © 1996-2012, Valve Corporation, All rights reserved. ============//
//
// Purpose: Stub - epidermis skin visualization removed with econ
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "cs_custom_epidermis_visualsdata_processor.h"

void CCSEpidermisVisualsDataCompare::SerializeToBuffer( CUtlBuffer &buf )
{
	CBaseVisualsDataCompare::SerializeToBuffer( buf );
	Serialize( buf, m_bIsBody );
	Serialize( buf, CUtlString( m_pchSkinIdent ) );
}

CCSEpidermisVisualsDataProcessor::CCSEpidermisVisualsDataProcessor( CCSEpidermisVisualsDataCompare &&compareObject, const char *pCompositingShaderName )
	: m_pCompositingShaderName( NULL )
{
	m_compareObject = Move( compareObject );
	m_compareObject.FillCompareBlob();
	memset( &m_visualsData, 0, sizeof( m_visualsData ) );
}

CCSEpidermisVisualsDataProcessor::~CCSEpidermisVisualsDataProcessor()
{
	if ( m_pCompositingShaderName )
	{
		delete [] m_pCompositingShaderName;
		m_pCompositingShaderName = NULL;
	}
}

void CCSEpidermisVisualsDataProcessor::SetVisualsData( const char *pCompositingShaderName )
{
}

void CCSEpidermisVisualsDataProcessor::SetSkinRootIdent()
{
}

KeyValues *CCSEpidermisVisualsDataProcessor::GenerateCustomMaterialKeyValues()
{
	return NULL;
}

KeyValues *CCSEpidermisVisualsDataProcessor::GenerateCompositeMaterialKeyValues( int nMaterialParamId )
{
	return NULL;
}

const char* CCSEpidermisVisualsDataProcessor::GetOriginalMaterialName() const
{
	return "";
}
