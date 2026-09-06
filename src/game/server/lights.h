//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef LIGHTS_H
#define LIGHTS_H

#include "reflect_annotations.h"
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CLight : public CPointEntity
{
public:
	DECLARE_CLASS( CLight, CPointEntity );

	bool	KeyValue( const char *szKeyName, const char *szValue );
	void	Spawn( void );
	void	FadeThink( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
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

#endif // LIGHTS_H
