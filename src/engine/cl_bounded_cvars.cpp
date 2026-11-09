// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

// Purpose: baseclientstate.cpp: implementation of the CBaseClientState class.

import ks.tier1;
#include "client.h"
import std;

// These are the server cvars that control our cvars.
extern int ClampClientRate( int nRate );

extern ConVar  sv_mincmdrate;
extern ConVar  sv_maxcmdrate;
extern ConVar  sv_minupdaterate;
extern ConVar  sv_maxupdaterate;
extern ConVar  sv_client_cmdrate_difference;

extern ConVar  sv_client_interp;
extern ConVar  sv_client_predict;


// ------------------------------------------------------------------------------------------ //
// rate
// ------------------------------------------------------------------------------------------ //

void CL_RateCvarChanged( ConVar *pConVar, const char *pOldValue, float flOldValue )
{
}

static float CBoundedCvar_Rate_GetFloat( const ConVar_ServerBounded &bounded )
{
#ifdef DEDICATED
	return bounded.GetBaseFloatValue();
#else
	if ( GetBaseLocalClient().m_nSignonState >= SIGNONSTATE_FULL )
	{
		int nRate = (int)bounded.GetBaseFloatValue();
		return (float)ClampClientRate( nRate );
	}
	else
	{
		return bounded.GetBaseFloatValue();
	}
#endif
}

static ConVar_ServerBounded cl_rate_var( CBoundedCvar_Rate_GetFloat,
	"rate", 
		  "DEFAULT_RATE",
		  FCVAR_ARCHIVE | FCVAR_USERINFO,
		  "Max bytes/sec the host can receive data", 
		  CL_RateCvarChanged );
ConVar_ServerBounded *cl_rate = &cl_rate_var;


// ------------------------------------------------------------------------------------------ //
// cl_cmdrate
// ------------------------------------------------------------------------------------------ //

static float CBoundedCvar_CmdRate_GetFloat( const ConVar_ServerBounded &bounded )
{
	float flCmdRate = bounded.GetBaseFloatValue();

#ifndef DEDICATED
	if ( GetBaseLocalClient().ishltv )
	{
		extern ConVar tv_snapshotrate;
		return tv_snapshotrate.GetFloat();
	}
	else if ( sv_mincmdrate.GetInt() != 0 && GetBaseLocalClient().m_nSignonState >= SIGNONSTATE_FULL )
	{
		// First, we make it stay within range of cl_updaterate.
		float diff = flCmdRate - cl_updaterate->GetFloat();
		if ( fabs( diff ) > sv_client_cmdrate_difference.GetFloat() )
		{
			if ( diff > 0 )
				flCmdRate = cl_updaterate->GetFloat() + sv_client_cmdrate_difference.GetFloat();
			else
				flCmdRate = cl_updaterate->GetFloat() - sv_client_cmdrate_difference.GetFloat();
		}

		// Then we clamp to the min/max values the server has set.
		return clamp( flCmdRate, sv_mincmdrate.GetFloat(), sv_maxcmdrate.GetFloat() );
	}
	else
#endif
	{
		return flCmdRate;
	}
}

static ConVar_ServerBounded cl_cmdrate_var( CBoundedCvar_CmdRate_GetFloat,
	"cl_cmdrate", 
		  "64", 
		  FCVAR_ARCHIVE | FCVAR_USERINFO, 
		  "Max number of command packets sent to server per second", true, MIN_CMD_RATE, true, MAX_CMD_RATE );
ConVar_ServerBounded *cl_cmdrate = &cl_cmdrate_var;



// ------------------------------------------------------------------------------------------ //
// cl_updaterate
// ------------------------------------------------------------------------------------------ //

static float CBoundedCvar_UpdateRate_GetFloat( const ConVar_ServerBounded &bounded )
{
	// Clamp to the min/max values the server has set.
	//
	// This cvar only takes effect on the server anyway, and this is done there too,
	// but we have this here so they'll get the **note thing telling them the value 
	// isn't functioning the way they set it.
	float flValue = bounded.GetBaseFloatValue();
#ifndef DEDICATED
	if ( GetBaseLocalClient().ishltv )
	{
		extern ConVar tv_snapshotrate;
		return tv_snapshotrate.GetFloat();
	}
#endif
	return clamp( flValue, sv_minupdaterate.GetFloat(), sv_maxupdaterate.GetFloat() );
}

static ConVar_ServerBounded cl_updaterate_var( CBoundedCvar_UpdateRate_GetFloat,
	"cl_updaterate",
		  "64", 
		  FCVAR_ARCHIVE | FCVAR_USERINFO | FCVAR_NOT_CONNECTED, 
		  "Number of packets per second of updates you are requesting from the server" );
ConVar_ServerBounded *cl_updaterate = &cl_updaterate_var;
