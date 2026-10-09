#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#ifdef __cplusplus
#include "vphysics_interface.h"
class IPhysics;
class IPhysicsCollision;

extern CreateInterfaceFn	GetPhysicsFactory( void );

extern "C" {
#endif

// tools need to force the path
void					PhysicsDLLPath( const char *pPathname );

#ifdef __cplusplus
}
#endif
