#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

#include "reflect_annotations.h"
#include "baseentity.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CLight : public CPointEntity
{
public:
	DECLARE_CLASS( CLight, CPointEntity );

	using BaseClass::KeyValue_Impl;
	bool	KeyValue_Impl( const char *szKeyName, const char *szValue );
	void	Spawn_Impl( void );
	void	FadeThink( void );
	void	Use_Impl( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	void	TurnOn( void );
	void	TurnOff( void );
	void	Toggle( void );

	// Input handlers
	[[= ks::reflect::Input{ .name = "SetPattern", .type = FIELD_STRING } ]] void	InputSetPattern( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "FadeToPattern", .type = FIELD_STRING } ]] void	InputFadeToPattern( inputdata_t &inputdata );

	[[= ks::reflect::Input{ .name = "Toggle", .type = FIELD_VOID } ]] void	InputToggle( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOn", .type = FIELD_VOID } ]] void	InputTurnOn( inputdata_t &inputdata );
	[[= ks::reflect::Input{ .name = "TurnOff", .type = FIELD_VOID } ]] void	InputTurnOff( inputdata_t &inputdata );

	DECLARE_DATADESC();

private:
	[[= ks::reflect::Key{ .name = "style" } ]] int		m_iStyle;
	[[= ks::reflect::Key{ .name = "defaultstyle" } ]] int		m_iDefaultStyle;
	[[= ks::reflect::Key{ .name = "pattern" } ]] string_t m_iszPattern;
	char	m_iCurrentFade;
	char	m_iTargetFade;
};
