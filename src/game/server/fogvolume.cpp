//--------------------------------------------------------------------------------------------------------
// Copyright (c) 2007 Turtle Rock Studios, Inc. - All Rights Reserved

import std;
import ks.tier1;
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
#include "base_transmit_proxy.h"
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
#include "te.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "fogvolume.h"
#include "collisionutils.h"


std::vector< CFogVolume * > TheFogVolumes;

ConVar fog_volume_debug( "fog_volume_debug", "0", 0, "If enabled, prints diagnostic information about the current fog volume" );

//--------------------------------------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS(fog_volume, CFogVolume);

IMPLEMENT_REFLECT_DATAMAP( CFogVolume )


//--------------------------------------------------------------------------------------------------------
CFogVolume *CFogVolume::FindFogVolumeForPosition( const Vector &position )
{
	CFogVolume *fogVolume = nullptr;
	for ( int i=0; i<std::ssize( TheFogVolumes ); ++i )
	{
		fogVolume = TheFogVolumes[i];

		Vector vecRelativeCenter;
		fogVolume->CollisionProp()->WorldToCollisionSpace( position, &vecRelativeCenter );
		if ( IsBoxIntersectingSphere( fogVolume->CollisionProp()->OBBMins(), fogVolume->CollisionProp()->OBBMaxs(), vecRelativeCenter, 1.0f ) )
		{
			break;
		}
		fogVolume = nullptr;
	}

	// This doesn't work well if there are multiple players or multiple fog volume queries per frame; might want to relocate this if that's the case
	if ( fog_volume_debug.GetBool() )
	{
		if ( fogVolume )
		{
			char fogVolumeName[256];
			fogVolume->GetKeyValue( "targetname", fogVolumeName, 256 );
			engine->Con_NPrintf( 0, "Fog Volume ""%s"" found at position (%f %f %f)", fogVolumeName, position.x, position.y, position.z );
			engine->Con_NPrintf( 1, "Fog: %s, post process: %s, color correct: %s", fogVolume->m_fogName.ToCStr(), fogVolume->m_postProcessName.ToCStr(), fogVolume->m_colorCorrectionName.ToCStr() );
		}
		else
		{
			engine->Con_NPrintf( 0, "No Fog Volume found at given position (%f %f %f)", position.x, position.y, position.z );
		}
	}
	
	return fogVolume;
}


//--------------------------------------------------------------------------------------------------------
CFogVolume::CFogVolume() :
	BaseClass(),
	m_bDisabled( false ),
	m_bInFogVolumesList( false )
{
}


//--------------------------------------------------------------------------------------------------------
CFogVolume::~CFogVolume()
{
	RemoveFromGlobalList();
}


//--------------------------------------------------------------------------------------------------------
void CFogVolume::Spawn( void )
{
	BaseClass::Spawn();

	SetSolid( SOLID_BSP );
	SetSolidFlags( FSOLID_NOT_SOLID );
	SetModel( STRING( GetModelName() ) );
}


//--------------------------------------------------------------------------------------------------------
void CFogVolume::AddToGlobalList()
{
	if ( !m_bInFogVolumesList )
	{
		TheFogVolumes.push_back( this );
		m_bInFogVolumesList = true;
	}
}


//--------------------------------------------------------------------------------------------------------
void CFogVolume::RemoveFromGlobalList()
{
	if ( m_bInFogVolumesList )
	{
		ks::erase_first( TheFogVolumes, this );
		m_bInFogVolumesList = false;
	}
}


//----------------------------------------------------------------------------
void CFogVolume::InputEnable( inputdata_t &data )
{
	m_bDisabled = false;
	AddToGlobalList();
}


//----------------------------------------------------------------------------
void CFogVolume::InputDisable( inputdata_t &data )
{
	m_bDisabled = true;
	RemoveFromGlobalList();
}


//----------------------------------------------------------------------------
// Called when the level loads or is restored
//----------------------------------------------------------------------------
void CFogVolume::Activate()
{
	BaseClass::Activate();

	m_hFogController = dynamic_cast< CFogController* >( gEntList.FindEntityByName( nullptr, m_fogName ) );
	m_hPostProcessController = dynamic_cast< CPostProcessController* >( gEntList.FindEntityByName( nullptr, m_postProcessName ) );
	m_hColorCorrectionController = dynamic_cast< CColorCorrection* >( gEntList.FindEntityByName( nullptr, m_colorCorrectionName ) );

	if ( !m_bDisabled )
	{
		AddToGlobalList();
	}
}
