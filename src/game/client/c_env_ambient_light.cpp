// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Ambient light controller entity with simple radial falloff

#include "rocketui/rmlui.h"
#include "predictable_entity.h"
#include "util_shared.h"
#include "baseentity_shared.h"
#include "c_basecombatcharacter.h"
#include "gamerules.h"
#include "reflect_recvtable.h"
#include "reflect_annotations.h"

#include "c_spatialentity.h"
#include "spatialentitymgr.h"
#include "c_env_ambient_light.h"

static ConVar cl_ambient_light_disableentities( "cl_ambient_light_disableentities", "0", FCVAR_NONE, "Disable map ambient light entities." );


static CSpatialEntityMgr s_EnvAmbientLightMgr;


IMPLEMENT_REFLECT_CLIENTCLASS( C_EnvAmbientLight, DT_EnvAmbientLight, CEnvAmbientLight )


void C_EnvAmbientLight::ApplyAccumulation( void )
{
	Vector rgbVal = BlendedValue();

	static ConVarRef mat_ambient_light_r( "mat_ambient_light_r" );
	static ConVarRef mat_ambient_light_g( "mat_ambient_light_g" );
	static ConVarRef mat_ambient_light_b( "mat_ambient_light_b" );

	if ( mat_ambient_light_r.IsValid() )
	{
		mat_ambient_light_r.SetValue( rgbVal.x );
	}

	if ( mat_ambient_light_g.IsValid() )
	{
		mat_ambient_light_g.SetValue( rgbVal.y );
	}

	if ( mat_ambient_light_b.IsValid() )
	{
		mat_ambient_light_b.SetValue( rgbVal.z );
	}
}


void C_EnvAmbientLight::AddToPersonalSpatialEntityMgr( void )
{
	s_EnvAmbientLightMgr.AddSpatialEntity( this );
}

void C_EnvAmbientLight::RemoveFromPersonalSpatialEntityMgr( void )
{
	s_EnvAmbientLightMgr.RemoveSpatialEntity( this );
}



void C_EnvAmbientLight::SetColor( const Vector &vecColor, float flLerpTime )
{
	if ( flLerpTime <= 0 )
	{
		m_Value = vecColor / 255.0f;
		m_colorTimer.Invalidate();
		return;
	}

	m_vecStartColor = m_Value;
	m_vecTargetColor = vecColor / 255.0f;
	m_colorTimer.Start( flLerpTime );
}

void C_EnvAmbientLight::ClientThink( void )
{
	BaseClass::ClientThink();

	if ( m_colorTimer.HasStarted() )
	{
		if ( m_colorTimer.IsElapsed() )
		{
			m_Value = m_vecTargetColor;
			m_colorTimer.Invalidate();
		}
		else
		{
			m_Value = m_vecTargetColor - ( m_vecTargetColor - m_vecStartColor ) * m_colorTimer.GetRemainingRatio();
		}
	}
}
