#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: Implements visual effects entities: sprites, beams, bubbles, etc.

import ks.vstdlib;
#include "reflect_annotations.h"
#include "networkvar.h"

import std;

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CSoundPatch;

//-----------------------------------------------------------------------------
// Implementation of the class that computes windspeed
//-----------------------------------------------------------------------------
class [[= ks::reflect::NetTable{ .name = "DT_EnvWindShared", .base = false } ]]
      [[= ks::reflect::From<"m_iMinWind", ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED }>{} ]]
      [[= ks::reflect::From<"m_iMaxWind", ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED }>{} ]]
      [[= ks::reflect::From<"m_iMinGust", ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED }>{} ]]
      [[= ks::reflect::From<"m_iMaxGust", ks::reflect::Net{ .bits = 10, .flags = SPROP_UNSIGNED }>{} ]]
      [[= ks::reflect::From<"m_flMinGustDelay", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_flMaxGustDelay", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_iGustDirChange", ks::reflect::Net{ .bits = 9, .flags = SPROP_UNSIGNED }>{} ]]
      [[= ks::reflect::From<"m_iWindSeed", ks::reflect::Net{ .bits = 32, .flags = SPROP_UNSIGNED }>{} ]]
      [[= ks::reflect::From<"m_iInitialWindDir", ks::reflect::Net{ .bits = 9, .flags = SPROP_UNSIGNED }>{} ]]
      [[= ks::reflect::From<"m_flInitialWindSpeed", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_flStartTime", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
      [[= ks::reflect::From<"m_flGustDuration", ks::reflect::Net{ .bits = 0, .flags = SPROP_NOSCALE }>{} ]]
      CEnvWindShared
{
public:
	DECLARE_CLASS_NOBASE( CEnvWindShared );
	DECLARE_EMBEDDED_NETWORKVAR();
	
	CEnvWindShared();
	~CEnvWindShared();

	CEnvWindShared( const CEnvWindShared & ) = delete;
	CEnvWindShared& operator= ( const CEnvWindShared& ) = delete;

	void Init( int iEntIndex, int iRandomSeed, float flTime, int iWindDir, float flInitialWindSpeed );

	void SetLocation( const Vector &location );

	// Method to update the wind speed
	// Time passed in here is global time, not delta time
	// The function returns the time at which it must be called again
	float WindThink( float flTime );

	// FIXME: These really should be private
	CNetworkVar( float, m_flStartTime );

	CNetworkVar( int, m_iWindSeed );		// random number seed...

	CNetworkVar( int, m_iMinWind );			// the slowest the wind can normally blow
	CNetworkVar( int, m_iMaxWind );			// the fastest the wind can normally blow
	CNetworkVar( int, m_windRadius );		// the radius this entity affects with its windiness, so a map can have multiple
	CNetworkVar( int, m_iMinGust );			// the slowest that a gust can be
	CNetworkVar( int, m_iMaxGust );			// the fastest that a gust can be

	CNetworkVar( float, m_flMinGustDelay );	// min time between gusts
	CNetworkVar( float, m_flMaxGustDelay );	// max time between gusts

	CNetworkVar( float, m_flGustDuration );	// max time between gusts

	CNetworkVar( int, m_iGustDirChange );	// max number of degrees wind dir changes on gusts.
	CNetworkVector( m_location );			// The location of this wind controller

	int m_iszGustSound;		// name of the wind sound to play for gusts.
	int m_iWindDir;			// wind direction (yaw)
	float m_flWindSpeed;	// the wind speed


	Vector m_currentWindVector;	// For all the talk of proper prediction, we ended up just storing and returning through a static vector.  Now we can have multiple env_wind, so we need this in here.
	Vector m_CurrentSwayVector;
	Vector m_PrevSwayVector;

	CNetworkVar( int, m_iInitialWindDir );
	CNetworkVar( float, m_flInitialWindSpeed );

#ifndef CLIENT_DLL
	COutputEvent m_OnGustStart;
	COutputEvent m_OnGustEnd;
#endif

private:
	void ComputeWindVariation( float flTime );

	// Updates the wind sound
	void UpdateWindSound( float flTotalWindSpeed );

	void UpdateTreeSway( float flTime );

	float	m_flVariationTime;
	float	m_flSwayTime;
	float	m_flSimTime;		// What's the time I last simulated up to?
	float	m_flSwitchTime;		// when do I actually switch from gust to not gust
	float	m_flAveWindSpeed;	// the average wind speed
	bool	m_bGusting;			// is the wind gusting right now?

	float m_flWindAngleVariation;
	float m_flWindSpeedVariation;



	int m_iEntIndex;

	// Used to generate random numbers
	CUniformRandomStream m_Stream;

	// NOTE: In order to make this algorithm independent of calling frequency
	// I have to decouple the stream used to generate average wind speed
	// and the stream used to generate wind variation since they are
	// simulated using different timesteps
	CUniformRandomStream m_WindVariationStream;

	// Used to generate the wind sound...
	CSoundPatch *m_pWindSound;
};

//-----------------------------------------------------------------------------
inline void CEnvWindShared::SetLocation( const Vector &location )
{
	m_location = location;
}


//-----------------------------------------------------------------------------
// Method to sample the wind speed at a particular location
//-----------------------------------------------------------------------------
Vector GetWindspeedAtLocation( const Vector &location );

//-----------------------------------------------------------------------------
// Method to sample the windspeed at a particular time
//-----------------------------------------------------------------------------
void GetWindspeedAtTime( float flTime, Vector &vecVelocity );

//-----------------------------------------------------------------------------
// Method to reset wind speed..
//-----------------------------------------------------------------------------
void ResetWindspeed();
