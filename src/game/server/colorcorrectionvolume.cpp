//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Color correction entity.
//
// $NoKeywords: $
//=============================================================================//

#include <string.h>

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "triggers.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//------------------------------------------------------------------------------
// FIXME: This really should inherit from something	more lightweight
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Purpose : Shadow control entity
//------------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_ColorCorrectionVolume", .base = false } ]]
      CColorCorrectionVolume : public CBaseTrigger
{
	DECLARE_CLASS( CColorCorrectionVolume, CBaseTrigger );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CColorCorrectionVolume();

	void Spawn( void );
	bool KeyValue( const char *szKeyName, const char *szValue );
	int  UpdateTransmitState();

	void ThinkFunc();

	virtual bool PassesTriggerFilters(CBaseEntity *pOther);
	virtual void StartTouch( CBaseEntity *pEntity );
	virtual void EndTouch( CBaseEntity *pEntity );

	virtual int	ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	
	// Inputs
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );

private:

	[[= ks::reflect::Key{ .name = "enabled" } ]] bool		m_bEnabled;
	[[= ks::reflect::Key{ .name = "StartDisabled" } ]] bool		m_bStartDisabled;

	CNetworkVar( float, m_Weight, [[= ks::reflect::Net{ .bits = 32 } ]] );
	CNetworkVar( float, m_MaxWeight, [[= ks::reflect::Key{ .name = "maxweight" } ]] ); 
	CNetworkString( m_lookupFilename, MAX_PATH, [[= ks::reflect::Net{} ]] );

	float		m_LastEnterWeight;
	float		m_LastEnterTime;

	float		m_LastExitWeight;
	float		m_LastExitTime;

	[[= ks::reflect::Key{ .name = "fadeDuration" } ]] float		m_FadeDuration;
};

LINK_ENTITY_TO_CLASS(color_correction_volume, CColorCorrectionVolume);

IMPLEMENT_REFLECT_DATAMAP( CColorCorrectionVolume )


IMPLEMENT_REFLECT_SERVERCLASS( CColorCorrectionVolume, DT_ColorCorrectionVolume )


CColorCorrectionVolume::CColorCorrectionVolume() : BaseClass()
{
	m_bEnabled = true;
	m_MaxWeight = 1.0f;
	m_lookupFilename.GetForModify()[0] = 0;
}


//------------------------------------------------------------------------------
// Purpose : Send even though we don't have a model
//------------------------------------------------------------------------------
int CColorCorrectionVolume::UpdateTransmitState()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}


bool CColorCorrectionVolume::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "filename" ) )
	{
		Q_strncpy( m_lookupFilename.GetForModify(), szValue, MAX_PATH );

		return true;
	}
	else if ( FStrEq( szKeyName, "maxweight" ) )
	{
		float max_weight;
		sscanf( szValue, "%f", &max_weight );
		m_MaxWeight = max_weight;

		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CColorCorrectionVolume::Spawn( void )
{
	BaseClass::Spawn();

	AddEFlags( EFL_FORCE_CHECK_TRANSMIT | EFL_DIRTY_ABSTRANSFORM );
	Precache();

	SetSolid( SOLID_BSP );
	SetSolidFlags( FSOLID_TRIGGER | FSOLID_NOT_SOLID );
	SetModel( STRING( GetModelName() ) );

	SetThink( &CColorCorrectionVolume::ThinkFunc );
	SetNextThink( gpGlobals->curtime + 0.01f );

	if( m_bStartDisabled )
	{
		m_bEnabled = false;
	}
	else
	{
		m_bEnabled = true;
	}
}

bool CColorCorrectionVolume::PassesTriggerFilters( CBaseEntity *pEntity )
{
	if( pEntity == UTIL_GetLocalPlayer() )
		return true;

	return false;
}

void CColorCorrectionVolume::StartTouch( CBaseEntity *pEntity )
{
	m_LastEnterTime = gpGlobals->curtime;
	m_LastEnterWeight = m_Weight;
}

void CColorCorrectionVolume::EndTouch( CBaseEntity *pEntity )
{
	m_LastExitTime = gpGlobals->curtime;
	m_LastExitWeight = m_Weight;
}

void CColorCorrectionVolume::ThinkFunc( )
{
	if( !m_bEnabled )
	{
		m_Weight.Set( 0.0f );
	}
	else
	{
		if( m_LastEnterTime > m_LastExitTime )
		{
			// we most recently entered the volume
		
			if( m_Weight < 1.0f )
			{
				float dt = gpGlobals->curtime - m_LastEnterTime;
				float weight = m_LastEnterWeight + dt / ((1.0f-m_LastEnterWeight)*m_FadeDuration);
				if( weight>1.0f )
					weight = 1.0f;

				m_Weight = weight;
			}
		}
		else
		{
			// we most recently exitted the volume
		
			if( m_Weight > 0.0f )
			{
				float dt = gpGlobals->curtime - m_LastExitTime;
				float weight = (1.0f-m_LastExitWeight) + dt / (m_LastExitWeight*m_FadeDuration);
				if( weight>1.0f )
					weight = 1.0f;

				m_Weight = 1.0f - weight;
			}
		}
	}

	SetNextThink( gpGlobals->curtime + 0.01f );
}


//------------------------------------------------------------------------------
// Purpose : Input handlers
//------------------------------------------------------------------------------
void CColorCorrectionVolume::InputEnable( inputdata_t &inputdata )
{
	m_bEnabled = true;
}

void CColorCorrectionVolume::InputDisable( inputdata_t &inputdata )
{
	m_bEnabled = false;
}
