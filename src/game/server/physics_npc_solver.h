#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

class CBaseEntity;
class CAI_BaseNPC;

extern CBaseEntity *NPCPhysics_CreateSolver( CAI_BaseNPC *pNPC, CBaseEntity *pPhysicsObject, bool disableCollisions, float separationDuration );
extern CBaseEntity *EntityPhysics_CreateSolver( CBaseEntity *pMovingEntity, CBaseEntity *pPhysicsBlocker, bool disableCollisions, float separationDuration );
bool NPCPhysics_SolverExists( CAI_BaseNPC *pNPC, CBaseEntity *pPhysicsObject );
