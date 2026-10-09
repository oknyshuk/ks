#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: insulates client DLL from dependencies on vphysics

import std;
import ks.tier1;
#include "physics_shared.h"

struct objectparams_t;
struct solid_t;

// HACKHACK: Make this part of IClientSystem somehow???
extern bool PhysicsDLLInit( CreateInterfaceFn physicsFactory );

extern void PhysicsReset();
extern void PhysicsSimulate();
extern float PhysGetSyncCreateTime();
extern void VPhysicsShadowDataChanged( bool bCreate, C_BaseEntity *pEntity );

void PhysAddShadow( C_BaseEntity *pEntity );
void PhysRemoveShadow( C_BaseEntity *pEntity );
void PhysSetPredictionCommandNum( int iCommandNum );
