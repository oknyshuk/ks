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
#include "reflect_datamap.h"
#include "reflect_annotations.h"

constexpr auto SF_NAV_START_ON = 0x0001;

enum navproperties_t
{
	NAV_IGNORE = 1<<0,
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CLogicNavigation : public CLogicalEntity,
						 public IEntityListener
{
	DECLARE_CLASS( CLogicNavigation, CLogicalEntity );

	public: using BaseClass::KeyValue_Impl;
	bool KeyValue_Impl( const char *szKeyName, const char *szValue );
	private:
	public: void Activate_Impl( void );
	private:

private:
	public: void UpdateOnRemove_Impl();
	private:

	void OnEntitySpawned( CBaseEntity *pEntity );

	// Inputs
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn( inputdata_t &inputdata ) { TurnOn(); }
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff( inputdata_t &inputdata ) { TurnOff(); }
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle( inputdata_t &inputdata ) 
	{ 
		if ( m_isOn )
			TurnOff();
		else
			TurnOn();
	}

	void TurnOn();
	void TurnOff();
	void UpdateProperty();

	DECLARE_DATADESC();

	bool				m_isOn;
	navproperties_t		m_navProperty;
};

LINK_ENTITY_TO_CLASS(logic_navigation, CLogicNavigation);


IMPLEMENT_REFLECT_DATAMAP( CLogicNavigation )



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *szKeyName - 
//			*szValue - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CLogicNavigation::KeyValue_Impl( const char *szKeyName, const char *szValue )
{
	if ( FStrEq(szKeyName, "navprop") )
	{
		if ( FStrEq( szValue, "Ignore" ) )
		{
			m_navProperty = NAV_IGNORE;
		}
		else
		{
			DevMsg( 1, "Unknown nav property %s\n", szValue );
		}
		return true;
	}

	return BaseClass::KeyValue_Impl( szKeyName, szValue );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLogicNavigation::Activate_Impl()
{
	BaseClass::Activate_Impl();

	if ( HasSpawnFlags( SF_NAV_START_ON ) )
	{
		TurnOn();
		RemoveSpawnFlags( SF_NAV_START_ON );
	}
	else if ( m_isOn )
	{
		gEntList.AddListenerEntity( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLogicNavigation::UpdateOnRemove_Impl()
{
	if ( m_isOn )
	{
		gEntList.RemoveListenerEntity( this );
	}

	BaseClass::UpdateOnRemove_Impl();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLogicNavigation::OnEntitySpawned( CBaseEntity *pEntity )
{
	if ( m_isOn && ( m_navProperty & NAV_IGNORE ) && pEntity->NameMatches( m_target ) )
	{
		pEntity->SetNavIgnore();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLogicNavigation::TurnOn()
{
	if ( m_isOn )
		return;
	m_isOn = true;
	gEntList.AddListenerEntity( this );
	UpdateProperty();
}

void CLogicNavigation::UpdateProperty()
{
	CBaseEntity *pEntity = nullptr;
	while ( ( pEntity = gEntList.FindEntityByName( pEntity, STRING(m_target) ) ) != nullptr )
	{
		if ( m_isOn )
		{
			if ( m_navProperty & NAV_IGNORE )
				pEntity->SetNavIgnore();
		}
		else
		{
			if ( m_navProperty & NAV_IGNORE )
				pEntity->ClearNavIgnore();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLogicNavigation::TurnOff()
{
	if ( !m_isOn )
		return;

	m_isOn = false;
	gEntList.RemoveListenerEntity( this );
	UpdateProperty();
}
