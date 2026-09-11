//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines the server side of a steam jet particle system entity.
//
// $NoKeywords: $
//=============================================================================//

#ifndef SMOKESTACK_H
#define SMOKESTACK_H

#include "reflect_annotations.h"
#pragma	once

#include "baseparticleentity.h"

//==================================================
// CSmokeStack
//==================================================

class CSmokeStackLightInfo
{
public:
	DECLARE_CLASS_NOBASE( CSmokeStackLightInfo );
	DECLARE_SIMPLE_DATADESC();
	DECLARE_NETWORKVAR_CHAIN();

	CNetworkVector( m_vPos );
	CNetworkVector( m_vColor );
	CNetworkVar( float, m_flIntensity );
};

class [[= ks::reflect::NetTable{ .name = "DT_SmokeStack" } ]]
      [[= ks::reflect::From<"m_DirLight.m_vPos", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .raw = true }>{} ]]
      [[= ks::reflect::From<"m_DirLight.m_vColor", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .raw = true }>{} ]]
      [[= ks::reflect::From<"m_DirLight.m_flIntensity", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .raw = true }>{} ]]
      [[= ks::reflect::From<"m_AmbientLight.m_vPos", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .raw = true }>{} ]]
      [[= ks::reflect::From<"m_AmbientLight.m_vColor", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .raw = true }>{} ]]
      [[= ks::reflect::From<"m_AmbientLight.m_flIntensity", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE, .raw = true }>{} ]]
      CSmokeStack : public CBaseParticleEntity
{
public:
	DECLARE_CLASS( CSmokeStack, CBaseParticleEntity );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

					CSmokeStack();
					~CSmokeStack();

	virtual void	Spawn( void );
	virtual void	Activate();
	virtual bool	KeyValue( const char *szKeyName, const char *szValue );
	virtual void	Precache();


protected:

	// Input handlers.
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void	InputTurnOn(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void	InputTurnOff(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void	InputToggle(inputdata_t &data);

	void	RecalcWindVector();


// Stuff from the datatable.
public:
	CNetworkVar( float, m_SpreadSpeed, [[= ks::reflect::Key{ .name = "SpreadSpeed", .input = true } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_Speed, [[= ks::reflect::Key{ .name = "Speed", .input = true } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_StartSize, [[= ks::reflect::Key{ .name = "StartSize" } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_EndSize, [[= ks::reflect::Key{ .name = "EndSize" } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_Rate, [[= ks::reflect::Key{ .name = "Rate", .input = true } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	CNetworkVar( float, m_JetLength, [[= ks::reflect::Key{ .name = "JetLength", .input = true } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );	// Length of the jet. Lifetime is derived from this.
	CNetworkVar( float, m_flRollSpeed, [[= ks::reflect::Key{ .name = "Roll" } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );

	CNetworkVar( int, m_bEmit, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );		// Emit particles?
	CNetworkVar( float, m_flBaseSpread, [[= ks::reflect::Key{ .name = "BaseSpread" } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	
	CSmokeStackLightInfo		m_AmbientLight;
	CSmokeStackLightInfo		m_DirLight;

	CNetworkVar( float, m_flTwist, [[= ks::reflect::Key{ .name = "Twist" } ]] [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );
	
	string_t		m_strMaterialModel;
	CNetworkVar( int, m_iMaterialModel, [[= ks::reflect::Net{ .bits = 16, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Proxy<SendProxy_IntAddOne, ks::reflect::WIRE_SEND>{} ]] );

	[[= ks::reflect::Key{ .name = "WindAngle" } ]] int				m_WindAngle;
	[[= ks::reflect::Key{ .name = "WindSpeed" } ]] int				m_WindSpeed;
	CNetworkVector( m_vWind, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] );		// m_vWind is just calculated from m_WindAngle and m_WindSpeed.

	[[= ks::reflect::Key{ .name = "InitialState" } ]] bool			m_InitialState;
};

#endif // SMOKESTACK_H

