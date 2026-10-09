#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2012 Valve Corporation

inline constexpr auto CUSTOM_EPIDERMIS_MAX_VALID_STYLE = 4;

#include "cs_shareddefs.h"
#include "materialsystem/base_visuals_data_processor.h"

//
// Epidermis specific Visuals Data & Processor
//

struct CCSSEpidermisVisualsData_t 
{
	bool bUsed;

	int nStyle;

	char szDiffuseVTF[ MAX_PATH ];
	char szOrigVMTName[ MAX_PATH ];
	char szNormalVTF[ MAX_PATH ];
	char szExpVTF[ MAX_PATH ];
	char szPaintVTF[ MAX_PATH ];
	char szTatVTF[ MAX_PATH ];

};

class CCSEpidermisVisualsDataCompare : public CBaseVisualsDataCompare
{
public:
	CCSEpidermisVisualsDataCompare() = default;
	CCSEpidermisVisualsDataCompare( CCSEpidermisVisualsDataCompare&& moveFrom ) // = default;
		: CBaseVisualsDataCompare( std::move( moveFrom ) )
		, m_bIsBody( std::move( moveFrom.m_bIsBody ) )
	{
		memcpy( m_pchSkinIdent, moveFrom.m_pchSkinIdent, sizeof( m_pchSkinIdent ) );
	}
	CCSEpidermisVisualsDataCompare& operator=(CCSEpidermisVisualsDataCompare && moveFrom) // = default;
	{
		*( CBaseVisualsDataCompare* )this = std::move( moveFrom );
		m_bIsBody = std::move( moveFrom.m_bIsBody );
		memcpy( m_pchSkinIdent, moveFrom.m_pchSkinIdent, sizeof( m_pchSkinIdent ) );
		return *this;
	}

	virtual void SerializeToBuffer( ks::byte_writer &buf );

	bool m_bIsBody;
	char m_pchSkinIdent[ 16 ];
};

class CCSEpidermisVisualsDataProcessor : public CBaseVisualsDataProcessor< CCSEpidermisVisualsDataCompare >
{
public:
	CCSEpidermisVisualsDataProcessor( CCSEpidermisVisualsDataCompare &&compareObject, const char *pCompositingShaderName = nullptr );

	void SetVisualsData( const char *pCompositingShaderName = nullptr );

	virtual KeyValues *GenerateCustomMaterialKeyValues();
	virtual KeyValues *GenerateCompositeMaterialKeyValues( int nMaterialParamId );
	virtual bool HasCustomMaterial() const { return ( m_visualsData.nStyle > 0 && m_visualsData.nStyle <= CUSTOM_EPIDERMIS_MAX_VALID_STYLE ); }
	virtual const char* GetOriginalMaterialName() const;
	virtual const char* GetOriginalMaterialBaseName() const { return nullptr; }

	int GetStyle() const { return  m_visualsData.nStyle; }

private:
	virtual ~CCSEpidermisVisualsDataProcessor();

	void SetSkinRootIdent();

	CCSSEpidermisVisualsData_t m_visualsData;
	char *m_pCompositingShaderName;
};
