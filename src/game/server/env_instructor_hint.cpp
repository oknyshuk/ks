//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: An entity for creating instructor hints entirely with map logic
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "reflect_datamap.h"
#include "reflect_annotations.h"
#include "baseentity.h"
#include "world.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEnvInstructorHint : public CPointEntity
{
public:
	DECLARE_CLASS( CEnvInstructorHint, CPointEntity );
	DECLARE_DATADESC();

private:
	[[= ks::reflect::Input{ .name = "ShowHint", .type = FIELD_STRING } ]] void InputShowHint( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "EndHint", .type = FIELD_VOID } ]] void InputEndHint( inputdata_t &inputdata );
	
	[[= ks::reflect::Key{ .name = "hint_replace_key" } ]] string_t	m_iszReplace_Key;
	[[= ks::reflect::Key{ .name = "hint_target" } ]] string_t	m_iszHintTargetEntity;
	[[= ks::reflect::Key{ .name = "hint_timeout" } ]] int			m_iTimeout;
	[[= ks::reflect::Key{ .name = "hint_icon_onscreen" } ]] string_t	m_iszIcon_Onscreen;
	[[= ks::reflect::Key{ .name = "hint_icon_offscreen" } ]] string_t	m_iszIcon_Offscreen;
	[[= ks::reflect::Key{ .name = "hint_caption" } ]] string_t	m_iszCaption;
	[[= ks::reflect::Key{ .name = "hint_activator_caption" } ]] string_t	m_iszActivatorCaption;
	[[= ks::reflect::Key{ .name = "hint_color" } ]] color32		m_Color;
	[[= ks::reflect::Key{ .name = "hint_icon_offset" } ]] float		m_fIconOffset;
	[[= ks::reflect::Key{ .name = "hint_range" } ]] float		m_fRange;
	[[= ks::reflect::Key{ .name = "hint_pulseoption" } ]] uint8		m_iPulseOption;
	[[= ks::reflect::Key{ .name = "hint_alphaoption" } ]] uint8		m_iAlphaOption;
	[[= ks::reflect::Key{ .name = "hint_shakeoption" } ]] uint8		m_iShakeOption;
	[[= ks::reflect::Key{ .name = "hint_static" } ]] bool		m_bStatic;
	[[= ks::reflect::Key{ .name = "hint_nooffscreen" } ]] bool		m_bNoOffscreen;
	[[= ks::reflect::Key{ .name = "hint_forcecaption" } ]] bool		m_bForceCaption;
	[[= ks::reflect::Key{ .name = "hint_binding" } ]] string_t	m_iszBinding;
	[[= ks::reflect::Key{ .name = "hint_gamepad_binding" } ]] string_t	m_iszGamepadBinding;
	[[= ks::reflect::Key{ .name = "hint_allow_nodraw_target" } ]] bool		m_bAllowNoDrawTarget;
	[[= ks::reflect::Key{ .name = "hint_local_player_only" } ]] bool		m_bLocalPlayerOnly;
};


LINK_ENTITY_TO_CLASS( env_instructor_hint, CEnvInstructorHint );

IMPLEMENT_REFLECT_DATAMAP( CEnvInstructorHint )


#define LOCATOR_ICON_FX_PULSE_SLOW		0x00000001
#define LOCATOR_ICON_FX_ALPHA_SLOW		0x00000008
#define LOCATOR_ICON_FX_SHAKE_NARROW	0x00000040
#define LOCATOR_ICON_FX_STATIC			0x00000100	// This icon draws at a fixed location on the HUD.

