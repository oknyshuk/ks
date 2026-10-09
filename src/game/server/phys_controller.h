#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"

inline constexpr auto SF_THRUST_STARTACTIVE = 0x0001;
inline constexpr auto SF_THRUST_FORCE = 0x0002;
inline constexpr auto SF_THRUST_TORQUE = 0x0004;
inline constexpr auto SF_THRUST_LOCAL_ORIENTATION = 0x0008;
inline constexpr auto SF_THRUST_MASS_INDEPENDENT = 0x0010;
inline constexpr auto SF_THRUST_IGNORE_POS = 0x0020;

CBaseEntity *CreateKeepUpright( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, float flAngularLimit, bool bActive );
CBaseEntity *CreatePhysThruster( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, float flForce, float flForcetime, bool bActive, int nFlags );

AngularImpulse ComputeRotSpeedToAlignAxes( const Vector &testAxis, const Vector &alignAxis, const AngularImpulse &currentSpeed, 
										  float damping, float scale, float maxSpeed );
