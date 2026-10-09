#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
import ks.tier1;
#include "reflect_annotations.h"

import std;
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"

struct SlideMaterialList_t
{
	char				szSlideKeyword[64];
	std::vector< int >		iSlideMaterials;
	std::vector< int >		iSlideIndex;
};


class [[= ks::reflect::NetTable{ .name = "DT_SlideshowDisplay" } ]]
      C_SlideshowDisplay : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_SlideshowDisplay, CBaseEntity );
	DECLARE_CLIENTCLASS();

	C_SlideshowDisplay();
	virtual ~C_SlideshowDisplay();

	void Spawn( void );

	virtual void	OnDataChanged( DataUpdateType_t updateType );

	void ClientThink( void );

	bool IsEnabled( void ) { return m_bEnabled; }

	void GetDisplayText( char *pchText ) { V_strcpy( pchText, m_szDisplayText ); }
	int CurrentMaterialIndex( void ) { return m_iCurrentMaterialIndex; }
	int GetMaterialIndex( int iSlideIndex );
	int NumMaterials( void );
	int CurrentSlideIndex( void ) { return m_iCurrentSlideIndex; }

private:

	void BuildSlideShowImagesList( void );

private:

	[[= ks::reflect::Net{} ]] bool	m_bEnabled;

	[[= ks::reflect::Net{} ]] char	m_szDisplayText[ 128 ];

	[[= ks::reflect::Net{} ]] char	m_szSlideshowDirectory[ 128 ];

	std::vector< SlideMaterialList_t* >	m_SlideMaterialLists;
	[[= ks::reflect::Net{} ]] unsigned char						m_chCurrentSlideLists[ 16 ];
	int									m_iCurrentMaterialIndex;
	int									m_iCurrentSlideIndex;

	[[= ks::reflect::Net{} ]] float	m_fMinSlideTime;
	[[= ks::reflect::Net{} ]] float	m_fMaxSlideTime;

	float	m_NextSlideTime;

	[[= ks::reflect::Net{} ]] int		m_iCycleType;
	[[= ks::reflect::Net{} ]] bool	m_bNoListRepeats;
	int		m_iCurrentSlideList;
	int		m_iCurrentSlide;
};

extern std::vector< C_SlideshowDisplay* > g_SlideshowDisplays;
