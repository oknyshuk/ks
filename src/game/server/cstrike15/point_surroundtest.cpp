//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


class CSurroundTest : public CPointEntity
{
public:
	DECLARE_CLASS( CSurroundTest, CPointEntity );

	[[= ks::reflect::Input{ .name = "FireCorrectOutput", .type = FIELD_VOID } ]] void	FireCorrectOutput( inputdata_t &inputdata );
	void	Spawn( void );

private:
	
	[[= ks::reflect::Key{ .name = "On2Speakers" } ]] COutputEvent m_On2Speakers;
	[[= ks::reflect::Key{ .name = "On4Speakers" } ]] COutputEvent m_On4Speakers;
	[[= ks::reflect::Key{ .name = "On51Speakers" } ]] COutputEvent m_On51Speakers;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( point_surroundtest, CSurroundTest );

IMPLEMENT_REFLECT_DATAMAP( CSurroundTest )

enum
{
	SND_SURROUND_HEADPHONES = 0,
	SND_SURROUND_2SPEAKERS = 2,
	SND_SURROUND_4SPEAKERS = 4,
	SND_SURROUND_51SPEAKERS, 
};

void CSurroundTest::FireCorrectOutput( inputdata_t &inputdata )
{
	ConVar const *pSurroundCVar = cvar->FindVar( "snd_surround_speakers" );

	if ( pSurroundCVar )
	{
		int iSetting = pSurroundCVar->GetInt();
		
		if ( iSetting == SND_SURROUND_HEADPHONES || iSetting == SND_SURROUND_2SPEAKERS )
		{
			m_On2Speakers.FireOutput( this, this );
		}
		else if ( iSetting == SND_SURROUND_4SPEAKERS )
		{
			m_On4Speakers.FireOutput( this, this );
		}
		else if ( iSetting == SND_SURROUND_51SPEAKERS )
		{
			m_On51Speakers.FireOutput( this, this );
		}
	}
}

void CSurroundTest::Spawn( void )
{
	BaseClass::Spawn();
}
