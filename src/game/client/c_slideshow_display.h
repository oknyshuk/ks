//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef C_SLIDESHOW_DISPLAY_H
#define C_SLIDESHOW_DISPLAY_H

#include "reflect_annotations.h"

#include "cbase.h"
#include "utlvector.h"


struct SlideMaterialList_t
{
	char				szSlideKeyword[64];
	CUtlVector<int>		iSlideMaterials;
	CUtlVector<int>		iSlideIndex;
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

	void GetDisplayText( char *pchText ) { Q_strcpy( pchText, m_szDisplayText ); }
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

	CUtlVector<SlideMaterialList_t*>	m_SlideMaterialLists;
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

extern CUtlVector< C_SlideshowDisplay* > g_SlideshowDisplays;

#endif //C_SLIDESHOW_STATS_DISPLAY_H
