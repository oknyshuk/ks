// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Color correction entity.


#include "string_t.h"
#include "vphysics_interface.h"
#include "const.h"
#include "edict.h"
#include "eiface.h"
#include "iserverentity.h"
#include "dt_send.h"
#include "shareddefs.h"
#include "ehandle.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "predictableid.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "soundflags.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "igameevents.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "entitylist.h"
#include "basetempentity.h"
#include "player.h"
#include "ndebugoverlay.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "triggers.h"

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

	void Spawn_Impl( void );
	using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	int  UpdateTransmitState_Impl();

	void ThinkFunc();

	virtual bool PassesTriggerFilters(CBaseEntity *pOther);
	void StartTouch_Impl( CBaseEntity *pEntity );
	void EndTouch_Impl( CBaseEntity *pEntity );

	int	ObjectCaps_Impl( void ) { return BaseClass::ObjectCaps_Impl() & ~FCAP_ACROSS_TRANSITION; }
	
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
int CColorCorrectionVolume::UpdateTransmitState_Impl()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}


bool CColorCorrectionVolume::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "filename" ) )
	{
		V_strncpy( m_lookupFilename.GetForModify(), szValue, MAX_PATH );

		return true;
	}
	else if ( FStrEq( szKeyName, "maxweight" ) )
	{
		float max_weight;
		std::sscanf( szValue, "%f", &max_weight );
		m_MaxWeight = max_weight;

		return true;
	}

	return BaseClass::KeyValue_Impl( szKeyName, szValue );
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CColorCorrectionVolume::Spawn_Impl( void )
{
	BaseClass::Spawn_Impl();

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

void CColorCorrectionVolume::StartTouch_Impl( CBaseEntity *pEntity )
{
	m_LastEnterTime = gpGlobals->curtime;
	m_LastEnterWeight = m_Weight;
}

void CColorCorrectionVolume::EndTouch_Impl( CBaseEntity *pEntity )
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
