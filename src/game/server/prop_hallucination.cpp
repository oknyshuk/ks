// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

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
#include "baseanimating.h"

class [[= ks::reflect::NetTable{ .name = "DT_Prop_Hallucination" } ]]
      CProp_Hallucination : public CBaseAnimating
{
public:
	DECLARE_CLASS( CProp_Hallucination, CBaseAnimating );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CProp_Hallucination( void );
	void Precache_Impl( void );
	void Spawn_Impl( void );
	int DrawDebugTextOverlays_Impl( void );
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void InputDisable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetVisibleTime", .type = FIELD_FLOAT } ]] void InputSetVisibleTime( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "SetRechargeTime", .type = FIELD_FLOAT } ]] void InputSetRechargeTime( inputdata_t &inputdata );

	CNetworkVar( bool, m_bEnabled, [[= ks::reflect::Net{} ]] );
	[[= ks::reflect::Key{ .name = "EnabledChance" } ]] float m_fStartEnabledChance; //0.0 - 100.0% chance that this hallucination will start enabled
	CNetworkVar( float, m_fVisibleTime, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "VisibleTime" } ]] ); //how long in seconds this hallucination can remain on screen from first sighting
	CNetworkVar( float, m_fRechargeTime, [[= ks::reflect::Net{ .bits = 32 } ]] [[= ks::reflect::Key{ .name = "RechargeTime" } ]] ); //how long in seconds it takes the hallucination to recharge before becoming visible again. 0 to disable
};

IMPLEMENT_REFLECT_DATAMAP( CProp_Hallucination )

IMPLEMENT_REFLECT_SERVERCLASS( CProp_Hallucination, DT_Prop_Hallucination )

LINK_ENTITY_TO_CLASS( prop_hallucination, CProp_Hallucination );


CProp_Hallucination::CProp_Hallucination( void )
{
}

void CProp_Hallucination::Precache_Impl( void )
{
	BaseClass::Precache_Impl();
	PrecacheModel( m_ModelName.ToCStr() );
}

void CProp_Hallucination::Spawn_Impl( void )
{
	Precache();
	BaseClass::Spawn_Impl();
	SetModel( m_ModelName.ToCStr() );

	if( m_fStartEnabledChance > 0.0f )
	{
		m_bEnabled = RandomFloat() <= (m_fStartEnabledChance * 0.01f);	
	}
}

void CProp_Hallucination::InputEnable( inputdata_t &inputdata )
{
	m_bEnabled = true;
}

void CProp_Hallucination::InputDisable( inputdata_t &inputdata )
{
	m_bEnabled = false;
}

void CProp_Hallucination::InputSetVisibleTime( inputdata_t &inputdata )
{
	m_fVisibleTime = inputdata.value.Float();
}

void CProp_Hallucination::InputSetRechargeTime( inputdata_t &inputdata )
{
	m_fRechargeTime = inputdata.value.Float();
}


//-----------------------------------------------------------------------------
// Draw debug overlays
//-----------------------------------------------------------------------------
int CProp_Hallucination::DrawDebugTextOverlays_Impl()
{
	int text_offset = BaseClass::DrawDebugTextOverlays_Impl();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[255];

		V_snprintf( tempstr, sizeof(tempstr), "%s", m_bEnabled ? "Enabled" : "Disabled" );
		EntityText( text_offset++, tempstr, 0 );

		V_snprintf( tempstr, sizeof(tempstr), "Start Enabled Chance: %f%%", m_fStartEnabledChance );
		EntityText( text_offset++, tempstr, 0 );

		V_snprintf( tempstr, sizeof(tempstr), "Visible Time: %fs", m_fVisibleTime.Get() );
		EntityText( text_offset++, tempstr, 0 );

		V_snprintf( tempstr, sizeof(tempstr), "Recharge Time: %fs", m_fRechargeTime.Get() );
		EntityText( text_offset++, tempstr, 0 );
	}
	

	return text_offset;
}
