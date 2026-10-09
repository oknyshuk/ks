#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2013 Valve Corporation

// Purpose: Provide custom texture generation (compositing)

import ks.tier1;
#include "ivisualsdataprocessor.h"

import std;

// derive from and extend this class if you have additional items to compare in your Visuals Data Processor
class CBaseVisualsDataCompare : public IVisualsDataCompare
{
public:
	CBaseVisualsDataCompare() = default;

	CBaseVisualsDataCompare( CBaseVisualsDataCompare&& moveFrom ) // = default;
		: m_nIndex( std::move( moveFrom.m_nIndex ) )
		, m_nSeed( std::move( moveFrom.m_nSeed ) )
		, m_flWear( std::move( moveFrom.m_flWear ) )
		, m_nLOD( std::move( moveFrom.m_nLOD ) )
		, m_nModelID( std::move( moveFrom.m_nModelID ) )
		, m_compareBlob( std::move( moveFrom.m_compareBlob ) )
	{}

	CBaseVisualsDataCompare& operator=( CBaseVisualsDataCompare&& moveFrom ) // = default;
	{
		m_nIndex = std::move( moveFrom.m_nIndex );
		m_nSeed = std::move( moveFrom.m_nSeed );
		m_flWear = std::move( moveFrom.m_flWear );
		m_nLOD = std::move( moveFrom.m_nLOD );
		m_nModelID = std::move( moveFrom.m_nModelID );
		m_compareBlob = std::move( moveFrom.m_compareBlob );
		return *this;
	}

	virtual void FillCompareBlob()
	{
		ks::byte_writer w;
		SerializeToBuffer( w );
		m_compareBlob = w.release();
	}
	virtual const std::vector< std::byte > &GetCompareBlob() const
	{
		return m_compareBlob;
	}

	virtual bool Compare( std::span< const std::byte > other )
	{
		return std::ranges::equal( m_compareBlob, other );
	}

	int m_nIndex;
	int m_nSeed;
	float m_flWear;
	int m_nLOD;
	int m_nModelID;  // for weapons this is CSWeaponID, for clothing this is ClothingDefinitionSlotId_t

protected:
	virtual void SerializeToBuffer( ks::byte_writer &buf )
	{
		buf.clear();
		Serialize( buf, m_nIndex );
		Serialize( buf, m_nSeed );
		Serialize( buf, m_flWear );
		Serialize( buf, m_nLOD );
		Serialize( buf, m_nModelID );
	}

private:
	std::vector< std::byte > m_compareBlob;
};

template< class T >
class CBaseVisualsDataProcessor : public IVisualsDataProcessor
{
public:
	CBaseVisualsDataProcessor()	{}

	virtual bool Compare( std::span< const std::byte > other ) { return GetCompareObject()->Compare( other ); }
	virtual IVisualsDataCompare *GetCompareObject() { return &m_compareObject; }
	virtual const char *GetPatternVTFName() const { return nullptr; }

protected:
	T m_compareObject;
};

enum MaterialParamID_t
{
	MATERIAL_PARAM_ID_BASE_DIFFUSE_TEXTURE = 0,
	MATERIAL_PARAM_ID_PHONG_EXPONENT_TEXTURE,
	MATERIAL_PARAM_ID_BUMP_MAP,
	MATERIAL_PARAM_ID_ANISOTROPY_MAP,
	MATERIAL_PARAM_ID_MASKS1_MAP,

	MATERIAL_PARAM_ID_COUNT
};

extern const char g_szMaterialParamNames[MATERIAL_PARAM_ID_COUNT][32];
