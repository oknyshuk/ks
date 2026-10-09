// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Ambient light controller entity.

import std;
import ks.math;
#include "vphysics_interface.h"
#include "edict.h"
#include "eiface.h"
#include "shareddefs.h"
#include "datamap.h"
#include "util.h"
#include "predictable_entity.h"
#include "variant_t.h"
#include "takedamageinfo.h"
#include "touchlink.h"
#include "groundlink.h"
#include "networkvar.h"
#include "sharedvar.h"
#include "baseentity_shared.h"
#include "basetoggle.h"
#include "precache_register.h"
#include "baseanimating.h"
#include "basecombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "basetempentity.h"
#include "player.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "spatialentity.h"
#include "env_ambient_light.h"

inline float AMBIENT_LIGHT_ENT_THINK_RATE() { return TICK_INTERVAL; }

LINK_ENTITY_TO_CLASS(env_ambient_light, CEnvAmbientLight);

IMPLEMENT_REFLECT_DATAMAP( CEnvAmbientLight )

IMPLEMENT_REFLECT_SERVERCLASS( CEnvAmbientLight, DT_EnvAmbientLight )


void CEnvAmbientLight::Spawn( void )
{
	BaseClass::Spawn();

	m_vecColor.SetX( static_cast<float>( m_Color.r ) / 255.0f );
	m_vecColor.SetY( static_cast<float>( m_Color.g ) / 255.0f );
	m_vecColor.SetZ( static_cast<float>( m_Color.b ) / 255.0f );
}


void CEnvAmbientLight::InputSetColor(inputdata_t &inputdata)
{
	m_Color = inputdata.value.Color32();
	m_vecColor.SetX( static_cast<float>( m_Color.r ) / 255.0f );
	m_vecColor.SetY( static_cast<float>( m_Color.g ) / 255.0f );
	m_vecColor.SetZ( static_cast<float>( m_Color.b ) / 255.0f );
}


void CEnvAmbientLight::SetColor( const Vector &vecColor )
{
	m_vecColor = vecColor;
}
