#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "edict.h"
#include "server_class.h"

class SendTable;

extern void LoadMapEntities( const char *pMapEntities );
extern void	DispatchObjectCollisionBox( edict_t *pent );
extern float DispatchObjectPhysicsVelocity( edict_t *pent, float moveTime );
extern ServerClass* DispatchGetObjectServerClass(edict_t *pent);
extern ServerClass* GetAllServerClasses();
extern void ResetGlobalState( void );
extern void ClearEntities( void );
extern void FreeContainingEntity( edict_t *ed );