//-----------------------------------------------------------------------------
// Purpose: Input handler for showing the message and/or playing the sound.
//-----------------------------------------------------------------------------
void CEnvInstructorHint::InputShowHint( inputdata_t &inputdata )
{
	static int s_InstructorServerHintEventCreate = 0;
	IGameEvent * event = gameeventmanager->CreateEvent( "instructor_server_hint_create", false, &s_InstructorServerHintEventCreate );
	if ( event )
	{
		CBaseEntity *pTargetEntity = gEntList.FindEntityByName( nullptr, m_iszHintTargetEntity );
		if( pTargetEntity == nullptr )
			pTargetEntity = inputdata.pActivator;

		if( pTargetEntity == nullptr )
			pTargetEntity = GetWorldEntity();

		char szColorString[128];
		Q_snprintf( szColorString, sizeof( szColorString ), "%.3d,%.3d,%.3d", m_Color.r, m_Color.g, m_Color.b );

		int iFlags = 0;
		
		iFlags |= (m_iPulseOption == 0) ? 0 : (LOCATOR_ICON_FX_PULSE_SLOW << (m_iPulseOption - 1));
		iFlags |= (m_iAlphaOption == 0) ? 0 : (LOCATOR_ICON_FX_ALPHA_SLOW << (m_iAlphaOption - 1));
		iFlags |= (m_iShakeOption == 0) ? 0 : (LOCATOR_ICON_FX_SHAKE_NARROW << (m_iShakeOption - 1));
		iFlags |= m_bStatic ? LOCATOR_ICON_FX_STATIC : 0;

		CBasePlayer *pActivator = nullptr;
		bool bFilterByActivator = m_bLocalPlayerOnly;
		if ( bFilterByActivator )
			pActivator = dynamic_cast<CBasePlayer*>( inputdata.pActivator );

		if ( inputdata.value.StringID() != NULL_STRING )
		{
			CBaseEntity *pTarget = gEntList.FindEntityByName( nullptr, inputdata.value.String() );
			pActivator = dynamic_cast<CBasePlayer*>( pTarget );
			if ( pActivator )
			{
				bFilterByActivator = true;
			}
		}
		else
		{
			if ( GameRules()->IsMultiplayer() == false )
			{
				pActivator = UTIL_GetLocalPlayer(); 
			}
			else
			{
				if ( !pTargetEntity )
				{
					Warning( "Failed to play server side instructor hint: no player specified for hint\n" );
					Assert( 0 );
				}
			}
		}

		const char *pActivatorCaption = m_iszActivatorCaption.ToCStr();
		if ( !pActivatorCaption || pActivatorCaption[ 0 ] == '\0' )
		{
			pActivatorCaption = m_iszCaption.ToCStr();
		}

		event->SetString( "hint_name", GetEntityName().ToCStr() );
		event->SetString( "hint_replace_key", m_iszReplace_Key.ToCStr() );
		event->SetInt( "hint_target", pTargetEntity->entindex() );
		event->SetInt( "hint_activator_userid", ( pActivator ? pActivator->GetUserID() : 0 ) );
		event->SetInt( "hint_timeout", m_iTimeout );
		event->SetString( "hint_icon_onscreen", m_iszIcon_Onscreen.ToCStr() );
		event->SetString( "hint_icon_offscreen", m_iszIcon_Offscreen.ToCStr() );
		event->SetString( "hint_caption", m_iszCaption.ToCStr() );
		event->SetString( "hint_activator_caption", pActivatorCaption );
		event->SetString( "hint_color", szColorString );
		event->SetFloat( "hint_icon_offset", m_fIconOffset );
		event->SetFloat( "hint_range", m_fRange );
		event->SetInt( "hint_flags", iFlags );
		event->SetString( "hint_binding", m_iszBinding.ToCStr() );
		event->SetString( "hint_gamepad_binding", m_iszGamepadBinding.ToCStr() );
		event->SetBool( "hint_allow_nodraw_target", m_bAllowNoDrawTarget );
		event->SetBool( "hint_nooffscreen", m_bNoOffscreen );
		event->SetBool( "hint_forcecaption", m_bForceCaption );
		event->SetBool( "hint_local_player_only", bFilterByActivator );

		gameeventmanager->FireEvent( event );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvInstructorHint::InputEndHint( inputdata_t &inputdata )
{
	static int s_InstructorServerHintEventStop = 0;
	IGameEvent * event = gameeventmanager->CreateEvent( "instructor_server_hint_stop", false, &s_InstructorServerHintEventStop );
	if ( event )
	{
		event->SetString( "hint_name", GetEntityName().ToCStr() );

		gameeventmanager->FireEvent( event );
	}
}

//-----------------------------------------------------------------------------
// Purpose: A generic target entity that gets replicated to the client for instructor hint targetting
//-----------------------------------------------------------------------------
class CInfoInstructorHintTarget : public CPointEntity
{
public:
	DECLARE_CLASS( CInfoInstructorHintTarget, CPointEntity );

	virtual int UpdateTransmitState( void )	// set transmit filter to transmit always
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

};

LINK_ENTITY_TO_CLASS( info_target_instructor_hint, CInfoInstructorHintTarget );

