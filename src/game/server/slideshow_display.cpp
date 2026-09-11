//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the big scary boom-boom machine Antlions fear.
//
//=============================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "EnvMessage.h"
#include "fmtstr.h"
#include "filesystem.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


#define SLIDESHOW_LIST_BUFFER_MAX 8192


struct SlideKeywordList_t
{
	char	szSlideKeyword[64];
};


class [[= ks::reflect::NetTable{ .name = "DT_SlideshowDisplay" } ]]
      CSlideshowDisplay : public CBaseEntity
{
public:

	DECLARE_CLASS( CSlideshowDisplay, CBaseEntity );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	virtual ~CSlideshowDisplay();

	virtual bool KeyValue( const char *szKeyName, const char *szValue );

	virtual int  UpdateTransmitState();
	virtual void SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void OnRestore( void );

	void	ScreenVisible( bool bVisible );

	void	Disable( void );
	void	Enable( void );

	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "SetDisplayText", .type = FIELD_STRING } ]] void	InputSetDisplayText( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "RemoveAllSlides", .type = FIELD_VOID } ]] void	InputRemoveAllSlides( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "AddSlides", .type = FIELD_STRING } ]] void	InputAddSlides( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "SetMinSlideTime", .type = FIELD_FLOAT } ]] void	InputSetMinSlideTime( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetMaxSlideTime", .type = FIELD_FLOAT } ]] void	InputSetMaxSlideTime( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "SetCycleType", .type = FIELD_INTEGER } ]] void	InputSetCycleType( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetNoListRepeats", .type = FIELD_BOOLEAN } ]] void	InputSetNoListRepeats( inputdata_t &inputdata );

private:

	// Control panel
	void GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	void GetControlPanelClassName( int nPanelIndex, const char *&pPanelName );
	void SpawnControlPanels( void );
	void RestoreControlPanels( void );
	void BuildSlideShowImagesList( void );

private:

	CNetworkVar( bool, m_bEnabled, [[= ks::reflect::Net{} ]] );

	CNetworkString( m_szDisplayText, 128, [[= ks::reflect::Net{} ]] );

	CNetworkString( m_szSlideshowDirectory, 128, [[= ks::reflect::Net{} ]] );
	[[= ks::reflect::Key{ .name = "directory" } ]] string_t	m_String_tSlideshowDirectory;

	CUtlVector<SlideKeywordList_t*>		m_SlideKeywordList;
	CNetworkArray( unsigned char, m_chCurrentSlideLists, 16, [[= ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED } ]] );

	CNetworkVar( float, m_fMinSlideTime, [[= ks::reflect::Net{ .bits = 11, .low = 0.0f, .high = 20.0f } ]] [[= ks::reflect::Key{ .name = "minslidetime" } ]] );
	CNetworkVar( float, m_fMaxSlideTime, [[= ks::reflect::Net{ .bits = 11, .low = 0.0f, .high = 20.0f } ]] [[= ks::reflect::Key{ .name = "maxslidetime" } ]] );

	CNetworkVar( int, m_iCycleType, [[= ks::reflect::Net{ .bits = 2, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "cycletype" } ]] );
	CNetworkVar( bool, m_bNoListRepeats, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "nolistrepeats" } ]] );

	[[= ks::reflect::Key{ .name = "width" } ]] int		m_iScreenWidth;
	[[= ks::reflect::Key{ .name = "height" } ]] int		m_iScreenHeight;

	bool	m_bDoFullTransmit;
};


LINK_ENTITY_TO_CLASS( vgui_slideshow_display, CSlideshowDisplay );

//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CSlideshowDisplay )

IMPLEMENT_REFLECT_SERVERCLASS( CSlideshowDisplay, DT_SlideshowDisplay )


CSlideshowDisplay::~CSlideshowDisplay()
{
}

