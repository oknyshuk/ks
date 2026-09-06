//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======
//
// Purpose: Projects a texture into the world (like the flashlight)
//
//=============================================================================

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_sendtable.h"
#include "reflect_annotations.h"
#include "shareddefs.h"
#include "env_projectedtexture.h"
#include "world.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( env_projectedtexture, CEnvProjectedTexture );

IMPLEMENT_REFLECT_DATAMAP( CEnvProjectedTexture )

IMPLEMENT_REFLECT_SERVERCLASS( CEnvProjectedTexture, DT_EnvProjectedTexture )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEnvProjectedTexture::CEnvProjectedTexture( void )
{
	m_bState = true;
	m_bAlwaysUpdate = false;
	m_flLightFOV = 45.0f;
	m_bEnableShadows = false;
	m_bSimpleProjection = false;
	m_bLightOnlyTarget = false;
	m_bLightWorld = true;
	m_bCameraSpace = false;

	Q_strcpy( m_SpotlightTextureName.GetForModify(), "effects/flashlight_border" );
	Q_strcpy( m_SpotlightTextureName.GetForModify(), "effects/flashlight001" );

	m_nSpotlightTextureFrame = 0;
	m_flBrightnessScale = 1.0f;
	m_LightColor.Init( 255, 255, 255, 255 );
	m_flColorTransitionTime = 0.5f;
	m_flAmbient = 0.0f;
	m_flNearZ = 4.0f;
	m_flFarZ = 750.0f;
	m_nShadowQuality = 0;
	m_flProjectionSize = 500.0f;
	m_flRotation = 0.0f;
}

void UTIL_ColorStringToLinearFloatColor( Vector &color, const char *pString )
{
	float tmp[4];
	UTIL_StringToFloatArray( tmp, 4, pString );
	if( tmp[3] <= 0.0f )
	{
		tmp[3] = 255.0f;
	}
	tmp[3] *= ( 1.0f / 255.0f );
	color.x = tmp[0] * ( 1.0f / 255.0f ) * tmp[3];
	color.y = tmp[1] * ( 1.0f / 255.0f ) * tmp[3];
	color.z = tmp[2] * ( 1.0f / 255.0f ) * tmp[3];
}

bool CEnvProjectedTexture::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "lightcolor" ) )
	{
		float tmp[4];
		UTIL_StringToFloatArray( tmp, 4, szValue );

		m_LightColor.SetR( tmp[0] );
		m_LightColor.SetG( tmp[1] );
		m_LightColor.SetB( tmp[2] );
		m_LightColor.SetA( tmp[3] );
	}
	else if ( FStrEq( szKeyName, "texturename" ) )
	{
		Q_strcpy( m_SpotlightTextureName.GetForModify(), szValue );
	}
	else
	{
		return BaseClass::KeyValue( szKeyName, szValue );
	}

	return true;
}

bool CEnvProjectedTexture::GetKeyValue( const char *szKeyName, char *szValue, int iMaxLen )
{
	if ( FStrEq( szKeyName, "lightcolor" ) )
	{
		Q_snprintf( szValue, iMaxLen, "%d %d %d %d", m_LightColor.GetR(), m_LightColor.GetG(), m_LightColor.GetB(), m_LightColor.GetA() );
		return true;
	}
	else if ( FStrEq( szKeyName, "texturename" ) )
	{
		Q_snprintf( szValue, iMaxLen, "%s", m_SpotlightTextureName.Get() );
		return true;
	}
	return BaseClass::GetKeyValue( szKeyName, szValue, iMaxLen );
}

void CEnvProjectedTexture::InputTurnOn( inputdata_t &inputdata )
{
	// Force all other projected textures off
	EnforceSingleProjectionRules();

	m_bState = true;
}

void CEnvProjectedTexture::InputTurnOff( inputdata_t &inputdata )
{
	m_bState = false;
}

void CEnvProjectedTexture::InputAlwaysUpdateOn( inputdata_t &inputdata )
{
	m_bAlwaysUpdate = true;
}

void CEnvProjectedTexture::InputAlwaysUpdateOff( inputdata_t &inputdata )
{
	m_bAlwaysUpdate = false;
}

void CEnvProjectedTexture::InputSetFOV( inputdata_t &inputdata )
{
	m_flLightFOV = inputdata.value.Float();
}

void CEnvProjectedTexture::InputSetTarget( inputdata_t &inputdata )
{
	m_hTargetEntity = inputdata.value.Entity();
}

void CEnvProjectedTexture::InputSetCameraSpace( inputdata_t &inputdata )
{
	m_bCameraSpace = inputdata.value.Bool();
}

void CEnvProjectedTexture::InputSetLightOnlyTarget( inputdata_t &inputdata )
{
	m_bLightOnlyTarget = inputdata.value.Bool();
}

