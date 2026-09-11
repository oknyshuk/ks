//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Contains the set of functions for manipulating entity hierarchies.
//
// $NoKeywords: $
//=============================================================================//

#ifndef HIERARCHY_H
#define HIERARCHY_H

#include "utlvector.h"

class CBaseEntity;

void		UnlinkFromParent( CBaseEntity *pRemove );
void		TransferChildren( CBaseEntity *pOldParent, CBaseEntity *pNewParent );
void		LinkChild( CBaseEntity *pParent, CBaseEntity *pChild );
void		UnlinkAllChildren( CBaseEntity *pParent );
int			GetAllChildren( CBaseEntity *pParent, CUtlVector<CBaseEntity *> &list );
bool		EntityIsParentOf( CBaseEntity *pParent, CBaseEntity *pEntity );
int			GetAllInHierarchy( CBaseEntity *pParent, CUtlVector<CBaseEntity *> &list );

#endif // HIERARCHY_H
