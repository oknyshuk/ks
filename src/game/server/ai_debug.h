#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "ai_debug_shared.h"
#include "gametrace.h"
#include "engine/IEngineTrace.h"

class CAI_BaseNPC;
class CBaseEntity;

// This dumps a summary result on exit
//#define PROFILE_AI 1

#define AI_PROFILE_SCOPE_BEGIN( tag )	if (0) ; else { 
#define AI_PROFILE_SCOPE_BEGIN_( pszName )	if (0) ; else { AI_PROFILE_SCOPE_( pszName )
#define AI_PROFILE_SCOPE_END()			} do {} while (0)

#define VProfAI() false

#ifndef AI_PROFILE_SCOPE_
#define AI_PROFILE_SCOPE_( pszName ) 	((void)0)
#endif

enum AIMsgFlags
{
	AIMF_IGNORE_SELECTED = 0x01
};

using ::DevMsg;
void DevMsg( CAI_BaseNPC *pAI, unsigned flags, const char *pszFormat, ... ) [[gnu::format(printf, 3, 4)]];
void DevMsg( CAI_BaseNPC *pAI, const char *pszFormat, ... ) [[gnu::format(printf, 2, 3)]];


//-----------------------------------------------------------------------------
// Purpose: Use this to perform AI tracelines that are trying to determine LOS between points.
//			LOS checks between entities should use FVisible.
//-----------------------------------------------------------------------------
void AI_TraceLOS( const Vector& vecAbsStart, const Vector& vecAbsEnd, CBaseEntity *pLooker, trace_t *ptr, ITraceFilter *pFilter = nullptr );

//-----------------------------------------------------------------------------

#ifdef DEBUG
extern bool g_fTestSteering;
#define TestingSteering() g_fTestSteering
#else
#define TestingSteering() false
#endif

//-----------------------------------------------------------------------------


#ifdef _DEBUG
extern ConVar ai_debug_doors;
#define AIIsDebuggingDoors( pNPC ) ( ai_debug_doors.GetBool() && pNPC->m_bSelected )
#define AIDoorDebugMsg( pNPC, msg )	if ( !AIIsDebuggingDoors( pNPC ) ) ; else Msg( msg )
#else
#define AIIsDebuggingDoors( pNPC ) (false)
#define AIDoorDebugMsg( pNPC, msg )	((void)(0)) 
#endif


//-----------------------------------------------------------------------------
