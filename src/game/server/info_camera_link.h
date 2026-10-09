#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"

#include "baseentity.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CPointCamera;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity *CreateInfoCameraLink( CBaseEntity *pTarget, CPointCamera *pCamera );


//-----------------------------------------------------------------------------
// Sets up visibility 
//-----------------------------------------------------------------------------
void PointCameraSetupVisibility( CBaseEntity *pPlayer, int area, unsigned char *pvs, int pvssize );