void CEnvProjectedTexture::InputSetLightWorld( inputdata_t &inputdata )
{
	m_bLightWorld = inputdata.value.Bool();
}

void CEnvProjectedTexture::InputSetEnableShadows( inputdata_t &inputdata )
{
	m_bEnableShadows = inputdata.value.Bool();
}

void CEnvProjectedTexture::InputSetLightColor( inputdata_t &inputdata )
{
	m_LightColor = inputdata.value.Color32();
}

void CEnvProjectedTexture::InputSetAmbient( inputdata_t &inputdata )
{
	m_flAmbient = inputdata.value.Float();
}

void CEnvProjectedTexture::InputSetLightStyle( inputdata_t &inputdata )
{
	m_iStyle = inputdata.value.Int();
}

void CEnvProjectedTexture::InputSetPattern( inputdata_t &inputdata )
{
	m_iszPattern = inputdata.value.StringID();
	engine->LightStyle( m_iStyle, (char *) STRING( m_iszPattern ) );
}

void CEnvProjectedTexture::InputSetNearZ( inputdata_t &inputdata )
{
	m_flNearZ = inputdata.value.Float();
}

void CEnvProjectedTexture::InputSetFarZ( inputdata_t &inputdata )
{
	m_flFarZ = inputdata.value.Float();
}

void CEnvProjectedTexture::InputSetSpotlightTexture( inputdata_t &inputdata )
{
	Assert( 0 );
	Warning( "SetSpotlightTexture is disabled. If you need this feature reimplemented, tell a programmer.\n" );
	//Q_strcpy( m_SpotlightTextureName.GetForModify(), inputdata.value.String() );
}

void CEnvProjectedTexture::Spawn( void )
{
	m_bState = ( ( GetSpawnFlags() & ENV_PROJECTEDTEXTURE_STARTON ) != 0 );
	m_bAlwaysUpdate = ( ( GetSpawnFlags() & ENV_PROJECTEDTEXTURE_ALWAYSUPDATE ) != 0 );

	// Update light styles
	if ( m_iStyle >= 32 )
	{
		if ( m_iszPattern == NULL_STRING && m_iDefaultStyle > 0 )
		{
			m_iszPattern = MAKE_STRING( GetDefaultLightstyleString( m_iDefaultStyle ) );
		}

		if ( m_bState == false )
			engine->LightStyle( m_iStyle, "a" );
		else if ( m_iszPattern != NULL_STRING )
			engine->LightStyle( m_iStyle, (char *) STRING( m_iszPattern ) );
		else
			engine->LightStyle( m_iStyle, "m" );
	}

	BaseClass::Spawn();
}

void CEnvProjectedTexture::EnforceSingleProjectionRules( bool bWarnOnEnforcement )
{
	// Once a light is turned on, turn off all other possible lights in the level
	CBaseEntity *pFlashlight = NULL;
	while ( ( pFlashlight = gEntList.FindEntityByClassname( pFlashlight, "env_projectedtexture" ) ) != NULL )
	{
		// Obviously, don't turn yourself off
		if ( pFlashlight == this )
			continue;

		if ( bWarnOnEnforcement )
		{
			CEnvProjectedTexture *pProjTex = static_cast<CEnvProjectedTexture *>(pFlashlight);
			if ( pProjTex && pProjTex->m_bState )
			{
				Warning( "Warning: env_projected_texture (%s) forced off by (%s)\n", pProjTex->GetEntityNameAsCStr(), GetEntityNameAsCStr() );
			}
		}

		variant_t emptyVariant;
		pFlashlight->AcceptInput( "TurnOff", this, this, emptyVariant, 0 );
	}
}

void CEnvProjectedTexture::Activate( void )
{
	SetThink( &CEnvProjectedTexture::InitialThink );
	SetNextThink( gpGlobals->curtime + 0.1f );

	BaseClass::Activate();

	if ( m_bState )
	{
		// Make sure that we stomp any other active projected texture off when we activate
		EnforceSingleProjectionRules( true );
	}
}

void CEnvProjectedTexture::InitialThink( void )
{
	m_hTargetEntity = gEntList.FindEntityByName( NULL, m_target );
}

int CEnvProjectedTexture::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}


// Console command for creating env_projectedtexture entities
void CC_CreateFlashlight( const CCommand &args )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if( !pPlayer )
		return;

	QAngle angles = pPlayer->EyeAngles();
	Vector origin = pPlayer->EyePosition();		

	CEnvProjectedTexture *pFlashlight = dynamic_cast< CEnvProjectedTexture * >( CreateEntityByName("env_projectedtexture") );
	if( args.ArgC() > 1 )
	{
		pFlashlight->SetName( AllocPooledString( args[1] ) );
	}

	pFlashlight->Teleport( &origin, &angles, NULL );

}
static ConCommand create_flashlight("create_flashlight", CC_CreateFlashlight, 0, FCVAR_CHEAT);
