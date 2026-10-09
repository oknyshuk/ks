#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation


import std;
import ks.tier1;

#if _DEBUG
extern bool g_bRenderingCameraView;		// For debugging (frustum fix for cameras)...
#endif



// near and far Z it uses to render the world.
inline constexpr int VIEW_NEARZ = 7;
//#define VIEW_FARZ	28400


//-----------------------------------------------------------------------------
// There's a difference between the 'current view' and the 'main view'
// The 'main view' is where the player is sitting. Current view is just
// what's currently being rendered, which, owing to monitors or water,
// could be just about anywhere.
//-----------------------------------------------------------------------------
const Vector &MainViewOrigin( int nSlot );
const QAngle &MainViewAngles( int nSlot );
const Vector &PrevMainViewOrigin( int nSlot );
const QAngle &PrevMainViewAngles( int nSlot );
const VMatrix &MainWorldToViewMatrix( int nSlot );
const Vector &MainViewForward( int nSlot );
const Vector &MainViewRight( int nSlot );
const Vector &MainViewUp( int nSlot );

const Vector &CurrentViewOrigin();
const QAngle &CurrentViewAngles();
const VMatrix &CurrentWorldToViewMatrix();
const Vector &CurrentViewForward();
const Vector &CurrentViewRight();
const Vector &CurrentViewUp();

void AllowCurrentViewAccess( bool allow );
bool IsCurrentViewAccessAllowed();

// Returns true of the sphere is outside the frustum defined by pPlanes.
// (planes point inwards).
bool R_CullSphere( const VPlane *pPlanes, int nPlanes, const Vector *pCenter, float radius );
float ScaleFOVByWidthRatio( float fovDegrees, float ratio );

extern ConVar mat_wireframe;

extern const ConVar *sv_cheats;


static inline int WireFrameMode( void )
{
	if ( !sv_cheats )
	{
		sv_cheats = cvar->FindVar( "sv_cheats" );
	}

	if ( sv_cheats && sv_cheats->GetBool() )
		return mat_wireframe.GetInt();
	else
		return 0;
}

static inline bool ShouldDrawInWireFrameMode( void )
{
	if ( !sv_cheats )
	{
		sv_cheats = cvar->FindVar( "sv_cheats" );
	}

	if ( sv_cheats && sv_cheats->GetBool() )
		return ( mat_wireframe.GetInt() != 0 );
	else
		return false;
}

void ComputeCameraVariables( const Vector &vecOrigin, const QAngle &vecAngles, Vector *pVecForward, Vector *pVecRight, Vector *pVecUp, VMatrix *pMatCamInverse );
