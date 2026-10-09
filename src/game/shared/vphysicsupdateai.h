#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// this is used to temporarily allow the vphysics shadow object to update the entity's position
// for entities that typically ignore those updates.
struct vphysicsupdateai_t
{
	float	startUpdateTime;
	float	stopUpdateTime;
	float	savedShadowControllerMaxSpeed;
};
