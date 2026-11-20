// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#pragma once

#define NUM_PARENTATTACHMENT_BITS	6
#ifndef AI_CriteriaSet
#define AI_CriteriaSet ResponseRules::CriteriaSet 
#endif
#define CREATE_ENTITY( newClass, className ) _CreateEntity( (newClass*)nullptr, className )
#define CREATE_UNSAVED_ENTITY( newClass, className ) _CreateEntityTemplate( (newClass*)nullptr, className )
#define SetTouch( a ) m_pfnTouch = static_cast <void (CBaseEntity::*)(CBaseEntity *)> (a)
#define SetUse( a ) m_pfnUse = static_cast <void (CBaseEntity::*)( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )> (a)
#define SetThink( a ) CBaseEntity::ThinkSet( static_cast <void (CBaseEntity::*)(void)> (a), 0, nullptr )
#define SetContextThink( a, b, context ) CBaseEntity::ThinkSet( static_cast <void (CBaseEntity::*)(void)> (a), (b), context )
#define SetMoveDone( a ) \
		(void)(m_pfnMoveDone = static_cast <void (CBaseEntity::*)(void)> (a))
#define ABS_QUERY_GUARD( state ) CAbsQueryScopeGuard s_AbsQueryGuard( state );
