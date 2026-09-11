//========= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Allows movies to be played as a VGUI screen in the world
//
//=====================================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "EnvMessage.h"
#include "fmtstr.h"
#include "filesystem.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

class [[= ks::reflect::NetTable{ .name = "DT_MovieDisplay" } ]]
      CMovieDisplay : public CBaseEntity
{
public:

	DECLARE_CLASS( CMovieDisplay, CBaseEntity );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CMovieDisplay()
		: m_bForcePrecache( false )
	{
	}

	virtual ~CMovieDisplay();

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
	[[= ks::reflect::Input{ .name = "TakeOverAsMaster", .type = FIELD_VOID } ]] void	InputTakeOverAsMaster( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "SetMovie", .type = FIELD_STRING } ]] void	InputSetMovie( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "SetUseCustomUVs", .type = FIELD_BOOLEAN } ]] void	InputSetUseCustomUVs( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetUMin", .type = FIELD_FLOAT } ]] void	InputSetUMin( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetVMin", .type = FIELD_FLOAT } ]] void	InputSetVMin( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetUMax", .type = FIELD_FLOAT } ]] void	InputSetUMax( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetVMax", .type = FIELD_FLOAT } ]] void	InputSetVMax( inputdata_t &inputdata );	

private:

	// Control panel
	void GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	void GetControlPanelClassName( int nPanelIndex, const char *&pPanelName );
	void SpawnControlPanels( void );
	void RestoreControlPanels( void );

private:
	CNetworkVar( bool, m_bEnabled, [[= ks::reflect::Net{} ]] );
	CNetworkVar( bool, m_bLooping, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "looping" } ]] );
	CNetworkVar( bool, m_bStretchToFill, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "stretch" } ]] );
	CNetworkVar( bool, m_bForcedSlave, [[= ks::reflect::Net{} ]] [[= ks::reflect::Key{ .name = "forcedslave" } ]] );
	[[= ks::reflect::Key{ .name = "forceprecache" } ]] bool m_bForcePrecache;

	CNetworkVar( bool, m_bUseCustomUVs, [[= ks::reflect::Net{} ]] );
	CNetworkVar( float, m_flUMin, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkVar( float, m_flUMax, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkVar( float, m_flVMin, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkVar( float, m_flVMax, [[= ks::reflect::Net{ .bits = 32 } ]] );

	CNetworkString( m_szDisplayText, 128 );

	// Filename of the movie to play
	CNetworkString( m_szMovieFilename, 128, [[= ks::reflect::Net{} ]] );
	[[= ks::reflect::Key{ .name = "moviefilename" } ]] string_t	m_strMovieFilename;

	// "Group" name.  Screens of the same group name will play the same movie at the same time
	// Effectively this lets multiple screens tune to the same "channel" in the world
	CNetworkString( m_szGroupName, 128, [[= ks::reflect::Net{} ]] );
	[[= ks::reflect::Key{ .name = "groupname" } ]] string_t	m_strGroupName;

	[[= ks::reflect::Key{ .name = "width" } ]] int			m_iScreenWidth;
	[[= ks::reflect::Key{ .name = "height" } ]] int			m_iScreenHeight;

	bool		m_bDoFullTransmit;
};

LINK_ENTITY_TO_CLASS( vgui_movie_display, CMovieDisplay );

//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------
IMPLEMENT_REFLECT_DATAMAP( CMovieDisplay )

IMPLEMENT_REFLECT_SERVERCLASS( CMovieDisplay, DT_MovieDisplay )

CMovieDisplay::~CMovieDisplay()
{
}

//-----------------------------------------------------------------------------
// Read in Hammer data
//-----------------------------------------------------------------------------
bool CMovieDisplay::KeyValue( const char *szKeyName, const char *szValue ) 
{
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

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int CMovieDisplay::UpdateTransmitState()
{
	if ( m_bDoFullTransmit )
	{
		m_bDoFullTransmit = false;
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	return SetTransmitState( FL_EDICT_FULLCHECK );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways )
{
	// Are we already marked for transmission?
	if ( pInfo->m_pTransmitEdict->Get( entindex() ) )
		return;

	BaseClass::SetTransmit( pInfo, bAlways );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::Spawn( void )
{
	// Move the strings into a networkable form
	Q_strcpy( m_szMovieFilename.GetForModify(), m_strMovieFilename.ToCStr() );
	Q_strcpy( m_szGroupName.GetForModify(), m_strGroupName.ToCStr() );

	Precache();

	BaseClass::Spawn();

	m_bEnabled = false;

	SpawnControlPanels();

	ScreenVisible( m_bEnabled );

	m_bDoFullTransmit = true;

	m_bUseCustomUVs = false;
	m_flUMin = 0;
	m_flUMax = 1;
	m_flVMin = 0;
	m_flVMax = 1;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::Precache( void )
{
	BaseClass::Precache();

	if ( m_bForcePrecache )
	{
		DevMsg( "Precaching vgui_movie_display %s with movie %s\n", m_iName->ToCStr(), m_szMovieFilename.Get() );
		PrecacheMovie( m_szMovieFilename );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::OnRestore( void )
{
	BaseClass::OnRestore();

	m_bDoFullTransmit = true;

	RestoreControlPanels();

	ScreenVisible( m_bEnabled );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::ScreenVisible( bool bVisible )
{
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::Disable( void )
{
	if ( !m_bEnabled )
		return;

	m_bEnabled = false;

	ScreenVisible( false );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::Enable( void )
{
	if ( m_bEnabled )
		return;

	m_bEnabled = true;

	ScreenVisible( true );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputDisable( inputdata_t &inputdata )
{
	Disable();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputEnable( inputdata_t &inputdata )
{
	Enable();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputSetUseCustomUVs( inputdata_t &inputdata )
{
	m_bUseCustomUVs = inputdata.value.Bool();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputSetUMin( inputdata_t &inputdata )
{
	m_flUMin = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputSetUMax( inputdata_t &inputdata )
{
	m_flUMax = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputSetVMin( inputdata_t &inputdata )
{
	m_flVMin = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputSetVMax( inputdata_t &inputdata )
{
	m_flVMax = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputTakeOverAsMaster( inputdata_t &inputdata )
{
	Enable();

	EntityMessageBegin( this );
		WRITE_BYTE( 0 );
	MessageEnd();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputSetDisplayText( inputdata_t &inputdata )
{
	Q_strcpy( m_szDisplayText.GetForModify(), inputdata.value.String() );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputSetMovie( inputdata_t &inputdata )
{
	Q_strncpy( m_szMovieFilename.GetForModify(), inputdata.value.String(), 128 );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "movie_display_screen";
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::GetControlPanelClassName( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "vgui_screen";
}

//-----------------------------------------------------------------------------
// This is called by the base object when it's time to spawn the control panels
//-----------------------------------------------------------------------------
void CMovieDisplay::SpawnControlPanels()
{
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::RestoreControlPanels( void )
{
}
