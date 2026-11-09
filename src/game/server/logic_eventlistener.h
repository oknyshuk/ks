#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2008 Valve Corporation

#include "reflect_annotations.h"
#include "baseentity.h"
#include "GameEventListener.h"

//-----------------------------------------------------------------------------
// Purpose: Used to relay outputs/inputs from the events to the world and vice versa
//-----------------------------------------------------------------------------
class CLogicEventListener : public CLogicalEntity, public CGameEventListener
{
	DECLARE_CLASS( CLogicEventListener, CLogicalEntity );
	DECLARE_DATADESC();

public:
	// FIXME: Subclass

	void Spawn_Impl( void );
	void FireGameEvent( IGameEvent *event );
	
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
	CLogicEventListenerItemEquip() { SetGameEventHandler( this ); }

	void Spawn_Impl( void );
	void FireGameEvent( IGameEvent *event );

	[[= ks::reflect::Input{ .name = "Enable", .type = FIELD_VOID } ]] void	InputEnable( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "Disable", .type = FIELD_VOID } ]] void	InputDisable( inputdata_t &inputdata );

private:
	[[= ks::reflect::Key{ .name = "IsEnabled" } ]] bool		m_bIsEnabled;
	[[= ks::reflect::Key{ .name = "TeamNum" } ]] int			m_nTeam;
	[[= ks::reflect::Key{ .name = "WeaponClassname" } ]] string_t	m_szWeaponClassname;
	[[= ks::reflect::Key{ .name = "WeaponType" } ]] int			m_nWeaponType;

	[[= ks::reflect::Key{ .name = "OnEventFired" } ]] COutputEvent m_OnEventFired;
};
