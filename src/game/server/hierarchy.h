#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Contains the set of functions for manipulating entity hierarchies.


class CBaseEntity;

void		UnlinkFromParent( CBaseEntity *pRemove );
void		TransferChildren( CBaseEntity *pOldParent, CBaseEntity *pNewParent );
void		LinkChild( CBaseEntity *pParent, CBaseEntity *pChild );
void		UnlinkAllChildren( CBaseEntity *pParent );
int			GetAllChildren( CBaseEntity *pParent, std::vector< CBaseEntity * > &list );
bool		EntityIsParentOf( CBaseEntity *pParent, CBaseEntity *pEntity );
int			GetAllInHierarchy( CBaseEntity *pParent, std::vector< CBaseEntity * > &list );
