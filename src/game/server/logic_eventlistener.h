//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ====
//
// Purpose:
//
//=============================================================================

#ifndef LOGIC_EVENTLISTENER_H
#define LOGIC_EVENTLISTENER_H

#include "reflect_annotations.h"
#pragma once

//-----------------------------------------------------------------------------
// Purpose: Used to relay outputs/inputs from the events to the world and vice versa
//-----------------------------------------------------------------------------
class CLogicEventListener : public CLogicalEntity, public CGameEventListener
{
	DECLARE_CLASS( CLogicEventListener, CLogicalEntity );
	DECLARE_DATADESC();

public:
	// FIXME: Subclass

	virtual void Spawn( void );
	virtual void FireGameEvent( IGameEvent *event );
	
	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );

private:

	[[= ks::reflect::Key{ .name = "EventName" } ]] string_t	m_iszEventName;
	[[= ks::reflect::Key{ .name = "IsEnabled" } ]] bool		m_bIsEnabled;
	[[= ks::reflect::Key{ .name = "TeamNum" } ]] int			m_nTeam;
	[[= ks::reflect::Key{ .name = "FetchEventData" } ]] bool		m_bFetchEventData;

	[[= ks::reflect::Key{ .name = "OnEventFired" } ]] COutputEvent m_OnEventFired;

};

//-----------------------------------------------------------------------------
// Purpose: Used to relay outputs/inputs from the events to the world and vice versa
//-----------------------------------------------------------------------------
class CLogicEventListenerItemEquip : public CLogicEventListener
{
	DECLARE_CLASS( CLogicEventListenerItemEquip, CLogicEventListener );
	DECLARE_DATADESC();

public:
	virtual void Spawn( void );
	virtual void FireGameEvent( IGameEvent *event );

	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );

private:
	[[= ks::reflect::Key{ .name = "IsEnabled" } ]] bool		m_bIsEnabled;
	[[= ks::reflect::Key{ .name = "TeamNum" } ]] int			m_nTeam;
	[[= ks::reflect::Key{ .name = "WeaponClassname" } ]] string_t	m_szWeaponClassname;
	[[= ks::reflect::Key{ .name = "WeaponType" } ]] int			m_nWeaponType;

	[[= ks::reflect::Key{ .name = "OnEventFired" } ]] COutputEvent m_OnEventFired;
};
#endif	// LOGIC_EVENTLISTENER_H