//-----------------------------------------------------------------------------
// Read in worldcraft data...
//-----------------------------------------------------------------------------
bool CSlideshowDisplay::KeyValue( const char *szKeyName, const char *szValue ) 
{
	//!! temp hack, until worldcraft is fixed
	// strip the # tokens from (duplicate) key names
	char *s = (char *)strchr( szKeyName, '#' );
	if ( s )
	{
		*s = '\0';
	}

	// NOTE: Have to do these separate because they set two values instead of one
	if( FStrEq( szKeyName, "angles" ) )
	{
		Assert( GetMoveParent() == nullptr );
		QAngle angles;
		UTIL_StringToVector( angles.Base(), szValue );

		// Because the vgui screen basis is strange (z is front, y is up, x is right)
		// we need to rotate the typical basis before applying it
		VMatrix mat, rotation, tmp;
		MatrixFromAngles( angles, mat );
		MatrixBuildRotationAboutAxis( rotation, Vector( 0, 1, 0 ), 90 );
		MatrixMultiply( mat, rotation, tmp );
		MatrixBuildRotateZ( rotation, 90 );
		MatrixMultiply( tmp, rotation, mat );
		MatrixToAngles( mat, angles );
		SetAbsAngles( angles );

		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}

int CSlideshowDisplay::UpdateTransmitState()
{
	if ( m_bDoFullTransmit )
	{
		m_bDoFullTransmit = false;
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	return SetTransmitState( FL_EDICT_FULLCHECK );
}

void CSlideshowDisplay::SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways )
{
	// Are we already marked for transmission?
	if ( pInfo->m_pTransmitEdict->Get( entindex() ) )
		return;

	BaseClass::SetTransmit( pInfo, bAlways );
}

void CSlideshowDisplay::Spawn( void )
{
	Q_strcpy( m_szSlideshowDirectory.GetForModify(), m_String_tSlideshowDirectory.ToCStr() );
	Precache();

	BaseClass::Spawn();

	m_bEnabled = false;
	
	// Clear out selected list
	m_chCurrentSlideLists.GetForModify( 0 ) = 0;	// Select all slides to begin with
	for ( int i = 1; i < 16; ++i )
		m_chCurrentSlideLists.GetForModify( i ) = (unsigned char)-1;

	SpawnControlPanels();

	ScreenVisible( m_bEnabled );

	m_bDoFullTransmit = true;
}

void CSlideshowDisplay::Precache( void )
{
	BaseClass::Precache();

	BuildSlideShowImagesList();
}

void CSlideshowDisplay::OnRestore( void )
{
	BaseClass::OnRestore();

	BuildSlideShowImagesList();

	RestoreControlPanels();

	ScreenVisible( m_bEnabled );
}

void CSlideshowDisplay::ScreenVisible( bool bVisible )
{
}

void CSlideshowDisplay::Disable( void )
{
	if ( !m_bEnabled )
		return;

	m_bEnabled = false;

	ScreenVisible( false );
}

void CSlideshowDisplay::Enable( void )
{
	if ( m_bEnabled )
		return;

	m_bEnabled = true;

	ScreenVisible( true );
}


void CSlideshowDisplay::InputDisable( inputdata_t &inputdata )
{
	Disable();
}

void CSlideshowDisplay::InputEnable( inputdata_t &inputdata )
{
	Enable();
}


void CSlideshowDisplay::InputSetDisplayText( inputdata_t &inputdata )
{
	Q_strcpy( m_szDisplayText.GetForModify(), inputdata.value.String() );
}

void CSlideshowDisplay::InputRemoveAllSlides( inputdata_t &inputdata )
{
	// Clear out selected list
	for ( int i = 0; i < 16; ++i )
		m_chCurrentSlideLists.GetForModify( i ) = (unsigned char)-1;
}

void CSlideshowDisplay::InputAddSlides( inputdata_t &inputdata )
{
	// Find the list with the current keyword
	int iList;
	for ( iList = 0; iList < m_SlideKeywordList.Count(); ++iList )
	{
		if ( Q_strcmp( m_SlideKeywordList[ iList ]->szSlideKeyword, inputdata.value.String() ) == 0 )
			break;
	}

	if ( iList < m_SlideKeywordList.Count() )
	{
		// Found the keyword list, so add this index to the selected lists
		int iNumCurrentSlideLists;
		for ( iNumCurrentSlideLists = 0; iNumCurrentSlideLists < 16; ++iNumCurrentSlideLists )
		{
			if ( m_chCurrentSlideLists[ iNumCurrentSlideLists ] == (unsigned char)-1 )
				break;
		}

		if ( iNumCurrentSlideLists >= 16 )
			return;

		m_chCurrentSlideLists.GetForModify( iNumCurrentSlideLists ) = iList;
	}
}


void CSlideshowDisplay::InputSetMinSlideTime( inputdata_t &inputdata )
{
	m_fMinSlideTime = inputdata.value.Float();
}

void CSlideshowDisplay::InputSetMaxSlideTime( inputdata_t &inputdata )
{
	m_fMaxSlideTime = inputdata.value.Float();
}


void CSlideshowDisplay::InputSetCycleType( inputdata_t &inputdata )
{
	m_iCycleType = inputdata.value.Int();
}

void CSlideshowDisplay::InputSetNoListRepeats( inputdata_t &inputdata )
{
	m_bNoListRepeats = inputdata.value.Bool();
}


void CSlideshowDisplay::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "slideshow_display_screen";
}

