#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

class IPhysicsObject;

class CBaseEntity;
class IPhysicsFluidController;

void PhysicsSplash( IPhysicsFluidController *pFluid, IPhysicsObject *pObject, CBaseEntity *pEntity );
