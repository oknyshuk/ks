#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;
import ks.tier1;

class IClientEntity;
class ClientClass;
class IClientNetworkable;
class CBaseHandle;
class IClientUnknown;

// Cached info for networked entities.
// NOTE: Changing this changes the interface between engine & client
struct EntityCacheInfo_t
{
	// Cached off because GetClientNetworkable is called a *lot*
	IClientNetworkable *m_pNetworkable;
	unsigned short m_BaseEntitiesIndex;	// Index into m_BaseEntities (or m_BaseEntities.InvalidIndex() if none).
	unsigned short m_bDormant;	// cached dormant state - this is only a bit
};

//-----------------------------------------------------------------------------
// Purpose: Exposes IClientEntity's to engine
//-----------------------------------------------------------------------------
class IClientEntityList
{
public:
	// Get IClientNetworkable interface for specified entity
	virtual IClientNetworkable*	GetClientNetworkable( int entnum ) = 0;
	virtual IClientNetworkable*	GetClientNetworkableFromHandle( CBaseHandle hEnt ) = 0;
	virtual IClientUnknown*		GetClientUnknownFromHandle( CBaseHandle hEnt ) = 0;

	// NOTE: This function is only a convenience wrapper.
	// It returns GetClientNetworkable( entnum )->GetIClientEntity().
	virtual IClientEntity*		GetClientEntity( int entnum ) = 0;
	virtual IClientEntity*		GetClientEntityFromHandle( CBaseHandle hEnt ) = 0;

	// Returns number of entities currently in use
	virtual int					NumberOfEntities( bool bIncludeNonNetworkable ) = 0;

	// Returns highest index actually used
	virtual int					GetHighestEntityIndex( void ) = 0;

	// Sizes entity list to specified size
	virtual void				SetMaxEntities( int maxents ) = 0;
	virtual int					GetMaxEntities( ) = 0;
	virtual EntityCacheInfo_t	*GetClientNetworkableArray() = 0;
};

extern IClientEntityList *entitylist;

inline constexpr const char *VCLIENTENTITYLIST_INTERFACE_VERSION = "VClientEntityList003";
