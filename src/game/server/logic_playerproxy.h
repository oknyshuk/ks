//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ====
//
// Purpose:
//
//=============================================================================

#ifndef LOGIC_PLAYERPROXY_H
#define LOGIC_PLAYERPROXY_H
#pragma once

#include "reflect_annotations.h"

//-----------------------------------------------------------------------------
// Purpose: Used to relay outputs/inputs from the player to the world and vice versa
//-----------------------------------------------------------------------------
class CLogicPlayerProxy : public CLogicalEntity
{
	DECLARE_CLASS( CLogicPlayerProxy, CLogicalEntity );
	DECLARE_DATADESC();

public:
	// FIXME: Subclass


	COutputEvent m_PlayerHasAmmo;
	COutputEvent m_PlayerHasNoAmmo;
	[[= ks::reflect::Key{ .name = "PlayerDied" } ]] COutputEvent m_PlayerDied;

	[[= ks::reflect::Key{ .name = "OnDuck" } ]] COutputEvent m_OnDuck;
	[[= ks::reflect::Key{ .name = "OnUnDuck" } ]] COutputEvent m_OnUnDuck;
	[[= ks::reflect::Key{ .name = "OnJump" } ]] COutputEvent m_OnJump;

	[[= ks::reflect::Key{ .name = "PlayerHealth" } ]] COutputInt m_RequestedPlayerHealth;


	void InputRequestPlayerHealth( inputdata_t &inputdata );
	void InputSetPlayerHealth( inputdata_t &inputdata );
	void InputRequestAmmoState( inputdata_t &inputdata );
	void InputEnableCappedPhysicsDamage( inputdata_t &inputdata );
	void InputDisableCappedPhysicsDamage( inputdata_t &inputdata );


	void Activate( void );

	bool PassesDamageFilter( const CTakeDamageInfo &info );

	EHANDLE m_hPlayer;
};


#endif	// LOGIC_PLAYERPROXY_H
