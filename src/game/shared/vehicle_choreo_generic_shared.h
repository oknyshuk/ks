#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "networkvar.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct vehicleview_t
{
	DECLARE_CLASS_NOBASE( vehicleview_t );
	DECLARE_EMBEDDED_NETWORKVAR();

#ifndef CLIENT_DLL
#endif

	CNetworkVar( bool,	bClampEyeAngles );	// Perform eye Z clamping

	CNetworkVar( float,	flPitchCurveZero );	// Pitch values below this are clamped to zero.
	CNetworkVar( float,	flPitchCurveLinear );	// Pitch values above this are mapped directly.
	//		Spline in between.
	CNetworkVar( float,	flRollCurveZero );	// Pitch values below this are clamped to zero.
	CNetworkVar( float,	flRollCurveLinear );	// Roll values above this are mapped directly.
	//		Spline in between.
	CNetworkVar( float,	flFOV );				// FOV when in the vehicle.

	CNetworkVar( float, flYawMin );
	CNetworkVar( float, flYawMax );

	CNetworkVar( float, flPitchMin );
	CNetworkVar( float, flPitchMax );
};
