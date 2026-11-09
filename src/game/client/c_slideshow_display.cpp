// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "cdll_util.h"
#include "util_shared.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "precache_register.h"
#include "c_basecombatweapon.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "c_baseplayer.h"
#include "vphysics_interface.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "reflect_recvtable.h"
#include "c_slideshow_display.h"
#include "tempent.h"
#include "engine/IEngineSound.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "filesystem.h"

constexpr auto SLIDESHOW_LIST_BUFFER_MAX = 8192;


enum SlideshowCycleTypes
{
	SLIDESHOW_CYCLE_RANDOM,
	SLIDESHOW_CYCLE_FORWARD,
	SLIDESHOW_CYCLE_BACKWARD,

	SLIDESHOW_CYCLE_TOTAL
};


std::vector< C_SlideshowDisplay* > g_SlideshowDisplays;


IMPLEMENT_REFLECT_CLIENTCLASS( C_SlideshowDisplay, DT_SlideshowDisplay, CSlideshowDisplay )


C_SlideshowDisplay::C_SlideshowDisplay()
{
	g_SlideshowDisplays.push_back( this );
}

C_SlideshowDisplay::~C_SlideshowDisplay()
{
	ks::erase_first( g_SlideshowDisplays, this );
}

void C_SlideshowDisplay::Spawn( void )
{
	BaseClass::Spawn();

	m_NextSlideTime = 0;

	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

void C_SlideshowDisplay::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		BuildSlideShowImagesList();
	}
	else if ( updateType == DATA_UPDATE_DATATABLE_CHANGED )
	{
		m_iCurrentSlideList = 0;
		m_iCurrentSlide = 0;
	}
}

int C_SlideshowDisplay::GetMaterialIndex( int iSlideIndex )
{
	if ( !m_SlideMaterialLists[ 0 ] )
		return 0;

	return m_SlideMaterialLists[ 0 ]->iSlideMaterials[ iSlideIndex ];
}

int C_SlideshowDisplay::NumMaterials( void )
{
	if ( !m_SlideMaterialLists[ 0 ] )
		return 0;

	return std::ssize( m_SlideMaterialLists[ 0 ]->iSlideMaterials );
}

void C_SlideshowDisplay::ClientThink( void )
{
	BaseClass::ClientThink();

	if ( !m_bEnabled )
		return;

	// Check if it's time for the next slide
	if ( m_NextSlideTime > gpGlobals->curtime )
		return;

	// Set the time to cycle to the next slide
	m_NextSlideTime = gpGlobals->curtime + RandomFloat( m_fMinSlideTime, m_fMaxSlideTime );

	// Get the amount of items to pick from
	int iNumCurrentSlideLists;
	for ( iNumCurrentSlideLists = 0; iNumCurrentSlideLists < 16; ++iNumCurrentSlideLists )
	{
		if ( m_chCurrentSlideLists[ iNumCurrentSlideLists ] == (unsigned char)-1 )
			break;
	}

	// Bail if no slide lists are selected
	if ( iNumCurrentSlideLists == 0 )
		return;

	// Cycle the list
	switch ( m_iCycleType )
	{
		case SLIDESHOW_CYCLE_RANDOM:
		{
			int iOldSlideList = m_iCurrentSlideList;
			m_iCurrentSlideList = RandomInt( 0, iNumCurrentSlideLists - 1 );

			// Prevent repeats if we don't want them
			if ( m_bNoListRepeats && iNumCurrentSlideLists > 1 && m_iCurrentSlideList == iOldSlideList )
			{
				++m_iCurrentSlideList;

				if ( m_iCurrentSlideList >= iNumCurrentSlideLists )
					m_iCurrentSlideList = 0;
			}

			break;
		}

		case SLIDESHOW_CYCLE_FORWARD:
			if ( m_iCurrentSlideList >= iNumCurrentSlideLists )
				m_iCurrentSlideList = 0;
			break;

		case SLIDESHOW_CYCLE_BACKWARD:
			if ( m_iCurrentSlideList < 0 )
				m_iCurrentSlideList = iNumCurrentSlideLists - 1;
			break;
	}

	SlideMaterialList_t *pSlideMaterialList = m_SlideMaterialLists[ m_chCurrentSlideLists[ m_iCurrentSlideList ] ];

	// Cycle in the list
	switch ( m_iCycleType )
	{
		case SLIDESHOW_CYCLE_RANDOM:
			m_iCurrentSlide = RandomInt( 0, std::ssize( pSlideMaterialList->iSlideMaterials ) - 1 );
			break;

		case SLIDESHOW_CYCLE_FORWARD:
			++m_iCurrentSlide;
			if ( m_iCurrentSlide >= std::ssize( pSlideMaterialList->iSlideMaterials ) )
			{
				++m_iCurrentSlideList;
				if ( m_iCurrentSlideList >= iNumCurrentSlideLists )
					m_iCurrentSlideList = 0;
				pSlideMaterialList = m_SlideMaterialLists[ m_chCurrentSlideLists[ m_iCurrentSlideList ] ];
				m_iCurrentSlide = 0;
			}
			break;

		case SLIDESHOW_CYCLE_BACKWARD:
			--m_iCurrentSlide;
			if ( m_iCurrentSlide < 0 )
			{
				--m_iCurrentSlideList;
				if ( m_iCurrentSlideList < 0 )
					m_iCurrentSlideList = iNumCurrentSlideLists - 1;
				pSlideMaterialList = m_SlideMaterialLists[ m_chCurrentSlideLists[ m_iCurrentSlideList ] ];
				m_iCurrentSlide = std::ssize( pSlideMaterialList->iSlideMaterials ) - 1;
			}
			break;
	}

	// Set the current material to what we've cycled to
	m_iCurrentMaterialIndex = pSlideMaterialList->iSlideMaterials[ m_iCurrentSlide ];
	m_iCurrentSlideIndex = pSlideMaterialList->iSlideIndex[ m_iCurrentSlide ];
}

