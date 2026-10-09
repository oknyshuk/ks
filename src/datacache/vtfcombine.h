#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2011 Valve Corporation

import ks.tier1;
#include "mdlcombine.h"
import std;

struct VTFFileHeader_t;
struct ResourceEntryInfo;
class CSimpleTexturePacker;


//#define DEBUG_VTF_COMBINE	1


inline constexpr auto MAX_COMBINED_MIP_LEVELS = 11;
inline constexpr auto MAX_COMBINED_WIDTH = 1024;
inline constexpr auto MAX_COMBINED_HEIGHT = 1024;
inline constexpr auto MAX_COMBINED_TEXTURE_MEMORY = ( ( 1024 + ( ( MAX_COMBINED_WIDTH >> 2 ) * ( MAX_COMBINED_HEIGHT >> 2 ) * 16 * 2 ) ) );

inline constexpr auto ATLAS_INFO_GROUP_INDEX = 0;
inline constexpr auto ATLAS_INFO_MATERIAL_INDEX = 1;
inline constexpr auto ATLAS_INFO_COUNT = (ATLAS_INFO_MATERIAL_INDEX + 1);

enum
{
	COMBINED_MATERIAL_FLAG_NOCULL =		0x00000001,
};


struct AtlasGroupData
{
	int					m_nNumMaterials;
	int					m_nMaterialIndices[ COMBINER_MAX_MATERIALS ];

	std::vector< std::byte >	m_pVTFData[ COMBINER_MAX_MATERIALS ];
	VTFFileHeader_t		*m_pVTFFileHeader[ COMBINER_MAX_MATERIALS ];
	ResourceEntryInfo	*m_pResources[ COMBINER_MAX_MATERIALS ];
	bool				m_bIsFlat[ COMBINER_MAX_MATERIALS ];

	KeyValues			*m_pCombinedMaterialKVs;

	byte				m_CombinedTextureMemory[ COMBINER_MAX_TEXTURES_PER_MATERIAL ][ MAX_COMBINED_TEXTURE_MEMORY ];
	int					m_nCombinedTextureSize[ COMBINER_MAX_TEXTURES_PER_MATERIAL ];
	VTFFileHeader_t		*m_CombinedHeaders[ COMBINER_MAX_TEXTURES_PER_MATERIAL ];

	CSimpleTexturePacker *m_pSimpleTexturePacker;
};

class CTextureCombine
{
public:
	CTextureCombine( );

	void	Init( TCombinedStudioData *pCombinedStudioData );
	void	Cleanup( );

	int		AddMaterial( const char *pszFileName );

	void	Resolve( );

	void	GetTextureInfo( int nIndex, Vector2D &vStartST, Vector2D &vSizeST, Vector2D &vPixelSize );

	void	FreeCombinedMaterials( );

	int		GetAtlasGroupIndex( int nIndex ) const
	{ 
		contract_assert( nIndex < COMBINER_MAX_MATERIALS );
		return m_nMaterialAtlasInfo[ nIndex ][ ATLAS_INFO_GROUP_INDEX ]; 
	}
	int		GetAtlasGroupMaterialIndex( int nIndex ) const
	{ 
		contract_assert( nIndex < COMBINER_MAX_MATERIALS );
		return m_nMaterialAtlasInfo[ nIndex ][ ATLAS_INFO_MATERIAL_INDEX ];
	}


private:
	TCombinedStudioData	*m_pCombinedStudioData;

	int					m_nNumMaterials;
	char				m_szMaterials[ COMBINER_MAX_MATERIALS ][ MAX_PATH ];
	int					m_nMaterialAtlasInfo[ COMBINER_MAX_MATERIALS ][ ATLAS_INFO_COUNT ];
	int					m_nMaxAtlasGroup;
	AtlasGroupData		m_AtlasGroups[ COMBINER_MAX_ATLAS_GROUPS ];

	KeyValues			*m_pMaterialKVs[ COMBINER_MAX_MATERIALS ];

	int					m_nMaterialToTexture;

	void	GatherAtlasInfo( );
	void	FindMaterialToTexture( );
	void	AddNonAtlasedMaterial( int nMaterial );

	bool	LoadVTFs( int nAtlasGroup, const char *pszTextureField, const char *pszFlatReplacement, char szTextureNames[ COMBINER_MAX_MATERIALS ][ MAX_PATH ] );

	bool	CombineTexture( int nAtlasGroup, int nTexture, const char *pszTextureField, const char *pszFlatReplacement );

	friend class CSimpleTexturePacker;
};


extern CTextureCombine& GetTextureCombiner();