void CSlideshowDisplay::GetControlPanelClassName( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "vgui_screen";
}

//-----------------------------------------------------------------------------
// This is called by the base object when it's time to spawn the control panels
//-----------------------------------------------------------------------------
void CSlideshowDisplay::SpawnControlPanels()
{
}

void CSlideshowDisplay::RestoreControlPanels( void )
{
}

void CSlideshowDisplay::BuildSlideShowImagesList( void )
{
	FileFindHandle_t matHandle;
	char szDirectory[_MAX_PATH];
	char szMatFileName[_MAX_PATH] = {'\0'};
	char szFileBuffer[ SLIDESHOW_LIST_BUFFER_MAX ];
	char *pchCurrentLine = nullptr;

	{
		Q_snprintf( szDirectory, sizeof( szDirectory ), "materials/vgui/%s/*.vmt", m_szSlideshowDirectory.Get() );
		const char *pMatFileName = g_pFullFileSystem->FindFirst( szDirectory, &matHandle );

		if ( pMatFileName )
			Q_strncpy( szMatFileName, pMatFileName, sizeof(szMatFileName) );
	}

	int iSlideIndex = 0;

	while ( szMatFileName[ 0 ] )
	{
		char szFileName[_MAX_PATH];
		Q_snprintf( szFileName, sizeof( szFileName ), "vgui/%s/%s", m_szSlideshowDirectory.Get(), szMatFileName );
		szFileName[ Q_strlen( szFileName ) - 4 ] = '\0';

		PrecacheMaterial( szFileName );	

		// Get material keywords
		char szFullFileName[_MAX_PATH];
		Q_snprintf( szFullFileName, sizeof( szFullFileName ), "materials/vgui/%s/%s", m_szSlideshowDirectory.Get(), szMatFileName );

		KeyValues *pMaterialKeys = new KeyValues( "material" );
		bool bLoaded = pMaterialKeys->LoadFromFile( g_pFullFileSystem, szFullFileName, nullptr );
		if ( bLoaded )
		{
			char szKeywords[ 256 ];
			Q_strcpy( szKeywords, pMaterialKeys->GetString( "%keywords", "" ) );

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
				for ( iList = 0; iList < m_SlideKeywordList.Count(); ++iList )
				{
					if ( Q_strcmp( m_SlideKeywordList[ iList ]->szSlideKeyword, pchKeyword ) == 0 )
						break;
				}

				if ( iList >= m_SlideKeywordList.Count() )
				{
					// Couldn't find the list, so create it
					iList = m_SlideKeywordList.AddToTail( new SlideKeywordList_t );
					Q_strcpy( m_SlideKeywordList[ iList ]->szSlideKeyword, pchKeyword );
				}

				pchKeyword = pNextKeyword;
			}
		}
		pMaterialKeys->deleteThis();
		pMaterialKeys = nullptr;

		// Find the generic list
		int iList;
		for ( iList = 0; iList < m_SlideKeywordList.Count(); ++iList )
		{
			if ( Q_strcmp( m_SlideKeywordList[ iList ]->szSlideKeyword, "" ) == 0 )
				break;
		}

		if ( iList >= m_SlideKeywordList.Count() )
		{
			// Couldn't find the generic list, so create it
			iList = m_SlideKeywordList.AddToHead( new SlideKeywordList_t );
			Q_strcpy( m_SlideKeywordList[ iList ]->szSlideKeyword, "" );
		}

		{
			const char *pMatFileName = g_pFullFileSystem->FindNext( matHandle );

			if ( pMatFileName )
				Q_strncpy( szMatFileName, pMatFileName, sizeof(szMatFileName) );
			else
				szMatFileName[ 0 ] = '\0';
		}

		++iSlideIndex;
	}

	g_pFullFileSystem->FindClose( matHandle );
}
