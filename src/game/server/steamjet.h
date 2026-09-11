//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines the server side of a steam jet particle system entity.
//
// $NoKeywords: $
//=============================================================================//

#ifndef STEAMJET_H
#define STEAMJET_H

#include "reflect_annotations.h"
#pragma	once

#include "baseparticleentity.h"

//NOTENOTE: Mirrored in cl_dlls\c_steamjet.cpp
#define	STEAM_NORMAL	0
#define	STEAM_HEATWAVE	1

//==================================================
// CSteamJet
//==================================================

class [[= ks::reflect::NetTable{ .name = "DT_SteamJet" } ]]
      [[= ks::reflect::From<"m_spawnflags", ks::reflect::Net{ .bits = 8, .flags = SPROP_UNSIGNED }>{} ]]
      CSteamJet : public CBaseParticleEntity
{
public:
	CSteamJet();
	DECLARE_CLASS( CSteamJet, CBaseParticleEntity );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	virtual void	Spawn( void );
	virtual void	Precache( void );

protected:

	// Input handlers.
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void InputTurnOn(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void InputTurnOff(inputdata_t &data);
	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void InputToggle(inputdata_t &data);

// Stuff from the datatable.
public:
	CNetworkVar( float, m_SpreadSpeed, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "SpreadSpeed", .input = true } ]] );
	CNetworkVar( float, m_Speed, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "Speed", .input = true } ]] );
	CNetworkVar( float, m_StartSize, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "StartSize" } ]] );
	CNetworkVar( float, m_EndSize, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "EndSize" } ]] );
	CNetworkVar( float, m_Rate, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "Rate", .input = true } ]] );
	CNetworkVar( float, m_JetLength, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "JetLength", .input = true } ]] );	// Length of the jet. Lifetime is derived from this.

	CNetworkVar( int, m_bEmit, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );		// Emit particles?
	CNetworkVar( bool, m_bFaceLeft, [[= ks::reflect::Net{ .bits = 1, .flags = SPROP_UNSIGNED } ]] );	// For support of legacy env_steamjet, which faced left instead of forward.
	[[= ks::reflect::Key{ .name = "InitialState" } ]] bool			m_InitialState;

	CNetworkVar( int, m_nType, [[= ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED } ]] [[= ks::reflect::Key{ .name = "Type" } ]] );		// Type of steam (normal, heatwave)
	CNetworkVar( float, m_flRollSpeed, [[= ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE } ]] [[= ks::reflect::Key{ .name = "RollSpeed" } ]] );

	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

#endif // STEAMJET_H