void C_SlideshowDisplay::BuildSlideShowImagesList( void )
{
	FileFindHandle_t matHandle;
	char szDirectory[k_cMaxPath];
	char szMatFileName[k_cMaxPath] = {'\0'};
	char szFileBuffer[ SLIDESHOW_LIST_BUFFER_MAX ];
	char *pchCurrentLine = nullptr;

	{
		V_snprintf( szDirectory, sizeof( szDirectory ), "materials/vgui/%s/*.vmt", m_szSlideshowDirectory );
		const char *pMatFileName = g_pFullFileSystem->FindFirst( szDirectory, &matHandle );

		if ( pMatFileName )
			V_strncpy( szMatFileName, pMatFileName, sizeof(szMatFileName) );
	}

	int iSlideIndex = 0;

	while ( szMatFileName[ 0 ] )
	{
		char szFileName[k_cMaxPath];
		V_snprintf( szFileName, sizeof( szFileName ), "vgui/%s/%s", m_szSlideshowDirectory, szMatFileName );
		szFileName[ V_strlen( szFileName ) - 4 ] = '\0';

		int iMatIndex = ::GetMaterialIndex( szFileName );

		// Get material keywords
		char szFullFileName[k_cMaxPath];
		V_snprintf( szFullFileName, sizeof( szFullFileName ), "materials/vgui/%s/%s", m_szSlideshowDirectory, szMatFileName );

		KeyValues *pMaterialKeys = new KeyValues( "material" );
		bool bLoaded = pMaterialKeys->LoadFromFile( g_pFullFileSystem, szFullFileName, nullptr );
		if ( bLoaded )
		{
			char szKeywords[ 256 ];
			V_strcpy_safe( szKeywords, pMaterialKeys->GetString( "%keywords", "" ) );

			char *pchKeyword = szKeywords;

			while ( pchKeyword[ 0 ] != '\0' )
			{
				char *pNextKeyword = pchKeyword;

				// Skip commas and spaces
				while ( pNextKeyword[ 0 ] != '\0' && pNextKeyword[ 0 ] != ',' )
					++pNextKeyword;

				if ( pNextKeyword[ 0 ] != '\0' )
				{
					pNextKeyword[ 0 ] = '\0';
					++pNextKeyword;

					while ( pNextKeyword[ 0 ] != '\0' && ( pNextKeyword[ 0 ] == ',' || pNextKeyword[ 0 ] == ' ' ) )
						++pNextKeyword;
				}

				// Find the list with the current keyword
				int iList;
				for ( iList = 0; iList < std::ssize( m_SlideMaterialLists ); ++iList )
				{
					if ( V_strcmp( m_SlideMaterialLists[ iList ]->szSlideKeyword, pchKeyword ) == 0 )
						break;
				}

				if ( iList >= std::ssize( m_SlideMaterialLists ) )
				{
					// Couldn't find the list, so create it
					iList = std::ssize( m_SlideMaterialLists ); m_SlideMaterialLists.push_back( new SlideMaterialList_t );
					V_strcpy_safe( m_SlideMaterialLists[ iList ]->szSlideKeyword, pchKeyword );
				}

				// Add material index to this list
				m_SlideMaterialLists[ iList ]->iSlideMaterials.push_back( iMatIndex );
				m_SlideMaterialLists[ iList ]->iSlideIndex.push_back( iSlideIndex );

				pchKeyword = pNextKeyword;
			}
		}
		pMaterialKeys->deleteThis();
		pMaterialKeys = nullptr;

		// Find the generic list
		int iList;
		for ( iList = 0; iList < std::ssize( m_SlideMaterialLists ); ++iList )
		{
			if ( V_strcmp( m_SlideMaterialLists[ iList ]->szSlideKeyword, "" ) == 0 )
				break;
		}

		if ( iList >= std::ssize( m_SlideMaterialLists ) )
		{
			// Couldn't find the generic list, so create it
			m_SlideMaterialLists.insert( m_SlideMaterialLists.begin(), new SlideMaterialList_t ); iList = 0;
			V_strcpy_safe( m_SlideMaterialLists[ iList ]->szSlideKeyword, "" );
		}

		// Add material index to this list
		m_SlideMaterialLists[ iList ]->iSlideMaterials.push_back( iMatIndex );
		m_SlideMaterialLists[ iList ]->iSlideIndex.push_back( iSlideIndex );
		
		{
			const char *pMatFileName = g_pFullFileSystem->FindNext( matHandle );

			if ( pMatFileName )
				V_strncpy( szMatFileName, pMatFileName, sizeof(szMatFileName) );
			else
				szMatFileName[ 0 ] = '\0';
		}

		++iSlideIndex;
	}

	g_pFullFileSystem->FindClose( matHandle );
}
